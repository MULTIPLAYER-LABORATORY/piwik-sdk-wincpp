///////////////////////////////////////////////////////////////////////////////////////////////////
//
// File:         Client.h
// Description:  Definition of the PiwikClient class providing main tracking facilities
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
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string>
#include <ostream>

using namespace std;

class PiwikClient
{
private:
	TSTRING Application;
	TSTRING Location;
	time_t SessionStart;
	int SessionTimeout;
	bool Persistent;
	bool Disabled;

	PiwikBasicState State;
	PiwikDispatcher Dispatcher;
	PiwikLock Mutex;
	PiwikLogger Logger;
	
public:
	PiwikClient (LPCTSTR url, int id = 0);
	
	int  CurrentSiteId ();
	void SetSiteId (int id);
	TSTRING CurrentUserId ();
	void SetUserId (LPCTSTR p);
	TSTRING CurrentApiUrl ();
	bool SetApiUrl (LPCTSTR p);
	int  CurrentRequestMethod ();
	void SetRequestMethod (PiwikMethod m);
	bool UsesSecureConnection ();
	void SetSecureConnection (bool f);
	TSTRING CurrentUserAgent ();
	void SetUserAgent (LPCTSTR p);
	TSTRING CurrentLanguage ();
	void SetLanguage (LPCTSTR p);
	void SetUserVariable (LPCTSTR nam, LPCTSTR val, int ind = -1);
	TSTRING CurrentApplication ();
	void SetApplication (LPCTSTR p);
	TSTRING CurrentLocation ();
	void SetLocation (LPCTSTR p);
	bool SetSessionTimeout (int t);
	void SetConnectionTimeout (int t);
	void SetDispatchInterval (int t);
	void StartNewSession ();
	bool IsPersistent ();
	void SetPersistent (bool v);
	bool IsDisabled ();
	void SetDisabled (bool v);
	bool IsDryRun ();
	void SetDryRun (bool v);
	void SetLogger (wostream* s, PiwikLogLevel lvl = PIWIK_INITIAL_LOG_LEVEL);
    void SetVisitDimensions (int nDimensionNum, ...);

	int  TrackEvent (LPCTSTR path, LPCTSTR ctg = 0, LPCTSTR act = 0, LPCTSTR nam = 0, float val = 0);
	int  TrackScreen (LPCTSTR path, LPCTSTR act = 0, int amountOfTime = 0, LPCTSTR nam1 = 0, LPCTSTR val1 = 0, LPCTSTR nam2 = 0, LPCTSTR val2 = 0, 
		              LPCTSTR nam3 = 0, LPCTSTR val3 = 0, LPCTSTR nam4 = 0, LPCTSTR val4 = 0, LPCTSTR nam5 = 0, LPCTSTR val5 = 0,
                      LPCTSTR nam6 = 0, LPCTSTR val6 = 0, LPCTSTR nam7 = 0, LPCTSTR val7 = 0, LPCTSTR nam8 = 0, LPCTSTR val8 = 0);

    int TrackAction( LPCTSTR path, LPCTSTR act, int amountOfTime, int nDimensionNum, ... );

	int  TrackGoal (LPCTSTR path, int goal, float rev = 0);
	int  TrackOutLink (LPCTSTR path);
	int  TrackImpression (LPCTSTR path, LPCTSTR content, LPCTSTR piece, LPCTSTR target);
	int  TrackInteraction (LPCTSTR path, LPCTSTR content, LPCTSTR piece, LPCTSTR target, LPCTSTR action);

	int  Track (PiwikState& st);
	bool Flush ();
	int  RequestStatus (int rqst, int wait = 0);
};
