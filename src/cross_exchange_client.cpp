#include "cross_exchange_client.h"
#include "curl_global.h"

#include <curl/curl.h>

#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <future>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {

std::int64_t nowMilliseconds() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
}

size_t appendResponse(char* data, size_t size, size_t count, void* context) {
    const auto length = size * count;
    static_cast<std::string*>(context)->append(data, length);
    return length;
}

struct HttpResponse {
    std::string body;
    std::int64_t request_start_ms;
    std::int64_t request_end_ms;
};

HttpResponse get(const char* url) {
    auto* curl = curl_easy_init();
    if (curl == nullptr) {
        throw std::runtime_error("Failed to create HTTP request");
    }
    struct CurlCleanup {
        CURL* handle;
        ~CurlCleanup() {
            curl_easy_cleanup(handle);
        }
    } cleanup{curl};

    HttpResponse response{{}, nowMilliseconds(), 0};
    auto check_option = [](CURLcode result) {
        if (result != CURLE_OK) {
            throw std::runtime_error(std::string("Failed to configure HTTP request: ") +
                                     curl_easy_strerror(result));
        }
    };
    check_option(curl_easy_setopt(curl, CURLOPT_URL, url));
    check_option(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, appendResponse));
    check_option(curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body));
    check_option(curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L));
    check_option(curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L));
    check_option(curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L));
    check_option(curl_easy_setopt(curl, CURLOPT_USERAGENT,
                                  "CryptoMarketClient/1.0"));

    const auto request_result = curl_easy_perform(curl);
    response.request_end_ms = nowMilliseconds();
    if (request_result != CURLE_OK) {
        throw std::runtime_error(std::string("Market data request failed: ") +
                                 curl_easy_strerror(request_result));
    }

    long http_status = 0;
    check_option(curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status));
    if (http_status < 200 || http_status >= 300) {
        throw std::runtime_error("Market data API returned HTTP status " +
                                 std::to_string(http_status));
    }
    return response;
}

size_t skipWhitespace(std::string_view text, size_t position) {
    while (position < text.size() &&
           (text[position] == ' ' || text[position] == '\n' ||
            text[position] == '\r' || text[position] == '\t')) {
        ++position;
    }
    return position;
}

std::string_view jsonField(std::string_view json, std::string_view key) {
    const std::string token = "\"" + std::string(key) + "\"";
    const auto key_position = json.find(token);
    if (key_position == std::string_view::npos) {
        throw std::runtime_error("Market data response is missing " +
                                 std::string(key));
    }
    auto position = skipWhitespace(json, key_position + token.size());
    if (position == json.size() || json[position] != ':') {
        throw std::runtime_error("Market data response contains invalid JSON");
    }
    position = skipWhitespace(json, position + 1);
    if (position == json.size()) {
        throw std::runtime_error("Market data response contains an incomplete field");
    }

    if (json[position] == '"') {
        const auto start = ++position;
        const auto end = json.find('"', start);
        if (end == std::string_view::npos ||
            json.substr(start, end - start).find('\\') != std::string_view::npos) {
            throw std::runtime_error("Market data response contains an invalid field");
        }
        return json.substr(start, end - start);
    }

    const auto start = position;
    while (position < json.size() && json[position] != ',' &&
           json[position] != '}' && json[position] != ' ' &&
           json[position] != '\n' && json[position] != '\r' &&
           json[position] != '\t') {
        ++position;
    }
    if (position == start) {
        throw std::runtime_error("Market data response contains an invalid field");
    }
    return json.substr(start, position - start);
}

double parsePrice(std::string_view value) {
    double price = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(),
                                        price, std::chars_format::general);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
        !std::isfinite(price) || price <= 0) {
        throw std::runtime_error("Market data API returned an invalid price");
    }
    return price;
}

std::int64_t parseTimestamp(std::string_view value) {
    std::int64_t timestamp = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(),
                                        timestamp);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
        timestamp < 0) {
        throw std::runtime_error("Market data API returned an invalid timestamp");
    }
    return timestamp;
}

TopOfBookQuote parseBinanceQuote(const HttpResponse& response) {
    const auto midpoint = response.request_start_ms +
                          (response.request_end_ms - response.request_start_ms) / 2;
    TopOfBookQuote quote{
        parsePrice(jsonField(response.body, "bidPrice")),
        parsePrice(jsonField(response.body, "askPrice")),
        midpoint,
    };
    if (quote.bid > quote.ask) {
        throw std::runtime_error("Binance returned an invalid bid/ask quote");
    }
    return quote;
}

TopOfBookQuote parseOkxQuote(const HttpResponse& response) {
    TopOfBookQuote quote{
        parsePrice(jsonField(response.body, "bidPx")),
        parsePrice(jsonField(response.body, "askPx")),
        parseTimestamp(jsonField(response.body, "ts")),
    };
    if (quote.bid > quote.ask) {
        throw std::runtime_error("OKX returned an invalid bid/ask quote");
    }
    return quote;
}

}  // namespace

CrossExchangeComparison CrossExchangeClient::compareBtcUsdtSpot() const {
    ensureCurlGlobalInitialized();
    auto binance_request = std::async(std::launch::async, [] {
        return get("https://api.binance.com/api/v3/ticker/bookTicker"
                   "?symbol=BTCUSDT");
    });
    auto okx_request = std::async(std::launch::async, [] {
        return get("https://www.okx.com/api/v5/market/ticker"
                   "?instId=BTC-USDT");
    });

    const auto binance = parseBinanceQuote(binance_request.get());
    const auto okx = parseOkxQuote(okx_request.get());

    return {
        binance,
        okx,
        okx.timestamp_ms - binance.timestamp_ms,
        okx.bid - binance.ask,
        binance.bid - okx.ask,
    };
}
