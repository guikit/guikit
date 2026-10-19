#include "guikit/net/NetClient.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstring>
#include <deque>
#include <format>
#include <mutex>
#include <thread>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#elif defined(__EMSCRIPTEN__)
#include <emscripten/fetch.h>
#include <emscripten/websocket.h>
#elif defined(GUIKIT_HAS_CURL)
#include <curl/curl.h>
#endif

namespace guikit::net {

namespace {

[[nodiscard]] char AsciiToLower(char c) noexcept {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<char>(c - 'A' + 'a');
    }
    return c;
}

[[nodiscard]] bool EqualsIgnoreCase(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (AsciiToLower(a[i]) != AsciiToLower(b[i])) return false;
    }
    return true;
}

[[nodiscard]] std::string_view TrimAscii(std::string_view sv) noexcept {
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\r' || sv.front() == '\n')) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' || sv.back() == '\r' || sv.back() == '\n')) {
        sv.remove_suffix(1);
    }
    return sv;
}

/// Guards against CRLF header injection in caller-supplied HTTP headers.
[[nodiscard]] bool IsSafeHeaderToken(std::string_view sv) noexcept {
    for (const char c : sv) {
        if (c == '\r' || c == '\n' || c == '\0') {
            return false;
        }
    }
    return true;
}

[[nodiscard]] const char* HttpMethodToCString(HttpMethod method) noexcept {
    switch (method) {
        case HttpMethod::Get:    return "GET";
        case HttpMethod::Post:   return "POST";
        case HttpMethod::Put:    return "PUT";
        case HttpMethod::Delete: return "DELETE";
        case HttpMethod::Head:   return "HEAD";
        case HttpMethod::Patch:  return "PATCH";
    }
    return "GET";
}

#if defined(_WIN32)
/// Exception-safe RAII wrapper for WinHTTP HINTERNET handles.
class ScopedWinHttpHandle {
public:
    ScopedWinHttpHandle() noexcept = default;
    explicit ScopedWinHttpHandle(HINTERNET h) noexcept : m_handle(h) {}
    ~ScopedWinHttpHandle() noexcept { Reset(); }

    ScopedWinHttpHandle(const ScopedWinHttpHandle&) = delete;
    ScopedWinHttpHandle& operator=(const ScopedWinHttpHandle&) = delete;

    ScopedWinHttpHandle(ScopedWinHttpHandle&& other) noexcept
        : m_handle(other.Release()) {}

    ScopedWinHttpHandle& operator=(ScopedWinHttpHandle&& other) noexcept {
        if (this != &other) {
            Reset(other.Release());
        }
        return *this;
    }

    void Reset(HINTERNET newHandle = nullptr) noexcept {
        if (m_handle) {
            WinHttpCloseHandle(m_handle);
        }
        m_handle = newHandle;
    }

    [[nodiscard]] HINTERNET Release() noexcept {
        HINTERNET tmp = m_handle;
        m_handle = nullptr;
        return tmp;
    }

    [[nodiscard]] HINTERNET Get() const noexcept { return m_handle; }
    [[nodiscard]] explicit operator bool() const noexcept { return m_handle != nullptr; }

private:
    HINTERNET m_handle{nullptr};
};

[[nodiscard]] std::string FormatWinHttpError(std::string_view context, DWORD errCode) {
    return std::format("{} (WinHTTP error={})", context, errCode);
}

[[nodiscard]] std::wstring Utf8ToWide(std::string_view utf8) {
    if (utf8.empty()) return {};
    const int needed = MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring wide(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), needed);
    return wide;
}

[[nodiscard]] std::string WideToUtf8(std::wstring_view wide) {
    if (wide.empty()) return {};
    const int needed = WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string utf8(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), needed, nullptr, nullptr);
    return utf8;
}

[[nodiscard]] const wchar_t* HttpMethodToWideCString(HttpMethod method) noexcept {
    switch (method) {
        case HttpMethod::Get:    return L"GET";
        case HttpMethod::Post:   return L"POST";
        case HttpMethod::Put:    return L"PUT";
        case HttpMethod::Delete: return L"DELETE";
        case HttpMethod::Head:   return L"HEAD";
        case HttpMethod::Patch:  return L"PATCH";
    }
    return L"GET";
}

void ParseWinHttpRawHeaders(HINTERNET hRequest, std::vector<HttpHeader>& outHeaders, std::string& outContentType) {
    DWORD byteSize = 0;
    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_RAW_HEADERS_CRLF,
        WINHTTP_HEADER_NAME_BY_INDEX,
        WINHTTP_NO_OUTPUT_BUFFER,
        &byteSize,
        WINHTTP_NO_HEADER_INDEX
    );
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || byteSize < sizeof(wchar_t)) {
        return;
    }

    std::wstring rawBuf(byteSize / sizeof(wchar_t), L'\0');
    if (!WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_RAW_HEADERS_CRLF,
            WINHTTP_HEADER_NAME_BY_INDEX,
            rawBuf.data(),
            &byteSize,
            WINHTTP_NO_HEADER_INDEX)) {
        return;
    }

    const std::string utf8Raw = WideToUtf8(rawBuf);
    std::string_view sv(utf8Raw);
    bool firstLine = true;
    while (!sv.empty()) {
        const size_t eol = sv.find("\r\n");
        const std::string_view line = (eol == std::string_view::npos) ? sv : sv.substr(0, eol);
        if (eol == std::string_view::npos) {
            sv = {};
        } else {
            sv.remove_prefix(eol + 2);
        }

        if (firstLine) {
            firstLine = false;
            continue;
        }
        if (line.empty()) continue;

        const size_t colon = line.find(':');
        if (colon == std::string_view::npos || colon == 0) continue;

        HttpHeader hdr{};
        hdr.name = std::string(TrimAscii(line.substr(0, colon)));
        hdr.value = std::string(TrimAscii(line.substr(colon + 1)));
        if (EqualsIgnoreCase(hdr.name, "content-type") && outContentType.empty()) {
            outContentType = hdr.value;
        }
        outHeaders.push_back(std::move(hdr));
    }
}
#endif

} // namespace

// ============================================================================
// 1. URL Parser Implementation
// ============================================================================

ParsedUrl ParseUrl(std::string_view url) {
    ParsedUrl result{};
    url = TrimAscii(url);
    if (url.empty()) return result;

    const size_t schemeSep = url.find("://");
    if (schemeSep == std::string_view::npos || schemeSep == 0) {
        return result;
    }

    result.scheme.reserve(schemeSep);
    for (size_t i = 0; i < schemeSep; ++i) {
        result.scheme.push_back(AsciiToLower(url[i]));
    }

    if (result.scheme == "http") {
        result.isSecure = false;
        result.isWebSocket = false;
        result.port = 80;
    } else if (result.scheme == "https") {
        result.isSecure = true;
        result.isWebSocket = false;
        result.port = 443;
    } else if (result.scheme == "ws") {
        result.isSecure = false;
        result.isWebSocket = true;
        result.port = 80;
    } else if (result.scheme == "wss") {
        result.isSecure = true;
        result.isWebSocket = true;
        result.port = 443;
    } else {
        return result;
    }

    std::string_view remainder = url.substr(schemeSep + 3);
    if (remainder.empty()) return result;

    const size_t pathPos = remainder.find_first_of("/?#");
    std::string_view authority = (pathPos == std::string_view::npos)
        ? remainder
        : remainder.substr(0, pathPos);

    std::string_view pathPart = (pathPos == std::string_view::npos)
        ? std::string_view{}
        : remainder.substr(pathPos);

    const size_t hashPos = pathPart.find('#');
    if (hashPos != std::string_view::npos) {
        pathPart = pathPart.substr(0, hashPos);
    }

    if (pathPart.empty()) {
        result.pathAndQuery = "/";
    } else if (pathPart.front() == '?') {
        result.pathAndQuery = "/" + std::string(pathPart);
    } else {
        result.pathAndQuery = std::string(pathPart);
    }

    const size_t atPos = authority.rfind('@');
    if (atPos != std::string_view::npos) {
        authority.remove_prefix(atPos + 1);
    }
    if (authority.empty()) return result;

    if (authority.front() == '[') {
        const size_t closeBracket = authority.find(']');
        if (closeBracket == std::string_view::npos) return result;
        result.host = std::string(authority.substr(1, closeBracket - 1));
        std::string_view portPart = authority.substr(closeBracket + 1);
        if (!portPart.empty()) {
            if (portPart.front() != ':') return result;
            portPart.remove_prefix(1);
            uint32_t parsedPort = 0;
            const auto [ptr, ec] = std::from_chars(portPart.data(), portPart.data() + portPart.size(), parsedPort);
            if (ec != std::errc{} || ptr != portPart.data() + portPart.size() || parsedPort == 0 || parsedPort > 65535) {
                return result;
            }
            result.port = static_cast<uint16_t>(parsedPort);
        }
    } else {
        const size_t colonPos = authority.rfind(':');
        if (colonPos != std::string_view::npos) {
            result.host = std::string(authority.substr(0, colonPos));
            std::string_view portPart = authority.substr(colonPos + 1);
            uint32_t parsedPort = 0;
            const auto [ptr, ec] = std::from_chars(portPart.data(), portPart.data() + portPart.size(), parsedPort);
            if (ec != std::errc{} || ptr != portPart.data() + portPart.size() || parsedPort == 0 || parsedPort > 65535) {
                return result;
            }
            result.port = static_cast<uint16_t>(parsedPort);
        } else {
            result.host = std::string(authority);
        }
    }

    if (result.host.empty()) return result;
    result.valid = true;
    return result;
}

std::string_view HttpResponse::GetHeader(std::string_view name) const noexcept {
    for (const auto& h : headers) {
        if (EqualsIgnoreCase(h.name, name)) {
            return h.value;
        }
    }
    return {};
}

// ============================================================================
// 2. HttpClient Implementation (with Persistent Connection Pooling)
// ============================================================================

struct HttpClient::Impl {
    std::string defaultUserAgent{"GuiKit-NetClient/1.0"};

#if defined(_WIN32)
    ScopedWinHttpHandle session{};

    explicit Impl(std::string ua) : defaultUserAgent(std::move(ua)) {
        const std::wstring wUa = Utf8ToWide(defaultUserAgent);
        HINTERNET rawSession = nullptr;
#ifdef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
        rawSession = WinHttpOpen(
            wUa.empty() ? L"GuiKit-NetClient/1.0" : wUa.c_str(),
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        );
#endif
        if (!rawSession) {
            rawSession = WinHttpOpen(
                wUa.empty() ? L"GuiKit-NetClient/1.0" : wUa.c_str(),
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS,
                0
            );
        }
        session.Reset(rawSession);

        if (session) {
            DWORD secureProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#ifdef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
            secureProtocols |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
#endif
            WinHttpSetOption(session.Get(), WINHTTP_OPTION_SECURE_PROTOCOLS, &secureProtocols, sizeof(secureProtocols));

#ifdef WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL
#ifdef WINHTTP_PROTOCOL_FLAG_HTTP2
            DWORD http2Flag = WINHTTP_PROTOCOL_FLAG_HTTP2;
            WinHttpSetOption(session.Get(), WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL, &http2Flag, sizeof(http2Flag));
#endif
#endif
        }
    }

#elif defined(GUIKIT_HAS_CURL)
    mutable std::mutex poolMutex{};
    mutable std::vector<CURL*> handlePool{};

    explicit Impl(std::string ua) : defaultUserAgent(std::move(ua)) {
        static std::once_flag s_curlInitFlag;
        std::call_once(s_curlInitFlag, []() {
            curl_global_init(CURL_GLOBAL_DEFAULT);
        });
    }

    ~Impl() {
        std::lock_guard<std::mutex> lk(poolMutex);
        for (CURL* h : handlePool) {
            if (h) curl_easy_cleanup(h);
        }
        handlePool.clear();
    }

    [[nodiscard]] CURL* AcquireHandle() const {
        {
            std::lock_guard<std::mutex> lk(poolMutex);
            if (!handlePool.empty()) {
                CURL* h = handlePool.back();
                handlePool.pop_back();
                curl_easy_reset(h);
                return h;
            }
        }
        return curl_easy_init();
    }

    void ReleaseHandle(CURL* h) const noexcept {
        if (!h) return;
        std::lock_guard<std::mutex> lk(poolMutex);
        if (handlePool.size() < 16) {
            handlePool.push_back(h);
        } else {
            curl_easy_cleanup(h);
        }
    }
#else
    explicit Impl(std::string ua) : defaultUserAgent(std::move(ua)) {}
#endif
};

HttpClient::HttpClient(std::string defaultUserAgent)
    : m_impl(std::make_unique<Impl>(std::move(defaultUserAgent))) {}

HttpClient::~HttpClient() = default;
HttpClient::HttpClient(HttpClient&&) noexcept = default;
HttpClient& HttpClient::operator=(HttpClient&&) noexcept = default;

std::string_view HttpClient::BackendName() noexcept {
#if defined(_WIN32)
    return "WinHTTP";
#elif defined(__EMSCRIPTEN__)
    return "EmscriptenFetch";
#elif defined(GUIKIT_HAS_CURL)
    return "libcurl";
#else
    return "OfflineStub";
#endif
}

HttpResponse HttpClient::Get(
    std::string_view url,
    std::span<const HttpHeader> headers,
    uint32_t timeoutMs
) const {
    HttpRequest req{};
    req.url = std::string(url);
    req.method = HttpMethod::Get;
    req.headers.assign(headers.begin(), headers.end());
    if (m_impl && !m_impl->defaultUserAgent.empty()) {
        req.userAgent = m_impl->defaultUserAgent;
    }
    req.connectTimeoutMs = timeoutMs;
    req.readTimeoutMs = timeoutMs;
    return Execute(req);
}

HttpResponse HttpClient::Post(
    std::string_view url,
    std::span<const uint8_t> body,
    std::string_view contentType,
    std::span<const HttpHeader> headers,
    uint32_t timeoutMs
) const {
    HttpRequest req{};
    req.url = std::string(url);
    req.method = HttpMethod::Post;
    req.body.assign(body.begin(), body.end());
    req.headers.assign(headers.begin(), headers.end());
    if (!contentType.empty()) {
        req.headers.push_back(HttpHeader{.name = "Content-Type", .value = std::string(contentType)});
    }
    if (m_impl && !m_impl->defaultUserAgent.empty()) {
        req.userAgent = m_impl->defaultUserAgent;
    }
    req.connectTimeoutMs = timeoutMs;
    req.readTimeoutMs = timeoutMs;
    return Execute(req);
}

HttpResponse HttpClient::Execute(const HttpRequest& request) const {
    const auto t0 = std::chrono::steady_clock::now();
    HttpResponse response{};

    const ParsedUrl parsed = ParseUrl(request.url);
    if (!parsed.valid || parsed.isWebSocket) {
        response.errorMessage = "Invalid HTTP/HTTPS URL";
        return response;
    }

#if defined(_WIN32)
    if (!m_impl || !m_impl->session) {
        response.errorMessage = "WinHTTP session not initialized";
        return response;
    }

    const std::wstring wHost = Utf8ToWide(parsed.host);
    ScopedWinHttpHandle hConnect(WinHttpConnect(
        m_impl->session.Get(),
        wHost.c_str(),
        static_cast<INTERNET_PORT>(parsed.port),
        0
    ));
    if (!hConnect) {
        response.errorMessage = FormatWinHttpError("WinHttpConnect failed", GetLastError());
        return response;
    }

    const std::wstring wPath = Utf8ToWide(parsed.pathAndQuery);
    const DWORD requestFlags = parsed.isSecure ? WINHTTP_FLAG_SECURE : 0;
    ScopedWinHttpHandle hRequest(WinHttpOpenRequest(
        hConnect.Get(),
        HttpMethodToWideCString(request.method),
        wPath.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        requestFlags
    ));
    if (!hRequest) {
        response.errorMessage = FormatWinHttpError("WinHttpOpenRequest failed", GetLastError());
        return response;
    }

    const int connTimeout = static_cast<int>(std::max(100u, request.connectTimeoutMs));
    const int ioTimeout = static_cast<int>(std::max(100u, request.readTimeoutMs));
    WinHttpSetTimeouts(hRequest.Get(), connTimeout, connTimeout, ioTimeout, ioTimeout);

    if (!request.followRedirects) {
        DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        WinHttpSetOption(hRequest.Get(), WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));
    }

    if (parsed.isSecure && !request.verifyTlsPeer) {
        DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                         SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE |
                         SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                         SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        WinHttpSetOption(hRequest.Get(), WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
    }

    std::wstring extraHeaders;
    const std::string& effectiveUa = request.userAgent.empty() ? m_impl->defaultUserAgent : request.userAgent;
    if (!effectiveUa.empty() && IsSafeHeaderToken(effectiveUa)) {
        extraHeaders += L"User-Agent: " + Utf8ToWide(effectiveUa) + L"\r\n";
    }
    for (const auto& hdr : request.headers) {
        if (!hdr.name.empty() && IsSafeHeaderToken(hdr.name) && IsSafeHeaderToken(hdr.value)) {
            extraHeaders += Utf8ToWide(hdr.name) + L": " + Utf8ToWide(hdr.value) + L"\r\n";
        }
    }

    LPVOID bodyPtr = request.body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<uint8_t*>(request.body.data());
    const DWORD bodyLen = static_cast<DWORD>(request.body.size());

    if (!WinHttpSendRequest(
            hRequest.Get(),
            extraHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : extraHeaders.c_str(),
            extraHeaders.empty() ? 0 : static_cast<DWORD>(extraHeaders.size()),
            bodyPtr,
            bodyLen,
            bodyLen,
            0)) {
        response.errorMessage = FormatWinHttpError("WinHttpSendRequest failed", GetLastError());
        return response;
    }

    if (!WinHttpReceiveResponse(hRequest.Get(), nullptr)) {
        response.errorMessage = FormatWinHttpError("WinHttpReceiveResponse failed", GetLastError());
        return response;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (WinHttpQueryHeaders(
            hRequest.Get(),
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX)) {
        response.statusCode = static_cast<int>(statusCode);
    }

    ParseWinHttpRawHeaders(hRequest.Get(), response.headers, response.contentType);

    if (request.method != HttpMethod::Head) {
        DWORD dwSize = 0;
        while (WinHttpQueryDataAvailable(hRequest.Get(), &dwSize) && dwSize > 0) {
            const size_t oldSize = response.body.size();
            if (oldSize + dwSize > request.maxResponseBytes) {
                response.errorMessage = "Response exceeded maxResponseBytes limit";
                response.body.clear();
                break;
            }
            response.body.resize(oldSize + dwSize);
            DWORD dwDownloaded = 0;
            if (!WinHttpReadData(hRequest.Get(), response.body.data() + oldSize, dwSize, &dwDownloaded)) {
                response.errorMessage = FormatWinHttpError("WinHttpReadData failed", GetLastError());
                response.body.clear();
                break;
            }
            response.body.resize(oldSize + dwDownloaded);
        }
    }

#elif defined(__EMSCRIPTEN__)
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    std::strncpy(attr.requestMethod, HttpMethodToCString(request.method), sizeof(attr.requestMethod) - 1);
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_SYNCHRONOUS;
    attr.timeoutMSecs = std::max(request.connectTimeoutMs, request.readTimeoutMs);

    std::vector<const char*> headerKvs;
    headerKvs.reserve(request.headers.size() * 2 + 1);
    for (const auto& h : request.headers) {
        if (!h.name.empty() && IsSafeHeaderToken(h.name) && IsSafeHeaderToken(h.value)) {
            headerKvs.push_back(h.name.c_str());
            headerKvs.push_back(h.value.c_str());
        }
    }
    headerKvs.push_back(nullptr);
    if (headerKvs.size() > 1) {
        attr.requestHeaders = headerKvs.data();
    }

    if (!request.body.empty()) {
        attr.requestData = reinterpret_cast<const char*>(request.body.data());
        attr.requestDataSize = request.body.size();
    }

    emscripten_fetch_t* fetch = emscripten_fetch(&attr, request.url.c_str());
    if (!fetch) {
        response.errorMessage = "emscripten_fetch failed to initialize";
        return response;
    }

    response.statusCode = static_cast<int>(fetch->status);
    if (fetch->data && fetch->numBytes > 0) {
        if (fetch->numBytes <= request.maxResponseBytes) {
            const auto* bytes = reinterpret_cast<const uint8_t*>(fetch->data);
            response.body.assign(bytes, bytes + fetch->numBytes);
        } else {
            response.errorMessage = "Response exceeded maxResponseBytes limit";
        }
    }
    emscripten_fetch_close(fetch);

#elif defined(GUIKIT_HAS_CURL)
    if (!m_impl) {
        response.errorMessage = "HttpClient not initialized";
        return response;
    }

    CURL* curl = m_impl->AcquireHandle();
    if (!curl) {
        response.errorMessage = "curl_easy_init failed";
        return response;
    }

    struct WriteContext {
        std::vector<uint8_t>* body{nullptr};
        size_t maxBytes{0};
        bool overflow{false};
    } writeCtx{&response.body, request.maxResponseBytes, false};

    auto writeCb = +[](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
        auto* ctx = static_cast<WriteContext*>(userdata);
        const size_t total = size * nmemb;
        if (!ctx || !ctx->body) return 0;
        if (ctx->body->size() + total > ctx->maxBytes) {
            ctx->overflow = true;
            return 0;
        }
        const size_t oldSize = ctx->body->size();
        ctx->body->resize(oldSize + total);
        std::memcpy(ctx->body->data() + oldSize, ptr, total);
        return total;
    };

    auto headerCb = +[](char* buffer, size_t size, size_t nitems, void* userdata) -> size_t {
        auto* resp = static_cast<HttpResponse*>(userdata);
        const size_t total = size * nitems;
        if (!resp || total == 0) return total;
        std::string_view line(buffer, total);
        line = TrimAscii(line);
        const size_t colon = line.find(':');
        if (colon != std::string_view::npos && colon > 0) {
            HttpHeader hdr{};
            hdr.name = std::string(TrimAscii(line.substr(0, colon)));
            hdr.value = std::string(TrimAscii(line.substr(colon + 1)));
            if (EqualsIgnoreCase(hdr.name, "content-type") && resp->contentType.empty()) {
                resp->contentType = hdr.value;
            }
            resp->headers.push_back(std::move(hdr));
        }
        return total;
    };

    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(std::max(100u, request.connectTimeoutMs)));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(std::max(100u, request.connectTimeoutMs + request.readTimeoutMs)));
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, request.followRedirects ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, request.verifyTlsPeer ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, request.verifyTlsPeer ? 2L : 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writeCtx);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response);

    const std::string& effectiveUa = request.userAgent.empty() ? m_impl->defaultUserAgent : request.userAgent;
    if (!effectiveUa.empty() && IsSafeHeaderToken(effectiveUa)) {
        curl_easy_setopt(curl, CURLOPT_USERAGENT, effectiveUa.c_str());
    }

    if (request.method == HttpMethod::Head) {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    } else if (request.method == HttpMethod::Post) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.empty() ? "" : reinterpret_cast<const char*>(request.body.data()));
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
    } else if (request.method != HttpMethod::Get) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, HttpMethodToCString(request.method));
        if (!request.body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, reinterpret_cast<const char*>(request.body.data()));
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
        }
    }

    struct curl_slist* headerList = nullptr;
    for (const auto& h : request.headers) {
        if (!h.name.empty() && IsSafeHeaderToken(h.name) && IsSafeHeaderToken(h.value)) {
            const std::string combined = h.name + ": " + h.value;
            headerList = curl_slist_append(headerList, combined.c_str());
        }
    }
    if (headerList) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    }

    const CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        response.statusCode = static_cast<int>(httpCode);
    } else if (writeCtx.overflow) {
        response.body.clear();
        response.errorMessage = "Response exceeded maxResponseBytes limit";
    } else {
        response.errorMessage = curl_easy_strerror(res);
    }

    if (headerList) {
        curl_slist_free_all(headerList);
    }
    m_impl->ReleaseHandle(curl);
#else
    response.errorMessage = "No HTTP backend compiled for this platform";
#endif

    const auto t1 = std::chrono::steady_clock::now();
    response.elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return response;
}

// ============================================================================
// 3. WebSocketClient Implementation
// ============================================================================

struct WebSocketClient::Impl {
    mutable std::mutex mutex{};
    WebSocketState state{WebSocketState::Disconnected};
    std::string lastError{};
    size_t maxMessageBytes{16u * 1024u * 1024u};

#if defined(_WIN32)
    ScopedWinHttpHandle hSession{};
    ScopedWinHttpHandle hConnect{};
    ScopedWinHttpHandle hWebSocket{};

    void CleanupHandles() noexcept {
        hWebSocket.Reset();
        hConnect.Reset();
        hSession.Reset();
    }
#elif defined(__EMSCRIPTEN__)
    EMSCRIPTEN_WEBSOCKET_T wsHandle{0};
    std::deque<WebSocketMessage> incomingQueue{};

    void CleanupHandles() noexcept {
        if (wsHandle > 0) {
            emscripten_websocket_delete(wsHandle);
            wsHandle = 0;
        }
    }

    ~Impl() {
        CleanupHandles();
    }
#elif defined(GUIKIT_HAS_CURL)
    CURL* curl{nullptr};

    void CleanupHandles() noexcept {
        if (curl) {
            curl_easy_cleanup(curl);
            curl = nullptr;
        }
    }

    ~Impl() {
        CleanupHandles();
    }
#else
    void CleanupHandles() noexcept {}
#endif
};

WebSocketClient::WebSocketClient()
    : m_impl(std::make_unique<Impl>()) {}

WebSocketClient::~WebSocketClient() {
    Close();
}

WebSocketClient::WebSocketClient(WebSocketClient&&) noexcept = default;
WebSocketClient& WebSocketClient::operator=(WebSocketClient&&) noexcept = default;

WebSocketState WebSocketClient::GetState() const noexcept {
    if (!m_impl) return WebSocketState::Disconnected;
    std::lock_guard<std::mutex> lk(m_impl->mutex);
    return m_impl->state;
}

std::string WebSocketClient::GetLastError() const {
    if (!m_impl) return {};
    std::lock_guard<std::mutex> lk(m_impl->mutex);
    return m_impl->lastError;
}

bool WebSocketClient::Connect(std::string_view url, const WebSocketConnectOptions& options) {
    if (!m_impl) return false;
    Close();

    const ParsedUrl parsed = ParseUrl(url);
    if (!parsed.valid || !parsed.isWebSocket) {
        std::lock_guard<std::mutex> lk(m_impl->mutex);
        m_impl->state = WebSocketState::Error;
        m_impl->lastError = "Invalid WebSocket URL (expected ws:// or wss://)";
        return false;
    }

    {
        std::lock_guard<std::mutex> lk(m_impl->mutex);
        m_impl->state = WebSocketState::Connecting;
        m_impl->lastError.clear();
        m_impl->maxMessageBytes = options.maxMessageBytes;
    }

#if defined(_WIN32)
    const std::wstring wUa = Utf8ToWide(options.userAgent.empty() ? "GuiKit-NetClient/1.0" : options.userAgent);
    ScopedWinHttpHandle hSession(WinHttpOpen(
        wUa.c_str(),
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    ));
    if (!hSession) {
        const DWORD err = ::GetLastError();
        std::lock_guard<std::mutex> lk(m_impl->mutex);
        m_impl->state = WebSocketState::Error;
        m_impl->lastError = FormatWinHttpError("WinHttpOpen failed for WebSocket", err);
        return false;
    }

    const int timeoutMs = static_cast<int>(std::max(500u, options.connectTimeoutMs));
    WinHttpSetTimeouts(hSession.Get(), timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    const std::wstring wHost = Utf8ToWide(parsed.host);
    ScopedWinHttpHandle hConnect(WinHttpConnect(
        hSession.Get(),
        wHost.c_str(),
        static_cast<INTERNET_PORT>(parsed.port),
        0
    ));
    if (!hConnect) {
        const DWORD err = ::GetLastError();
        std::lock_guard<std::mutex> lk(m_impl->mutex);
        m_impl->state = WebSocketState::Error;
        m_impl->lastError = FormatWinHttpError("WinHttpConnect failed for WebSocket", err);
        return false;
    }

    const std::wstring wPath = Utf8ToWide(parsed.pathAndQuery);
    ScopedWinHttpHandle hRequest(WinHttpOpenRequest(
        hConnect.Get(),
        L"GET",
        wPath.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        parsed.isSecure ? WINHTTP_FLAG_SECURE : 0
    ));
    if (!hRequest) {
        const DWORD err = ::GetLastError();
        std::lock_guard<std::mutex> lk(m_impl->mutex);
        m_impl->state = WebSocketState::Error;
        m_impl->lastError = FormatWinHttpError("WinHttpOpenRequest failed for WebSocket", err);
        return false;
    }

    if (parsed.isSecure && !options.verifyTlsPeer) {
        DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                         SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE |
                         SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                         SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        WinHttpSetOption(hRequest.Get(), WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
    }

    if (!WinHttpSetOption(hRequest.Get(), WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0)) {
        const DWORD err = ::GetLastError();
        std::lock_guard<std::mutex> lk(m_impl->mutex);
        m_impl->state = WebSocketState::Error;
        m_impl->lastError = FormatWinHttpError("WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET failed", err);
        return false;
    }

    std::wstring extraHeaders;
    for (const auto& hdr : options.headers) {
        if (!hdr.name.empty() && IsSafeHeaderToken(hdr.name) && IsSafeHeaderToken(hdr.value)) {
            extraHeaders += Utf8ToWide(hdr.name) + L": " + Utf8ToWide(hdr.value) + L"\r\n";
        }
    }

    if (!WinHttpSendRequest(
            hRequest.Get(),
            extraHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : extraHeaders.c_str(),
            extraHeaders.empty() ? 0 : static_cast<DWORD>(extraHeaders.size()),
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0) ||
        !WinHttpReceiveResponse(hRequest.Get(), nullptr)) {
        const DWORD err = ::GetLastError();
        std::lock_guard<std::mutex> lk(m_impl->mutex);
        m_impl->state = WebSocketState::Error;
        m_impl->lastError = FormatWinHttpError("WebSocket HTTP upgrade handshake failed", err);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (WinHttpQueryHeaders(
            hRequest.Get(),
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX) &&
        statusCode != 101) {
        std::lock_guard<std::mutex> lk(m_impl->mutex);
        m_impl->state = WebSocketState::Error;
        m_impl->lastError = std::format("WebSocket upgrade rejected by server (HTTP {})", statusCode);
        return false;
    }

    ScopedWinHttpHandle hWebSocket(WinHttpWebSocketCompleteUpgrade(hRequest.Get(), 0));
    hRequest.Reset();

    if (!hWebSocket) {
        const DWORD err = ::GetLastError();
        std::lock_guard<std::mutex> lk(m_impl->mutex);
        m_impl->state = WebSocketState::Error;
        m_impl->lastError = FormatWinHttpError("WinHttpWebSocketCompleteUpgrade failed", err);
        return false;
    }

    {
        std::lock_guard<std::mutex> lk(m_impl->mutex);
        m_impl->hSession = std::move(hSession);
        m_impl->hConnect = std::move(hConnect);
        m_impl->hWebSocket = std::move(hWebSocket);
        m_impl->state = WebSocketState::Connected;
    }
    return true;

#elif defined(__EMSCRIPTEN__)
    if (!emscripten_websocket_is_supported()) {
        std::lock_guard<std::mutex> lk(m_impl->mutex);
        m_impl->state = WebSocketState::Error;
        m_impl->lastError = "Emscripten WebSockets not supported in this environment";
        return false;
    }

    const std::string urlStr(url);
    EmscriptenWebSocketCreateAttributes wsAttr;
    emscripten_websocket_init_create_attributes(&wsAttr);
    wsAttr.url = urlStr.c_str();
    wsAttr.createOnMainThread = EM_TRUE;

    EMSCRIPTEN_WEBSOCKET_T ws = emscripten_websocket_new(&wsAttr);
    if (ws <= 0) {
        std::lock_guard<std::mutex> lk(m_impl->mutex);
        m_impl->state = WebSocketState::Error;
        m_impl->lastError = "emscripten_websocket_new failed";
        return false;
    }

    emscripten_websocket_set_onopen_callback(ws, m_impl.get(), +[](int, const EmscriptenWebSocketOpenEvent*, void* userData) -> EM_BOOL {
        auto* self = static_cast<Impl*>(userData);
        std::lock_guard<std::mutex> lk(self->mutex);
        self->state = WebSocketState::Connected;
        return EM_TRUE;
    });

    emscripten_websocket_set_onmessage_callback(ws, m_impl.get(), +[](int, const EmscriptenWebSocketMessageEvent* ev, void* userData) -> EM_BOOL {
        auto* self = static_cast<Impl*>(userData);
        if (!ev) return EM_TRUE;
        std::lock_guard<std::mutex> lk(self->mutex);
        if (ev->numBytes <= self->maxMessageBytes) {
            WebSocketMessage msg{};
            msg.type = ev->isText ? WebSocketFrameType::Text : WebSocketFrameType::Binary;
            const size_t len = (ev->isText && ev->numBytes > 0 && ev->data[ev->numBytes - 1] == 0)
                ? static_cast<size_t>(ev->numBytes - 1)
                : static_cast<size_t>(ev->numBytes);
            msg.data.assign(ev->data, ev->data + len);
            self->incomingQueue.push_back(std::move(msg));
        }
        return EM_TRUE;
    });

    emscripten_websocket_set_onerror_callback(ws, m_impl.get(), +[](int, const EmscriptenWebSocketErrorEvent*, void* userData) -> EM_BOOL {
        auto* self = static_cast<Impl*>(userData);
        std::lock_guard<std::mutex> lk(self->mutex);
        self->state = WebSocketState::Error;
        self->lastError = "WebSocket error event";
        return EM_TRUE;
    });

    emscripten_websocket_set_onclose_callback(ws, m_impl.get(), +[](int, const EmscriptenWebSocketCloseEvent* ev, void* userData) -> EM_BOOL {
        auto* self = static_cast<Impl*>(userData);
        std::lock_guard<std::mutex> lk(self->mutex);
        self->state = WebSocketState::Disconnected;
        WebSocketMessage closeMsg{};
        closeMsg.type = WebSocketFrameType::Close;
        closeMsg.closeStatus = ev ? ev->code : 1000;
        self->incomingQueue.push_back(std::move(closeMsg));
        return EM_TRUE;
    });

    {
        std::lock_guard<std::mutex> lk(m_impl->mutex);
        m_impl->wsHandle = ws;
    }
    return true;

#elif defined(GUIKIT_HAS_CURL) && (LIBCURL_VERSION_NUM >= 0x075600)
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::lock_guard<std::mutex> lk(m_impl->mutex);
        m_impl->state = WebSocketState::Error;
        m_impl->lastError = "curl_easy_init failed for WebSocket";
        return false;
    }

    const std::string urlStr(url);
    curl_easy_setopt(curl, CURLOPT_URL, urlStr.c_str());
    curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(std::max(500u, options.connectTimeoutMs)));
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, options.verifyTlsPeer ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, options.verifyTlsPeer ? 2L : 0L);

    const CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::lock_guard<std::mutex> lk(m_impl->mutex);
        m_impl->state = WebSocketState::Error;
        m_impl->lastError = curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        return false;
    }

    {
        std::lock_guard<std::mutex> lk(m_impl->mutex);
        m_impl->curl = curl;
        m_impl->state = WebSocketState::Connected;
    }
    return true;
#else
    (void)options;
    std::lock_guard<std::mutex> lk(m_impl->mutex);
    m_impl->state = WebSocketState::Error;
    m_impl->lastError = "WebSocket backend not available in this build";
    return false;
#endif
}

bool WebSocketClient::SendText(std::string_view text) {
    if (!m_impl) return false;
#if defined(_WIN32)
    HINTERNET hWs = nullptr;
    {
        std::lock_guard<std::mutex> lk(m_impl->mutex);
        if (m_impl->state != WebSocketState::Connected || !m_impl->hWebSocket) return false;
        hWs = m_impl->hWebSocket.Get();
    }
    const DWORD err = WinHttpWebSocketSend(
        hWs,
        WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
        const_cast<char*>(text.data()),
        static_cast<DWORD>(text.size())
    );
    return err == NO_ERROR;
#elif defined(__EMSCRIPTEN__)
    std::lock_guard<std::mutex> lk(m_impl->mutex);
    if (m_impl->state != WebSocketState::Connected || m_impl->wsHandle <= 0) return false;
    const std::string copy(text);
    return emscripten_websocket_send_utf8_text(m_impl->wsHandle, copy.c_str()) == EMSCRIPTEN_RESULT_SUCCESS;
#elif defined(GUIKIT_HAS_CURL) && (LIBCURL_VERSION_NUM >= 0x075600)
    std::lock_guard<std::mutex> lk(m_impl->mutex);
    if (m_impl->state != WebSocketState::Connected || !m_impl->curl) return false;
    size_t sent = 0;
    return curl_ws_send(m_impl->curl, text.data(), text.size(), &sent, 0, CURLWS_TEXT) == CURLE_OK;
#else
    (void)text;
    return false;
#endif
}

bool WebSocketClient::SendBinary(std::span<const uint8_t> payload) {
    if (!m_impl) return false;
#if defined(_WIN32)
    HINTERNET hWs = nullptr;
    {
        std::lock_guard<std::mutex> lk(m_impl->mutex);
        if (m_impl->state != WebSocketState::Connected || !m_impl->hWebSocket) return false;
        hWs = m_impl->hWebSocket.Get();
    }
    const DWORD err = WinHttpWebSocketSend(
        hWs,
        WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE,
        const_cast<uint8_t*>(payload.data()),
        static_cast<DWORD>(payload.size())
    );
    return err == NO_ERROR;
#elif defined(__EMSCRIPTEN__)
    std::lock_guard<std::mutex> lk(m_impl->mutex);
    if (m_impl->state != WebSocketState::Connected || m_impl->wsHandle <= 0) return false;
    return emscripten_websocket_send_binary(
        m_impl->wsHandle,
        const_cast<uint8_t*>(payload.data()),
        static_cast<uint32_t>(payload.size())
    ) == EMSCRIPTEN_RESULT_SUCCESS;
#elif defined(GUIKIT_HAS_CURL) && (LIBCURL_VERSION_NUM >= 0x075600)
    std::lock_guard<std::mutex> lk(m_impl->mutex);
    if (m_impl->state != WebSocketState::Connected || !m_impl->curl) return false;
    size_t sent = 0;
    return curl_ws_send(m_impl->curl, payload.data(), payload.size(), &sent, 0, CURLWS_BINARY) == CURLE_OK;
#else
    (void)payload;
    return false;
#endif
}

bool WebSocketClient::Receive(WebSocketMessage& outMessage) {
    outMessage = {};
    if (!m_impl) return false;

#if defined(_WIN32)
    HINTERNET hWs = nullptr;
    size_t maxBytes = 0;
    {
        std::lock_guard<std::mutex> lk(m_impl->mutex);
        if (m_impl->state != WebSocketState::Connected || !m_impl->hWebSocket) return false;
        hWs = m_impl->hWebSocket.Get();
        maxBytes = m_impl->maxMessageBytes;
    }

    uint8_t chunk[8192];
    for (;;) {
        DWORD bytesRead = 0;
        WINHTTP_WEB_SOCKET_BUFFER_TYPE bufType = WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE;
        const DWORD err = WinHttpWebSocketReceive(hWs, chunk, sizeof(chunk), &bytesRead, &bufType);
        if (err != NO_ERROR) {
            std::lock_guard<std::mutex> lk(m_impl->mutex);
            m_impl->state = WebSocketState::Error;
            m_impl->lastError = FormatWinHttpError("WinHttpWebSocketReceive failed", err);
            return false;
        }

        if (bufType == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
            USHORT status = 1000;
            DWORD reasonConsumed = 0;
            char reasonBuf[128]{};
            WinHttpWebSocketQueryCloseStatus(hWs, &status, reasonBuf, sizeof(reasonBuf), &reasonConsumed);
            outMessage.type = WebSocketFrameType::Close;
            outMessage.closeStatus = status;
            outMessage.data.assign(reinterpret_cast<const uint8_t*>(reasonBuf),
                                   reinterpret_cast<const uint8_t*>(reasonBuf) + reasonConsumed);
            std::lock_guard<std::mutex> lk(m_impl->mutex);
            m_impl->state = WebSocketState::Disconnected;
            return true;
        }

        if (outMessage.data.size() + bytesRead > maxBytes) {
            std::lock_guard<std::mutex> lk(m_impl->mutex);
            m_impl->state = WebSocketState::Error;
            m_impl->lastError = "WebSocket message exceeded maxMessageBytes";
            return false;
        }

        outMessage.data.insert(outMessage.data.end(), chunk, chunk + bytesRead);
        if (bufType == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) {
            outMessage.type = WebSocketFrameType::Text;
            return true;
        }
        if (bufType == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE) {
            outMessage.type = WebSocketFrameType::Binary;
            return true;
        }
    }

#elif defined(__EMSCRIPTEN__)
    std::lock_guard<std::mutex> lk(m_impl->mutex);
    if (m_impl->incomingQueue.empty()) return false;
    outMessage = std::move(m_impl->incomingQueue.front());
    m_impl->incomingQueue.pop_front();
    return true;

#elif defined(GUIKIT_HAS_CURL) && (LIBCURL_VERSION_NUM >= 0x075600)
    CURL* curl = nullptr;
    size_t maxBytes = 0;
    {
        std::lock_guard<std::mutex> lk(m_impl->mutex);
        if (m_impl->state != WebSocketState::Connected || !m_impl->curl) return false;
        curl = m_impl->curl;
        maxBytes = m_impl->maxMessageBytes;
    }

    uint8_t chunk[8192];
    for (;;) {
        size_t rlen = 0;
        const struct curl_ws_frame* meta = nullptr;
        const CURLcode res = curl_ws_recv(curl, chunk, sizeof(chunk), &rlen, &meta);
        if (res == CURLE_AGAIN) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        if (res != CURLE_OK || !meta) {
            std::lock_guard<std::mutex> lk(m_impl->mutex);
            m_impl->state = WebSocketState::Error;
            m_impl->lastError = curl_easy_strerror(res);
            return false;
        }
        if (meta->flags & CURLWS_CLOSE) {
            outMessage.type = WebSocketFrameType::Close;
            std::lock_guard<std::mutex> lk(m_impl->mutex);
            m_impl->state = WebSocketState::Disconnected;
            return true;
        }
        if (outMessage.data.size() + rlen > maxBytes) {
            std::lock_guard<std::mutex> lk(m_impl->mutex);
            m_impl->state = WebSocketState::Error;
            m_impl->lastError = "WebSocket message exceeded maxMessageBytes";
            return false;
        }
        outMessage.data.insert(outMessage.data.end(), chunk, chunk + rlen);
        if (meta->bytesleft == 0 && !(meta->flags & CURLWS_CONT)) {
            outMessage.type = (meta->flags & CURLWS_TEXT) ? WebSocketFrameType::Text : WebSocketFrameType::Binary;
            return true;
        }
    }
#else
    return false;
#endif
}

void WebSocketClient::Close(uint16_t statusCode, std::string_view reason) noexcept {
    if (!m_impl) return;
#if defined(_WIN32)
    HINTERNET hWs = nullptr;
    {
        std::lock_guard<std::mutex> lk(m_impl->mutex);
        if (m_impl->hWebSocket && m_impl->state == WebSocketState::Connected) {
            hWs = m_impl->hWebSocket.Get();
            m_impl->state = WebSocketState::Closing;
        }
    }
    if (hWs) {
        WinHttpWebSocketClose(
            hWs,
            statusCode,
            reason.empty() ? nullptr : const_cast<char*>(reason.data()),
            static_cast<DWORD>(std::min<size_t>(reason.size(), 120u))
        );
    }
    {
        std::lock_guard<std::mutex> lk(m_impl->mutex);
        m_impl->CleanupHandles();
        m_impl->state = WebSocketState::Disconnected;
    }
#elif defined(__EMSCRIPTEN__)
    std::lock_guard<std::mutex> lk(m_impl->mutex);
    if (m_impl->wsHandle > 0) {
        const std::string reasonStr(reason.substr(0, std::min<size_t>(reason.size(), 120u)));
        emscripten_websocket_close(m_impl->wsHandle, statusCode, reasonStr.c_str());
        m_impl->CleanupHandles();
    }
    m_impl->state = WebSocketState::Disconnected;
#elif defined(GUIKIT_HAS_CURL) && (LIBCURL_VERSION_NUM >= 0x075600)
    std::lock_guard<std::mutex> lk(m_impl->mutex);
    if (m_impl->curl && m_impl->state == WebSocketState::Connected) {
        size_t sent = 0;
        curl_ws_send(m_impl->curl, "", 0, &sent, 0, CURLWS_CLOSE);
    }
    m_impl->CleanupHandles();
    m_impl->state = WebSocketState::Disconnected;
    (void)statusCode;
    (void)reason;
#else
    (void)statusCode;
    (void)reason;
    std::lock_guard<std::mutex> lk(m_impl->mutex);
    m_impl->state = WebSocketState::Disconnected;
#endif
}

} // namespace guikit::net
