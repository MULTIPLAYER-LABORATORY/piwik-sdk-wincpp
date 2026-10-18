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
	Secure = DryRun = Synchronous = Running = false; 
	SerialNumber = LastAcknowledged = 0;
	Service = Wake = 0;
}

PiwikDispatcher::~PiwikDispatcher ()
{
	ShutdownService ();
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
	ConnectionTimeout = t;
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

	itm.Serial = ++SerialNumber;
	itm.Host   = ApiHost;
	itm.Path   = ApiPath;
	itm.Method = Method;
	itm.Query  = st.Serialize ((Method == PIWIK_METHOD_GET ? PIWIK_FORMAT_URL : PIWIK_FORMAT_JSON));

	Requests.push_back (itm);

	if (! Service)
		LaunchService ();

	if (Synchronous)
		Flush ();

	Logger.Debug (L"Submitting query: ", itm.Query.c_str (), itm.Serial);

	return itm.Serial; 
}

bool PiwikDispatcher::Flush ()
{
	return (Service && ::SetEvent (Wake));
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

// Internals

// This function will launch the dispatching thread and create all its required resources

bool PiwikDispatcher::LaunchService ()
{
	Logger.Info (L"Starting Piwik dispatch service");

	Running = true;
	if ((Wake = ::CreateEvent (0, FALSE, FALSE, 0)))
		if ((Service = (HANDLE) _beginthreadex (0, 0, ServiceRoutine, this, 0, 0)))
			return true;

	Logger.Error (L"Could not launch Piwik dispatch service");
	return false;
}

// This function will stop the dispatching thread and delete all associated resources

void PiwikDispatcher::ShutdownService ()
{
	//the following code causing a crash TBD
	//Logger.Info (L"Terminating Piwik dispatch service");

	Running = false;
	Flush ();
	Sleep (250); // seems to be required if service has been recently launched
	if (Service && ::WaitForSingleObject (Service, PIWIK_SHUTDOWN_WAIT * 1000) != WAIT_OBJECT_0)
		::TerminateThread (Service, -1);
	if (Service)
		CloseHandle (Service), Service = 0;
	if (Wake)
		CloseHandle (Wake), Wake = 0;
}

// Main dispatching routine run in the service thread

unsigned __stdcall PiwikDispatcher::ServiceRoutine (void* arg)
{
	PiwikDispatcher* dsp = (PiwikDispatcher*) arg;
	Request itm;
	string msg;
	int grp[PIWIK_POST_BUNDLE];
	int cnt, avl;
	bool vld;

	while (dsp && dsp->Running)
	{
		::WaitForSingleObject (dsp->Wake, (dsp->DispatchInterval > 0 ? dsp->DispatchInterval * 1000 - 500 : INFINITE));
		cnt = 0, msg.clear ();
		while (true)
		{
			dsp->Mutex.Activate ();
			if (dsp->Requests.empty ())
			{
				dsp->Mutex.Release ();
				break;
			}
			itm = dsp->Requests.front ();
			dsp->Requests.pop_front ();
			avl = dsp->Requests.size ();
			dsp->Mutex.Release ();

			grp[cnt++] = itm.Serial;

			if (itm.Method == PIWIK_METHOD_GET)
				vld = dsp->SendRequest (itm.Host, itm.Path, PIWIK_METHOD_GET, itm.Query);
			else
			{
                msg += (msg.empty () ? "{" QUOTES "requests" QUOTES ":[\"" : ",\"") + itm.Query + QUOTES;
				if (cnt < PIWIK_POST_BUNDLE && avl)
					continue;
				vld = dsp->SendRequest (itm.Host, itm.Path, PIWIK_METHOD_POST, msg + "]}");
			}

			if (vld)
			{
				PiwikScopedLock lck (dsp->Mutex);
				dsp->LastAcknowledged = itm.Serial;
			}
			else
			{
				dsp->Mutex.Activate ();
				for (int i = 0; i < cnt; ++i)
					dsp->Failures.push_back (grp[i]);
				dsp->Mutex.Release ();
			}

			cnt = 0, msg.clear ();
		}
	}

	_endthreadex (0);
	return 0;
}

bool PiwikDispatcher::SendRequest (wstring& host, wstring& path, PiwikMethod mth, string& qry)
{
    if (DryRun)
    {
        Logger.Log (L"DRYRUN - Not sending request: ", qry.c_str ());
        return true;
    }

    piwik::detail::HttpRequest request;
    request.host = host;
    request.path = path;
    request.secure = Secure;
    request.verifyServerCertificate = VerifyServerCertificate.load();
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
    if (response.errorCode != ERROR_SUCCESS || !response.errorMessage.empty())
    {
        Logger.Error(response.errorMessage.empty() ? L"HTTP transport failed" :
            response.errorMessage.c_str(), 0, response.errorCode);
        return false;
    }
    if (response.statusCode / 100 != 2)
    {
        Logger.Error(L"Unexpected HTTP response", 0, response.statusCode);
        return false;
    }
#ifdef PIWIK_SERVER_IS_IN_DEBUG_MODE
    if (!response.body.empty())
        Logger.Log(L"Response: ", response.body.c_str());
#endif
    Logger.Debug(L"Sent HTTP request: ", qry.c_str());
    return true;
}
