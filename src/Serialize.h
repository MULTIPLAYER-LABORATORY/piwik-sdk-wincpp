///////////////////////////////////////////////////////////////////////////////////////////////////
//
// File:         Serialize.h
// Description:  Generic methods to stringize the values to be sent in a HTTP request
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
#include <string>
#include <sstream>

using namespace std;

class PiwikQueryBuilder : public ostringstream
{
private:
	PiwikQueryFormat Format;
	int Items;

public:
	PiwikQueryBuilder (PiwikQueryFormat frmt)  { Format = frmt; Items = 0; }

	template <typename T> void AddParameter (LPCSTR nam, T val);
	template <> void AddParameter<TSTRING> (LPCSTR nam, TSTRING val);
	template <> void AddParameter<PiwikVariableSet> (LPCSTR nam, PiwikVariableSet val);

	char*  Prefix ()           { if (Format == PIWIK_FORMAT_URL) return (! Items ? "?" : "&"); return (! Items ? "{" QUOTES : "," QUOTES); }
	char*  Assign ()           { if (Format == PIWIK_FORMAT_URL) return "="; return QUOTES ":"; }
	char*  Quotes ()           { if (Format == PIWIK_FORMAT_URL) return ""; return QUOTES; }
	string Encode (string& s)  { if (Format == PIWIK_FORMAT_URL) return PercentEncode (s); return JsonEncode (s); }
	string Result ()           { return this->str () + (Format == PIWIK_FORMAT_URL ? "" : (Items ? "}" : "{}")); }
};

// Externals

template <typename T> void PiwikQueryBuilder::AddParameter (LPCSTR nam, T val)
{
	*this << Prefix () << nam << Assign () << val;
	Items++;
}

template <> void PiwikQueryBuilder::AddParameter<TSTRING> (LPCSTR nam, TSTRING val)
{
	*this << Prefix () << nam << Assign () << Quotes () << Encode (UTF8_STRING (val)) << Quotes ();
	Items++;
}

template <> void PiwikQueryBuilder::AddParameter<PiwikVariableSet> (LPCSTR nam, PiwikVariableSet val)
{
	int n = 0;

	*this << Prefix () << nam << Assign () << "{";
	for (int i = 0; i < ARRAY_COUNT (val.Items); i++)
		if (val.Items[i].IsValid ())
			*this << (n++ > 0 ? "," : "") << QUOTES << (i + 1) << QUOTES << ":[" << QUOTES << Encode (UTF8_STRING (val.Items[i].Name)) <<
			         QUOTES << "," << QUOTES << Encode (UTF8_STRING (val.Items[i].Value)) << QUOTES << "]";
	*this << "}";
	Items++;
}

