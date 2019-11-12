///////////////////////////////////////////////////////////////////////////////////////////////////
//
// File:         QueryParams.h
// Description:  Definition of the HTTP request parameters recognized by the Piwik API
// Project:      Piwik-SDK-Win-C++
// Version:      1.0
// Date:         2016-09-19
// Author:       Manfred Klimt - Diogen Software-Entwicklung (bramfeld@diogen.de)
// Copyright:    (c) 2016 mplabsorg
// License:      See provided LICENSE file
//
///////////////////////////////////////////////////////////////////////////////////////////////////


// Required parameters

// The ID of the website we're tracking a visit/action for
#define PARAM_SITE_ID   "idsite"

// Required for tracking, must be set to 1
#define PARAM_RECORDING   "rec"

// The full URL for the current action.
#define PARAM_URL_PATH   "url"


// Recommended parameters

// The title of the action being tracked.
// It is possible to use slashes / to set one or several categories for this action.
// For example, Help / Feedback will create the Action Feedback in the category Help.
#define PARAM_ACTION_NAME   "action_name"

// The unique visitor ID, must be a 16 characters hexadecimal string.
// Every unique visitor must be assigned a different ID and this ID must not change after it is assigned.
// If this value is not set Piwik will still track visits, but the unique visitors metric might be less accurate.
#define PARAM_VISITOR_ID   "_id"

// Meant to hold a random value that is generated before each request.
// Using it helps avoid the tracking request being cached by the browser or a proxy.
#define PARAM_RANDOM_NUMBER   "rand"

// The parameter &apiv=1 defines the api version to use (currently always set to 1)
#define PARAM_API_VERSION   "apiv"


// Optional User info

// The full HTTP Referrer URL.
// This value is used to determine how someone got to your website (ie, through a website, search engine or campaign).
#define PARAM_REFERRER   "urlref"

// Visit scope custom variables
// This is a JSON encoded string of the custom variable array.
#define PARAM_VISIT_SCOPE_CUSTOM_VARIABLES   "_cvar"

// The current count of visits for this visitor.
// To set this value correctly, it would be required to store the value for each visitor in your application (using sessions or
// persisting in a database).
// Then you would manually increment the counts by one on each new visit or "session", depending on how you choose to define a visit.
// This value is used to populate the report Visitors > Engagement > Visits by visit number.
#define PARAM_TOTAL_NUMBER_OF_VISITS   "_idvc"

// The UNIX timestamp of this visitor's previous visit (seconds since Jan 01 1970. (UTC)).
// This parameter is used to populate the report Visitors > Engagement > Visits by days since last visit.
#define PARAM_PREVIOUS_VISIT_TIMESTAMP   "_viewts"

// The UNIX timestamp of this visitor's first visit (seconds since Jan 01 1970. (UTC)).
// This could be set to the date where the user first started using your software/app, or when he/she created an account.
// This parameter is used to populate the Goals > Days to Conversion report.
#define PARAM_FIRST_VISIT_TIMESTAMP   "_idts"

// The Campaign name (see http://piwik.org/docs/tracking-campaigns/
// Used to populate the Referrers > Campaigns report.
// Note: this parameter will only be used for the first pageview of a visit.
#define PARAM_CAMPAIGN_NAME   "_rcn"

// The Campaign Keyword (see http://piwik.org/docs/tracking-campaigns/
// Used to populate the Referrers > Campaigns report (clicking on a campaign loads all keywords for this campaign).
// Note: this parameter will only be used for the first pageview of a visit.
#define PARAM_CAMPAIGN_KEYWORD   "_rck"

// The resolution of the device the visitor is using, eg 1280x1024.
#define PARAM_SCREEN_RESOLUTION   "res"

// The current hour (local time)
#define PARAM_HOURS   "h"

// The current minute (local time)
#define PARAM_MINUTES   "m"

// The current second (local time)
#define PARAM_SECONDS   "s"

// An override value for the User-Agent HTTP header field.
// The user agent is used to detect the operating system and browser used.
#define PARAM_USER_AGENT   "ua"

// An override value for the Accept-Language HTTP header field.
// This value is used to detect the visitor's country if GeoIP is not enabled.
#define PARAM_LANGUAGE   "lang"

// Defines the User ID for this request.
// User ID is any non empty unique string identifying the user (such as an email address or a username).
// The User ID appears in the visitor log, the Visitor profile, and you can Segment reports for one or several User ID.
// When specified, the User ID will be "enforced". This means that if there is no recent visit with this User ID,
// a new one will be created. If a visit is found in the last 30 minutes with your specified User ID, 
// then the new action will be recorded to this existing visit.
#define PARAM_USER_ID   "uid"

// If set to 1, will force a new visit to be created for this action.
#define PARAM_SESSION_START   "new_visit"


// Optional action info

// Page scope custom variables.
// This is a JSON encoded string of the custom variable array.
#define PARAM_SCREEN_SCOPE_CUSTOM_VARIABLES   "cvar"

// An external URL the user has opened.
// Used for tracking outlink clicks. We recommend to also set the url parameter to this same value.
#define PARAM_LINK   "link"

// URL of a file the user has downloaded.
// Used for tracking downloads. We recommend to also set the url parameter to this same value.
#define PARAM_DOWNLOAD   "download"

// The Site Search keyword.
// When specified, the request will not be tracked as a normal pageview but will instead be tracked as a Site Search request.
#define PARAM_SEARCH_KEYWORD   "search"

// When SEARCH_KEYWORD is specified, you can optionally specify a search category with this parameter.
#define PARAM_SEARCH_CATEGORY   "search_cat"

// When SEARCH_KEYWORD is specified, we also recommend to set this to the number of search results.
#define PARAM_SEARCH_NUMBER_OF_HITS   "search_count"

// If specified, the tracking request will trigger a conversion for the goal of the website being tracked with this ID.
#define PARAM_GOAL_ID   "idgoal"

// A monetary value that was generated as revenue by this goal conversion.
// Only used if GOAL_ID is specified in the request.
#define PARAM_REVENUE   "revenue"

// The amount of time it took the server to generate this action, in milliseconds
// This value is used to process the Page speed report Avg. generation time column.
#define PARAM_AMOUNT_OF_TIME   "gt_ms"

// 32 character authorization key used to authenticate the API request.
// deprecated due to security concerns.
#define PARAM_AUTHENTICATION_TOKEN   "token_auth"

// An override value for the country.
// Should be set to the two letter country code of the visitor (lowercase), eg fr, de, us.
// Requires AUTHENTICATION_TOKEN.
#define PARAM_COUNTRY   "country"

// An override value for the visitor's latitude, eg 22.456.
// Requires AUTHENTICATION_TOKEN.
#define PARAM_LATITUDE   "lat"

// An override value for the visitor's longitude, eg 22.456.
// Requires AUTHENTICATION_TOKEN.
#define PARAM_LONGITUDE   "long"

// Override for the datetime of the request (normally the current time is used).
// This can be used to record visits and page views in the past.
// The expected format is: 2011-04-05 00:11:42 (in UTC timezone).
// Note: if you record data in the past, you will need to force Piwik to re-process reports for the past dates.
// If you set cdt to a datetime older than four hours then token_auth must be set.
// If you set cdt with a datetime in the last four hours then you don't need to pass AUTHENTICATION_TOKEN.
#define PARAM_DATETIME_OF_REQUEST   "cdt"

// The name of the content. For instance 'Ad Foo Bar'
// see http://piwik.org/docs/content-tracking/
#define PARAM_CONTENT_NAME   "c_n"

// The actual content piece. For instance the path to an image, video, audio, any text
// see http://piwik.org/docs/content-tracking/"
#define PARAM_CONTENT_PIECE   "c_p"

// The target of the content. For instance the URL of a landing page
// see http://piwik.org/docs/content-tracking/
#define PARAM_CONTENT_TARGET   "c_t"

// The name of the interaction with the content. For instance a 'click'
// see http://piwik.org/docs/content-tracking/
#define PARAM_CONTENT_INTERACTION   "c_i"

// The event category. Must not be empty. (eg. Videos, Music, Games...)
// see http://piwik.org/docs/event-tracking/
#define PARAM_EVENT_CATEGORY   "e_c"

// The event action. Must not be empty. (eg. Play, Pause, Duration, Add Playlist, Downloaded, Clicked...)
// see http://piwik.org/docs/event-tracking/
#define PARAM_EVENT_ACTION   "e_a"

// The event name. (eg. a Movie name, or Song name, or File name...)
// see http://piwik.org/docs/event-tracking/
#define PARAM_EVENT_NAME   "e_n"

// The event value. Must be a float or integer value (numeric), not a string.
// see http://piwik.org/docs/event-tracking/
#define PARAM_EVENT_VALUE   "e_v"

// Ecommerce parameters

// Items in your cart or order for ecommerce tracking
#define PARAM_ECOMMERCE_ITEMS   "ec_items"

// The amount of tax paid for the order
#define PARAM_TAX   "ec_tx"

// The unique identifier for the order
#define PARAM_ORDER_ID   "ec_id"

// The amount of shipping paid on the order
#define PARAM_SHIPPING   "ec_sh"

// The amount of the discount on the order
#define PARAM_DISCOUNT   "ec_dt"

// The sub total amount of the order
#define PARAM_SUBTOTAL   "ec_st"

// Other parameters

// If set to 0 Piwik will respond with a HTTP 204 response code instead of a GIF image.
// This improves performance and can fix errors if images are not allowed to be obtained directly (eg Chrome Apps). 
// Available since Piwik 2.10.0
#define PARAM_SEND_IMAGE   "send_image"

