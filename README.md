# Gold price client

A small C++17 client that fetches the latest gold price in USD per troy ounce
from Gold-API without requiring an API key.

## Requirements

- CMake 3.16 or later
- A C++17 compiler
- libcurl development files

## Build

```sh
cmake -S . -B build
cmake --build build
```

Use the class:

```cpp
#include "gold_price_client.h"

#include <iostream>

int main() {
    GoldPriceClient client;
    const auto price = client.getLatestPrice();
    std::cout << "Gold: $" << price.usd_per_troy_ounce
              << " per troy ounce (Unix timestamp " << price.timestamp << ")\n";
}
```

`getLatestPrice()` throws `std::runtime_error` if the request fails or the
response is invalid.

Run the included example:

```sh
./build/fetch_latest_gold
```
