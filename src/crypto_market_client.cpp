#include "crypto_market_client.h"
#include "curl_global.h"

#include <curl/curl.h>

#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

CURL* initializeCurl() {
    ensureCurlGlobalInitialized();
    auto* curl = curl_easy_init();
    if (curl == nullptr) {
        throw std::runtime_error("Failed to create HTTP request");
    }
    return curl;
}

void checkCurlOption(CURLcode result) {
    if (result != CURLE_OK) {
        throw std::runtime_error(std::string("Failed to configure HTTP request: ") +
                                 curl_easy_strerror(result));
    }
}

size_t appendResponse(char* data, size_t size, size_t count, void* context) {
    const auto length = size * count;
    static_cast<std::string*>(context)->append(data, length);
    return length;
}

size_t skipWhitespace(std::string_view text, size_t position) {
    while (position < text.size() &&
           (text[position] == ' ' || text[position] == '\n' ||
            text[position] == '\r' || text[position] == '\t')) {
        ++position;
    }
    return position;
}

void expectCharacter(std::string_view text, size_t& position, char expected) {
    position = skipWhitespace(text, position);
    if (position == text.size() || text[position] != expected) {
        throw std::runtime_error("Binance returned an invalid candle response");
    }
    ++position;
}

std::string_view parseScalar(std::string_view text, size_t& position) {
    position = skipWhitespace(text, position);
    if (position == text.size()) {
        throw std::runtime_error("Binance returned an incomplete candle response");
    }

    if (text[position] == '"') {
        const auto start = ++position;
        const auto end = text.find('"', start);
        if (end == std::string_view::npos ||
            text.substr(start, end - start).find('\\') != std::string_view::npos) {
            throw std::runtime_error("Binance returned an invalid candle value");
        }
        position = end + 1;
        return text.substr(start, end - start);
    }

    const auto start = position;
    while (position < text.size() && text[position] != ',' &&
           text[position] != ']' && text[position] != ' ' &&
           text[position] != '\n' && text[position] != '\r' &&
           text[position] != '\t') {
        ++position;
    }
    if (position == start) {
        throw std::runtime_error("Binance returned an invalid candle value");
    }
    return text.substr(start, position - start);
}

std::int64_t parseTimestamp(std::string_view value) {
    std::int64_t timestamp = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(),
                                        timestamp);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
        timestamp < 0) {
        throw std::runtime_error("Binance returned an invalid candle timestamp");
    }
    return timestamp;
}

double parsePrice(std::string_view value) {
    double price = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(),
                                        price, std::chars_format::general);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
        !std::isfinite(price) || price <= 0) {
        throw std::runtime_error("Binance returned an invalid candle price");
    }
    return price;
}

CryptoCandle parseCandle(std::string_view json, size_t& position) {
    expectCharacter(json, position, '[');
    std::vector<std::string_view> values;
    while (true) {
        values.push_back(parseScalar(json, position));
        position = skipWhitespace(json, position);
        if (position == json.size()) {
            throw std::runtime_error("Binance returned an incomplete candle response");
        }
        if (json[position] == ']') {
            ++position;
            break;
        }
        expectCharacter(json, position, ',');
    }

    if (values.size() < 7) {
        throw std::runtime_error("Binance returned an incomplete candle");
    }

    CryptoCandle candle{
        parseTimestamp(values[0]),
        parseTimestamp(values[6]),
        parsePrice(values[1]),
        parsePrice(values[2]),
        parsePrice(values[3]),
        parsePrice(values[4]),
    };
    if (candle.low > candle.high || candle.open < candle.low ||
        candle.open > candle.high || candle.close < candle.low ||
        candle.close > candle.high || candle.close_time_ms < candle.open_time_ms) {
        throw std::runtime_error("Binance returned inconsistent candle data");
    }
    return candle;
}

std::vector<CryptoCandle> parseCandles(std::string_view json) {
    std::vector<CryptoCandle> candles;
    size_t position = 0;
    expectCharacter(json, position, '[');
    position = skipWhitespace(json, position);
    if (position < json.size() && json[position] == ']') {
        ++position;
    } else {
        while (true) {
            candles.push_back(parseCandle(json, position));
            position = skipWhitespace(json, position);
            if (position == json.size()) {
                throw std::runtime_error("Binance returned an incomplete candle response");
            }
            if (json[position] == ']') {
                ++position;
                break;
            }
            expectCharacter(json, position, ',');
        }
    }

    if (skipWhitespace(json, position) != json.size()) {
        throw std::runtime_error("Binance returned unexpected data after candles");
    }
    return candles;
}

}  // namespace

std::vector<CryptoCandle> BinanceMarketDataClient::getLastHourBtcUsdtCandles() const {
    using namespace std::chrono;
    const auto end_time = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    const auto start_time = end_time - duration_cast<milliseconds>(hours(1)).count();
    const auto url = std::string("https://api.binance.com/api/v3/klines") +
                     "?symbol=BTCUSDT&interval=1m&startTime=" +
                     std::to_string(start_time) + "&endTime=" +
                     std::to_string(end_time) + "&limit=1000";

    auto* curl = initializeCurl();
    struct CurlCleanup {
        CURL* handle;
        ~CurlCleanup() {
            curl_easy_cleanup(handle);
        }
    } cleanup{curl};

    std::string response;
    checkCurlOption(curl_easy_setopt(curl, CURLOPT_URL, url.c_str()));
    checkCurlOption(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, appendResponse));
    checkCurlOption(curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response));
    checkCurlOption(curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L));
    checkCurlOption(curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L));
    checkCurlOption(curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L));
    checkCurlOption(curl_easy_setopt(curl, CURLOPT_USERAGENT,
                                     "CryptoMarketClient/1.0"));

    const auto request_result = curl_easy_perform(curl);
    if (request_result != CURLE_OK) {
        throw std::runtime_error(std::string("Binance request failed: ") +
                                 curl_easy_strerror(request_result));
    }

    long http_status = 0;
    checkCurlOption(curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status));
    if (http_status < 200 || http_status >= 300) {
        throw std::runtime_error("Binance returned HTTP status " +
                                 std::to_string(http_status));
    }

    return parseCandles(response);
}
