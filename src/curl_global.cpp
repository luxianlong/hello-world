#include "curl_global.h"

#include <curl/curl.h>

#include <stdexcept>
#include <string>

namespace {

class CurlGlobal {
public:
    CurlGlobal() {
        const auto result = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (result != CURLE_OK) {
            throw std::runtime_error(std::string("Failed to initialize libcurl: ") +
                                     curl_easy_strerror(result));
        }
    }

    ~CurlGlobal() {
        curl_global_cleanup();
    }
};

}  // namespace

void ensureCurlGlobalInitialized() {
    static const CurlGlobal global;
    (void)global;
}
