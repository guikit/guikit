#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace guikit::net {

// ============================================================================
// 1. RFC 3986 URL Parser (http / https / ws / wss)
// ============================================================================

struct ParsedUrl {
    std::string scheme{};       // "http", "https", "ws", "wss" (lowercase)
    std::string host{};         // Domain, IPv4, or bracketed IPv6
    uint16_t port{0};           // Explicit or default port (80 or 443)
    std::string pathAndQuery{}; // Starts with '/', includes '?query' if present
    bool isSecure{false};       // true for "https" and "wss"
    bool isWebSocket{false};    // true for "ws" and "wss"
    bool valid{false};
};

[[nodiscard]] ParsedUrl ParseUrl(std::string_view url);

// ============================================================================
// 2. Cross-Platform HTTP / HTTPS Client (WinHTTP / libcurl / Emscripten Fetch)
// ============================================================================

enum class HttpMethod : uint8_t {
    Get = 0,
    Post,
    Put,
    Delete,
    Head,
    Patch
};

struct HttpHeader {
    std::string name{};
    std::string value{};
};

struct HttpRequest {
    std::string url{};
    HttpMethod method{HttpMethod::Get};
    std::vector<HttpHeader> headers{};
    std::vector<uint8_t> body{};
    std::string userAgent{"GuiKit-NetClient/1.0"};
    uint32_t connectTimeoutMs{2500};
    uint32_t readTimeoutMs{5000};
    size_t maxResponseBytes{16u * 1024u * 1024u}; // 16 MB safety cap
    bool followRedirects{true};
    bool verifyTlsPeer{true};
};

struct HttpResponse {
    int statusCode{0};
    std::vector<HttpHeader> headers{};
    std::vector<uint8_t> body{};
    std::string contentType{};
    std::string errorMessage{};
    double elapsedMs{0.0};

    [[nodiscard]] bool Succeeded() const noexcept {
        return statusCode >= 200 && statusCode < 300 && errorMessage.empty();
    }

    [[nodiscard]] std::string_view BodyAsText() const noexcept {
        if (body.empty()) return {};
        return std::string_view(reinterpret_cast<const char*>(body.data()), body.size());
    }

    [[nodiscard]] std::string_view GetHeader(std::string_view name) const noexcept;
};

class IHttpClient {
public:
    virtual ~IHttpClient() = default;
    [[nodiscard]] virtual HttpResponse Execute(const HttpRequest& request) const = 0;
};

class HttpClient final : public IHttpClient {
public:
    explicit HttpClient(std::string defaultUserAgent = "GuiKit-NetClient/1.0");
    ~HttpClient() override;

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&&) noexcept;
    HttpClient& operator=(HttpClient&&) noexcept;

    /// Thread-safe synchronous HTTP/HTTPS execution reusing the underlying OS session pool.
    [[nodiscard]] HttpResponse Execute(const HttpRequest& request) const override;

    /// Convenience helper for GET requests.
    [[nodiscard]] HttpResponse Get(
        std::string_view url,
        std::span<const HttpHeader> headers = {},
        uint32_t timeoutMs = 3000
    ) const;

    /// Convenience helper for POST requests.
    [[nodiscard]] HttpResponse Post(
        std::string_view url,
        std::span<const uint8_t> body,
        std::string_view contentType = "application/octet-stream",
        std::span<const HttpHeader> headers = {},
        uint32_t timeoutMs = 5000
    ) const;

    /// Returns active native backend identifier ("WinHTTP", "libcurl", "EmscriptenFetch", or "OfflineStub").
    [[nodiscard]] static std::string_view BackendName() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// ============================================================================
// 3. Cross-Platform WebSocket Client (WinHTTP WS / libcurl WS / Emscripten WS)
// ============================================================================

enum class WebSocketState : uint8_t {
    Disconnected = 0,
    Connecting,
    Connected,
    Closing,
    Error
};

enum class WebSocketFrameType : uint8_t {
    Text = 0,
    Binary,
    Close
};

struct WebSocketMessage {
    WebSocketFrameType type{WebSocketFrameType::Text};
    std::vector<uint8_t> data{};
    uint16_t closeStatus{1000};

    [[nodiscard]] std::string_view AsText() const noexcept {
        if (data.empty()) return {};
        return std::string_view(reinterpret_cast<const char*>(data.data()), data.size());
    }
};

struct WebSocketConnectOptions {
    std::vector<HttpHeader> headers{};
    std::string userAgent{"GuiKit-NetClient/1.0"};
    uint32_t connectTimeoutMs{5000};
    size_t maxMessageBytes{16u * 1024u * 1024u};
    bool verifyTlsPeer{true};
};

class IWebSocketClient {
public:
    virtual ~IWebSocketClient() = default;
    [[nodiscard]] virtual bool Connect(std::string_view url, const WebSocketConnectOptions& options = {}) = 0;
    [[nodiscard]] virtual bool SendText(std::string_view text) = 0;
    [[nodiscard]] virtual bool SendBinary(std::span<const uint8_t> payload) = 0;
    [[nodiscard]] virtual bool Receive(WebSocketMessage& outMessage) = 0;
    virtual void Close(uint16_t statusCode = 1000, std::string_view reason = "Normal closure") noexcept = 0;
    [[nodiscard]] virtual WebSocketState GetState() const noexcept = 0;
    [[nodiscard]] virtual std::string GetLastError() const = 0;
};

class WebSocketClient final : public IWebSocketClient {
public:
    WebSocketClient();
    ~WebSocketClient() override;

    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;
    WebSocketClient(WebSocketClient&&) noexcept;
    WebSocketClient& operator=(WebSocketClient&&) noexcept;

    [[nodiscard]] bool Connect(std::string_view url, const WebSocketConnectOptions& options = {}) override;
    [[nodiscard]] bool SendText(std::string_view text) override;
    [[nodiscard]] bool SendBinary(std::span<const uint8_t> payload) override;
    [[nodiscard]] bool Receive(WebSocketMessage& outMessage) override;
    void Close(uint16_t statusCode = 1000, std::string_view reason = "Normal closure") noexcept override;

    [[nodiscard]] WebSocketState GetState() const noexcept override;
    [[nodiscard]] std::string GetLastError() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace guikit::net
