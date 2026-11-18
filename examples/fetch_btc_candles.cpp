#include "crypto_market_client.h"

#include <exception>
#include <iomanip>
#include <iostream>

int main() {
    try {
        const auto candles =
            BinanceMarketDataClient{}.getLastHourBtcUsdtCandles();
        std::cout << "open_time_ms,open,high,low,close\n"
                  << std::fixed << std::setprecision(2);
        for (const auto& candle : candles) {
            std::cout << candle.open_time_ms << ','
                      << candle.open << ',' << candle.high << ','
                      << candle.low << ',' << candle.close << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "Could not fetch BTCUSDT candles: " << error.what() << '\n';
        return 1;
    }
}
