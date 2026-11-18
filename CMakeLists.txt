cmake_minimum_required(VERSION 3.16)
project(gold_price_client LANGUAGES CXX)

find_package(CURL REQUIRED)

add_library(gold_price_client src/gold_price_client.cpp)
target_include_directories(gold_price_client PUBLIC include)
target_compile_features(gold_price_client PUBLIC cxx_std_17)
target_link_libraries(gold_price_client PRIVATE CURL::libcurl)

add_executable(fetch_latest_gold examples/fetch_latest_gold.cpp)
target_link_libraries(fetch_latest_gold PRIVATE gold_price_client)
