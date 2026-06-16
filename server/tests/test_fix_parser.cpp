#include "core/FIXParser.h"
#include <cassert>
#include <iostream>
#include <cstring>

int testFIXParser() {
    int failed = 0;
    HFT::FIXParser parser;

    const char SOH = HFT::FIXParser::SOH;

    std::string fixMsg = "8=FIX.4.4" + std::string(1, SOH) +
                         "9=145" + std::string(1, SOH) +
                         "35=D" + std::string(1, SOH) +
                         "49=CLIENT1" + std::string(1, SOH) +
                         "56=BROKER" + std::string(1, SOH) +
                         "34=1" + std::string(1, SOH) +
                         "52=20240101-12:00:00.000" + std::string(1, SOH) +
                         "11=12345" + std::string(1, SOH) +
                         "54=1" + std::string(1, SOH) +
                         "55=BTC-USDT" + std::string(1, SOH) +
                         "38=10" + std::string(1, SOH) +
                         "44=100.0000" + std::string(1, SOH) +
                         "40=2" + std::string(1, SOH) +
                         "59=0" + std::string(1, SOH);

    uint8_t checksum = 0;
    for (char c : fixMsg) checksum += static_cast<uint8_t>(c);
    checksum %= 256;
    char checksumStr[4];
    std::snprintf(checksumStr, sizeof(checksumStr), "%03d", checksum);
    fixMsg += "10=" + std::string(checksumStr) + std::string(1, SOH);

    HFT::FIXMessage parsedMsg;
    bool parseResult = parser.parse(fixMsg.c_str(), fixMsg.size(), parsedMsg);
    assert(parseResult);
    assert(parsedMsg.msgType == HFT::FIXMsgType::NEW_ORDER);
    assert(parsedMsg.fields.at(11) == "12345");
    assert(parsedMsg.fields.at(54) == "1");
    assert(parsedMsg.fields.at(55) == "BTC-USDT");
    assert(parsedMsg.fields.at(38) == "10");
    assert(parsedMsg.fields.at(44) == "100.0000");
    std::cout << "  ✓ FIX message parsing" << std::endl;

    HFT::Order order;
    bool orderResult = parser.parseNewOrder(parsedMsg, order);
    assert(orderResult);
    assert(order.clientOrderId == 12345);
    assert(order.side == HFT::Side::BUY);
    assert(order.price == 1000000);
    assert(order.quantity == 10);
    assert(order.symbol == "BTC-USDT");
    assert(order.type == HFT::OrderType::LIMIT);
    std::cout << "  ✓ New order parsing" << std::endl;

    std::string cancelMsg = "8=FIX.4.4" + std::string(1, SOH) +
                            "9=50" + std::string(1, SOH) +
                            "35=F" + std::string(1, SOH) +
                            "11=12346" + std::string(1, SOH) +
                            "41=12345" + std::string(1, SOH);

    checksum = 0;
    for (char c : cancelMsg) checksum += static_cast<uint8_t>(c);
    checksum %= 256;
    std::snprintf(checksumStr, sizeof(checksumStr), "%03d", checksum);
    cancelMsg += "10=" + std::string(checksumStr) + std::string(1, SOH);

    HFT::FIXMessage parsedCancel;
    assert(parser.parse(cancelMsg.c_str(), cancelMsg.size(), parsedCancel));
    uint64_t cancelId = parser.parseCancelOrder(parsedCancel);
    assert(cancelId == 12346);
    std::cout << "  ✓ Cancel order parsing" << std::endl;

    char buffer[512];
    size_t len = parser.buildExecutionReport(12345, HFT::OrderStatus::FILLED,
                                             1000000, 10, "BTC-USDT",
                                             buffer, sizeof(buffer));
    assert(len > 0);
    std::cout << "  ✓ Execution report serialization" << std::endl;

    std::string badMsg = "8=FIX.4.4" + std::string(1, SOH) +
                         "9=10" + std::string(1, SOH) +
                         "35=D" + std::string(1, SOH) +
                         "10=000" + std::string(1, SOH);
    HFT::FIXMessage badParsed;
    bool badResult = parser.parse(badMsg.c_str(), badMsg.size(), badParsed);
    assert(!badResult);
    std::cout << "  ✓ Invalid checksum rejection" << std::endl;

    return failed;
}
