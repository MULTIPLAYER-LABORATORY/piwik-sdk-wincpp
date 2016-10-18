///////////////////////////////////////////////////////////////////////////////////////////////////
//
// File:         Dispatcher.h
// Description:  Definition of the PiwikDispatcher class storing and servicing HTTP requests
// Project:      Piwik-SDK-Win-C++
// Version:      1.0
// Date:         2016-09-19
// Author:       Manfred Klimt - Diogen Software-Entwicklung (bramfeld@diogen.de)
// Copyright:    (c) 2016 mplabsorg
// License:      See provided LICENSE file
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <windows.h>
#include <winhttp.h>
#include <tchar.h>
#include <stdlib.h>
#include <time.h>
#include <process.h>
#include <string>
#include <deque>
#include <vector>
#include <ostream>

using namespace std;

class PiwikDispatcher
{
private:
	struct Request
	{
		int Serial;
		wstring Host;
		wstring Path;
		PiwikMethod Method;
		string Query;
	};

	TSTRING ApiUrl;
	wstring ApiHost, ApiPath;
	PiwikMethod Method;
	int ConnectionTimeout;
	int DispatchInterval;
	bool Secure;
	bool DryRun;
	bool Synchronous;
	bool Running;

	std::deque<Request> Requests;
	std::vector<int> Failures;
	int SerialNumber;
	int LastAcknowledged;
	PiwikLock Mutex;
	PiwikLogger Logger;
	HANDLE Service;
	HANDLE Wake;
	HINTERNET Session;

public:
	PiwikDispatcher ();
	~PiwikDispatcher ();

	TSTRING CurrentApiUrl ();
	bool SetApiUrl (LPCTSTR p);
	int  CurrentRequestMethod ();
	void SetRequestMethod (PiwikMethod m);
	bool UsesSecureConnection ();
	void SetSecureConnection (bool f);
	void SetConnectionTimeout (int t);
	void SetDispatchInterval (int t);
	bool IsDryRun ();
	void SetDryRun (bool v);
	void SetLogger (wostream* s, PiwikLogLevel lvl);

	int  Submit (PiwikState& st);
	bool Flush ();
	int  RequestStatus (int rqst);

private:
	bool LaunchService ();
	void ShutdownService ();
	static unsigned __stdcall ServiceRoutine (void*);
	bool SendRequest (wstring& host, wstring& path, PiwikMethod mth, string& qry);
	void ReadResponse (HINTERNET rqst);
};

