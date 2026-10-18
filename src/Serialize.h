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
    void AddDimensions(const PiwikVisitDimensionsSet& visit,
                       const PiwikDimensionsSet& legacyAction,
                       const PiwikActionDimensions& action);

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

void PiwikQueryBuilder::AddDimensions(const PiwikVisitDimensionsSet& visit,
                                    const PiwikDimensionsSet& legacyAction,
                                    const PiwikActionDimensions& action)
{
    // Emit each wire key once. Request-local values take precedence over visit
    // defaults; typed values take precedence over the legacy action array.
    std::map<TSTRING, TSTRING> dimensions;
    for (const auto& item : visit.Items)
    {
        if (!item.Name.empty() && !item.Value.empty())
            dimensions[item.Name] = item.Value;
    }
    for (const auto& item : legacyAction.Items)
    {
        if (!item.Name.empty() && !item.Value.empty())
            dimensions[item.Name] = item.Value;
    }
    for (const auto& item : action.Values())
    {
        std::basic_ostringstream<TCHAR> name;
        name.imbue(std::locale::classic());
        name << _T("dimension") << item.first;
        dimensions[name.str()] = item.second;
    }
    for (auto& item : dimensions)
    {
        // Bulk requests also contain URL query strings inside their JSON array.
        // URL encoding makes the result JSON-safe and preserves &, +, =, etc.
        TSTRING name = item.first;
        std::string encodedName = UTF8_STRING(name);
        std::string encodedValue = UTF8_STRING(item.second);
        *this << Prefix () << PercentEncode(encodedName) << Assign () << PercentEncode(encodedValue);
        Items++;
    }
}
