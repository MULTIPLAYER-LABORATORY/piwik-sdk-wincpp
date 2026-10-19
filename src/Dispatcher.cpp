///////////////////////////////////////////////////////////////////////////////////////////////////
//
// File:         Dispatcher.cpp
// Description:  Implementation of the PiwikDispatcher class storing and servicing HTTP requests
// Project:      Piwik-SDK-Win-C++
// Version:      1.0
// Date:         2016-09-19
// Author:       Manfred Klimt - Diogen Software-Entwicklung (bramfeld@diogen.de)
// Copyright:    (c) 2016 mplabsorg
// License:      See provided LICENSE file
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "Config.h"
#include "Utilities.h"
#include "State.h"
#include "Dispatcher.h"
#include "HttpTransport.h"
#include <utility>
#include <mutex>
#include <limits>
#include <system_error>

namespace {
// Adapt the existing recursive Windows mutex for condition_variable_any.
struct CompletionLock
{
    PiwikLock& mutex;
    void lock() { mutex.Activate(); }
    void unlock() { mutex.Release(); }
};
}

// Configuration

PiwikDispatcher::PiwikDispatcher ()
: PiwikDispatcher(std::unique_ptr<piwik::detail::IHttpTransport>(
    new piwik::detail::WinHttpTransport()))
{
}

PiwikDispatcher::PiwikDispatcher (std::unique_ptr<piwik::detail::IHttpTransport> transport)
: Transport(std::move(transport))
{
	if (!Transport)
		Transport.reset(new piwik::detail::WinHttpTransport());
	Method = PIWIK_BASIC_METHOD; 
	ConnectionTimeout = PIWIK_CONNECTION_TIMEOUT; 
	DispatchInterval = PIWIK_DISPATCH_INTERVAL; 
	Secure = DryRun = Synchronous = false;
	SerialNumber = LastAcknowledged = LastCompleted = 0;
	Service = Wake = 0;
}

PiwikDispatcher::~PiwikDispatcher ()
{
	Shutdown(PiwikShutdownMode::Drain);
}

TSTRING PiwikDispatcher::CurrentApiUrl ()  
{ 
	PiwikScopedLock lck (Mutex);
	
	return ApiUrl; 
}

bool PiwikDispatcher::SetApiUrl (LPCTSTR p)      
{ 
	PiwikScopedLock lck (Mutex);
	
	TSTRING url = p; 
	if (url.compare (0, 7, _T("http://")) == 0)
		url = url.substr (7), Secure = false;
	else if (url.compare (0, 8, _T("https://")) == 0)
		url = url.substr (8), Secure = true;
	else if (url.find (_T("://")) != TSTRING::npos)
		return false;

	if (url.find (_T("piwik.php")) != TSTRING::npos || url.find (_T("piwik-proxy.php")) != TSTRING::npos)
		ApiUrl = url;
	else
		ComposeUrl (url, (ApiUrl = _T("piwik.php")));

	int j = ApiUrl.find ('/');
	ApiHost = WIDE_STRING (ApiUrl.substr (0, j));
	ApiPath = WIDE_STRING (ApiUrl.substr (j));

	Logger.Info ((L"Changed API URL to host: " + ApiHost + L" path: " + ApiPath).c_str ());

	return true;
}

int PiwikDispatcher::CurrentRequestMethod ()
{
	return Method;
}

void PiwikDispatcher::SetRequestMethod (PiwikMethod m)
{
	Method = m;
}

bool PiwikDispatcher::UsesSecureConnection ()
{
	return Secure;
}

void PiwikDispatcher::SetSecureConnection (bool f)
{
	Secure = f;
}

bool PiwikDispatcher::VerifiesServerCertificate ()
{
	return VerifyServerCertificate.load ();
}

void PiwikDispatcher::SetVerifyServerCertificate (bool verify)
{
	const bool previous = VerifyServerCertificate.exchange (verify);
	if (previous && !verify)
		Logger.Log (L"WARNING: Server certificate validation disabled for this client (HTTPS requests).");
}

void PiwikDispatcher::SetConnectionTimeout (int t)
{
	if (t > 0 && t <= (std::numeric_limits<int>::max)() / 1000)
		ConnectionTimeout = t;
	else
		Logger.Error(L"HTTP phase timeout must be positive seconds without millisecond overflow");
}

void PiwikDispatcher::SetDispatchInterval (int t)
{
	DispatchInterval = t;
	Synchronous = (t == 0);
	Logger.Info ((Synchronous ? L"Entering synchronous mode" : L"Changed dispatch interval"), 0, t);
}

bool PiwikDispatcher::IsDryRun ()                     
{ 
	return DryRun; 
}

void PiwikDispatcher::SetDryRun (bool v)              
{ 
	DryRun = v; 
	Logger.Info ((v ? L"Activated dry run mode" : L"Activated network mode"));
}

void PiwikDispatcher::SetLogger (wostream* s, PiwikLogLevel lvl)
{
	Logger.SetStream (s);
	Logger.SetLevel (lvl);
}

// Dispatching

int PiwikDispatcher::Submit (PiwikState& st)
{
	if (!st.HasFiniteNumbers())
	{
		Logger.Error (L"Event value and revenue must be finite; request not queued");
		return 0;
	}
	PiwikScopedLock lck (Mutex);
	Request itm;
	if (!Accepting) return 0;

	itm.Serial = SerialNumber + 1;
	itm.Host   = ApiHost;
	itm.Path   = ApiPath;
	itm.Method = Method;
	itm.Query  = st.Serialize ((Method == PIWIK_METHOD_GET ? PIWIK_FORMAT_URL : PIWIK_FORMAT_JSON));

	Requests.push_back (itm);
	SerialNumber = itm.Serial;

	if (! Service)
		LaunchService ();

	if (Synchronous)
		Flush ();

	Logger.Debug (L"Submitting query: ", itm.Query.c_str (), itm.Serial);

	return itm.Serial; 
}

bool PiwikDispatcher::Flush ()
{
	PiwikScopedLock lck (Mutex);
	return (Service && ::SetEvent (Wake));
}

PiwikFlushResult PiwikDispatcher::FlushAndWait (std::chrono::milliseconds timeout)
{
	using Clock = std::chrono::steady_clock;
	const auto start = Clock::now();
	const auto maxWait = std::chrono::duration_cast<std::chrono::milliseconds>((Clock::time_point::max)() - start);
	const auto deadline = timeout >= maxWait ? (Clock::time_point::max)() :
	    start + (timeout.count() > 0 ? timeout : std::chrono::milliseconds::zero());
	CompletionLock adapter{Mutex};
	std::unique_lock<CompletionLock> lock(adapter);
	const int target = SerialNumber;
	if (LastCompleted < target)
	{
		const auto boundary = FlushTargets.insert(target);
		struct RemoveBoundary
		{
			std::multiset<int>& targets;
			std::multiset<int>::iterator position;
			~RemoveBoundary() { targets.erase(position); }
		} removeBoundary{FlushTargets, boundary};
		if ((!Service && !LaunchService()) || !::SetEvent(Wake))
			return PiwikFlushResult::Failed;
		if (!CompletionChanged.wait_until(lock, deadline, [&] { return LastCompleted >= target; }))
			return PiwikFlushResult::TimedOut;
	}
	// Cumulative through the captured request ID; an earlier failure must not be
	// hidden by a later success or by completion just before this call starts.
	for (int failure : Failures)
		if (failure <= target) return PiwikFlushResult::Failed;
	return PiwikFlushResult::Success;
}

int PiwikDispatcher::RequestStatus (int rqst)
{
	PiwikScopedLock lck (Mutex);

	for (size_t i = 0; i < Failures.size (); ++i)
		if (Failures[i] == rqst)
			return -1;

	if (LastAcknowledged >= rqst)
		return 1;

	return 0;
}

PiwikRequestResult PiwikDispatcher::RequestResult (int rqst)
{
	PiwikScopedLock lock(Mutex);
	const auto found = Results.find(rqst);
	if (found != Results.end()) return found->second;
	PiwikRequestResult result;
	if (rqst > 0 && rqst <= SerialNumber) result.state = PiwikDeliveryState::Pending;
	return result;
}

// Internals

// This function will launch the dispatching thread and create all its required resources

bool PiwikDispatcher::LaunchService ()
{
	Logger.Info (L"Starting Piwik dispatch service");

	if ((Wake = ::CreateEvent (0, FALSE, FALSE, 0)))
		if ((Service = (HANDLE) _beginthreadex (0, 0, ServiceRoutine, this, 0, 0)))
			return true;

	Logger.Error (L"Could not launch Piwik dispatch service");
	if (Wake) ::CloseHandle(Wake), Wake = 0;
	return false;
}

// This function will stop the dispatching thread and delete all associated resources

PiwikFlushResult PiwikDispatcher::Shutdown (PiwikShutdownMode mode)
{
	// Serialize join/handle cleanup. Never hold the queue mutex while joining.
	std::lock_guard<std::mutex> shutdownLock(ShutdownMutex);
	HANDLE worker;
	{
		PiwikScopedLock lock(Mutex);
		Accepting = false;
		Stopping = true;
		if (mode == PiwikShutdownMode::DiscardPending)
		{
			Cancellation->store(true);
			for (const auto& request : Requests)
			{
				Failures.push_back(request.Serial);
				auto& result = Results[request.Serial];
				result.state = PiwikDeliveryState::Discarded;
				result.message = L"Queued request discarded during shutdown";
			}
			Requests.clear();
		}
		if (!Service && !Requests.empty() && !LaunchService())
		{
			for (const auto& request : Requests)
			{
				Failures.push_back(request.Serial);
				auto& result = Results[request.Serial];
				result.state = PiwikDeliveryState::Failed;
				result.message = L"Could not launch dispatch worker during shutdown";
			}
			Requests.clear();
		}
		worker = Service;
		if (worker) ::SetEvent(Wake);
		else LastCompleted = SerialNumber;
	}
	CompletionChanged.notify_all();
	if (worker && ::WaitForSingleObject(worker, INFINITE) != WAIT_OBJECT_0)
		throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(), "Could not join dispatch worker");
	{
		PiwikScopedLock lock(Mutex);
		if (Service) ::CloseHandle(Service), Service = 0;
		if (Wake) ::CloseHandle(Wake), Wake = 0;
		return Failures.empty() ? PiwikFlushResult::Success : PiwikFlushResult::Failed;
	}
}

// Main dispatching routine run in the service thread

unsigned __stdcall PiwikDispatcher::ServiceRoutine (void* arg)
{
	PiwikDispatcher* dsp = (PiwikDispatcher*) arg;

	while (dsp)
	{
		::WaitForSingleObject (dsp->Wake, (dsp->DispatchInterval > 0 ? dsp->DispatchInterval * 1000 - 500 : INFINITE));
		while (true)
		{
			std::vector<Request> batch;
			batch.reserve(PIWIK_POST_BUNDLE);
			dsp->Mutex.Activate ();
			if (dsp->Requests.empty ())
			{
				const bool stopped = dsp->Stopping;
				if (stopped) dsp->LastCompleted = dsp->SerialNumber;
				dsp->Mutex.Release ();
				if (stopped)
				{
					dsp->CompletionChanged.notify_all();
					return 0;
				}
				break;
			}
			batch.push_back(dsp->Requests.front());
			dsp->Requests.pop_front();
			const auto& first = batch.front();
			const int boundary = dsp->FlushTargets.empty() ? dsp->SerialNumber : *dsp->FlushTargets.begin();
			// Complete a POST batch before a GET or endpoint transition. Never
			// acknowledge an unsent request as part of a different HTTP request.
			while (first.Method == PIWIK_METHOD_POST && batch.size() < PIWIK_POST_BUNDLE && !dsp->Requests.empty())
			{
				const auto& next = dsp->Requests.front();
				if (next.Serial > boundary || next.Method != first.Method || next.Host != first.Host || next.Path != first.Path) break;
				batch.push_back(next);
				dsp->Requests.pop_front();
			}
			dsp->Mutex.Release ();
			auto& request = batch.front();
			PiwikRequestResult result;
			if (request.Method == PIWIK_METHOD_GET)
				result = dsp->SendRequest(request.Host, request.Path, PIWIK_METHOD_GET, request.Query);
			else
			{
				string msg = "{\"requests\":[";
				for (size_t i = 0; i < batch.size(); ++i)
					msg += (i ? ",\"" : "\"") + batch[i].Query + "\"";
				msg += "]}";
				result = dsp->SendRequest(request.Host, request.Path, PIWIK_METHOD_POST, msg);
			}
			{
				PiwikScopedLock lck (dsp->Mutex);
				for (const auto& item : batch) dsp->Results[item.Serial] = result;
				if (result.state == PiwikDeliveryState::Succeeded) dsp->LastAcknowledged = batch.back().Serial;
				else for (const auto& item : batch) dsp->Failures.push_back(item.Serial);
				dsp->LastCompleted = batch.back().Serial;
			}
			dsp->CompletionChanged.notify_all();
		}
	}

	_endthreadex (0);
	return 0;
}

PiwikRequestResult PiwikDispatcher::SendRequest (wstring& host, wstring& path, PiwikMethod mth, string& qry)
{
    PiwikRequestResult result;
    result.state = PiwikDeliveryState::Failed;
    if (Cancellation->load())
    {
        result.errorCode = ERROR_OPERATION_ABORTED;
        result.message = L"HTTP request cancelled during shutdown";
        return result;
    }
    if (DryRun)
    {
        Logger.Log (L"DRYRUN - Not sending request: ", qry.c_str ());
        result.state = PiwikDeliveryState::Succeeded;
        result.dryRun = true;
        result.message = L"Dry run; request was not sent";
        return result;
    }

    piwik::detail::HttpRequest request;
    request.host = host;
    request.path = path;
    request.secure = Secure;
    request.verifyServerCertificate = VerifyServerCertificate.load();
    request.timeoutMilliseconds = ConnectionTimeout.load() * 1000;
    request.cancellation = Cancellation;
    request.headers = L"Content-Type:application/json;charset=UTF-8\r\n";
    if (mth == PIWIK_METHOD_GET)
    {
        request.method = L"GET";
        request.path += ToWide(qry);
    }
    else
    {
        request.method = L"POST";
        request.body = qry;
    }

    const auto response = Transport->Send(request);
    result.httpStatus = response.statusCode;
    result.errorCode = response.errorCode;
    if (response.errorCode != ERROR_SUCCESS || !response.errorMessage.empty())
    {
        Logger.Error(response.errorMessage.empty() ? L"HTTP transport failed" :
            response.errorMessage.c_str(), 0, response.errorCode);
        result.message = response.errorMessage.empty() ? L"HTTP transport failed" : response.errorMessage;
        return result;
    }
    if (response.statusCode / 100 != 2)
    {
        Logger.Error(L"Unexpected HTTP response", 0, response.statusCode);
        result.message = L"Unexpected HTTP response";
        return result;
    }
#ifdef PIWIK_SERVER_IS_IN_DEBUG_MODE
    if (!response.body.empty())
        Logger.Log(L"Response: ", response.body.c_str());
#endif
    Logger.Debug(L"Sent HTTP request: ", qry.c_str());
    result.state = PiwikDeliveryState::Succeeded;
    return result;
}
