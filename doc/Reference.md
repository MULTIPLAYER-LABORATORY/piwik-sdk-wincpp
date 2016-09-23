# piwik-sdk-wincpp

## Reference Documentation

These are the entry points provided by the Piwik tracking library:

1. Construction:

	``PiwikClient (LPCTSTR url, int id = 0)``
	
		Parameters:
	
		url: the Piwik endpoint where requests are to be sent (can be changed later with SetApiUrl)
		id: the numeric ID of the site being tracked (can be changed later with SetSiteId)
	
2. Configuration:
	
	``int  CurrentSiteId ()``
	
		Returns the ID of the site currently being tracked.
			
	``void SetSiteId (int id)``
	
		Allows to set the numeric ID of the site being tracked, as known to the target Piwik server.
		
	``TSTRING CurrentUserId ()``
	
		Returns the string identifying the user accessing the Piwik service in this tracker.
		
	``void SetUserId (LPCTSTR p)``
	
		Allows to set the string identifying the user on this tracker (usually username or email address).
		This value will be used to generate the field VisitorId.
		
	``TSTRING CurrentApiUrl ()``
	
		Returns the URL of the target Piwik server.
		
	``void SetApiUrl (LPCTSTR p)``
	
		Allows to set the URL of the Piwik server where requests are to be sent. If no scheme is provided, 
		http:// will be assumed. If http:// is explicitly specified, a non secure connection on port 80 will
		be used. If https:// is explicitly specified, a secure connection on port 443 will
		be established. The new value will be used for all tracking calls issued after this point.
		
	``int  CurrentRequestMethod ()``
	
		Returns the HTTP request method currently used to send requests to the Piwik server. 
			
	``void SetRequestMethod (PiwikMethod m)``
	
		Allows to set the HTTP request method to be used. It can be one of the following values: 
		
			```
			PIWIK_METHOD_GET
			PIWIK_METHOD_POST			
			```
		
		Using GET will send the query parameters URL-encoded within the URL line of the request. 
		Using POST (the default) will send the query encoded as a JSON object in the body of the request. 
		This results in a more efficient transfer since several queries (up to 20) can be sent together 
		in one physical request.
		
	``bool UsesSecureConnection ()``
	
		Returns if secure mode (SSL) is being used to communicate with the Piwik server.
		
	``void SetSecureConnection (bool f)``
	
		Allows to select the use of secure connections. 
		When true, SSL will be used to access the Piwik service on port 443. 
		Otherwise requests wil be sent unencoded over port 80.
		
	``TSTRING CurrentUserAgent ()``
	
		Returns the name of the user agent transmitted to Piwik with each query.
		
	``void SetUserAgent (LPCTSTR p)``
	
		Allows to set the name of the user agent transmitted to Piwik with each query. If not specified, the default
		value ("Piwik Client Windows Desktop") will be used.
		
	``TSTRING CurrentLanguage ()``
	
		Returns the language code being transmitted to Piwik with each query.
		
	``void SetLanguage (LPCTSTR p)``
	
		Allows to set the 2-letter language code to be transmitted to Piwik with each query.
		
	``void SetUserVariable (LPCTSTR nam, LPCTSTR val, int ind = -1)``
	
		Allows to set the name and value of a user (or global) variable. Both name and value are limited
		to 200 characters. There are up to 5 such variables available on each site, and each one of them
		should keep always the same position in the set in order to be correctly tracked. The parameter ind 
		allows to specify explicitly this position. If not provided, the tracker will try to deduce its value 
		by looking up a variable with the same name in the current set. If found, its same index will
		be used, otherwise a free slot will be taken. If no free slots are remaining, the last slot will
		be overwritten. Setting a NULL value for the variable will free the corresponding slot.
		
	``TSTRING CurrentApplication ()``
	
		Returns the name of the application being used to write settings in the Registry.
		
	``void SetApplication (LPCTSTR p)``
	
		Allows to set the name of the application to be used when writing settings to the Registry. 
		
	``TSTRING CurrentLocation ()``
	
		Returns the base location corresponding to the site being tracked.
		
	``void SetLocation (LPCTSTR p)``
	
		Allows to set the base location of the site being tracked. This value will be prepended to the 
		specific URL supplied in each tracking call if this URL is not absolute (i.e. if it doesn't
		contain a scheme delimited by a colon). Using this mechanism allows to simplify the URL
		parameter sent with each tracking call.
	
	``bool SetSessionTimeout (int t)``
	
		Allows to set the timeout in seconds after which a new session will be started. It defaults to 
		30 minutes. At the start of each session general identification data about the site being tracked 
		is sent again to the	Pikik service.
		
	``void SetConnectionTimeout (int t)``
	
		Allows to set the timeout in seconds when trying to open a connection to the Piwik server.
	
	``void SetDispatchInterval (int t)``
	
		Allows to set the delay in seconds between successive connections to the Piwik server when
		using asynchronous mode. Tracking queries will remain pending during this time and be sent 
		together	at the end of each interval. Setting this value to 0 will effectively set synchronous
		behavior. Setting it to a negative value will disable automatic transmission and will require 
		a call to Flush to send data to the server. A value of 120 seconds will be used by default.
		
	``void StartNewSession ()``
	
		Allows to force the start of a new session.
		
	``bool IsPersistent ()``
	
		Returns if user data is being persisted to the registry.	
		
	``void SetPersistent (bool v)``
	
		Allows to select persistent mode for user statistics. When persistent mode is active, number of 
		visits and visit times will be stored in the Windows Registry for each user under the key 
		HKEY_CURRENT_USER\Software\<application>\<username>. This allows the Piwik server to produce better
		analytics for registered users.
		
	``bool IsDisabled ()``
	
		Returns if the tracker is temporarily out of service.
		
	``void SetDisabled (bool v)``
	
		Allows to set the tracker temporarily out of service. Tracking calls will be ignored in this state.
		
	``bool IsDryRun ()``
	
		Returns if the dry run mode is active.
		
	``void SetDryRun (bool v)``
	
		Allows to set dry run mode, in which tracking requests are processed but not sent effectively
		to the network. If logging is active, they will be dumped instead to the log stream.
		
	``void SetLogger (wostream* s, PiwikLogLevel lvl)``
	
		Allows to provide an output stream to which diagnostics of the library will be written for use
		by the hosting application.

3. Tracking

	Following calls can be used to track specific behavior:
	
	``bool TrackEvent (LPCTSTR path, LPCTSTR ctg = 0, LPCTSTR act = 0, LPCTSTR nam = 0, float val = 0)``
	
		Parameters:
		
		path: the URL of the event (required)
		ctg: the category of the event
		act: the action being tracked
		nam: the name of the property being tracked
		val: the numeric value associated with this property
		
	```	
	bool TrackScreen (LPCTSTR path, LPCTSTR act = 0, LPCTSTR nam1 = 0, LPCTSTR val1 = 0, 
                     LPCTSTR nam2 = 0, LPCTSTR val2 = 0, LPCTSTR nam3 = 0, LPCTSTR val3 = 0, 
                     LPCTSTR nam4 = 0, LPCTSTR val4 = 0, LPCTSTR nam5 = 0, LPCTSTR val5 = 0)
	```
		Parameters:
		
		path: the URL of the event (required)
		act: the action being tracked
		nam[1-5]: the name of a local variable associated with this screen
		val[1-5]: the value of this variable
		
		Local variables should keep always the same position within this set in order to be correctly tracked. 
		When sending these values to the server they will be assigned to the slot corresponding to the positional
		parameter being used. Any unneeded slots can be filled with NULL.
		
	``bool TrackGoal (LPCTSTR path, int goal, float rev = 0)``
	
		Parameters:
		
		path: the URL of the event (required)
		goal: the numeric identifier of the goal being tracked
		rev: the amount indicating the revenue corresponding to this goal
		
	``bool TrackOutLink (LPCTSTR path)``
	
		Parameters:
		
		path: the URL of the followed link (required)
		
	``bool TrackImpression (LPCTSTR path, LPCTSTR content, LPCTSTR piece, LPCTSTR target)``
	
		Parameters:
		
		path: the URL of the event (required)
		content: the name of the content being shown
		piece: the specific piece within this content
		target: the target associated with this content
		
	``bool TrackInteraction (LPCTSTR path, LPCTSTR content, LPCTSTR piece, LPCTSTR target, LPCTSTR action)``
	
		Parameters:
		
		path: the URL of the event (required)
		content: the name of the content being shown
		piece: the specific piece within this content
		target: the target associated with this content
		action: the specific action being tracked
		
	``bool Track (PiwikState& st)``
	
		To track complex situations a ``PiwikState`` can be explicitly constructed and any of these
		fields may be specified:
		
		```
		int SiteId;
		TSTRING UserId;
		TSTRING VisitorId;
		int ApiVersion;
		TSTRING UserAgent;
		TSTRING Language;
		TSTRING ScreenRes;
		PiwikVariableSet UserVariables;
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
		```
		
	``bool Flush ()``
	
		Allows to send all pending requests synchronously to the server. This is called
		implicitly when terminating the Piwik instance.
		
	
