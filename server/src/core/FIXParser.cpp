#include "core/FIXParser.h"
#include <cstdlib>
#include <charconv>

namespace HFT {

FIXParser::FIXParser() = default;

bool FIXParser::parse(const char* data, size_t length, FIXMessage& outMsg) {
    if (!data || length < 10) {
        return false;
    }

    if (!validateChecksum(data, length)) {
        return false;
    }

    outMsg.rawMessage.assign(data, length);
    outMsg.fields.clear();

    size_t pos = 0;
    int tag;
    std::string value;

    while (findNextField(data, length, pos, tag, value)) {
        outMsg.fields[tag] = value;
    }

    auto it = outMsg.fields.find(35);
    if (it != outMsg.fields.end()) {
        outMsg.msgType = getMsgType(it->second);
    }

    return true;
}

bool FIXParser::parseNewOrder(const FIXMessage& msg, Order& outOrder) {
    auto itClOrdID = msg.fields.find(11);
    auto itSide = msg.fields.find(54);
    auto itPrice = msg.fields.find(44);
    auto itQty = msg.fields.find(38);
    auto itSymbol = msg.fields.find(55);
    auto itOrdType = msg.fields.find(40);

    if (itClOrdID == msg.fields.end() || itSide == msg.fields.end() ||
        itPrice == msg.fields.end() || itQty == msg.fields.end() ||
        itSymbol == msg.fields.end()) {
        return false;
    }

    uint64_t clOrdId = std::strtoull(itClOrdID->second.c_str(), nullptr, 10);
    int64_t price = static_cast<int64_t>(std::strtod(itPrice->second.c_str(), nullptr) * 10000);
    uint64_t qty = std::strtoull(itQty->second.c_str(), nullptr, 10);

    outOrder.clientOrderId = clOrdId;
    outOrder.side = parseSide(itSide->second);
    outOrder.price = price;
    outOrder.quantity = qty;
    outOrder.symbol = itSymbol->second;

    if (itOrdType != msg.fields.end()) {
        outOrder.type = parseOrderType(itOrdType->second);
    }

    outOrder.status = OrderStatus::ACTIVE;
    outOrder.timestamp = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();

    return true;
}

uint64_t FIXParser::parseCancelOrder(const FIXMessage& msg) {
    auto it = msg.fields.find(11);
    if (it == msg.fields.end()) {
        auto itOrigClOrdID = msg.fields.find(41);
        if (itOrigClOrdID != msg.fields.end()) {
            return std::strtoull(itOrigClOrdID->second.c_str(), nullptr, 10);
        }
        return 0;
    }
    return std::strtoull(it->second.c_str(), nullptr, 10);
}

size_t FIXParser::serialize(const FIXMessage& msg, char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize < 32) {
        return 0;
    }

    size_t pos = 0;
    auto appendField = [&](int tag, const std::string& val) {
        std::string tagStr = std::to_string(tag);
        if (pos + tagStr.size() + 1 + val.size() + 1 > bufferSize) return;
        std::memcpy(buffer + pos, tagStr.c_str(), tagStr.size());
        pos += tagStr.size();
        buffer[pos++] = '=';
        std::memcpy(buffer + pos, val.c_str(), val.size());
        pos += val.size();
        buffer[pos++] = SOH;
    };

    std::string body;
    for (const auto& [tag, value] : msg.fields) {
        if (tag != 8 && tag != 9 && tag != 10) {
            body += std::to_string(tag) + "=" + value + SOH;
        }
    }

    std::string bodyLen = std::to_string(body.size());

    appendField(8, "FIX.4.4");
    appendField(9, bodyLen);

    if (pos + body.size() > bufferSize) return 0;
    std::memcpy(buffer + pos, body.c_str(), body.size());
    pos += body.size();

    uint8_t checksum = calculateChecksum(buffer, pos);
    char checksumStr[4];
    std::snprintf(checksumStr, sizeof(checksumStr), "%03d", checksum % 256);
    appendField(10, std::string(checksumStr));

    return pos;
}

size_t FIXParser::buildExecutionReport(uint64_t orderId, OrderStatus status,
                                      int64_t price, uint64_t quantity,
                                      const std::string& symbol,
                                      char* buffer, size_t bufferSize) {
    FIXMessage msg;
    msg.msgType = FIXMsgType::EXECUTION_REPORT;
    msg.fields[35] = "8";
    msg.fields[37] = std::to_string(orderId);
    msg.fields[11] = std::to_string(orderId);
    msg.fields[17] = std::to_string(orderId);
    msg.fields[55] = symbol;
    msg.fields[54] = "1";
    msg.fields[38] = std::to_string(quantity);
    msg.fields[44] = std::to_string(static_cast<double>(price) / 10000.0);
    msg.fields[150] = std::to_string(static_cast<int>(status));
    msg.fields[39] = std::to_string(static_cast<int>(status));
    msg.fields[32] = "0";
    msg.fields[31] = std::to_string(static_cast<double>(price) / 10000.0);
    msg.fields[6] = "0";
    msg.fields[14] = "0";

    return serialize(msg, buffer, bufferSize);
}

bool FIXParser::validateChecksum(const char* data, size_t length) {
    if (length < 7) return false;

    size_t checksumPos = std::string::npos;
    for (size_t i = length - 7; i < length; ++i) {
        if (data[i] == '1' && data[i + 1] == '0' && data[i + 2] == '=') {
            checksumPos = i + 3;
            break;
        }
    }

    if (checksumPos == std::string::npos) {
        return false;
    }

    uint8_t expected = 0;
    for (size_t i = 0; i < checksumPos; ++i) {
        expected += static_cast<uint8_t>(data[i]);
    }
    expected %= 256;

    int actual = std::atoi(data + checksumPos);
    return (actual % 256) == expected;
}

uint8_t FIXParser::calculateChecksum(const char* data, size_t length) {
    uint8_t sum = 0;
    for (size_t i = 0; i < length; ++i) {
        sum += static_cast<uint8_t>(data[i]);
    }
    return sum % 256;
}

bool FIXParser::findNextField(const char* data, size_t length, size_t& pos,
                           int& tag, std::string& value) {
    if (pos >= length) return false;

    size_t tagStart = pos;
    while (pos < length && data[pos] != '=') ++pos;
    if (pos >= length) return false;

    std::string tagStr(data + tagStart, pos - tagStart);
    tag = std::atoi(tagStr.c_str());
    ++pos;

    size_t valStart = pos;
    while (pos < length && data[pos] != SOH) ++pos;
    if (pos >= length) return false;

    value.assign(data + valStart, pos - valStart);
    ++pos;

    return true;
}

FIXMsgType FIXParser::getMsgType(const std::string& typeStr) {
    if (typeStr == "D") return FIXMsgType::NEW_ORDER;
    if (typeStr == "F") return FIXMsgType::CANCEL_ORDER;
    if (typeStr == "8") return FIXMsgType::EXECUTION_REPORT;
    if (typeStr == "0") return FIXMsgType::HEARTBEAT;
    if (typeStr == "A") return FIXMsgType::LOGON;
    if (typeStr == "5") return FIXMsgType::LOGOUT;
    return FIXMsgType::HEARTBEAT;
}

Side FIXParser::parseSide(const std::string& sideStr) {
    if (sideStr == "1" || sideStr == "B") return Side::BUY;
    if (sideStr == "2" || sideStr == "S") return Side::SELL;
    return Side::BUY;
}

OrderType FIXParser::parseOrderType(const std::string& typeStr) {
    if (typeStr == "1") return OrderType::MARKET;
    return OrderType::LIMIT;
}

}
