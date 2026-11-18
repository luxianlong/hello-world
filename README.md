# Crypto market data client

A small C++17 client that fetches the latest hour of BTCUSDT one-minute
candlesticks from Binance's public Spot REST API. Public market data does not
require an API key.

## Requirements

- CMake 3.16 or later
- A C++17 compiler
- libcurl development files

## Build

```sh
cmake -S . -B build
cmake --build build
```

The client returns each candle's open and close timestamps (Unix milliseconds)
and open, high, low, and close prices:

```cpp
#include "crypto_market_client.h"

#include <iostream>

int main() {
    BinanceMarketDataClient client;
    for (const auto& candle : client.getLastHourBtcUsdtCandles()) {
        std::cout << candle.open_time_ms << ','
                  << candle.open << ',' << candle.high << ','
                  << candle.low << ',' << candle.close << '\n';
    }
}
```

`getLastHourBtcUsdtCandles()` throws `std::runtime_error` if the request fails
or the response is invalid. The returned data is one-minute OHLC candles, not
individual trades.

Run the included example:

```sh
./build/fetch_btc_candles
```

Compare current BTC-USDT spot bid/ask quotes on Binance and OKX:

```sh
./build/compare_btc_quotes
```

The comparison requests both public tickers concurrently and reports both
buy/sell directions and quote-time skew. Binance's book-ticker response has no
exchange timestamp, so its request midpoint is used as an estimate; this is a
best-effort snapshot comparison, not a guaranteed synchronized arbitrage signal.
Gross spreads do not include fees, slippage, transfer costs, or execution risk,
and the example does not place trades.
