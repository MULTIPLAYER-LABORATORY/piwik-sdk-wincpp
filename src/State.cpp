///////////////////////////////////////////////////////////////////////////////////////////////////
//
// File:         State.cpp
// Description:  Implementation of the PiwikState class encapsulating all trackable parameters
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
#include "QueryParams.h"
#include "State.h"

// Serialization of a tracking snapshot associating its values to the Piwik request parameters

string& PiwikState::Serialize (string& qry)
{
	PiwikQueryBuilder qb;

	qb.AddParameter (PARAM_SITE_ID, SiteId);
	qb.AddParameter (PARAM_URL_PATH, TrackedPath);
	qb.AddParameter (PARAM_RECORDING, Recording);
	qb.AddParameter (PARAM_SEND_IMAGE, ReturnImage);
	qb.AddParameter (PARAM_RANDOM_NUMBER, Random);

	if (! TrackedAction.empty ())
		qb.AddParameter (PARAM_ACTION_NAME, TrackedAction);
	if (! UserId.empty ())
		qb.AddParameter (PARAM_USER_ID, UserId);
	if (! VisitorId.empty ())
		qb.AddParameter (PARAM_VISITOR_ID, VisitorId);
	if (ApiVersion)
		qb.AddParameter (PARAM_API_VERSION, ApiVersion);
	if (! UserAgent.empty ())
		qb.AddParameter (PARAM_USER_AGENT, UserAgent);
	if (! Language.empty ())
		qb.AddParameter (PARAM_LANGUAGE, Language);
	if (! ScreenRes.empty ())
		qb.AddParameter (PARAM_SCREEN_RESOLUTION, ScreenRes);
	if (! EventCategory.empty ())
		qb.AddParameter (PARAM_EVENT_CATEGORY, EventCategory);
	if (! EventAction.empty ())
		qb.AddParameter (PARAM_EVENT_ACTION, EventAction);
	if (! EventName.empty ())
		qb.AddParameter (PARAM_EVENT_NAME, EventName);
	if (EventValue)
		qb.AddParameter (PARAM_EVENT_VALUE, EventValue);
	if (Goal)
		qb.AddParameter (PARAM_GOAL_ID, Goal);
	if (Revenue)
		qb.AddParameter (PARAM_REVENUE, Revenue);
	if (! OutLink.empty ())
		qb.AddParameter (PARAM_LINK, OutLink);
	if (! ContentName.empty ())
		qb.AddParameter (PARAM_CONTENT_NAME, ContentName);
	if (! ContentPiece.empty ())
		qb.AddParameter (PARAM_CONTENT_PIECE, ContentPiece);
	if (! ContentTarget.empty ())
		qb.AddParameter (PARAM_CONTENT_TARGET, ContentTarget);
	if (! ContentInteraction.empty ())
		qb.AddParameter (PARAM_CONTENT_INTERACTION, ContentInteraction);
	if (NewSession)
		qb.AddParameter (PARAM_SESSION_START, NewSession);
	if (VisitCount)
		qb.AddParameter (PARAM_TOTAL_NUMBER_OF_VISITS, VisitCount);
	if (FirstVisit)
		qb.AddParameter (PARAM_FIRST_VISIT_TIMESTAMP, FirstVisit);
	if (LastVisit)
		qb.AddParameter (PARAM_PREVIOUS_VISIT_TIMESTAMP, LastVisit);
	if (UserVariables.IsValid ())
		qb.AddParameter (PARAM_VISIT_SCOPE_CUSTOM_VARIABLES, UserVariables); 
	if (ScreenVariables.IsValid ())
		qb.AddParameter (PARAM_SCREEN_SCOPE_CUSTOM_VARIABLES, ScreenVariables);

	qry = qb.str ();

	return qry;
}


