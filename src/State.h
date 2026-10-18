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
    PiwikVisitDimensionsSet VistDimensionVariables;
	int Recording;
	int ReturnImage;

	PiwikBasicState () : SiteId(0), ApiVersion(PIWIK_API_VERSION), UserAgent(PIWIK_USER_AGENT), 
                         Recording(PIWIK_RECORDING_VALUE), ReturnImage(PIWIK_SEND_IMAGE)
    { 
    }
};

struct PiwikState : public PiwikBasicState
{
	TSTRING TrackedPath;
	TSTRING TrackedAction;
	TSTRING EventCategory;
	TSTRING EventAction;
	TSTRING EventName;
	double EventValue;
	bool HasEventValue;
	int Goal;
	float Revenue;
	TSTRING OutLink;
	TSTRING ContentName;
	TSTRING ContentPiece;
	TSTRING ContentTarget;
	TSTRING ContentInteraction;
	PiwikVariableSet ScreenVariables;
    PiwikDimensionsSet DimensionVariables;
    PiwikActionDimensions ActionDimensions;
	int NewSession;
	int VisitCount;
	time_t FirstVisit;
	time_t LastVisit;
	int Random;
    // Legacy gt_ms in milliseconds: unsupported by Matomo 4+.
    // Retained for older servers; use an explicit event value for app durations.
    int AmountOfTime;

	PiwikState (): NewSession(0), VisitCount(0), FirstVisit(0), LastVisit(0), 
                   EventValue(0), HasEventValue(false), Goal(0), Revenue(0), Random(0), AmountOfTime(0)
    {
    }
								
	void SetEventValue (double value);
	void ClearEventValue ();
	bool HasFiniteNumbers () const;
	// Throws std::invalid_argument for a non-finite event value or revenue.
	// Both formats return a URL-encoded query; bulk mode embeds it in JSON.
	string Serialize (PiwikQueryFormat frmt);
};

