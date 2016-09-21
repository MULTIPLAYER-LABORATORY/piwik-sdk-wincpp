///////////////////////////////////////////////////////////////////////////////////////////////////
//
// File:         Client.cpp
// Description:  Implementation of the PiwikClient class providing main tracking facilities
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
#include "Client.h"

// Configuration

PiwikClient::PiwikClient (LPCTSTR url, int id)  
{
	SetApiUrl (url);
	SetSiteId (id);
	GetScreenResolution (State.ScreenRes);
	SetLocation (_T(""));
	SessionStart = 0; 
	SessionTimeout = PIWIK_SESSION_TIMEOUT;
	Persistent = Disabled = false; 
	srand ((int) time (0));
}

int PiwikClient::CurrentSiteId ()                
{ 
	return State.SiteId; 
}

void PiwikClient::SetSiteId (int id)
{ 
	State.SiteId = id; 
}

bool PiwikClient::CurrentUserId (TSTRING& str)
{ 
	PiwikScopedLock lck (Mutex);
	
	str = State.UserId; 

	return ! str.empty (); 
}

void PiwikClient::SetUserId (LPCTSTR p)         
{
	PiwikScopedLock lck (Mutex);
	
	State.UserId = p; 
	MakeHexDigest (State.UserId, State.VisitorId, 16);
}

bool PiwikClient::CurrentApiUrl (TSTRING& str)  
{
	return Dispatcher.CurrentApiUrl (str);
}

bool PiwikClient::SetApiUrl (LPCTSTR p)      
{ 
	return Dispatcher.SetApiUrl (p);
}

int PiwikClient::CurrentRequestMethod ()
{
	return Dispatcher.CurrentRequestMethod ();
}

void PiwikClient::SetRequestMethod (PiwikMethod m)
{
	Dispatcher.SetRequestMethod (m);
}

bool PiwikClient::UsesSecureConnection ()
{
	return Dispatcher.UsesSecureConnection ();
}

void PiwikClient::SetSecureConnection (bool f)
{
	Dispatcher.SetSecureConnection (f);
}

bool PiwikClient::CurrentUserAgent (TSTRING& str)  
{ 
	PiwikScopedLock lck (Mutex);
	
	str = State.UserAgent; 

	return ! str.empty (); 
}

void PiwikClient::SetUserAgent (LPCTSTR p)      
{ 
	PiwikScopedLock lck (Mutex);
	
	State.UserAgent = p;
}
	
bool PiwikClient::CurrentLanguage (TSTRING& str)  
{ 
	PiwikScopedLock lck (Mutex);
	
	str = State.Language; 

	return ! str.empty (); 
}

void PiwikClient::SetLanguage (LPCTSTR p)      
{ 
	PiwikScopedLock lck (Mutex);
	
	State.Language = p; 
}

// Index if omitted will be determined by looking up a variable with the same name in the current set

void PiwikClient::SetUserVariable (LPCTSTR nam, LPCTSTR val, int ind)
{
	if (nam)
	{
		int i = (ind > 0 ? ind - 1 : State.UserVariables.GetIndex (nam));
		if ((UINT) i < PIWIK_CUSTOM_VARIABLES)
			State.UserVariables.Items[i].Set (nam, val);
	}
}

bool PiwikClient::CurrentApplication (TSTRING& str)  
{ 
	PiwikScopedLock lck (Mutex);
	
	str = Application; 

	return ! str.empty (); 
}

void PiwikClient::SetApplication (LPCTSTR p)      
{ 
	PiwikScopedLock lck (Mutex);
	
	Application = p; 
}

bool PiwikClient::CurrentLocation (TSTRING& str)  
{ 
	PiwikScopedLock lck (Mutex);
	
	str = Location; 

	return ! str.empty (); 
}

void PiwikClient::SetLocation (LPCTSTR p)      
{ 
	PiwikScopedLock lck (Mutex);
	
	Location = p; 
}

bool PiwikClient::SetSessionTimeout (int t)
{
	return (t > 0 && (SessionTimeout = t));
}

void PiwikClient::SetConnectionTimeout (int t)
{
	Dispatcher.SetConnectionTimeout (t);
}

void PiwikClient::SetDispatchInterval (int t)
{
	Dispatcher.SetDispatchInterval (t);
}

void PiwikClient::StartNewSession ()              
{ 
	SessionStart = 0; 
}

// Persistent will try to store statistical data for this user in the registry

bool PiwikClient::IsPersistent ()                     
{ 
	return Persistent; 
}

void PiwikClient::SetPersistent (bool v)              
{ 
	Persistent = v; 
}

// Disabling the client will effectively stop tracking by keeping all active configuration

bool PiwikClient::IsDisabled ()                     
{ 
	return Disabled; 
}

void PiwikClient::SetDisabled (bool v)              
{ 
	Disabled = v; 
}

// DryRun will follow all tracking steps but will dunp the request to the logger instead of the network

bool PiwikClient::IsDryRun ()                     
{ 
	return Dispatcher.IsDryRun (); 
}

void PiwikClient::SetDryRun (bool v)              
{ 
	Dispatcher.SetDryRun (v); 
}

void PiwikClient::SetLogger (wostream* s, PiwikLogLevel lvl)
{
	Logger.SetStream (s);
	Logger.SetLevel (lvl);
	Dispatcher.SetLogger (s, lvl);
}

// Tracking

// TrackEvent: path (PARAM_URL_PATH) is the only required parameter.
// Event category, action, name and value are optional.

bool PiwikClient::TrackEvent (LPCTSTR path, LPCTSTR ctg, LPCTSTR act, LPCTSTR nam, float val)
{
	PiwikState st;

	st.TrackedPath = path;
	if (ctg)
		st.EventCategory = ctg;
	if (act)
		st.EventAction = act;
	if (nam)
		st.EventName = nam;
	if (val)
		st.EventValue = val;

	return Track (st);
}

// TrackTrackScreen: path (PARAM_URL_PATH) is the only required parameter.
// Up to 5 screen variables can be passed in the position corresponding to the associated index.

bool PiwikClient::TrackScreen (LPCTSTR path, LPCTSTR act, LPCTSTR nam1, LPCTSTR val1, LPCTSTR nam2, LPCTSTR val2, 
		                         LPCTSTR nam3, LPCTSTR val3, LPCTSTR nam4, LPCTSTR val4, LPCTSTR nam5, LPCTSTR val5)
{
	PiwikState st;

	st.TrackedPath = path;
	if (act)
		st.TrackedAction = act;
	if (nam1 && val1)
		st.ScreenVariables.Items[0].Set (nam1, val1);
	if (nam2 && val2)
		st.ScreenVariables.Items[1].Set (nam2, val2);
	if (nam3 && val3)
		st.ScreenVariables.Items[2].Set (nam3, val3);
	if (nam4 && val4)
		st.ScreenVariables.Items[3].Set (nam4, val4);
	if (nam5 && val5)
		st.ScreenVariables.Items[4].Set (nam5, val5);

	return Track (st);
}

// TrackGoal: path (PARAM_URL_PATH) is the only required parameter.
// Goal ID and revenue are optional.

bool PiwikClient::TrackGoal (LPCTSTR path, int goal, float rev)
{
	PiwikState st;

	st.TrackedPath = path;
	st.Goal = goal;
	st.Revenue = rev;

	return Track (st);
}

// TrackOutLink: path will be both the followed link and the PARAM_URL_PATH of the request

bool PiwikClient::TrackOutLink (LPCTSTR path)
{
	PiwikState st;

	st.TrackedPath = path;
	st.OutLink = path;

	return Track (st);
}

bool PiwikClient::TrackImpression (LPCTSTR path, LPCTSTR content, LPCTSTR piece, LPCTSTR target)
{
	PiwikState st;

	st.TrackedPath = path;
	if (content)
		st.ContentName = content;
	if (piece)
		st.ContentPiece = piece;
	if (target)
		st.ContentTarget = target;

	return Track (st);
}

bool PiwikClient::TrackInteraction (LPCTSTR path, LPCTSTR content, LPCTSTR piece, LPCTSTR target, LPCTSTR action)
{
	PiwikState st;

	st.TrackedPath = path;
	if (content)
		st.ContentName = content;
	if (piece)
		st.ContentPiece = piece;
	if (target)
		st.ContentTarget = target;
	if (action)
		st.ContentInteraction = action;

	return Track (st);
}

// Generic tracking routine

bool PiwikClient::Track (PiwikState& st)
{
	PiwikScopedLock lck (Mutex);

	if (! Disabled && State.SiteId && ! st.TrackedPath.empty ())
	{
		time_t t = time (0);
		if (t - SessionStart > SessionTimeout)
		{
			st.NewSession = 1;
			st.UserAgent = State.UserAgent;
			st.Language = State.Language;
			st.ScreenRes = State.ScreenRes;

			if (Persistent && ! Application.empty () && ! State.UserId.empty ())
			{
				st.VisitCount = (int) ReadRegistryValue (Application.c_str (), State.UserId.c_str (), _T("VisitCount"));
				WriteRegistryValue (Application.c_str (), State.UserId.c_str (), _T("VisitCount"), st.VisitCount + 1);
				st.FirstVisit = ReadRegistryValue (Application.c_str (), State.UserId.c_str (), _T("FirstVisit"));
				if (st.FirstVisit == 0)
					WriteRegistryValue (Application.c_str (), State.UserId.c_str (), _T("FirstVisit"), (st.FirstVisit = t));
				st.LastVisit = ReadRegistryValue (Application.c_str (), State.UserId.c_str (), _T("LastVisit"));
				WriteRegistryValue (Application.c_str (), State.UserId.c_str (), _T("LastVisit"), t);
			}
			
			SessionStart = t;
		}

		if (st.TrackedPath.find (':') == TSTRING::npos)
			ComposeUrl (Location, st.TrackedPath);

		st.SiteId = State.SiteId;
		st.UserId = State.UserId;
		st.VisitorId = State.VisitorId;
		st.ApiVersion = State.ApiVersion;
		st.UserVariables = State.UserVariables;
		st.Recording = State.Recording;
		st.ReturnImage = State.ReturnImage;
		st.Random = rand ();

		return Dispatcher.Submit (st); 
	}

	return false;
}

bool PiwikClient::Flush ()
{
	return (! Disabled && Dispatcher.Flush ());
}

