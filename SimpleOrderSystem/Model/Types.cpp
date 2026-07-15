#include "Types.h"
#include <stdexcept>
#include <unordered_map>

namespace Model {

const std::string& ToString(OrderStatus status) {
    static const std::string kReserved = "RESERVED";
    static const std::string kRejected = "REJECTED";
    static const std::string kProducing = "PRODUCING";
    static const std::string kConfirmed = "CONFIRMED";
    static const std::string kRelease = "RELEASE";
    switch (status) {
        case OrderStatus::RESERVED: return kReserved;
        case OrderStatus::REJECTED: return kRejected;
        case OrderStatus::PRODUCING: return kProducing;
        case OrderStatus::CONFIRMED: return kConfirmed;
        case OrderStatus::RELEASE: return kRelease;
    }
    throw std::invalid_argument("Unknown OrderStatus value");
}

OrderStatus OrderStatusFromString(const std::string& text) {
    static const std::unordered_map<std::string, OrderStatus> kMap = {
        {"RESERVED", OrderStatus::RESERVED},
        {"REJECTED", OrderStatus::REJECTED},
        {"PRODUCING", OrderStatus::PRODUCING},
        {"CONFIRMED", OrderStatus::CONFIRMED},
        {"RELEASE", OrderStatus::RELEASE},
    };
    auto it = kMap.find(text);
    if (it == kMap.end()) {
        throw std::invalid_argument("Unknown OrderStatus string: " + text);
    }
    return it->second;
}

const std::string& ToString(StockState state) {
    static const std::string kAbundant = "여유";
    static const std::string kShortage = "부족";
    static const std::string kDepleted = "고갈";
    switch (state) {
        case StockState::ABUNDANT: return kAbundant;
        case StockState::SHORTAGE: return kShortage;
        case StockState::DEPLETED: return kDepleted;
    }
    throw std::invalid_argument("Unknown StockState value");
}

} // namespace Model
