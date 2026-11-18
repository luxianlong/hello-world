cmake_minimum_required(VERSION 3.16)
project(crypto_market_client LANGUAGES CXX)

find_package(CURL REQUIRED)

add_library(crypto_market_client
    src/crypto_market_client.cpp
    src/cross_exchange_client.cpp
    src/curl_global.cpp
)
target_include_directories(crypto_market_client PUBLIC include)
target_compile_features(crypto_market_client PUBLIC cxx_std_17)
target_link_libraries(crypto_market_client PRIVATE CURL::libcurl)

add_executable(fetch_btc_candles examples/fetch_btc_candles.cpp)
target_link_libraries(fetch_btc_candles PRIVATE crypto_market_client)

add_executable(compare_btc_quotes examples/compare_btc_quotes.cpp)
target_link_libraries(compare_btc_quotes PRIVATE crypto_market_client)
