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
	Secure = DryRun = Running = false; 
	Service = Wake = 0;
	Session = 0;
}

PiwikDispatcher::~PiwikDispatcher ()
{
	if (Service)
		StopService ();
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
}

bool PiwikDispatcher::IsDryRun ()                     
{ 
	return DryRun; 
}

void PiwikDispatcher::SetDryRun (bool v)              
{ 
	DryRun = v; 
}

void PiwikDispatcher::SetLogger (wostream* s, PiwikLogLevel lvl)
{
	Logger.SetStream (s);
	Logger.SetLevel (lvl);
}

// Dispatching

bool PiwikDispatcher::Submit (PiwikState& st)
{
	PiwikScopedLock lck (Mutex);
	Request itm;

	itm.Host   = ApiHost;
	itm.Path   = ApiPath;
	itm.Method = Method;
	itm.Query  = st.Serialize ((Method == PIWIK_METHOD_GET ? PIWIK_FORMAT_URL : PIWIK_FORMAT_JSON));

	Requests.push_back (itm);

	if (! Service)
		LaunchService ();

	return Running; 
}

bool PiwikDispatcher::Flush ()
{
	::SetEvent (Wake);
	return true;
}

// Internals

bool PiwikDispatcher::LaunchService ()
{
	Logger.Info (L"Starting Piwik service to host " + ApiHost);
	Running = true;
	Session = ::WinHttpOpen (L"Piwik Desktop Client", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	Wake = ::CreateEvent (0, FALSE, FALSE, 0);
	Service = (HANDLE) _beginthreadex (0, 0, ServiceRoutine, this, 0, 0);
	return (Session && Wake && Service);
}

void PiwikDispatcher::StopService ()
{
	Logger.Info (L"Terminating Piwik service to host " + ApiHost);
	Running = false;
	::SetEvent (Wake);
	if (::WaitForSingleObject (Service, 5000) != WAIT_OBJECT_0)
		::TerminateThread (Service, -1);
	CloseHandle (Service); Service = 0;
	CloseHandle (Wake); Wake = 0;
	WinHttpCloseHandle (Session); Session = 0;
}

unsigned __stdcall PiwikDispatcher::ServiceRoutine (void* arg)
{
	PiwikDispatcher* dsp = (PiwikDispatcher*) arg;
	Request itm;
	string msg;
	int cnt, avl;

	while (dsp && dsp->Running)
	{
		::WaitForSingleObject (dsp->Wake, (dsp->DispatchInterval >= 0 ? dsp->DispatchInterval * 1000 : INFINITE));
		cnt = 0, msg.clear ();
		while (dsp->Requests.size ())
		{
			dsp->Mutex.Activate ();
			itm = dsp->Requests.front ();
			dsp->Requests.pop_front ();
			cnt++;
			avl = dsp->Requests.size ();
			dsp->Mutex.Release ();

			if (itm.Method == PIWIK_METHOD_GET)
				dsp->SendRequest (itm.Host, itm.Path, PIWIK_METHOD_GET, itm.Query);
			else
			{
				msg += (msg.empty () ? "{" QUOTES "requests" QUOTES ":[" : "," ) + itm.Query;
				if (cnt >= PIWIK_POST_BUNDLE || ! avl)
					if (dsp->SendRequest (itm.Host, itm.Path, PIWIK_METHOD_POST, msg + "]}"))
						cnt = 0, msg.clear ();
			}
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
	int size;
	BOOL rsl = 0;

	if (DryRun)
	{
		Logger.Log (L"DRYRUN - Host: " + host + L" Path: " + path + L" Query: " + ToWide (qry));
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
		Logger.Error (L"Could not open HTTP connection to host " + host);
		return false;
	}

	Request = ::WinHttpOpenRequest (Connection, verb, path.c_str (), 0, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 
									WINHTTP_FLAG_ESCAPE_DISABLE_QUERY | WINHTTP_FLAG_REFRESH | (Secure ? WINHTTP_FLAG_SECURE : 0));
	if (! Request)
	{
		Logger.Error (L"Could not create HTTP request to path " + path);
		::WinHttpCloseHandle (Connection);
		return false;
	}

	rsl = ::WinHttpSendRequest (Request, L"Content-Type:application/json;charset=UTF-8\r\n", -1, data, size, size, 0);
	if (! rsl)
	{
		Logger.Error (L"Could not send HTTP request to path " + path);
	}
	else
	{
		rsl = ::WinHttpReceiveResponse (Request, 0);
		#ifdef PIWIK_SERVER_IS_IN_DEBUG_MODE
			ReadResponse (Request);
		#endif
	}

	::WinHttpCloseHandle (Request);
	::WinHttpCloseHandle (Connection);
	
	return (rsl == TRUE);
}

void PiwikDispatcher::ReadResponse (HINTERNET rqst)
{
	DWORD size = 0, wrt = 0;
	if (::WinHttpQueryDataAvailable (rqst, &size) && size > 0)
	{
		LPSTR rsp = (LPSTR) malloc (size + 2); memset (rsp, 0, size + 2);
		::WinHttpReadData (rqst, (void*) rsp, size, &wrt);
		Logger.Log (ToWide (string (rsp)));
		free (rsp);
	}
}

