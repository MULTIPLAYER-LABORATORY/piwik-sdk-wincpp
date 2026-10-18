#pragma once

#include <windows.h>
#include <winhttp.h>
#include <string>

// Internal dependency boundary. Not part of the installed application API.
namespace piwik { namespace detail {

struct HttpRequest
{
    std::wstring host;
    std::wstring path;
    std::wstring method;
    std::wstring headers;
    std::string body;
    bool secure = false;
};

struct HttpResponse
{
    unsigned int statusCode = 0;
    DWORD errorCode = ERROR_SUCCESS;
    std::wstring errorMessage;
    std::string body;
};

class IHttpTransport
{
public:
    virtual ~IHttpTransport() = default;
    // Called serially by the dispatcher worker. Implementations must not throw.
    virtual HttpResponse Send(const HttpRequest& request) = 0;
};

class WinHttpTransport final : public IHttpTransport
{
public:
    WinHttpTransport() = default;
    ~WinHttpTransport() override;
    WinHttpTransport(const WinHttpTransport&) = delete;
    WinHttpTransport& operator=(const WinHttpTransport&) = delete;
    HttpResponse Send(const HttpRequest& request) override;

private:
    HINTERNET session_ = nullptr;
};

}} // namespace piwik::detail
