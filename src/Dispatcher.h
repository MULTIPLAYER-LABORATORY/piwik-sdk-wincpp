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
#include <ostream>

using namespace std;

class PiwikDispatcher
{
private:
	PiwikLogger Logger;
	TSTRING ApiUrl;
	wstring ApiHost, ApiPath;
	PiwikMethod Method;
	int ConnectionTimeout;
	int DispatchInterval;
	bool Secure;
	bool DryRun;
	bool Running;

	std::deque<string> Queries;
	PiwikLock Mutex;
	HANDLE Service;
	HANDLE Wake;
	HINTERNET Session;

public:
	PiwikDispatcher ();
	~PiwikDispatcher ();

	void SetLogger (wostream* s, PiwikLogLevel lvl);
	bool CurrentApiUrl (TSTRING& str);
	void SetApiUrl (LPCTSTR p);
	int  CurrentRequestMethod ();
	void SetRequestMethod (PiwikMethod m);
	bool UsesSecureConnection ();
	void SetSecureConnection (bool f);
	void SetConnectionTimeout (int t);
	void SetDispatchInterval (int t);
	bool IsDryRun ();
	void SetDryRun (bool v);

	bool Submit (string& qry);
	bool Flush ();

private:
	bool LaunchService ();
	void StopService ();
	static unsigned __stdcall ServiceRoutine (void*);
	bool SendRequest (PiwikMethod mth, string& msg);
};

