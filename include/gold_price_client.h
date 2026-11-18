#pragma once

#include <ctime>
struct GoldPrice {
    double usd_per_troy_ounce;
    std::time_t timestamp;
};

class GoldPriceClient {
public:
    GoldPrice getLatestPrice() const;
};
