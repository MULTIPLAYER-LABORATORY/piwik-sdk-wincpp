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

using namespace std;

class PiwikClient
{
private:
	PiwikBasicState State;
	PiwikLogger Logger;
	TSTRING Application;
	TSTRING Location;
	time_t SessionStart;
	int SessionTimeout;
	bool Persistent;
	bool Disabled;

protected:	
	PiwikDispatcher Dispatcher;
	PiwikLock Mutex;
	
private:
	PiwikClient (LPCTSTR url, int id = 0);
	
public:
	void SetLogger (wostream* s);
	int  CurrentSiteId ();
	void SetSiteId (int v);
	bool CurrentUserId (TSTRING& str);
	void SetUserId (LPCTSTR p);
	bool CurrentApiUrl (TSTRING& str);
	void SetApiUrl (LPCTSTR p);
	int  CurrentRequestMethod ();
	void SetRequestMethod (PiwikMethod m);
	bool UsesSecureConnection ();
	void SetSecureConnection (bool f);
	bool CurrentUserAgent (TSTRING& str);
	void SetUserAgent (LPCTSTR p);
	bool CurrentLanguage (TSTRING& str);
	void SetLanguage (LPCTSTR p);
	void SetUserVariable (LPCTSTR nam, LPCTSTR val, int ind = -1);
	bool CurrentApplication (TSTRING& str);
	void SetApplication (LPCTSTR p);
	bool CurrentLocation (TSTRING& str);
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

	bool TrackEvent (LPCTSTR path, LPCTSTR ctg = 0, LPCTSTR act = 0, LPCTSTR nam = 0, float val = 0);
	bool TrackScreen (LPCTSTR path, LPCTSTR act = 0, LPCTSTR nam1 = 0, LPCTSTR val1 = 0, LPCTSTR nam2 = 0, LPCTSTR val2 = 0, 
		              LPCTSTR nam3 = 0, LPCTSTR val3 = 0, LPCTSTR nam4 = 0, LPCTSTR val4 = 0, LPCTSTR nam5 = 0, LPCTSTR val5 = 0);
	bool TrackGoal (LPCTSTR path, int goal, float rev = 0);
	bool TrackOutLink (LPCTSTR path);
	bool TrackImpression (LPCTSTR path, LPCTSTR content, LPCTSTR piece, LPCTSTR target);
	bool TrackInteraction (LPCTSTR path, LPCTSTR content, LPCTSTR piece, LPCTSTR target, LPCTSTR action);
	bool Track (PiwikState& st);
	bool Flush ();
	
	static PiwikClient* NewTracker (LPCTSTR url, int id = 0);
};
