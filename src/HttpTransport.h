#pragma once

#include <windows.h>
#include <winhttp.h>
#include <string>

// Internal dependency boundary. Not part of the installed application API.
namespace piwik { namespace detail {

struct HttpRequest
{
    std::wstring host;
    INTERNET_PORT port = INTERNET_DEFAULT_PORT;
    std::wstring path;
    std::wstring method;
    std::wstring headers;
    std::string body;
    bool secure = false;
    bool verifyServerCertificate = true;
};

// Zero keeps WinHTTP's certificate checks enabled. An explicit HTTPS opt-out
// relaxes only the legacy hostname, validity date, issuer, and usage checks.
DWORD CertificateSecurityFlags(const HttpRequest& request);

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
    bool sessionVerifiesServerCertificate_ = true;
};

}} // namespace piwik::detail
