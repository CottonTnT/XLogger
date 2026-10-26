#include "order_service.h"

#include "LoggerFactory.h"

namespace {

// 文件作用域的全局 logger：仅本 .cpp 可见（internal linkage），不对外暴露。
//
// 通过 LoggerFactory 按名称获取：同名 logger 全局唯一，内部会缓存，
// 因此无论调用多少次 getLogger("OrderService") 拿到的都是同一个实例。
const auto logger = X::LoggerFactory::getLogger("OrderService");

} // namespace

void OrderService::placeOrder(int orderId, const std::string& customer)
{
    logger->info("下单：orderId={}, customer={}", orderId, customer);
    // ... 业务逻辑 ...
}

void OrderService::cancelOrder(int orderId)
{
    logger->warn("取消订单：orderId={}", orderId);
    // ... 业务逻辑 ...
}
