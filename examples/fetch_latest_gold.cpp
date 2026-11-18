#include "gold_price_client.h"

#include <exception>
#include <iomanip>
#include <iostream>

int main() {
    try {
        const auto price = GoldPriceClient{}.getLatestPrice();
        std::cout << std::fixed << std::setprecision(2)
                  << "Gold: $" << price.usd_per_troy_ounce
                  << " per troy ounce (Unix timestamp " << price.timestamp << ")\n";
    } catch (const std::exception& error) {
        std::cerr << "Could not fetch the latest gold price: " << error.what() << '\n';
        return 1;
    }
}
