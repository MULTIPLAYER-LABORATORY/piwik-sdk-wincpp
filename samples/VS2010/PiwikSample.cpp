// PiwikSample.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "PiwikSample.h"
#include <fstream>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name
HWND MainWindow, tf1, tf2, tf3, tf4, bf1;

// Piwik

#include "../../include/Piwik.h"

#define PIWIK_URL   L"http://diogen.de/piwik/piwik.php"
#define SITE_ID     1
#define LOCATION    L"dashboard:"

PiwikClient* Piwik;
wofstream PiwikLog;

void PiwikInitialize ()
{
	Piwik = new PiwikClient (PIWIK_URL, SITE_ID);
	if (Piwik)
	{
		Piwik->SetLocation (LOCATION);
		Piwik->SetUserId (L"wang@mail.com");
		//Piwik->SetDispatchInterval (5);
		//Piwik->SetRequestMethod (PIWIK_METHOD_GET);
		//Piwik->SetDryRun (true);
		PiwikLog.open ("Piwik.log");
		Piwik->SetLogger (&PiwikLog);
	}
}

void PiwikTrackLaunch ()
{
	if (Piwik)
	{
		Piwik->TrackScreen (L"/application/mainview", L"Login", L"user name", L"Wang", 0, 0, L"time", L"22:05", 0, 0, L"system memory", L"150MB");
		Piwik->TrackEvent (L"/application/general", L"Launch", L"Application launching", L"Main menu");
		Piwik->SetUserVariable (L"Color", L"Red");
		Piwik->SetUserVariable (L"Temperature", L"25°C");
		Piwik->TrackInteraction (L"/application/reports", L"Sent weekly mail", 0, L"Tokio office", 0);
		Piwik->SetUserVariable (L"Metal", L"Cupper");
		Piwik->SetUserVariable (L"Color", L"Blue");
		Piwik->TrackEvent (L"/application/accounting", L"Invoice", L"Invoice Creation", L"Sumitomo Electronics", 25000.0);
	}
}

void PiwikTrackEvent ()
{
	WCHAR url[256];
	WCHAR ctg[256];
	WCHAR act[256];
	WCHAR nam[256];

	if (Piwik)
	{
		GetWindowText (tf1, url, 256); 
		GetWindowText (tf2, ctg, 256); 
		GetWindowText (tf3, act, 256); 
		GetWindowText (tf4, nam, 256); 
		Piwik->TrackEvent (url, ctg, act, nam);
	}
}

void PiwikShutdown ()
{
	if (Piwik)
	{
		Piwik->TrackEvent (L"/application/general", L"Shutdown", L"Application closing");
		Piwik->Flush ();
		delete Piwik;
		Piwik = 0;
	}
}

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_PIWIKSAMPLE, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_PIWIKSAMPLE));

	tf1 = CreateWindowEx (0, L"Edit", L"URL", WS_BORDER | WS_CHILDWINDOW | WS_TABSTOP | WS_VISIBLE, 50, 50, 200, 25, MainWindow, 0, 0, 0);
	tf2 = CreateWindowEx (0, L"Edit", L"category", WS_BORDER | WS_CHILDWINDOW | WS_TABSTOP | WS_VISIBLE, 50, 100, 200, 25, MainWindow, 0, 0, 0);
	tf3 = CreateWindowEx (0, L"Edit", L"action", WS_BORDER | WS_CHILDWINDOW | WS_TABSTOP | WS_VISIBLE, 50, 150, 200, 25, MainWindow, 0, 0, 0);
	tf4 = CreateWindowEx (0, L"Edit", L"name", WS_BORDER | WS_CHILDWINDOW | WS_TABSTOP | WS_VISIBLE, 50, 200, 200, 25, MainWindow, 0, 0, 0);

	bf1 = CreateWindowEx (0, L"Button", L"Track Event", WS_BORDER | WS_CHILDWINDOW | WS_TABSTOP | WS_VISIBLE, 300, 100, 200, 75, MainWindow, 0, 0, 0);

	PiwikInitialize ();

	PiwikTrackLaunch ();

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	PiwikShutdown ();

	return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PIWIKSAMPLE));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_PIWIKSAMPLE);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // Store instance handle in our global variable

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);
   MainWindow = hWnd;

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_COMMAND:
		if ((HWND) lParam == bf1)
		{
			PiwikTrackEvent ();
			return 0;
		}
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
