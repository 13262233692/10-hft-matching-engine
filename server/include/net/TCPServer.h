#pragma once

#include "core/FIXParser.h"
#include "core/OrderBook.h"
#include "net/SocketCommon.h"
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <vector>

namespace HFT {

using OrderReceivedCallback = std::function<void(Order*)>;
using CancelReceivedCallback = std::function<void(uint64_t)>;

class TCPServer {
public:
    TCPServer(uint16_t port, OrderBook& orderBook);
    ~TCPServer();

    bool start();

    void stop();

    bool isRunning() const { return running.load(); }

    void setOrderCallback(OrderReceivedCallback callback) { orderCallback = std::move(callback); }

    void setCancelCallback(CancelReceivedCallback callback) { cancelCallback = std::move(callback); }

    uint16_t getPort() const { return port; }

    size_t getConnectedClients() const { return clientCount.load(); }

    size_t getMessagesReceived() const { return messagesReceived.load(); }

    size_t getBytesReceived() const { return bytesReceived.load(); }

private:
    uint16_t port;
    OrderBook& orderBook;
    SOCKET_TYPE listenSocket;
    std::atomic<bool> running;
    std::atomic<size_t> clientCount;
    std::atomic<size_t> messagesReceived;
    std::atomic<size_t> bytesReceived;
    std::unique_ptr<std::thread> acceptThread;
    std::vector<std::thread> clientThreads;
    FIXParser parser;
    OrderReceivedCallback orderCallback;
    CancelReceivedCallback cancelCallback;
    uint64_t orderIdCounter;

    void acceptLoop();
    void handleClient(SOCKET_TYPE clientSocket);
    bool processMessage(const char* data, size_t length);
    void sendResponse(SOCKET_TYPE clientSocket, const char* data, size_t length);
    void cleanupSocket(SOCKET_TYPE sock);
};

}
