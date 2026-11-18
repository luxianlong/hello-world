#pragma once

#include <cstdint>
#include <vector>

struct CryptoCandle {
    std::int64_t open_time_ms;
    std::int64_t close_time_ms;
    double open;
    double high;
    double low;
    double close;
};

class BinanceMarketDataClient {
public:
    std::vector<CryptoCandle> getLastHourBtcUsdtCandles() const;
};
