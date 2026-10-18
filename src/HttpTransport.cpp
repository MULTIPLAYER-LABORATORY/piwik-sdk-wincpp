#include "Config.h"
#include "HttpTransport.h"

namespace piwik { namespace detail {
namespace {
class ScopedInternetHandle
{
public:
    explicit ScopedInternetHandle(HINTERNET handle) : value(handle) {}
    ~ScopedInternetHandle() { if (value) ::WinHttpCloseHandle(value); }
    ScopedInternetHandle(const ScopedInternetHandle&) = delete;
    ScopedInternetHandle& operator=(const ScopedInternetHandle&) = delete;
    HINTERNET value;
};

HttpResponse Failure(const wchar_t* message)
{
    const DWORD error = ::GetLastError();
    HttpResponse response;
    response.errorCode = error;
    response.errorMessage = message;
    return response;
}
}

WinHttpTransport::~WinHttpTransport()
{
    if (session_) ::WinHttpCloseHandle(session_);
}

HttpResponse WinHttpTransport::Send(const HttpRequest& request)
{
    if (!session_) {
        session_ = ::WinHttpOpen(L"Piwik Desktop Client", WINHTTP_ACCESS_TYPE_NO_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session_) return Failure(L"Could not open HTTP session");
    }

    ScopedInternetHandle connection(::WinHttpConnect(session_, request.host.c_str(),
        INTERNET_DEFAULT_PORT, 0));
    if (!connection.value) return Failure(L"Could not open HTTP connection");

    ScopedInternetHandle httpRequest(::WinHttpOpenRequest(connection.value,
        request.method.c_str(), request.path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_ESCAPE_DISABLE_QUERY |
        WINHTTP_FLAG_REFRESH | (request.secure ? WINHTTP_FLAG_SECURE : 0)));
    if (!httpRequest.value) return Failure(L"Could not create HTTP request");

    // Preserve the legacy policy in this extraction. Configurable certificate
    // validation is a separate change with its own migration and tests.
    DWORD options = SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
        SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_UNKNOWN_CA |
        SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
    ::WinHttpSetOption(httpRequest.value, WINHTTP_OPTION_SECURITY_FLAGS,
        &options, sizeof options);

    const DWORD size = static_cast<DWORD>(request.body.size());
    void* body = request.body.empty() ? WINHTTP_NO_REQUEST_DATA :
        const_cast<char*>(request.body.data());
    if (!::WinHttpSendRequest(httpRequest.value, request.headers.c_str(),
        static_cast<DWORD>(-1), body, size, size, 0))
        return Failure(L"Could not send HTTP request");

    if (!::WinHttpReceiveResponse(httpRequest.value, nullptr))
        return Failure(L"Could not receive HTTP response");

    HttpResponse response;
    DWORD statusCode = 0;
    DWORD statusSize = sizeof statusCode;
    if (!::WinHttpQueryHeaders(httpRequest.value,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
        WINHTTP_NO_HEADER_INDEX))
        return Failure(L"Could not read HTTP status");
    response.statusCode = statusCode;

#ifdef PIWIK_SERVER_IS_IN_DEBUG_MODE
    // Match the previous diagnostic response read; delivery still depends on
    // HTTP status, not body contents. Bulk-body validation comes separately.
    if (statusCode / 100 == 2) {
        DWORD available = 0, read = 0;
        if (::WinHttpQueryDataAvailable(httpRequest.value, &available) && available) {
            response.body.resize(available);
            if (::WinHttpReadData(httpRequest.value, &response.body[0], available, &read))
                response.body.resize(read);
            else
                response.body.clear();
        }
    }
#endif
    return response;
}

}} // namespace piwik::detail
