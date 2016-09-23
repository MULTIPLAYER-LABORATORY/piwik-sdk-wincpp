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

#include <windows.h>
#include <tchar.h>
#include <stdlib.h>
#include <time.h>
#include <string>
#include <sstream>
#include <ostream>

using namespace std;

// Macros

#define ARRAY_COUNT(a)  ((int) (sizeof(a) / sizeof(a[0])))
#define QUOTES "\""
#define AT_MOST(n, m)   ((n) < (m) ? (n) : (m));

#ifdef UNICODE
#define UTF8_STRING(s)  ToUTF8(s)
#else
#define UTF8_STRING(s)  s
#endif

#ifdef UNICODE
#define WIDE_STRING(s)  s
#else
#define WIDE_STRING(s)  ToWide(s)
#endif

// Types

typedef std::basic_string<TCHAR> TSTRING;

enum PiwikMethod
{
	PIWIK_METHOD_GET,
	PIWIK_METHOD_POST
};

enum PiwikQueryFormat
{
	PIWIK_FORMAT_URL,
	PIWIK_FORMAT_JSON
};

enum PiwikLogLevel
{
	PIWIK_LOG_DEBUG,
	PIWIK_LOG_INFO,
	PIWIK_LOG_ERROR
};

// Objects

struct PiwikVariable
{
	TSTRING Name;
	TSTRING Value;

	void Set (LPCTSTR nam, LPCTSTR val)   { Name = nam; if (Name.length () > PIWIK_VARIABLE_LENGTH) Name = Name.substr (0, PIWIK_VARIABLE_LENGTH);
	                                        Value = val; if (Value.length () > PIWIK_VARIABLE_LENGTH) Value = Value.substr (0, PIWIK_VARIABLE_LENGTH); }
	bool IsValid ()                       { return (! Name.empty () && ! Value.empty ()); }
};

struct PiwikVariableSet
{
	PiwikVariable Items[PIWIK_CUSTOM_VARIABLES];

	int  GetIndex (LPCTSTR nam);
	bool IsValid ();
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
	PiwikLogLevel Level;

public:
	PiwikLogger ()                      { Stream = 0; Level = PIWIK_INITIAL_LOG_LEVEL; }
	void SetStream (wostream* s)        { Stream = s; }
	void SetLevel (PiwikLogLevel lvl)   { Level = lvl; }
	void WritePrefix ()                 { Stream->write (PIWIK_LOG_PREFIX, wcslen (PIWIK_LOG_PREFIX)); }
	void WriteSuffix ()                 { Stream->write (L"\n", 1); }
	void Log (wstring& msg)             { if (Stream) WritePrefix (), Stream->write (msg.data (), msg.length ()), WriteSuffix (); }
	void Debug (wstring& msg)           { if (PIWIK_LOG_DEBUG >= Level) Log (msg); }
	void Info (wstring& msg)            { if (PIWIK_LOG_INFO >= Level) Log (msg); }
	void Error (wstring& msg)           { if (PIWIK_LOG_ERROR >= Level) Log (msg); }
	bool IsValid ()                     { return (Stream != 0); }
};

// Helpers

string   ToUTF8 (wstring& src);
wstring  ToWide (string& src);
string   PercentEncode (string& src);
TSTRING  MakeHexDigest (TSTRING& src, int lng);
TSTRING  GetScreenResolution ();
bool     ComposeUrl (TSTRING& prf, TSTRING& url);
_int64   ReadRegistryValue (LPCTSTR apl, LPCTSTR usr, LPCTSTR name);
bool     WriteRegistryValue (LPCTSTR apl, LPCTSTR usr, LPCTSTR name, _int64 val);

