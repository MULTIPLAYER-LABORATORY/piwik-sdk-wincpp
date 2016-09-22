///////////////////////////////////////////////////////////////////////////////////////////////////
//
// File:         State.h
// Description:  Definition of the PiwikState class encapsulating all trackable parameters
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

using namespace std;

struct PiwikBasicState
{
	int SiteId;
	TSTRING UserId;
	TSTRING VisitorId;
	int ApiVersion;
	TSTRING UserAgent;
	TSTRING Language;
	TSTRING ScreenRes;
	PiwikVariableSet UserVariables;
	int Recording;
	int ReturnImage;

	PiwikBasicState ()    { SiteId = 0; ApiVersion = PIWIK_API_VERSION; UserAgent = PIWIK_USER_AGENT; 
	                        Recording = PIWIK_RECORDING_VALUE; ReturnImage = PIWIK_SEND_IMAGE; }
};

struct PiwikState : public PiwikBasicState
{
	TSTRING TrackedPath;
	TSTRING TrackedAction;
	TSTRING EventCategory;
	TSTRING EventAction;
	TSTRING EventName;
	float EventValue;
	int Goal;
	float Revenue;
	TSTRING OutLink;
	TSTRING ContentName;
	TSTRING ContentPiece;
	TSTRING ContentTarget;
	TSTRING ContentInteraction;
	PiwikVariableSet ScreenVariables;
	int NewSession;
	int VisitCount;
	time_t FirstVisit;
	time_t LastVisit;
	int Random;

	PiwikState ()      { NewSession = 0; VisitCount = 0; FirstVisit = LastVisit = 0; EventValue = 0; 
	                     Goal = 0; Revenue = 0; Random = 0; }
								
	string Serialize (PiwikQueryFormat frmt);
};

