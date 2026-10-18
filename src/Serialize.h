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
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <stdexcept>
#include <type_traits>

using namespace std;

class PiwikQueryBuilder : public ostringstream
{
private:
	PiwikQueryFormat Format;
	int Items;

public:
	PiwikQueryBuilder (PiwikQueryFormat frmt)  { Format = frmt; Items = 0; imbue(std::locale::classic()); }

	template <typename T> void AddParameter (LPCSTR nam, T val);
	template <> void AddParameter<TSTRING> (LPCSTR nam, TSTRING val);
	template <> void AddParameter<PiwikVariableSet> (LPCSTR nam, PiwikVariableSet val);
    void AddDimension(PiwikDimensionsSet val);
    void AddDimension(PiwikVisitDimensionsSet val);

    char*  Prefix ()           {  return (! Items ? "?" : "&"); }
	char*  Assign ()           { return "="; }
	char*  Quotes ()           { if (Format == PIWIK_FORMAT_URL) return QUOTES; return "\\" QUOTES; }
	string Encode (string& s)  { if (Format == PIWIK_FORMAT_URL) return PercentEncode (s); return JsonEncode (s); }
    string Result ()           { return this->str () ; }
};

// Externals

template <typename T> void PiwikQueryBuilder::AddParameter (LPCSTR nam, T val)
{
	if (std::is_floating_point<T>::value)
	{
		if (!std::isfinite(static_cast<long double>(val)))
			throw std::invalid_argument("Tracking numbers must be finite");
		std::ostringstream number;
		number.imbue(std::locale::classic());
		number << std::setprecision(std::numeric_limits<T>::max_digits10) << val;
		std::string encoded = number.str();
		// Both formats contain a query string. Escape an exponent's '+' so
		// Matomo's query parser does not turn it into a space.
		*this << Prefix () << nam << Assign () << PercentEncode(encoded);
	}
	else
		*this << Prefix () << nam << Assign () << val;
	Items++;
}

template <> void PiwikQueryBuilder::AddParameter<TSTRING> (LPCSTR nam, TSTRING val)
{
    *this << Prefix () << nam << Assign () << Encode (UTF8_STRING (val));
	Items++;
}

template <> void PiwikQueryBuilder::AddParameter<PiwikVariableSet> (LPCSTR nam, PiwikVariableSet val)
{
	int n = 0;

	*this << Prefix () << nam << Assign () << "{";
	for (int i = 0; i < ARRAY_COUNT (val.Items); i++)
		if (val.Items[i].IsValid ())
			*this << (n++ > 0 ? "," : "") << Quotes () << (i + 1) << Quotes () << ":[" << Quotes () << Encode (UTF8_STRING (val.Items[i].Name)) <<
			            Quotes () << "," << Quotes () << Encode (UTF8_STRING (val.Items[i].Value)) << Quotes () << "]";
	*this << "}";
	Items++;
}

void PiwikQueryBuilder::AddDimension(PiwikDimensionsSet val)
{
    for (int i = 0; i < ARRAY_COUNT (val.Items); i++)
    {
        if (val.Items[i].IsValid ())
        {
            *this << Prefix () << UTF8_STRING(val.Items[i].Name) << Assign () << Quotes () << Encode (UTF8_STRING (val.Items[i].Value)) << Quotes ();
        }
    }
    Items++;
}

void PiwikQueryBuilder::AddDimension(PiwikVisitDimensionsSet val)
{
    for (int i = 0; i < ARRAY_COUNT (val.Items); i++)
    {
        if (val.Items[i].IsValid ())
        {
            *this << Prefix () << UTF8_STRING(val.Items[i].Name) << Assign () <<  Encode (UTF8_STRING (val.Items[i].Value)) ;
        }
    }
    Items++;
}
