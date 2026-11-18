#pragma once

#include <cstdint>

struct TopOfBookQuote {
    double bid;
    double ask;
    std::int64_t timestamp_ms;
};

struct CrossExchangeComparison {
    TopOfBookQuote binance;
    TopOfBookQuote okx;
    std::int64_t quote_time_skew_ms;
    double buy_binance_sell_okx_spread;
    double buy_okx_sell_binance_spread;
};

class CrossExchangeClient {
public:
    CrossExchangeComparison compareBtcUsdtSpot() const;
};
