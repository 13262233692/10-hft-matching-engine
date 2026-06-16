#include "net/TCPServer.h"
#include <iostream>
#include <cstring>
#include <iomanip>

namespace HFT {

TCPServer::TCPServer(uint16_t port, OrderBook& ob)
    : port(port), orderBook(ob), listenSocket(INVALID_SOCKET_VALUE),
      running(false), clientCount(0), messagesReceived(0),
      bytesReceived(0), orderIdCounter(0) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

TCPServer::~TCPServer() {
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool TCPServer::start() {
    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET_VALUE) {
        return false;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(port);

    if (bind(listenSocket, reinterpret_cast<sockaddr*>(&serverAddr),
        sizeof(serverAddr)) == SOCKET_ERROR) {
        cleanupSocket(listenSocket);
        listenSocket = INVALID_SOCKET_VALUE;
        return false;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        cleanupSocket(listenSocket);
        listenSocket = INVALID_SOCKET_VALUE;
        return false;
    }

    running.store(true);
    acceptThread = std::make_unique<std::thread>(&TCPServer::acceptLoop, this);

    std::cout << "[TCP] Server started on port " << port << std::endl;
    return true;
}

void TCPServer::stop() {
    running.store(false);

    if (listenSocket != INVALID_SOCKET_VALUE) {
#ifdef _WIN32
        shutdown(listenSocket, SD_BOTH);
        closesocket(listenSocket);
#else
        shutdown(listenSocket, SHUT_RDWR);
        close(listenSocket);
#endif
        listenSocket = INVALID_SOCKET_VALUE;
    }

    if (acceptThread && acceptThread->joinable()) {
        acceptThread->join();
    }

    for (auto& thread : clientThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    clientThreads.clear();
}

void TCPServer::acceptLoop() {
    while (running.load()) {
        sockaddr_in clientAddr;
        int addrLen = sizeof(clientAddr);

        SOCKET_TYPE clientSocket = accept(listenSocket,
            reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);

        if (clientSocket == INVALID_SOCKET_VALUE) {
            if (running.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            continue;
        }

        clientCount++;
        std::cout << "[TCP] Client connected: " << clientCount.load() << std::endl;

        clientThreads.emplace_back(&TCPServer::handleClient, this, clientSocket);
    }
}

void TCPServer::handleClient(SOCKET_TYPE clientSocket) {
    const size_t BUFFER_SIZE = 65536;
    char buffer[BUFFER_SIZE];
    std::vector<char> messageBuffer;

    while (running.load()) {
        int bytesRead = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);

        if (bytesRead <= 0) {
            break;
        }

        bytesReceived += bytesRead;
        messageBuffer.insert(messageBuffer.end(), buffer, buffer + bytesRead);

        size_t pos = 0;
        while (pos < messageBuffer.size()) {
            size_t msgEnd = std::string::npos;
            for (size_t i = pos; i + 6 < messageBuffer.size(); ++i) {
                if (messageBuffer[i] == '1' && messageBuffer[i+1] == '0' &&
                    messageBuffer[i+2] == '=' &&
                    messageBuffer[i+3] >= '0' && messageBuffer[i+3] <= '9' &&
                    messageBuffer[i+4] >= '0' && messageBuffer[i+4] <= '9' &&
                    messageBuffer[i+5] >= '0' && messageBuffer[i+5] <= '9' &&
                    messageBuffer[i+6] == '\x01') {
                    msgEnd = i + 7;
                    break;
                }
            }

            if (msgEnd == std::string::npos) {
                break;
            }

            if (processMessage(messageBuffer.data() + pos, msgEnd - pos)) {
                messagesReceived++;
            }

            pos = msgEnd;
        }

        if (pos > 0) {
            messageBuffer.erase(messageBuffer.begin(), messageBuffer.begin() + pos);
        }
    }

    cleanupSocket(clientSocket);
    clientCount--;
    std::cout << "[TCP] Client disconnected: " << clientCount.load() << std::endl;
}

bool TCPServer::processMessage(const char* data, size_t length) {
    FIXMessage msg;
    if (!parser.parse(data, length, msg)) {
        return false;
    }

    switch (msg.msgType) {
        case FIXMsgType::NEW_ORDER: {
            Order* order = new Order();
            if (parser.parseNewOrder(msg, *order)) {
                order->orderId = ++orderIdCounter;
                double price = order->price / 10000.0;
                const char* sideStr = (order->side == Side::BUY) ? "BUY" : "SELL";
                std::cout << "[ORDER] " << sideStr
                          << " " << order->quantity
                          << " @ " << std::fixed << std::setprecision(4) << price
                          << " ID=" << order->orderId << std::endl;
                if (orderCallback) {
                    orderCallback(order);
                } else {
                    orderBook.addOrder(order);
                }
            } else {
                std::cout << "[ERROR] Failed to parse new order" << std::endl;
                delete order;
            }
            break;
        }
        case FIXMsgType::CANCEL_ORDER: {
            uint64_t orderId = parser.parseCancelOrder(msg);
            if (orderId > 0) {
                if (cancelCallback) {
                    cancelCallback(orderId);
                } else {
                    orderBook.cancelOrder(orderId);
                }
            }
            break;
        }
        case FIXMsgType::HEARTBEAT:
        case FIXMsgType::LOGON:
        case FIXMsgType::LOGOUT:
        default:
            break;
    }

    return true;
}

void TCPServer::sendResponse(SOCKET_TYPE clientSocket, const char* data, size_t length) {
    send(clientSocket, data, static_cast<int>(length), 0);
}

void TCPServer::cleanupSocket(SOCKET_TYPE sock) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

}
