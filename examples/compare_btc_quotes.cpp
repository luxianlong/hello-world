#include "cross_exchange_client.h"

#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>

int main() {
    try {
        const auto comparison = CrossExchangeClient{}.compareBtcUsdtSpot();
        if (std::llabs(comparison.quote_time_skew_ms) > 1000) {
            std::cerr << "Warning: quote timestamps differ by more than 1 second; "
                         "the spread may not be synchronized.\n";
        }
        std::cout << std::fixed << std::setprecision(2)
                  << "Binance bid/ask: " << comparison.binance.bid << " / "
                  << comparison.binance.ask << " USDT"
                  << " (request-midpoint estimate "
                  << comparison.binance.timestamp_ms << " ms)\n"
                  << "OKX bid/ask:     " << comparison.okx.bid << " / "
                  << comparison.okx.ask << " USDT"
                  << " (exchange timestamp "
                  << comparison.okx.timestamp_ms << " ms)\n"
                  << "Quote-time skew: " << comparison.quote_time_skew_ms
                  << " ms (OKX minus Binance estimate)\n"
                  << "Buy Binance, sell OKX: "
                  << comparison.buy_binance_sell_okx_spread << " USDT ("
                  << comparison.buy_binance_sell_okx_spread /
                         comparison.binance.ask * 100.0
                  << "% gross)\n"
                  << "Buy OKX, sell Binance: "
                  << comparison.buy_okx_sell_binance_spread << " USDT ("
                  << comparison.buy_okx_sell_binance_spread /
                         comparison.okx.ask * 100.0
                  << "% gross)\n"
                  << "Gross spreads exclude fees and slippage; no trades were placed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Could not compare BTC-USDT quotes: " << error.what() << '\n';
        return 1;
    }
}
