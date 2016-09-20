///////////////////////////////////////////////////////////////////////////////////////////////////
//
// File:         Config.h
// Description:  Main symbols tailoring the environment and the initial behavior of the library
// Project:      Piwik-SDK-Win-C++
// Version:      1.0
// Date:         2016-09-19
// Author:       Manfred Klimt - Diogen Software-Entwicklung (bramfeld@diogen.de)
// Copyright:    (c) 2016 mplabsorg
// License:      See provided LICENSE file
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// Symbols

#define PIWIK_LOG_PREFIX           _T("PIWIK:")
#define PIWIK_API_VERSION          1
#define PIWIK_BASIC_METHOD         PIWIK_METHOD_POST
#define PIWIK_USER_AGENT           _T("Piwik Client Windows Desktop")
#define PIWIK_INITIAL_LOG_LEVEL    PIWIK_LOG_ERROR
#define PIWIK_RECORDING_VALUE      1
#define PIWIK_SEND_IMAGE           0
#define PIWIK_CUSTOM_VARIABLES     5
#define PIWIK_VARIABLE_LENGTH      200
#define PIWIK_SESSION_TIMEOUT      30   // sec before restarting a client session
#define PIWIK_CONNECTION_TIMEOUT   5    // sec while trying to establish a connection
#define PIWIK_DISPATCH_INTERVAL    120  // sec between API requests
#define PIWIK_DISPATCH_BUNDLE      20   // number of queries sent together in one request

// Target Operating System Version

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
