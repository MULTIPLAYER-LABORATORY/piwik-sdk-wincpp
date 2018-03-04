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

// Configuration

PiwikDispatcher::PiwikDispatcher ()
{
	Method = PIWIK_BASIC_METHOD; 
	ConnectionTimeout = PIWIK_CONNECTION_TIMEOUT; 
	DispatchInterval = PIWIK_DISPATCH_INTERVAL; 
	Secure = DryRun = Synchronous = Running = false; 
	SerialNumber = LastAcknowledged = 0;
	Service = Wake = 0;
	Session = 0;
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
	if ((Session = ::WinHttpOpen (L"Piwik Desktop Client", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)))
		if ((Wake = ::CreateEvent (0, FALSE, FALSE, 0)))
			if ((Service = (HANDLE) _beginthreadex (0, 0, ServiceRoutine, this, 0, 0)))
				return true;

	Logger.Error (L"Could not launch Piwik dispatch service");
	return false;
}

// This function will stop the dispatching thread and delete all associated resources

void PiwikDispatcher::ShutdownService ()
{
	Logger.Info (L"Terminating Piwik dispatch service");

	Running = false;
	Flush ();
	Sleep (250); // seems to be required if service has been recently launched
	if (Service && ::WaitForSingleObject (Service, PIWIK_SHUTDOWN_WAIT * 1000) != WAIT_OBJECT_0)
		::TerminateThread (Service, -1);
	if (Service)
		CloseHandle (Service), Service = 0;
	if (Wake)
		CloseHandle (Wake), Wake = 0;
	if (Session)
		WinHttpCloseHandle (Session), Session = 0;
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
		while (dsp->Requests.size ())
		{
			dsp->Mutex.Activate ();
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
				dsp->LastAcknowledged = itm.Serial;
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
	HINTERNET Connection = 0, Request = 0;
	wchar_t* verb;
	void* data;
	DWORD size;
	DWORD code = 0;
	DWORD opts;
	int rsl;
	bool vld;

	if (DryRun)
	{
		Logger.Log (L"DRYRUN - Not sending request: ", qry.c_str ());
		return true;
	}

	if (mth == PIWIK_METHOD_GET)
	{
		verb = L"GET";
		path += ToWide (qry);
		data = 0, size = 0;
	}
	else
	{
		verb = L"POST";
		data = (void*) qry.data (), size = qry.size ();
	}

	Connection = ::WinHttpConnect (Session, host.c_str (), INTERNET_DEFAULT_PORT, 0); 
	if (! Connection)
	{
		Logger.Error (L"Could not open HTTP connection", 0, GetLastError ());
		return false;
	}

	Request = ::WinHttpOpenRequest (Connection, verb, path.c_str (), 0, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 
									WINHTTP_FLAG_ESCAPE_DISABLE_QUERY | WINHTTP_FLAG_REFRESH | (Secure ? WINHTTP_FLAG_SECURE : 0));
	if (! Request)
	{
		Logger.Error (L"Could not create HTTP request", 0, GetLastError ());
		::WinHttpCloseHandle (Connection);
		return false;
	}
	
	opts = SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
	::WinHttpSetOption (Request, WINHTTP_OPTION_SECURITY_FLAGS, &opts, sizeof opts);

	rsl = ::WinHttpSendRequest (Request, L"Content-Type:application/json;charset=UTF-8\r\n", -1, data, size, size, 0);
	if (! rsl)
	{
		Logger.Error (L"Could not send HTTP request", 0, GetLastError ());
		::WinHttpCloseHandle (Request);
		::WinHttpCloseHandle (Connection);
		return false;
	}

	rsl = ::WinHttpReceiveResponse (Request, 0) && 
		  ::WinHttpQueryHeaders (Request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, 
				                          WINHTTP_HEADER_NAME_BY_INDEX, &code, &(size = sizeof code), WINHTTP_NO_HEADER_INDEX);
	if (rsl && code / 100 == 2)
	{
		#ifdef PIWIK_SERVER_IS_IN_DEBUG_MODE
			ReadResponse (Request);
		#endif
		Logger.Debug (L"Sent HTTP request: ", qry.c_str ());
		vld = true;
	}
	else
	{
		Logger.Error (L"Unexpected HTTP response", 0, code);
		vld = false;
	}

	::WinHttpCloseHandle (Request);
	::WinHttpCloseHandle (Connection);
	
	return vld;
}

void PiwikDispatcher::ReadResponse (HINTERNET rqst)
{
	DWORD size = 0, wrt = 0;
	if (::WinHttpQueryDataAvailable (rqst, &size) && size > 0)
	{
		LPSTR rsp = (LPSTR) malloc (size + 2); memset (rsp, 0, size + 2);
		::WinHttpReadData (rqst, (void*) rsp, size, &wrt);
		Logger.Log (L"Response: ", rsp);
		free (rsp);
	}
}

