#include "gold_price_client.h"

#include <curl/curl.h>

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

class CurlGlobal {
public:
    CurlGlobal() {
        const auto result = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (result != CURLE_OK) {
            throw std::runtime_error("Failed to initialize libcurl");
        }
    }

    ~CurlGlobal() {
        curl_global_cleanup();
    }
};

CURL* initializeCurl() {
    static const CurlGlobal global;
    (void)global;
    auto* curl = curl_easy_init();
    if (curl == nullptr) {
        throw std::runtime_error("Failed to create HTTP request");
    }
    return curl;
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

size_t findValueStart(std::string_view json, std::string_view key) {
    const std::string key_token = "\"" + std::string(key) + "\"";
    const auto key_position = json.find(key_token);
    if (key_position == std::string_view::npos) {
        throw std::runtime_error("Gold price response is missing the " +
                                 std::string(key) + " field");
    }

    auto position = skipWhitespace(json, key_position + key_token.size());
    if (position == json.size() || json[position] != ':') {
        throw std::runtime_error("Gold price response contains invalid JSON");
    }
    position = skipWhitespace(json, position + 1);
    return position;
}

double extractPrice(std::string_view json) {
    const auto position = findValueStart(json, "price");
    if (position == json.size()) {
        throw std::runtime_error("Gold price response contains invalid JSON");
    }
    const std::string value(json.substr(position));
    char* end = nullptr;
    const auto rate = std::strtod(value.c_str(), &end);
    if (end == value.c_str() || !std::isfinite(rate) || rate <= 0.0) {
        throw std::runtime_error("Gold price API returned an invalid price");
    }
    const auto end_position = position + static_cast<size_t>(end - value.c_str());
    if (end_position < json.size() && json[end_position] != ',' &&
        json[end_position] != '}' && json[end_position] != ' ' &&
        json[end_position] != '\n' && json[end_position] != '\r' &&
        json[end_position] != '\t') {
        throw std::runtime_error("Gold price response contains invalid JSON");
    }
    return rate;
}

std::time_t extractTimestamp(std::string_view json) {
    auto position = findValueStart(json, "updatedAt");
    if (position == json.size() || json[position] != '"') {
        throw std::runtime_error("Gold price response contains an invalid update time");
    }
    const auto end = json.find('"', position + 1);
    if (end == std::string_view::npos || end - position - 1 < 19) {
        throw std::runtime_error("Gold price response contains an invalid update time");
    }
    const std::string date(json.substr(position + 1, 19));
    std::tm time{};
    std::istringstream input(date);
    input >> std::get_time(&time, "%Y-%m-%dT%H:%M:%S");
    if (input.fail() || input.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error("Gold price response contains an invalid update time");
    }
    time.tm_isdst = 0;
#if defined(_WIN32)
    const auto timestamp = _mkgmtime(&time);
#else
    const auto timestamp = timegm(&time);
#endif
    if (timestamp <= 0) {
        throw std::runtime_error("Gold price response contains an invalid update time");
    }
    return timestamp;
}

}  // namespace

GoldPrice GoldPriceClient::getLatestPrice() const {
    auto* curl = initializeCurl();
    struct CurlCleanup {
        CURL* handle;
        ~CurlCleanup() {
            curl_easy_cleanup(handle);
        }
    } cleanup{curl};

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.gold-api.com/price/XAU");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, appendResponse);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "GoldPriceClient/1.0");

    const auto request_result = curl_easy_perform(curl);
    if (request_result != CURLE_OK) {
        throw std::runtime_error(std::string("Gold price request failed: ") +
                                 curl_easy_strerror(request_result));
    }

    long http_status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);
    if (http_status < 200 || http_status >= 300) {
        throw std::runtime_error("Gold price API returned HTTP status " +
                                 std::to_string(http_status));
    }

    return {extractPrice(response), extractTimestamp(response)};
}
