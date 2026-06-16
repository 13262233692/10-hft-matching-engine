#pragma once

#include "core/OrderBook.h"
#include "net/SocketCommon.h"
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <vector>
#include <mutex>
#include <chrono>

namespace HFT {

struct WSFrame {
    enum Opcode : uint8_t {
        CONTINUATION = 0x00,
        TEXT = 0x01,
        BINARY = 0x02,
        CLOSE = 0x08,
        PING = 0x09,
        PONG = 0x0A
    };

    bool fin;
    Opcode opcode;
    bool masked;
    uint64_t payloadLength;
    uint32_t maskingKey;
    std::vector<uint8_t> payload;
};

class WebSocketServer {
public:
    WebSocketServer(uint16_t port, OrderBook& orderBook);
    ~WebSocketServer();

    bool start();

    void stop();

    bool isRunning() const { return running.load(); }

    uint16_t getPort() const { return port; }

    size_t getConnectedClients() const { return clientCount.load(); }

    void broadcastSnapshot(const OrderBookSnapshot& snapshot);

    void broadcastTrade(const Trade& trade);

    void setBroadcastIntervalMs(uint32_t intervalMs) { broadcastIntervalMs = intervalMs; }

private:
    uint16_t port;
    OrderBook& orderBook;
    SOCKET_TYPE listenSocket;
    std::atomic<bool> running;
    std::atomic<size_t> clientCount;
    std::unique_ptr<std::thread> acceptThread;
    std::unique_ptr<std::thread> broadcastThread;
    std::vector<SOCKET_TYPE> clients;
    mutable std::mutex clientsMutex;
    uint32_t broadcastIntervalMs;
    uint64_t sequenceNumber;

    void acceptLoop();
    void broadcastLoop();
    void handleClient(SOCKET_TYPE clientSocket);
    bool performHandshake(SOCKET_TYPE clientSocket);
    bool readFrame(SOCKET_TYPE clientSocket, WSFrame& frame);
    bool sendFrame(SOCKET_TYPE clientSocket, const WSFrame& frame);
    bool sendText(SOCKET_TYPE clientSocket, const std::string& data);
    void sendToAllClients(const std::string& data);
    std::string createSnapshotJson(const OrderBookSnapshot& snapshot);
    std::string createTradeJson(const Trade& trade);
    void cleanupSocket(SOCKET_TYPE sock);
    void removeClient(SOCKET_TYPE clientSocket);

    static std::string base64Encode(const uint8_t* data, size_t length);
    static std::string sha1(const std::string& input);
    static void unmaskPayload(uint8_t* payload, uint64_t length, uint32_t maskingKey);
};

}
