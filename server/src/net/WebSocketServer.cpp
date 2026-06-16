#include "net/WebSocketServer.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <iomanip>
#include <map>

namespace HFT {

WebSocketServer::WebSocketServer(uint16_t port, OrderBook& ob)
    : port(port), orderBook(ob), listenSocket(INVALID_SOCKET_VALUE),
      running(false), clientCount(0), broadcastIntervalMs(50),
      sequenceNumber(0) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

WebSocketServer::~WebSocketServer() {
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool WebSocketServer::start() {
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
    acceptThread = std::make_unique<std::thread>(&WebSocketServer::acceptLoop, this);
    broadcastThread = std::make_unique<std::thread>(&WebSocketServer::broadcastLoop, this);

    std::cout << "[WS] Server started on port " << port << std::endl;
    return true;
}

void WebSocketServer::stop() {
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
    if (broadcastThread && broadcastThread->joinable()) {
        broadcastThread->join();
    }

    std::lock_guard<std::mutex> lock(clientsMutex);
    for (auto client : clients) {
        cleanupSocket(client);
    }
    clients.clear();
}

void WebSocketServer::acceptLoop() {
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

        if (performHandshake(clientSocket)) {
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                clients.push_back(clientSocket);
                clientCount++;
                std::cout << "[WS] Client connected: " << clientCount.load() << std::endl;
            }
            std::thread(&WebSocketServer::handleClient, this, clientSocket).detach();
        } else {
            cleanupSocket(clientSocket);
        }
    }
}

bool WebSocketServer::performHandshake(SOCKET_TYPE clientSocket) {
    const size_t BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];

    int bytesRead = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
    if (bytesRead <= 0) return false;

    buffer[bytesRead] = '\0';
    std::string request(buffer);

    size_t keyPos = request.find("Sec-WebSocket-Key:");
    if (keyPos == std::string::npos) return false;

    size_t keyStart = keyPos + 19;
    size_t keyEnd = request.find("\r\n", keyStart);
    std::string clientKey = request.substr(keyStart, keyEnd - keyStart);

    while (!clientKey.empty() && (clientKey[0] == ' ' || clientKey[0] == '\t')) {
        clientKey.erase(clientKey.begin());
    }
    while (!clientKey.empty() && (clientKey.back() == ' ' || clientKey.back() == '\t')) {
        clientKey.pop_back();
    }

    std::string acceptKey = clientKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string sha1Hash = sha1(acceptKey);
    std::string acceptEncoded = base64Encode(reinterpret_cast<const uint8_t*>(sha1Hash.c_str()), 20);

    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << acceptEncoded << "\r\n"
             << "\r\n";

    std::string responseStr = response.str();
    int sent = send(clientSocket, responseStr.c_str(), static_cast<int>(responseStr.size()), 0);
    return sent == static_cast<int>(responseStr.size());
}

std::string WebSocketServer::sha1(const std::string& input) {
    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;

    uint64_t bitLen = static_cast<uint64_t>(input.size()) * 8;
    std::vector<uint8_t> msg(input.begin(), input.end());

    msg.push_back(0x80);
    while (msg.size() % 64 != 56) {
        msg.push_back(0x00);
    }

    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<uint8_t>((bitLen >> (i * 8)) & 0xFF));
    }

    auto rotateLeft = [](uint32_t value, int bits) {
        return (value << bits) | (value >> (32 - bits));
    };

    for (size_t offset = 0; offset < msg.size(); offset += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(msg[offset + i * 4]) << 24) |
                   (static_cast<uint32_t>(msg[offset + i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(msg[offset + i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(msg[offset + i * 4 + 3]));
        }

        for (int i = 16; i < 80; ++i) {
            w[i] = rotateLeft(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;

        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | (~b & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            uint32_t temp = rotateLeft(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rotateLeft(b, 30);
            b = a;
            a = temp;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    std::string result;
    result.reserve(20);
    auto appendWord = [&](uint32_t word) {
        result.push_back(static_cast<char>((word >> 24) & 0xFF));
        result.push_back(static_cast<char>((word >> 16) & 0xFF));
        result.push_back(static_cast<char>((word >> 8) & 0xFF));
        result.push_back(static_cast<char>(word & 0xFF));
    };
    appendWord(h0);
    appendWord(h1);
    appendWord(h2);
    appendWord(h3);
    appendWord(h4);

    return result;
}

std::string WebSocketServer::base64Encode(const uint8_t* data, size_t length) {
    static const char* chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string result;
    result.reserve(((length + 2) / 3) * 4);

    size_t i = 0;
    while (i < length) {
        uint32_t triple = 0;
        int bytes = 0;

        for (int j = 0; j < 3 && i < length; ++j, ++i) {
            triple = (triple << 8) | data[i];
            bytes++;
        }

        triple <<= (3 - bytes) * 8;

        for (int j = 0; j < 4; ++j) {
            if (j <= bytes) {
                result.push_back(chars[(triple >> (18 - j * 6)) & 0x3F]);
            } else {
                result.push_back('=');
            }
        }
    }

    return result;
}

void WebSocketServer::unmaskPayload(uint8_t* payload, uint64_t length, uint32_t maskingKey) {
    uint8_t key[4];
    key[0] = static_cast<uint8_t>((maskingKey >> 24) & 0xFF);
    key[1] = static_cast<uint8_t>((maskingKey >> 16) & 0xFF);
    key[2] = static_cast<uint8_t>((maskingKey >> 8) & 0xFF);
    key[3] = static_cast<uint8_t>(maskingKey & 0xFF);

    for (uint64_t i = 0; i < length; ++i) {
        payload[i] ^= key[i % 4];
    }
}

bool WebSocketServer::readFrame(SOCKET_TYPE clientSocket, WSFrame& frame) {
    uint8_t header[2];
    int bytesRead = recv(clientSocket, reinterpret_cast<char*>(header), 2, 0);
    if (bytesRead != 2) return false;

    frame.fin = (header[0] & 0x80) != 0;
    frame.opcode = static_cast<WSFrame::Opcode>(header[0] & 0x0F);
    frame.masked = (header[1] & 0x80) != 0;
    frame.payloadLength = header[1] & 0x7F;

    if (frame.payloadLength == 126) {
        uint8_t extLen[2];
        bytesRead = recv(clientSocket, reinterpret_cast<char*>(extLen), 2, 0);
        if (bytesRead != 2) return false;
        frame.payloadLength = (static_cast<uint16_t>(extLen[0]) << 8) | extLen[1];
    } else if (frame.payloadLength == 127) {
        uint8_t extLen[8];
        bytesRead = recv(clientSocket, reinterpret_cast<char*>(extLen), 8, 0);
        if (bytesRead != 8) return false;
        frame.payloadLength = 0;
        for (int i = 0; i < 8; ++i) {
            frame.payloadLength = (frame.payloadLength << 8) | extLen[i];
        }
    }

    if (frame.masked) {
        uint8_t mask[4];
        bytesRead = recv(clientSocket, reinterpret_cast<char*>(mask), 4, 0);
        if (bytesRead != 4) return false;
        frame.maskingKey = (static_cast<uint32_t>(mask[0]) << 24) |
                          (static_cast<uint32_t>(mask[1]) << 16) |
                          (static_cast<uint32_t>(mask[2]) << 8) |
                          static_cast<uint32_t>(mask[3]);
    }

    if (frame.payloadLength > 0) {
        frame.payload.resize(frame.payloadLength);
        size_t totalRead = 0;
        while (totalRead < frame.payloadLength) {
            bytesRead = recv(clientSocket,
                reinterpret_cast<char*>(frame.payload.data() + totalRead),
                static_cast<int>(frame.payloadLength - totalRead), 0);
            if (bytesRead <= 0) return false;
            totalRead += bytesRead;
        }

        if (frame.masked) {
            unmaskPayload(frame.payload.data(), frame.payloadLength, frame.maskingKey);
        }
    }

    return true;
}

bool WebSocketServer::sendFrame(SOCKET_TYPE clientSocket, const WSFrame& frame) {
    std::vector<uint8_t> header;
    header.reserve(10);

    uint8_t firstByte = 0x80 | static_cast<uint8_t>(frame.opcode);
    header.push_back(firstByte);

    if (frame.payloadLength < 126) {
        header.push_back(static_cast<uint8_t>(frame.payloadLength));
    } else if (frame.payloadLength < 65536) {
        header.push_back(126);
        header.push_back(static_cast<uint8_t>((frame.payloadLength >> 8) & 0xFF));
        header.push_back(static_cast<uint8_t>(frame.payloadLength & 0xFF));
    } else {
        header.push_back(127);
        for (int i = 7; i >= 0; --i) {
            header.push_back(static_cast<uint8_t>((frame.payloadLength >> (i * 8)) & 0xFF));
        }
    }

    int sentHeader = send(clientSocket,
        reinterpret_cast<const char*>(header.data()),
        static_cast<int>(header.size()), 0);
    if (sentHeader != static_cast<int>(header.size())) return false;

    if (frame.payloadLength > 0) {
        int sentPayload = send(clientSocket,
            reinterpret_cast<const char*>(frame.payload.data()),
            static_cast<int>(frame.payloadLength), 0);
        if (sentPayload != static_cast<int>(frame.payloadLength)) return false;
    }

    return true;
}

bool WebSocketServer::sendText(SOCKET_TYPE clientSocket, const std::string& data) {
    WSFrame frame;
    frame.fin = true;
    frame.opcode = WSFrame::TEXT;
    frame.masked = false;
    frame.payloadLength = data.size();
    frame.payload.assign(data.begin(), data.end());
    return sendFrame(clientSocket, frame);
}

void WebSocketServer::sendToAllClients(const std::string& data) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    std::vector<SOCKET_TYPE> disconnected;

    for (auto client : clients) {
        if (!sendText(client, data)) {
            disconnected.push_back(client);
        }
    }

    for (auto client : disconnected) {
        removeClient(client);
    }
}

void WebSocketServer::removeClient(SOCKET_TYPE clientSocket) {
    auto it = std::find(clients.begin(), clients.end(), clientSocket);
    if (it != clients.end()) {
        clients.erase(it);
        cleanupSocket(clientSocket);
        clientCount--;
        std::cout << "[WS] Client disconnected: " << clientCount.load() << std::endl;
    }
}

std::string WebSocketServer::createSnapshotJson(const OrderBookSnapshot& snapshot) {
    std::ostringstream json;
    json << "{\"type\":\"snapshot\",\"symbol\":\"" << orderBook.getSymbol() << "\","
         << "\"timestamp\":" << snapshot.timestamp << ","
         << "\"sequence\":" << snapshot.sequence << ","
         << "\"bids\":[";

    for (size_t i = 0; i < snapshot.bids.size(); ++i) {
        if (i > 0) json << ",";
        json << "{\"price\":" << (snapshot.bids[i].price / 10000.0)
             << ",\"quantity\":" << snapshot.bids[i].quantity
             << ",\"orders\":" << snapshot.bids[i].orderCount << "}";
    }

    json << "],\"asks\":[";

    for (size_t i = 0; i < snapshot.asks.size(); ++i) {
        if (i > 0) json << ",";
        json << "{\"price\":" << (snapshot.asks[i].price / 10000.0)
             << ",\"quantity\":" << snapshot.asks[i].quantity
             << ",\"orders\":" << snapshot.asks[i].orderCount << "}";
    }

    json << "]}";
    return json.str();
}

std::string WebSocketServer::createTradeJson(const Trade& trade) {
    std::ostringstream json;
    json << "{\"type\":\"trade\",\"symbol\":\"" << trade.symbol << "\","
         << "\"tradeId\":" << trade.tradeId << ","
         << "\"price\":" << (trade.price / 10000.0) << ","
         << "\"quantity\":" << trade.quantity << ","
         << "\"timestamp\":" << trade.timestamp << "}";
    return json.str();
}

void WebSocketServer::broadcastSnapshot(const OrderBookSnapshot& snapshot) {
    std::string json = createSnapshotJson(snapshot);
    sendToAllClients(json);
}

void WebSocketServer::broadcastTrade(const Trade& trade) {
    std::string json = createTradeJson(trade);
    sendToAllClients(json);
}

void WebSocketServer::handleClient(SOCKET_TYPE clientSocket) {
    while (running.load()) {
        WSFrame frame;
        if (!readFrame(clientSocket, frame)) {
            break;
        }

        switch (frame.opcode) {
            case WSFrame::PING: {
                WSFrame pongFrame;
                pongFrame.fin = true;
                pongFrame.opcode = WSFrame::PONG;
                pongFrame.masked = false;
                pongFrame.payload = frame.payload;
                pongFrame.payloadLength = frame.payloadLength;
                sendFrame(clientSocket, pongFrame);
                break;
            }
            case WSFrame::PONG:
                break;
            case WSFrame::CLOSE: {
                WSFrame closeFrame;
                closeFrame.fin = true;
                closeFrame.opcode = WSFrame::CLOSE;
                closeFrame.masked = false;
                closeFrame.payloadLength = 0;
                sendFrame(clientSocket, closeFrame);
                break;
            }
            case WSFrame::TEXT:
            case WSFrame::BINARY:
            case WSFrame::CONTINUATION:
            default:
                break;
        }

        if (frame.opcode == WSFrame::CLOSE) {
            break;
        }
    }

    removeClient(clientSocket);
}

void WebSocketServer::broadcastLoop() {
    while (running.load()) {
        auto snapshot = orderBook.getSnapshot(10);
        snapshot.sequence = ++sequenceNumber;
        broadcastSnapshot(snapshot);

        std::this_thread::sleep_for(
            std::chrono::milliseconds(broadcastIntervalMs));
    }
}

void WebSocketServer::cleanupSocket(SOCKET_TYPE sock) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

}
