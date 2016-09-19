///////////////////////////////////////////////////////////////////////////////////////////////////
//
// File:         Utilities.cpp
// Description:  Implementation of generic objects and helper routines
// Project:      Piwik-SDK-Win-C++
// Version:      1.0
// Date:         2016-09-19
// Author:       Manfred Klimt - Diogen Software-Entwicklung (bramfeld@diogen.de)
// Copyright:    (c) 2016 mplabsorg
// License:      See provided LICENSE file
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#define _WIN32_WINNT 0x0501
#include <windows.h>
#include <tchar.h>
#include <wincrypt.h>
#include <stdio.h>
#include <cctype>
#include <string>
#include <sstream>
#include <deque>

#include "Utilities.h"

// PiwikVariableSet

int PiwikVariableSet::GetIndex (LPCTSTR nam)
{
	int i;

	for (i = 0; i < ARRAY_COUNT (Items); i++)
		if (Items[i].Name == nam)
			break;
	if (i >= ARRAY_COUNT (Items))
		for (i = 0; i < ARRAY_COUNT (Items); i++)
			if (! Items[i].IsValid ())
				break;
	if (i >= ARRAY_COUNT (Items))
		i = ARRAY_COUNT (Items) - 1;

	return i;
}

bool PiwikVariableSet::IsValid ()   
{ 
	for (int i = 0; i < ARRAY_COUNT (Items); i++) 
		if (Items[i].IsValid ()) 
			return true; 
	
	return false; 
}

// PiwikQueryBuilder

void PiwikQueryBuilder::AddParameter (LPCSTR nam, int val)
{
	*this << prefix () << nam << "=" << val;
	items++;
}

void PiwikQueryBuilder::AddParameter (LPCSTR nam, float val)
{
	*this << prefix () << nam << "=" << val;
	items++;
}

void PiwikQueryBuilder::AddParameter (LPCSTR nam, time_t val)
{
	*this << prefix () << nam << "=" << val;
	items++;
}

void PiwikQueryBuilder::AddParameter (LPCSTR nam, TSTRING& val)
{
	string s1, s2;
	
	*this << prefix () << nam << "=" << PercentEncode (UTF8_STRING (val, s1), s2);
	items++;
}

void PiwikQueryBuilder::AddParameter (LPCSTR nam, PiwikVariableSet& val)
{
	string s1, s2;
	int n = 0;

	*this << prefix () << nam << "={";
	for (int i = 0; i < ARRAY_COUNT (val.Items); i++)
		if (val.Items[i].IsValid ())
			*this << (n++ > 0 ? "," : "") << QUOTES << (i + 1) << QUOTES << ":[" << QUOTES << PercentEncode (UTF8_STRING (val.Items[i].Name, s1), s2) << 
			         QUOTES << "," << QUOTES << PercentEncode (UTF8_STRING (val.Items[i].Value, s1), s2) << QUOTES << "]";
	*this << "}";
	items++;
}

// Helpers

string& ToUTF8 (wstring& src, string& trg)
{
	char* bfr;
	int cnt;
	
	trg.clear ();

	// returned count doesn't include a null terminator if src.length is explicitly specified
	cnt = ::WideCharToMultiByte (CP_UTF8, 0, src.data (), src.length (), 0, 0, 0, 0);
	if (cnt > 0)
	{
		bfr = (char*) malloc (cnt);
		if (bfr)
		{
			cnt = ::WideCharToMultiByte (CP_UTF8, 0, src.data (), src.length (), bfr, cnt, 0, 0);
			if (cnt > 0)
				trg.assign (bfr, cnt);
			free (bfr);
		}
	}

	return trg;
}

wstring& ToUWide (string& src, wstring& trg)
{
	wchar_t* bfr;
	int cnt;
	
	trg.clear ();

	bfr = (wchar_t*) malloc (src.length () * sizeof (wchar_t));
	if (bfr)
	{
		// returned count doesn't include a null terminator if src.length is explicitly specified
		cnt = ::MultiByteToWideChar (CP_ACP, 0, src.data (), src.length (), bfr, src.length ());
		if (cnt > 0)
			trg.assign (bfr, cnt);
		free (bfr);
	}
	
	return trg;
}

string& PercentEncode (string& src, string& trg)
{
    ostringstream os;
	char hexdigits[] = "0123456789ABCDEF";

    for (string::const_iterator i = src.begin (), n = src.end (); i != n; ++i) 
	{
        unsigned char c = (*i);

        // Keep alphanumeric and other accepted characters intact
        if (isalnum (c) || c == '-' || c == '_' || c == '.' || c == '~') 
            os << c;
		// Any other characters are percent-encoded
		else
			os << '%' << hexdigits[c >> 4] << hexdigits[c & 0x0F];
    }

    trg = os.str ();

	return trg;
}

bool MakeHexDigest (TSTRING& src, TSTRING& dgst, int ext)
{
	#define MD5LEN  16
	DWORD stt = 0;
	HCRYPTPROV hProv = 0;
	HCRYPTHASH hHash = 0;
	BYTE md5[MD5LEN];
	DWORD cnt = MD5LEN;
	TCHAR result[MD5LEN * 2];
	CHAR hexdigits[] = "0123456789ABCDEF";
	string aux;
	
	string& enc = UTF8_STRING (src, aux);

	// Need some data to digest
	if (enc.length () < 4)
		return false;
	
	// Get handle to the crypto provider
	if (! ::CryptAcquireContext (&hProv, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
	{
		stt = ::GetLastError();
		return false;
	}

	// Create build object
	if (! ::CryptCreateHash (hProv, CALG_MD5, 0, 0, &hHash))
	{
		stt = ::GetLastError();
		::CryptReleaseContext (hProv, 0);
		return false;
	}

	// Insert source data
	if (! ::CryptHashData (hHash, (BYTE*) enc.data (), enc.length (), 0))
	{
		stt = ::GetLastError();
		::CryptDestroyHash (hHash);
		::CryptReleaseContext (hProv, 0);
		return false;
	}

	// Extract digest
	if (! ::CryptGetHashParam (hHash, HP_HASHVAL, md5, &cnt, 0))
	{
		stt = ::GetLastError();
		::CryptDestroyHash (hHash);
		::CryptReleaseContext (hProv, 0);
		return false;
	}

	for (DWORD i = 0; i < cnt; i++)
	{
		result[i * 2] = hexdigits[md5[i] >> 4];
		result[i * 2 + 1] = hexdigits[md5[i] & 0x0F];
	}

	::CryptDestroyHash (hHash);
	::CryptReleaseContext (hProv, 0);

	int n = AT_MOST (ext, (int) (cnt * 2));
	dgst.assign (result, n);
	
	return true; 
}   

bool GetScreenResolution (TSTRING& str)
{
	TCHAR bfr[64];
	
	int x = ::GetSystemMetrics (SM_CXSCREEN); 
	int y = ::GetSystemMetrics (SM_CYSCREEN); 
	if (x > 0 && y > 0)
	{
		_stprintf_s (bfr, _T("%dx%d"), x, y);
		str = bfr;
		return true;
	}
	
	return false;
}

bool IsAbsoluteUrl (TSTRING& url)
{
	return (url.compare (0, 7, _T("http://")) == 0 || url.compare (0, 8, _T("https://")) == 0 || url.compare (0, 6, _T("ftp://")) == 0);
}

void MakeAbsoluteUrl (TSTRING& url)
{
	if (! IsAbsoluteUrl (url))
		url = _T("http://") + url;
}

bool ComposeUrl (TSTRING& prf, TSTRING& url)
{
	int n = prf.length ();
	if (n > 0 && prf[n - 1] != '/' && ! url.empty () && url[0] != '/')
		url = _T("/") + url;
	url = prf + url;
	return ! url.empty ();
}


_int64 ReadRegistryValue (LPCTSTR apl, LPCTSTR usr, LPCTSTR name)
{
	TSTRING key;
	HKEY hKey;
	_int64 val;
	DWORD dwType, dwSize = sizeof(val);
	LONG rsl;

	key = _T("Software\\"); key += apl; key += _T("\\"); key += usr; key += _T("\\"); key += _T("\\Piwik");
	rsl = ::RegOpenKeyEx (HKEY_CURRENT_USER, key.c_str (), 0, KEY_READ, &hKey);
	if (rsl == ERROR_SUCCESS)
	{
		rsl = ::RegQueryValueEx (hKey, name, 0, &dwType, (LPBYTE) &val, &dwSize);
		::RegCloseKey (hKey);
	}

	return (rsl == ERROR_SUCCESS ? val : 0);
}

bool WriteRegistryValue (LPCTSTR apl, LPCTSTR usr, LPCTSTR name, _int64 val)
{
	TSTRING key;
	HKEY hKey;
	LONG rsl;

	key = _T("Software\\"); key += apl; key += _T("\\"); key += usr; key += _T("\\"); key += _T("\\Piwik");
	rsl = ::RegOpenKeyEx (HKEY_CURRENT_USER, key.c_str (), 0, KEY_SET_VALUE, &hKey);
	if (rsl == ERROR_SUCCESS)
	{
		rsl = ::RegSetValueEx (hKey, name, 0, REG_QWORD, (LPBYTE) &val, sizeof (val));
		::RegCloseKey (hKey);
	}

	return (rsl == ERROR_SUCCESS);
}
