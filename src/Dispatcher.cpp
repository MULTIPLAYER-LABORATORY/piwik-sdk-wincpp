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

void PiwikDispatcher::SetLogger (wostream* s, PiwikLogLevel lvl)
{
	Logger.SetStream (s);
	Logger.SetLevel (lvl);
}

bool PiwikDispatcher::CurrentApiUrl (TSTRING& str)  
{ 
	PiwikScopedLock lck (Mutex);
	
	str = ApiUrl; 

	return ! str.empty (); 
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

	wstring aux;
	int j = ApiUrl.find ('/');
	ApiHost = WIDE_STRING (ApiUrl.substr (0, j), aux);
	ApiPath = WIDE_STRING (ApiUrl.substr (j), aux);

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

// Dispatching

bool PiwikDispatcher::Submit (PiwikState& st)
{
	PiwikScopedLock lck (Mutex);
	string qry;

	Queries.push_back (st.Serialize (Method, qry));

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
	Running = true;
	Session = ::WinHttpOpen (PIWIK_USER_AGENT, WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	Wake = ::CreateEvent (0, FALSE, FALSE, 0);
	Service = (HANDLE) _beginthreadex (0, 0, ServiceRoutine, this, 0, 0);
	return (Session && Wake && Service);
}

void PiwikDispatcher::StopService ()
{
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
	string itm, msg;
	int cnt, avl;

	while (dsp && dsp->Running)
	{
		::WaitForSingleObject (dsp->Wake, (dsp->DispatchInterval >= 0 ? dsp->DispatchInterval * 1000 : INFINITE));
		cnt = 0, msg.clear ();
		while (dsp->Queries.size ())
		{
			dsp->Mutex.Activate ();
			itm = dsp->Queries.front ();
			dsp->Queries.pop_front ();
			cnt++;
			avl = dsp->Queries.size ();
			dsp->Mutex.Release ();

			if (itm[0] == '?')
				dsp->SendRequest (itm, PIWIK_METHOD_GET);
			else
			{
				msg += (msg.empty () ? "{" QUOTES "requests" QUOTES ":[" : "," ) + itm;
				if (cnt >= PIWIK_DISPATCH_BUNDLE || ! avl)
					if (dsp->SendRequest (msg + "]}", PIWIK_METHOD_POST))
						cnt = 0, msg.clear ();
			}
		}
	}

	_endthreadex (0);
	return 0;
}

bool PiwikDispatcher::SendRequest (string& msg, PiwikMethod mth)
{
	HINTERNET Connection = 0, Request = 0;
	wstring path;
	wchar_t* verb;
	wstring aux;
	void* data;
	int size;
	BOOL rsl = 0;

	if (DryRun)
	{
		Logger.Log (L"DRYRUN - Host: " + ApiHost + L" Path: " + ApiPath + L" Query: " + ToWide (msg, aux) + L"\n");
		return true;
	}

	if (mth == PIWIK_METHOD_GET)
	{
		verb = L"GET";
		path = ApiPath + ToWide (msg, aux);
		data = 0, size = 0;
	}
	else
	{
		verb = L"POST";
		path = ApiPath;
		data = (void*) msg.data (), size = msg.size ();
	}

	Connection = ::WinHttpConnect (Session, ApiHost.c_str (), INTERNET_DEFAULT_PORT, 0); 
	if (! Connection)
	{
		wostringstream os; os << L"Could not open HTTP connection to host " << ApiHost << L" (code: " << GetLastError () << L")";
		Logger.Error (os.str ());
		return false;
	}

	Request = ::WinHttpOpenRequest (Connection, verb, path.c_str (), 0, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 
									WINHTTP_FLAG_ESCAPE_DISABLE_QUERY | WINHTTP_FLAG_REFRESH | (Secure ? WINHTTP_FLAG_SECURE : 0));
	if (! Request)
	{
		wostringstream os; os << L"Could not create HTTP request (code: " << GetLastError () << L")";
		Logger.Error (os.str ());
		::WinHttpCloseHandle (Connection);
		return false;
	}

	rsl = ::WinHttpSendRequest (Request, L"Content-Type:application/json;charset=UTF-8\r\n", -1, data, size, size, 0);
	if (rsl)
	{
		rsl = ::WinHttpReceiveResponse (Request, 0);
		if (rsl)
		{
			DWORD size = 0, wrt = 0;
			if (::WinHttpQueryDataAvailable (Request, &size) && size > 0)
			{
				LPSTR rsp = (LPSTR ) malloc (size + 2); memset (rsp, 0, size + 2);
				rsl = ::WinHttpReadData (Request, (void*) rsp, size, &wrt);
				string s1; wstring s2; s1 = rsp; Logger.Log (ToWide (s1, s2));
				free (rsp);
			}
		}
	}

	::WinHttpCloseHandle (Request);
	::WinHttpCloseHandle (Connection);
	
	return (rsl == TRUE);
}
