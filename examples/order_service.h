#pragma once

#include <string>

// 演示用业务类：订单服务。
//
// 它的实现文件 order_service.cpp 会通过 LoggerFactory 构造一个
// 仅在该 .cpp 内可见的全局 logger 来打印日志，见 order_service.cpp。
class OrderService {
public:
    void placeOrder(int orderId, const std::string& customer);
    void cancelOrder(int orderId);
};
