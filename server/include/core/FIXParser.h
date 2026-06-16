#pragma once

#include "core/Order.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <cstring>

namespace HFT {

enum class FIXMsgType : uint8_t {
    NEW_ORDER = 0,
    CANCEL_ORDER = 1,
    EXECUTION_REPORT = 2,
    HEARTBEAT = 3,
    LOGON = 4,
    LOGOUT = 5
};

struct FIXMessage {
    FIXMsgType msgType;
    std::unordered_map<int, std::string> fields;
    std::string rawMessage;
};

class FIXParser {
public:
    FIXParser();
    ~FIXParser() = default;

    bool parse(const char* data, size_t length, FIXMessage& outMsg);

    bool parseNewOrder(const FIXMessage& msg, Order& outOrder);

    uint64_t parseCancelOrder(const FIXMessage& msg);

    size_t serialize(const FIXMessage& msg, char* buffer, size_t bufferSize);

    size_t buildExecutionReport(uint64_t orderId, OrderStatus status,
                                int64_t price, uint64_t quantity,
                                const std::string& symbol,
                                char* buffer, size_t bufferSize);

    static constexpr char SOH = 0x01;

private:
    bool validateChecksum(const char* data, size_t length);
    uint8_t calculateChecksum(const char* data, size_t length);
    bool findNextField(const char* data, size_t length, size_t& pos,
                       int& tag, std::string& value);
    FIXMsgType getMsgType(const std::string& typeStr);
    Side parseSide(const std::string& sideStr);
    OrderType parseOrderType(const std::string& typeStr);
};

}
