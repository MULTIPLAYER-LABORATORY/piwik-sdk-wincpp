///////////////////////////////////////////////////////////////////////////////////////////////////
//
// File:         Utilities.h
// Description:  Definition of generic symbols, types and helper routines
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

// Symbols

#define PIWIK_LOG_PREFIX           _T("PIWIK:")
#define PIWIK_API_VERSION          1
#define PIWIK_BASIC_METHOD         PIWIK_METHOD_POST
#define PIWIK_USER_AGENT           _T("Piwik Client Windows Desktop")
#define PIWIK_RECORDING_VALUE      1
#define PIWIK_SEND_IMAGE           0
#define PIWIK_CUSTOM_VARIABLES     5
#define PIWIK_VARIABLE_LENGTH      200
#define PIWIK_SESSION_TIMEOUT      30   // sec before restarting a client session
#define PIWIK_CONNECTION_TIMEOUT   5    // sec while trying to establish a connection
#define PIWIK_DISPATCH_INTERVAL    120  // sec between API requests
#define PIWIK_DISPATCH_BUNDLE      20   // number of queries sent together in one request

// Macros

#define ARRAY_COUNT(a)  ((int) (sizeof(a) / sizeof(a[0])))
#define QUOTES "\""
#define AT_MOST(n, m)   ((n) < (m) ? (n) : (m));

#ifdef UNICODE
#define UTF8_STRING(s,t) ToUTF8(s,t)
#else
#define UTF8_STRING(s,t) s
#endif

#ifdef UNICODE
#define WIDE_STRING(s,t) s
#else
#define WIDE_STRING(s,t) ToWide(s,t)
#endif

// Types

typedef std::basic_string<TCHAR> TSTRING;

enum PiwikMethod
{
	PIWIK_METHOD_GET,
	PIWIK_METHOD_POST
};

// Objects

struct PiwikVariable
{
	TSTRING Name;
	TSTRING Value;

	void Set (LPCTSTR nam, LPCTSTR val)   { Name.assign (nam, PIWIK_VARIABLE_LENGTH); Value.assign (val, PIWIK_VARIABLE_LENGTH); }
	bool IsValid ()                       { return (! Name.empty () && ! Value.empty ()); }
};

struct PiwikVariableSet
{
	PiwikVariable Items[PIWIK_CUSTOM_VARIABLES];

	int  GetIndex (LPCTSTR nam);
	bool IsValid ();
};

class PiwikQueryBuilder : public ostringstream
{
private:
	int  items;
	char prefix ()  { return (items == 0 ? '?' : '&'); }

public:
	PiwikQueryBuilder ()  { items = 0; }

	void AddParameter (LPCSTR nam, int val);
	void AddParameter (LPCSTR nam, float val);
	void AddParameter (LPCSTR nam, time_t val);
	void AddParameter (LPCSTR nam, TSTRING& val);
	void AddParameter (LPCSTR nam, PiwikVariableSet& val);
};

class PiwikLock
{
private:
	HANDLE Handle;

public:
	PiwikLock ()       { Handle = ::CreateMutex (0, false, 0); }
	~PiwikLock ()      { if (Handle) ::CloseHandle (Handle); }
	
	bool Activate ()   { return (Handle && ::WaitForSingleObject (Handle, INFINITE) == WAIT_OBJECT_0); }
	void Release ()    { if (Handle) ::ReleaseMutex (Handle); }
};

class PiwikScopedLock
{
private:
	PiwikLock* Lock;
	
public:
	PiwikScopedLock (PiwikLock& lck)    { Lock = (lck.Activate () ? &lck : 0); }
	~PiwikScopedLock ()                 { if (Lock) Lock->Release (); }
};

class PiwikLogger
{
private:
	wostream* Stream;

public:
	PiwikLogger ()                { Stream = 0; }
	void SetStream (wostream* s)  { Stream = s; }
	void Log (wstring& str)       { if (Stream) Stream->write (str.data (), str.length ()); }
	bool IsValid ()               { return (Stream != 0); }
};

// Helpers

string&  ToUTF8 (wstring& src, string& trg);
wstring& ToUWide (string& src, wstring& trg);
string&  PercentEncode (string& src, string& trg);
bool     MakeHexDigest (TSTRING& src, TSTRING& dgst, int ext);
bool     GetScreenResolution (TSTRING& str);
bool     IsAbsoluteUrl (TSTRING& url);
void     MakeAbsoluteUrl (TSTRING& url);
bool     ComposeUrl (TSTRING& prf, TSTRING& url);
_int64   ReadRegistryValue (LPCTSTR apl, LPCTSTR usr, LPCTSTR name);
bool     WriteRegistryValue (LPCTSTR apl, LPCTSTR usr, LPCTSTR name, _int64 val);

