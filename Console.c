/*
  Console Plugin

  Copyright (C) 2003-2004 Artur Dorochowicz
  All Rights Reserved.
*/

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "resources.h"
#include "version.h"

//---MACROS---------------------------------------------------------------------
#define MAX_VAR_LENGTH	63
#define MAX_CMD_LENGTH		531

#define PREVIOUS_COMMANDS_COUNT	11
#define WATCHES_COUNT			5

#define WATCH_WND_HEIGHT	150
#define TITLE_BAR_HEIGHT	20
#define SPACE_BETWEEN_WNDS	20	// space between watch window and edit control

#define ID_EDITBOX		200
#define ID_WATCHBOX		210
#define ID_EXITBTN		220
#define ID_MINIMIZEBTN	230
#define IDT_TIMER		300

#define ACTION_EXECUTE		1
#define ACTION_PRINT		2
#define ACTION_APPEND		3

#define MAIN_WND_BK_COLOR	(RGB(236, 234, 220))

//------------------------------------------------------------------------------
typedef struct tagPProServices
{
	void (* ErrMessage) (LPSTR, LPSTR);
	BOOL (* MatchCaption) (HWND, LPSTR);
	HWND (* FindMatchingWindow) (LPSTR,BOOL);
	BOOL (* IsRolled) (HWND hw);
	BOOL (* IsTrayMinned) (HWND hw);
	void (* GetExeFullPath) (HWND hw, LPSTR szt);
	void (* RollUp) (HWND hw);
	void (* TrayMin) (HWND hw);
	void (* SendKeys) (LPSTR sz);
	BOOL (* EvalExpr) (LPSTR sz, LPSTR szo);
	void (* Debug) (LPSTR sz1, LPSTR sz2,LPSTR sz3, LPSTR sz4, LPSTR sz5, LPSTR sz6);
	LPSTR (* AllocTemp) (UINT leng);
	void (* ReturnString) (LPSTR sz, LPSTR* szargs);
	LPSTR (* GetVarAddr) (LPSTR var);
	LPSTR (* SetVar) (LPSTR var, LPSTR val);
	void (* IgnoreNextClip) ();
	void (* Show) (HWND h);
	void (* RunCmd) (LPSTR szCmd, LPSTR szParam, LPSTR szWork);
	BOOL (* InsertStringForBar) ( LPSTR szStr, LPSTR szCmd);
	void (* ResetFocus) ();
} PPROSERVICES;

//---GLOBAL VARIABLES-----------------------------------------------------------
const char wnd_class_name [] = "Console Plugin " CONSOLE_VERSION;	// the main window class name
const char wnd_title [] = "Console Plugin";				// The title bar text
const unsigned char default_font [] = "Courier New";	// default font name
const unsigned char default_prompt [] = "?>";			// default prompt

PPROSERVICES * PProServices;

BOOL	awaiting_input;			// true if waiting for user input
BOOL	watches_enabled;		// true if watches are enabled
HBITMAP	hbTitleBar = NULL;		// bitmap for title bar
HBITMAP hbClose = NULL;			// handle of bitmap on close button
HBITMAP hbMinimize = NULL;		// handle of bitmap on minimize button
HBRUSH	title_bar_brush = NULL;	// brush for filling the title bar
HFONT	hFont = NULL;			// font handle for edit control
HINSTANCE instance = NULL;		// current instance
HWND	hEdit = NULL; 			// edit control window handle
HWND	hExitBtn = NULL;		// exit button handle
HWND	hMainWnd = NULL;		// main window handle
HWND	hMinimizeBtn = NULL;	// minimize button handle
HWND	hWatch = NULL;			// watch window handle
int buffer_size;				// size of buffer storing contents of edit box
int cmd_line_start;				// index of first character of current command line
int line_num;					// stores line number of currently shown line
int line_num_for_next;			// stores line number for saving next line
unsigned char * edit_ctrl_text = NULL;		// buffer for edit control text
unsigned char ** previous_commands = NULL;	// stores previous command lines
unsigned char ** watches = NULL;			// stores variables to check
WNDPROC	OrigEditWndProc;		// original window procedure of edit control

// settings
unsigned char * prompt = NULL;
unsigned char * font_name = NULL;
int font_size;
int wnd_height;
int wnd_width;
int wnd_pos_left;
int wnd_pos_top;
BOOL font_bold;
BOOL copyright_note;
BOOL wnd_resizable;

//---EXTERN---------------------------------------------------------------------
extern int yyparse( void );
extern void yyrestart( FILE * );

//---DECLARATIONS---------------------------------------------------------------
LRESULT CALLBACK WndProc (HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK EditWndProc (HWND, UINT, WPARAM, LPARAM);
BOOL AllocateMemory (void);
BOOL IsKeyword (const unsigned char *);
BOOL IsConsoleService (const unsigned char *);
BOOL SetDefaultSettings (void);
int DecreaseLineNum (int number);
int GetCaretPositionBySelection( void );
int GetCaretPositionByGetCaretPos( void );
int IncreaseLineNum (int number);
LRESULT	OnKeyDown (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT	OnKeyLeft (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT	OnKeyUp (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT OnKeyEnter(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT OnKeyHome (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
unsigned char * GetNextCommand (void);
unsigned char * GetPreviousCommand (void);
void AddCommandToList (const unsigned char *);
void DeleteCmdLine( void );
void DisableWatches (void);
void EnableWatches (int interval);
void FreeResources (void);
void GetCmdLine( unsigned char * );
void MakeAction (int action, LPSTR, LPSTR, BOOL (*GetVar)(LPSTR, LPSTR), void (*SetVar)(LPSTR, LPSTR), DWORD *, UINT, LPSTR *, PPROSERVICES *);
void MoveCaretToEnd( void );
void SetWatch( int watch_no, unsigned char * value );
void WriteCopyrightNote( void );
void WritePrompt( void );
void WriteText( HWND, const unsigned char * );
void ErrorMessage( const unsigned char * );

//---DEFINITIONS----------------------------------------------------------------
BOOL WINAPI DllMain (HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpvReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		instance = hinstDLL;		
	}
	else
	{
		if (dwReason == DLL_PROCESS_DETACH)
		{
			HWND oldhWnd;

			oldhWnd = FindWindow(wnd_class_name,NULL);
			if (oldhWnd)
				DestroyWindow(oldhWnd);
			UnregisterClass(wnd_class_name, hinstDLL);
		}
	}
	return TRUE;
}

//------------------------------------------------------------------------------
_declspec(dllexport) void show (LPSTR szv, LPSTR szx, BOOL (*GetVar)(LPSTR, LPSTR), void (*SetVar)(LPSTR, LPSTR), DWORD * pFlags, UINT nargs, LPSTR * szargs, PPROSERVICES * ppsv)
{
	WNDCLASS wc;
	HDC hdc;
	HWND oldhWnd;

	// return nothing
	**szargs = '\0';

	PProServices = ppsv;

	// need 1 or 0 arguments
	if (nargs >= 2)
	{
		ErrorMessage( "The service needs at most one argument." );
		return;
	}

	// if window exists - exit
	oldhWnd = FindWindow( wnd_class_name, NULL );
	if (oldhWnd != NULL)
		return;

	// set initial values
	awaiting_input = FALSE;
	watches_enabled = FALSE;
	line_num = 0;
	line_num_for_next = 0;
	buffer_size = 100000;

	// load bitmaps
	hbClose = LoadBitmap (instance, MAKEINTRESOURCE (IDB_CLOSE));
	hbMinimize = LoadBitmap (instance, MAKEINTRESOURCE (IDB_MINIMIZE));
	hbTitleBar = LoadBitmap (instance, MAKEINTRESOURCE (IDB_TITLE_BAR));
	if (hbClose == NULL || hbMinimize == NULL || hbTitleBar == NULL)
	{
		ErrorMessage( "Cannot load bitmaps." );
		FreeResources ();
		return;
	}

	// create brush
	title_bar_brush = CreatePatternBrush (hbTitleBar);
	if (title_bar_brush == NULL)
	{
		ErrorMessage( "Cannot create brush." );
		FreeResources ();
		return;
	}

	// load default settings
	if (FALSE == SetDefaultSettings ())
	{
		ErrorMessage( "Cannot allocate memory for default settings." );
		FreeResources ();
		return;
	}

	// if we have one argument then load settings from file
	if (nargs == 1)
	{
		FILE * file;
		
		file = fopen (szargs[1], "r");
		
		if (file == NULL)
		{
			ErrorMessage( "Cannot open specified file." );
			FreeResources ();
			return;
		}
		
		yyrestart (file);
		if (1 == yyparse ())
		{			
			fclose (file);
			
			// if got here because of allocation error
			if (prompt == NULL || font_name == NULL)
			{
				ErrorMessage( "Cannot allocate memory for loaded settings." );
			}
			// it's really a syntax error
			else
			{
				ErrorMessage( "Specified file has wrong format." );
			}
			
			FreeResources ();
			return;
		}

		fclose (file);
	}

	// allocate memory
	if ( FALSE == AllocateMemory( ) )
	{
		ErrorMessage( "Cannot allocate memory." );
		FreeResources();
		return;
	}

	// register class
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = ( WNDPROC ) WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = instance;
	wc.hIcon = LoadIcon( NULL, IDI_APPLICATION );
	wc.hCursor = LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = CreateSolidBrush( MAIN_WND_BK_COLOR );
	wc.lpszMenuName = NULL;
	wc.lpszClassName = wnd_class_name;

	if ( !RegisterClass( &wc ) )
	{
		ErrorMessage( "Cannot register window class." );
		FreeResources();
		return;
	}

	// create font
	hdc = GetDC( NULL );
	hFont = CreateFont( -MulDiv(font_size, GetDeviceCaps(hdc, LOGPIXELSY), 72),	// logical height of font
			0,					// logical average character width
			0,					// angle of escapement
			0,					// base-line orientation angle
			(font_bold == FALSE) ? (FW_NORMAL) : (FW_BOLD),	// font weight
			FALSE,				// italic attribute flag
			FALSE,				// underline attribute flag
			FALSE,				// strikeout attribute flag
			DEFAULT_CHARSET,		// character set identifier
			OUT_DEFAULT_PRECIS,		// output precision
			CLIP_DEFAULT_PRECIS,	// clipping precision
			DEFAULT_QUALITY,		// output quality
			DEFAULT_PITCH | FF_DONTCARE,	// pitch and family
			font_name);			// pointer to typeface name string
	ReleaseDC (NULL, hdc);
	if ( hFont == NULL )
	{
		ErrorMessage( "Cannot create requested font." );
		FreeResources();
		return;
	}

	// create main window
	hMainWnd = CreateWindow (wnd_class_name, wnd_title,
		(wnd_resizable == FALSE) ? (WS_POPUPWINDOW) : (WS_POPUPWINDOW | WS_SIZEBOX),
		wnd_pos_left, wnd_pos_top,
		wnd_width, wnd_height,
		NULL, NULL, instance, NULL);
	if (NULL == hMainWnd)
	{
		ErrorMessage( "Cannot create main window." );
		FreeResources();
		return;
	}

	ShowWindow(hMainWnd, SW_SHOW);
	UpdateWindow(hMainWnd);
}

//------------------------------------------------------------------------------
_declspec(dllexport) void close (LPSTR szv, LPSTR szx, BOOL (*GetVar)(LPSTR, LPSTR), void (*SetVar)(LPSTR, LPSTR), DWORD * pFlags, UINT nargs, LPSTR * szargs, PPROSERVICES * ppsv)
{
	HWND oldhWnd;

	**szargs = '\0';

	oldhWnd = FindWindow(wnd_class_name, NULL);
	if (oldhWnd)
	{
		DestroyWindow(oldhWnd);
	}
	UnregisterClass(wnd_class_name, instance);
}

//------------------------------------------------------------------------------
_declspec(dllexport) void execute (LPSTR szv, LPSTR szx, BOOL (*GetVar)(LPSTR, LPSTR), void (*SetVar)(LPSTR, LPSTR), DWORD * pFlags, UINT nargs, LPSTR * szargs, PPROSERVICES * ppsv)
{
	MakeAction (ACTION_EXECUTE, szv, szx, GetVar, SetVar, pFlags, nargs, szargs, ppsv);
}

//------------------------------------------------------------------------------
_declspec(dllexport) void print (LPSTR szv, LPSTR szx, BOOL (*GetVar)(LPSTR, LPSTR), void (*SetVar)(LPSTR, LPSTR), DWORD * pFlags, UINT nargs, LPSTR * szargs, PPROSERVICES * ppsv)
{
	MakeAction (ACTION_PRINT, szv, szx, GetVar, SetVar, pFlags, nargs, szargs, ppsv);
}

//------------------------------------------------------------------------------
_declspec(dllexport) void append (LPSTR szv, LPSTR szx, BOOL (*GetVar)(LPSTR, LPSTR), void (*SetVar)(LPSTR, LPSTR), DWORD * pFlags, UINT nargs, LPSTR * szargs, PPROSERVICES * ppsv)
{
	MakeAction (ACTION_APPEND, szv, szx, GetVar, SetVar, pFlags, nargs, szargs, ppsv);
}

//------------------------------------------------------------------------------
void MakeAction (int action, LPSTR szv, LPSTR szx, BOOL (*GetVar)(LPSTR, LPSTR), void (*SetVar)(LPSTR, LPSTR), DWORD * pFlags, UINT nargs, LPSTR * szargs, PPROSERVICES * ppsv)
{
	HWND oldhWnd;

	// return nothing
	**szargs = '\0';

	if (nargs != 1)
	{
		ErrorMessage( "The service needs exactly one argument" );
		return;
	}

	oldhWnd = FindWindow (wnd_class_name, NULL);
	if (oldhWnd == NULL)
	{
		// make show() believe that there are no arguments
		// but pass szargs because show() writes to it anyway
		show (szv, szx, GetVar, SetVar, pFlags, 0, szargs, ppsv);
	}

	SetForegroundWindow (hMainWnd);

	switch (action)
	{
	case ACTION_EXECUTE:
		awaiting_input = FALSE;
		DeleteCmdLine( );
		WriteText (hEdit, szargs[1]);
		awaiting_input = TRUE;
		SendMessage (hEdit, WM_CHAR, '\r', 0);
		break;
	case ACTION_PRINT:
		awaiting_input = FALSE;
		DeleteCmdLine( );
		WriteText (hEdit, szargs[1]);
		awaiting_input = TRUE;
		break;
	case ACTION_APPEND:
		MoveCaretToEnd( );
		awaiting_input = FALSE;
		WriteText (hEdit, szargs[1]);
		awaiting_input = TRUE;
		break;
	}
}

//------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_CREATE:
		// exit button
		hExitBtn = CreateWindow ("BUTTON", NULL, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_BITMAP | BS_FLAT,
			0, 0, 0, 0, hWnd, ( HMENU ) ID_EXITBTN, instance, NULL );
		SendMessage (hExitBtn, BM_SETIMAGE, (WPARAM) IMAGE_BITMAP, (LPARAM) (HANDLE) hbClose);

		// minimize button
		hMinimizeBtn = CreateWindow ("BUTTON", NULL, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_BITMAP | BS_FLAT,
			0, 0, 0, 0, hWnd, ( HMENU ) ID_MINIMIZEBTN, instance, NULL );
		SendMessage (hMinimizeBtn, BM_SETIMAGE, (WPARAM) IMAGE_BITMAP, (LPARAM) (HANDLE) hbMinimize);

		// edit control
		hEdit = CreateWindow("EDIT", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL| WS_BORDER | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
			0, 0, 0, 0, hWnd, (HMENU) ID_EDITBOX, (HINSTANCE) GetWindowLong(hWnd, GWL_HINSTANCE), NULL);
		SendMessage (hEdit, WM_SETFONT, (WPARAM) hFont, MAKELPARAM(TRUE, 0));

		// edit control for watches
		hWatch = CreateWindow ("EDIT", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
			0, 0, 0, 0, hWnd, (HMENU) ID_WATCHBOX, instance, NULL);
		SendMessage (hWatch, WM_SETFONT, (WPARAM) hFont, MAKELPARAM(TRUE, 0));

		if (copyright_note == TRUE)
		{
			WriteCopyrightNote( );
		}

		WritePrompt( );

		cmd_line_start = GetCaretPositionBySelection( );

		OrigEditWndProc = (WNDPROC) SetWindowLong (hEdit, GWL_WNDPROC, (LONG) EditWndProc);
		SetForegroundWindow (hWnd);

		awaiting_input = TRUE;
		break;
	case WM_SETFOCUS:
		SetFocus (hEdit);
		break;
	case WM_SIZE:
	{
		RECT wnd_rect;

		GetWindowRect( hWnd, &wnd_rect );
		wnd_width = wnd_rect.right - wnd_rect.left;
		wnd_height = wnd_rect.bottom - wnd_rect.top;

		// watches are enabled
		if ( watches_enabled == TRUE )
		{
			wnd_height -= WATCH_WND_HEIGHT + SPACE_BETWEEN_WNDS;
			MoveWindow( hEdit, 0, TITLE_BAR_HEIGHT, LOWORD(lParam), HIWORD(lParam) - WATCH_WND_HEIGHT - TITLE_BAR_HEIGHT - SPACE_BETWEEN_WNDS, TRUE );
			MoveWindow( hWatch, 0, HIWORD(lParam) - WATCH_WND_HEIGHT, LOWORD(lParam), WATCH_WND_HEIGHT, TRUE );
		}
		// watches are disabled
		else
		{
			MoveWindow( hEdit, 0, TITLE_BAR_HEIGHT, LOWORD(lParam), HIWORD(lParam) - TITLE_BAR_HEIGHT, TRUE );
			MoveWindow( hWatch, 0, HIWORD(lParam) + SPACE_BETWEEN_WNDS, LOWORD(lParam), WATCH_WND_HEIGHT, TRUE );
		}
		MoveWindow( hExitBtn, LOWORD(lParam) - 2 - 16, 2, 16, 16, TRUE );
		MoveWindow( hMinimizeBtn, LOWORD(lParam) - 2 - 16 - 1 - 16, 2, 16, 16, TRUE );

		return 0;
	}
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		switch (wmId)
		{
		case ID_EXITBTN:
			DestroyWindow(hWnd);
			UnregisterClass(wnd_class_name, instance);
			break;
		case ID_MINIMIZEBTN:
			CloseWindow (hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
	{
		HGDIOBJ old_obj;
		RECT client_rect;

		GetClientRect (hWnd, &client_rect);

		hdc = BeginPaint(hWnd, &ps);
		old_obj = SelectObject (hdc, title_bar_brush);
		PatBlt (hdc, 0, 0, client_rect.right - client_rect.left - 2 - 16 - 1 - 16 - 2, TITLE_BAR_HEIGHT, PATCOPY);
		SelectObject (hdc, old_obj);

		EndPaint(hWnd, &ps);
		return 0;
	}
	case WM_CLOSE:
		DestroyWindow (hWnd);
		UnregisterClass (wnd_class_name, instance);
		return 0;
	case WM_DESTROY:
		SetWindowLong(hEdit, GWL_WNDPROC, (LONG) OrigEditWndProc);
		KillTimer (hMainWnd, IDT_TIMER);
		FreeResources ();
		return 0;
	case WM_NCHITTEST:
	{
		LRESULT result;
		RECT WndRect;

		GetWindowRect (hWnd, &WndRect);
		result = DefWindowProc(hWnd, message, wParam, lParam);
		if (result == HTCLIENT && (HIWORD(lParam) - WndRect.top) < TITLE_BAR_HEIGHT && (LOWORD(lParam) - WndRect.left) < (wnd_width - 2 - 16 - 1 - 16 - 2))
			result = HTCAPTION;
		return result;
	}
	case WM_TIMER:
		switch (wParam)
		{
		case IDT_TIMER:
		{
			unsigned char cmd_result [MAX_CMD_LENGTH + 1];
			unsigned char * value;
			unsigned char num [20];
			int i;

			SendMessage (hWatch, WM_SETTEXT, 0, (LPARAM) NULL);
			for (i = 0; i < WATCHES_COUNT; i++)
			{
				sprintf (num, "%d", i + 1);
				WriteText (hWatch, num);
				WriteText (hWatch, ". ");
				if (_stricmp (watches[i], "") != 0)
				{
					WriteText (hWatch, watches[i]);
					WriteText (hWatch, " = ");
					value = (PProServices->GetVarAddr(watches[i]));
					if (value != NULL)
					{
						WriteText (hWatch, value);
						WriteText (hWatch, "\n");
					}
					else
					{
						WriteText (hWatch, "Error! No such variable.\n");
					}
				}
				else
				{
					WriteText (hWatch, "No watch defined.\n");
				}
			}
			for (i = 0; i < 32; i++)
			{
				sprintf (num, "pproflag(%d)", i);
				PProServices->EvalExpr (num, cmd_result);
				sprintf (num, "%2d(%c) ", i, cmd_result[0]);
				WriteText (hWatch, num);
			}
			return 0;
		}  // case IDT_TIMER
		}
		return 0;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

//------------------------------------------------------------------------------
LRESULT CALLBACK EditWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_RBUTTONDOWN:
		SetFocus(hWnd);
		return 0;
	case WM_LBUTTONDOWN:
		SetFocus(hWnd);
		return 0;
	case WM_LBUTTONDBLCLK:
		SetFocus(hWnd);
		return 0;
	case WM_UNDO:
		return 0;
	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_HOME:
			return OnKeyHome (hWnd, message, wParam, lParam);
		case VK_LEFT:
			return OnKeyLeft (hWnd, message, wParam, lParam);
		case VK_UP:
			return OnKeyUp (hWnd, message, wParam, lParam);
		case VK_DOWN:
			return OnKeyDown (hWnd, message, wParam, lParam);
		default:
			return CallWindowProc(OrigEditWndProc, hWnd, message, wParam, lParam);
		}
	case WM_CHAR:
		switch (wParam)
		{
		case '\r':
			if (awaiting_input == TRUE)
			{
				return OnKeyEnter (hWnd, message, wParam, lParam);
			}
			else
				return CallWindowProc(OrigEditWndProc, hWnd, message, wParam, lParam);
		case '\b':
			if (awaiting_input == TRUE)
			{
				if ( cmd_line_start == GetCaretPositionByGetCaretPos( ) )
				{
					return 0;
				}
				else
					return CallWindowProc(OrigEditWndProc, hWnd, message, wParam, lParam);
			}
			else
				return CallWindowProc(OrigEditWndProc, hWnd, message, wParam, lParam);
		default:
			return CallWindowProc(OrigEditWndProc, hWnd, message, wParam, lParam);
		}
	default:
		return CallWindowProc(OrigEditWndProc, hWnd, message, wParam, lParam);
	}
}

//------------------------------------------------------------------------------
void WriteText (HWND hwnd, const unsigned char * text)
{
	int i;
	int length = strlen (text);

	for (i = 0; i < length; i++)
	{
		SendMessage (hwnd, WM_CHAR, text[i], 0);
	}
}

//------------------------------------------------------------------------------
void WritePrompt( )
{
	WriteText( hEdit, prompt );
}

//------------------------------------------------------------------------------
int IncreaseLineNum (int number)
{
	if (number == PREVIOUS_COMMANDS_COUNT - 1)
	{
		return 0;
	}
	else
		return number + 1;
}

//------------------------------------------------------------------------------
int DecreaseLineNum (int number)
{
	if (number == 0)
	{
		return PREVIOUS_COMMANDS_COUNT - 1;
	}
	else
		return number - 1;
}

//------------------------------------------------------------------------------
BOOL AllocateMemory()
{
	int i;

	edit_ctrl_text = (unsigned char *) malloc (buffer_size);
	if (edit_ctrl_text == NULL) return FALSE;

	watches = (unsigned char **) malloc (WATCHES_COUNT * sizeof(unsigned char *));
	if (watches == NULL) return FALSE;
	for (i = 0; i < WATCHES_COUNT; i++)
	{
		// calloc - because we need it be clear
		watches[i] = (unsigned char *) calloc (MAX_VAR_LENGTH + 1, sizeof (unsigned char));
		if (watches[i] == NULL) return FALSE;
	}

	previous_commands = (unsigned char **) malloc (PREVIOUS_COMMANDS_COUNT * sizeof(unsigned char *));
	if (previous_commands == NULL) return FALSE;
	for (i = 0; i < PREVIOUS_COMMANDS_COUNT; i++)
	{
		// calloc - because we need it be clear
		previous_commands[i] = (unsigned char *) calloc (MAX_CMD_LENGTH + 1, sizeof (unsigned char));
		if (previous_commands[i] == NULL) return FALSE;
	}
	return TRUE;
}

//------------------------------------------------------------------------------
BOOL IsKeyword (const unsigned char * name)
// Not needed for PowerPro 3.8.02 and above
{
	if ( _stricmp ("acdc", name) == 0 ) return TRUE;
	if ( _stricmp ("allglobals", name) == 0 ) return TRUE;
	if ( _stricmp ("alt", name) == 0 ) return TRUE;
	if ( _stricmp ("batterypercent", name) == 0 ) return TRUE;
	if ( _stricmp ("browserDomain", name) == 0 ) return TRUE;
	if ( _stricmp ("browserSubdomain", name) == 0 ) return TRUE;
	if ( _stricmp ("browserURL", name) == 0 ) return TRUE;
	if ( _stricmp ("caption", name) == 0 ) return TRUE;
	if ( _stricmp ("captionunder", name) == 0 ) return TRUE;
	if ( _stricmp ("cdcurtrack", name) == 0 ) return TRUE;
	if ( _stricmp ("cdlasttrack", name) == 0 ) return TRUE;
	if ( _stricmp ("clip", name) == 0 ) return TRUE;
	if ( _stricmp ("cliptrackon", name) == 0 ) return TRUE;
	if ( _stricmp ("cpu", name) == 0 ) return TRUE;
	if ( _stricmp ("ctrl", name) == 0 ) return TRUE;
	if ( _stricmp ("currentdir", name) == 0 ) return TRUE;
	if ( _stricmp ("date", name) == 0) return TRUE;
	if ( _stricmp ("dayofweek", name) == 0 ) return TRUE;
	if ( _stricmp ("dayofyear", name) == 0 ) return TRUE;
	if ( _stricmp ("defaultprinter", name) == 0 ) return TRUE;
	if ( _stricmp ("deskempty", name) == 0 ) return TRUE;
	if ( _stricmp ("deskname", name) == 0 ) return TRUE;
	if ( _stricmp ("desknum", name) == 0 ) return TRUE;
	if ( _stricmp ("dialupname", name) == 0 ) return TRUE;
	if ( _stricmp ("disk", name) == 0 ) return TRUE;
	if ( _stricmp ("dunidle", name) == 0 ) return TRUE;
	if ( _stricmp ("dunrate", name) == 0 ) return TRUE;
	if ( _stricmp ("exefilename", name) == 0 ) return TRUE;
	if ( _stricmp ("exefullpath", name) == 0 ) return TRUE;
	if ( _stricmp ("gdi", name) == 0 ) return TRUE;
	if ( _stricmp ("inputcolor", name) == 0 ) return TRUE;
	if ( _stricmp ("inputdate", name) == 0 ) return TRUE;
	if ( _stricmp ("inputdatetime", name) == 0 ) return TRUE;
	if ( _stricmp ("inputfolder", name) == 0 ) return TRUE;
	if ( _stricmp ("inputpath", name) == 0 ) return TRUE;
	if ( _stricmp ("inputpathmulti", name) == 0 ) return TRUE;
	if ( _stricmp ("inputtext", name) == 0 ) return TRUE;
	if ( _stricmp ("keylog", name) == 0 ) return TRUE;
	if ( _stricmp ("keylogfile", name) == 0 ) return TRUE;
	if ( _stricmp ("lastactivehandle", name) == 0 ) return TRUE;
	if ( _stricmp ("lastautorunhandle", name) == 0 ) return TRUE;
	if ( _stricmp ("lastclipname", name) == 0 ) return TRUE;
	if ( _stricmp ("lastclippath", name) == 0 ) return TRUE;
	if ( _stricmp ("lastidletime", name) == 0 ) return TRUE;
	if ( _stricmp ("longdate", name) == 0 ) return TRUE;
	if ( _stricmp ("modem", name) == 0 ) return TRUE;
	if ( _stricmp ("mouseleft", name) == 0 ) return TRUE;
	if ( _stricmp ("mousemiddle", name) == 0 ) return TRUE;
	if ( _stricmp ("mouseright", name) == 0 ) return TRUE;
	if ( _stricmp ("muted", name) == 0 ) return TRUE;
	if ( _stricmp ("paper", name) == 0 ) return TRUE;
	if ( _stricmp ("pmem", name) == 0 ) return TRUE;
	if ( _stricmp ("pprofolder", name) == 0 ) return TRUE;
	if ( _stricmp ("pproversion", name) == 0 ) return TRUE;
	if ( _stricmp ("processcount", name) == 0 ) return TRUE;
	if ( _stricmp ("recycleItems", name) == 0 ) return TRUE;
	if ( _stricmp ("recycleSize", name) == 0 ) return TRUE;
	if ( _stricmp ("saver", name) == 0 ) return TRUE;
	if ( _stricmp ("saveractive", name) == 0 ) return TRUE;
	if ( _stricmp ("saverenabled", name) == 0 ) return TRUE;
	if ( _stricmp ("savertime", name) == 0 ) return TRUE;
	if ( _stricmp ("scriptfolder", name) == 0 ) return TRUE;
	if ( _stricmp ("scriptname", name) == 0 ) return TRUE;
	if ( _stricmp ("scriptpath", name) == 0 ) return TRUE;
	if ( _stricmp ("shift", name) == 0 ) return TRUE;
	if ( _stricmp ("shortdate", name) == 0) return TRUE;
	if ( _stricmp ("Subbarname", name) == 0 ) return TRUE;
	if ( _stricmp ("threadcount", name) == 0 ) return TRUE;
	if ( _stricmp ("time", name) == 0 ) return TRUE;
	if ( _stricmp ("timesec", name) == 0 ) return TRUE;
	if ( _stricmp ("timezone", name) == 0 ) return TRUE;
	if ( _stricmp ("unixdate", name) == 0 ) return TRUE;
	if ( _stricmp ("uptime", name) == 0 ) return TRUE;
	if ( _stricmp ("user", name) == 0 ) return TRUE;
	if ( _stricmp ("volume", name) == 0 ) return TRUE;
	if ( _stricmp ("win", name) == 0 ) return TRUE;
	if ( _stricmp ("xmouse", name) == 0 ) return TRUE;
	if ( _stricmp ("xscreen", name) == 0 ) return TRUE;
	if ( _stricmp ("xtime", name) == 0 ) return TRUE;
	if ( _stricmp ("ymouse", name) == 0 ) return TRUE;
	if ( _stricmp ("yscreen", name) == 0 ) return TRUE;

	return FALSE;
}

//------------------------------------------------------------------------------
unsigned char *	GetNextCommand ()
{
	int tmp;
	if (line_num == line_num_for_next)
	{
		return NULL;
	}

	tmp = IncreaseLineNum (line_num);

	if (tmp == line_num_for_next || strcmp(previous_commands[tmp], "") == 0)
	{
		return NULL;
	}
	else
	{
		line_num = tmp;
		return previous_commands [line_num];
	}
}

//------------------------------------------------------------------------------
unsigned char *	GetPreviousCommand ()
{
	int tmp = DecreaseLineNum (line_num);

	if (tmp == line_num_for_next || strcmp(previous_commands[tmp], "") == 0)
	{
		return NULL;
	}
	else
	{
		line_num = tmp;
		return previous_commands[line_num];
	}
}

//------------------------------------------------------------------------------
void AddCommandToList (const unsigned char * cmd)
{
	if (strcmp (cmd, "") != 0)
	{
		strcpy (previous_commands[line_num_for_next], cmd);
		line_num_for_next = IncreaseLineNum (line_num_for_next);
	}
	line_num = line_num_for_next;
}

//------------------------------------------------------------------------------
int GetCaretPositionBySelection( )
{
	int sel_start;
		
	SendMessage( hEdit, EM_GETSEL, (WPARAM) &sel_start, 0 );
	return sel_start;
}

//------------------------------------------------------------------------------
int GetCaretPositionByGetCaretPos( )
{
	POINT point;

	// if error getting caret's position then move caret to the end and return that new position
	if (!GetCaretPos (&point))
	{
		int sel_start;
		
		MoveCaretToEnd( );
		SendMessage( hEdit, EM_GETSEL, (WPARAM) &sel_start, 0 );

		return sel_start;		
	}
	
	// if caret is not visible then move it to the end and return that new position
	if (point.x < 0 || point.y < 0)
	{
		int sel_start;
		
		MoveCaretToEnd( );
		SendMessage( hEdit, EM_GETSEL, (WPARAM) &sel_start, 0 );
		
		return sel_start;
	}
		
	return LOWORD( SendMessage( hEdit, EM_CHARFROMPOS, 0, MAKELPARAM( point.x, point.y ) ) );
}

//------------------------------------------------------------------------------
void MoveCaretToEnd( )
{
	LRESULT length;
	
	length = SendMessage( hEdit, WM_GETTEXTLENGTH, 0, 0 );
	SendMessage( hEdit, EM_SETSEL, length, length );
}

//------------------------------------------------------------------------------
void GetCmdLine( unsigned char * target )
{
	/* OLD WAY, LocalLock always returns NULL on Windows 98
	LRESULT memhandle;
	char * text;
	int length;

	memhandle = SendMessage (hwnd, EM_GETHANDLE, 0, 0);
	text = LocalLock ( (HLOCAL) memhandle);

	length = strlen (text) - start_position;
	if (length > 531) length = 531;

	memcpy (target, &(text[start_position]), length);
	target[length] = '\0';

	LocalUnlock ((HLOCAL) memhandle);
	*/

	LRESULT length;

	length = SendMessage( hEdit, WM_GETTEXTLENGTH, 0, 0 );
	length++;	//for terminating NULL character;

	if (length > buffer_size)
	{
		buffer_size = 2 * length;
		free (edit_ctrl_text);
		edit_ctrl_text = (unsigned char *) malloc (buffer_size);

		if (edit_ctrl_text == NULL)
		{
			ErrorMessage( "Cannot increase size of the text buffer.\nThe Console Plugin will close now." );
			strcpy (target, "exit");
			return;
		}
	}

	GetWindowText( hEdit, edit_ctrl_text, length );

	length -= cmd_line_start;
	if (length > 532)
		length = 532;

	memcpy (target, &(edit_ctrl_text[cmd_line_start]), length);
}

//------------------------------------------------------------------------------
void DeleteCmdLine( )
{
	LRESULT length;

	length = SendMessage( hEdit, WM_GETTEXTLENGTH, 0, 0);
	MoveCaretToEnd( );

	length -= cmd_line_start;
	while (length-- > 0)
	{
		WriteText (hEdit, "\b");
	}
}

//------------------------------------------------------------------------------
LRESULT OnKeyHome (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int current_position = GetCaretPositionByGetCaretPos( );
	SHORT ctrl_pressed = GetAsyncKeyState (VK_CONTROL) & 0x8000;
	SHORT shift_pressed = GetAsyncKeyState (VK_SHIFT) & 0x8000;

	if ( ctrl_pressed && shift_pressed )
	{
		int sel_start;
		int sel_end;

		SendMessage (hWnd, EM_GETSEL, (WPARAM) &sel_start, (LPARAM)&sel_end);
		if (sel_start == current_position)
		{
			SendMessage (hWnd, EM_SETSEL, (WPARAM) sel_end, (LPARAM) cmd_line_start);
		}
		else
			SendMessage (hWnd, EM_SETSEL, (WPARAM) sel_start, (LPARAM) cmd_line_start);
		return 0;
	}
	else if ( ctrl_pressed )
	{
		SendMessage (hWnd, EM_SETSEL, (WPARAM) cmd_line_start, (LPARAM) cmd_line_start);
		return 0;
	}
	else if ( shift_pressed)
	{
		POINT caret_pos;

		if (cmd_line_start == current_position)
			return 0;

		GetCaretPos (&caret_pos);
		if (caret_pos.y == HIWORD(SendMessage (hWnd, EM_POSFROMCHAR, (WPARAM) cmd_line_start, 0)))
		{
			int sel_start, sel_end;

			SendMessage (hWnd, EM_GETSEL, (WPARAM) &sel_start, (LPARAM)&sel_end);
			if (sel_start == current_position)
			{
				SendMessage (hWnd, EM_SETSEL, (WPARAM) sel_end, (LPARAM) cmd_line_start);
			}
			else
				SendMessage (hWnd, EM_SETSEL, (WPARAM) sel_start, (LPARAM) cmd_line_start);
			return 0;
		}
		else
			return CallWindowProc(OrigEditWndProc, hWnd, message, wParam, lParam);
	}
	else
	{
		POINT caret_pos;

		if (cmd_line_start == current_position)
			return 0;

		GetCaretPos (&caret_pos);
		if (caret_pos.y == HIWORD(SendMessage (hWnd, EM_POSFROMCHAR, (WPARAM) cmd_line_start, 0)))
		{
			SendMessage (hWnd, EM_SETSEL, (WPARAM) cmd_line_start, (LPARAM) cmd_line_start);
			return 0;
		}
		else
			return CallWindowProc(OrigEditWndProc, hWnd, message, wParam, lParam);
	}
}

//------------------------------------------------------------------------------
LRESULT OnKeyLeft(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int current_position = GetCaretPositionByGetCaretPos( );
	SHORT ctrl_pressed = GetAsyncKeyState (VK_CONTROL) & 0x8000;
	SHORT shift_pressed = GetAsyncKeyState (VK_SHIFT) & 0x8000;

	if ( ctrl_pressed && shift_pressed )
	{
		int sel_start;
		int sel_end;

		SendMessage (hWnd, EM_GETSEL, (WPARAM) &sel_start, (LPARAM)&sel_end);
		if (sel_start == current_position)
		{
			SendMessage (hWnd, EM_SETSEL, (WPARAM) sel_end, (LPARAM) cmd_line_start);
		}
		else
			SendMessage (hWnd, EM_SETSEL, (WPARAM) sel_start, (LPARAM) cmd_line_start);
		return 0;
	}
	else if ( ctrl_pressed )
	{
		SendMessage (hWnd, EM_SETSEL, (WPARAM) cmd_line_start, (LPARAM) cmd_line_start);
		return 0;
	}
	else if ( cmd_line_start == current_position )
	{
		return 0;
	}
	else
	{
		return CallWindowProc(OrigEditWndProc, hWnd, message, wParam, lParam);
	}
}

//------------------------------------------------------------------------------
LRESULT	OnKeyUp (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	unsigned char * p = GetPreviousCommand();

	if (p != NULL)
	{
		awaiting_input = FALSE;

		DeleteCmdLine( );
		WriteText(hWnd, p);

		awaiting_input = TRUE;
	}
	return 0;
}

//------------------------------------------------------------------------------
LRESULT	OnKeyDown (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	unsigned char * p = GetNextCommand();

	if (p != NULL)
	{
		awaiting_input = FALSE;

		DeleteCmdLine( );
		WriteText (hWnd, p);

		awaiting_input = TRUE;
	}
	return 0;
}

//------------------------------------------------------------------------------
LRESULT OnKeyEnter(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	unsigned char cmd_line [MAX_CMD_LENGTH + 1];	// command line
	unsigned char cmd_result [MAX_CMD_LENGTH + 1];	// result of evaluation from PowerPro
	unsigned char tmp_cmd_line [MAX_CMD_LENGTH + 1];	// stores command line for temporary use
	unsigned char variable_name [MAX_VAR_LENGTH + 1];	// stores names of variables
	unsigned char * p = NULL;
	awaiting_input = FALSE;

	MoveCaretToEnd( );
	GetCmdLine( cmd_line );
	WriteText (hWnd, "\n");

	AddCommandToList (cmd_line);

	strcpy (tmp_cmd_line, cmd_line);
	p = strtok (tmp_cmd_line, " ");
	if (p != NULL)
	{
		if (_stricmp (p, "set") == 0)
		{
			p = strtok (NULL, " ");
			if (p != NULL)
			{
				if (strlen(p) <= MAX_VAR_LENGTH)
				{
					strcpy (variable_name, p);
					p = strtok (NULL, "\0");
					if (p != NULL)
					{
						PProServices->SetVar(variable_name, p);
						WriteText (hWnd, "Setting variable:\n");
						WriteText (hWnd, variable_name);
						WriteText (hWnd, " = ");
						WriteText (hWnd, p);
						WriteText (hWnd, "\n");
					}
					else
					{
						PProServices->SetVar(variable_name, "");
						WriteText (hWnd, "Setting variable:\n");
						WriteText (hWnd, variable_name);
						WriteText (hWnd, " =\n");
					}
				}
				else
				{
					WriteText (hWnd, "Error: Variable name cannot be longer than 63 characters.\n");
				}
			}
			else
			{
				WriteText (hWnd, "Error: 'Set' needs at least one argument.\n");
			}
		}
		else if (_stricmp (p, "woff") == 0)
		{
			DisableWatches ();
			WriteText (hWnd, "Watches are disabled.\n");
		}
		else if (_stricmp (p, "won") == 0)
		{
			p = strtok (NULL, " ");
			if (p != NULL)
			{
				int interval = (int) strtod (p, NULL);
				if (interval > 0)
				{
					WriteText (hWnd, "Watches are enabled. Interval: ");
					WriteText (hWnd, p);
					WriteText (hWnd, "\n");
					EnableWatches (interval);
				}
				else
				{
					WriteText (hWnd, "Error: Given interval is not valid.\n");
				}
			}
			else
			{
				WriteText (hWnd, "Watches are enabled. Using default interval: 3\n");
				EnableWatches (3);
			}
		}
		else if (_stricmp (p, "watch1") == 0)
		{
			p = strtok (NULL, "\0");
			SetWatch( 1, p );
		}
		else if (_stricmp (p, "watch2") == 0)
		{
			p = strtok (NULL, "\0");
			SetWatch( 2, p );
		}
		else if (_stricmp (p, "watch3") == 0)
		{
			p = strtok (NULL, "\0");
			SetWatch( 3, p );
		}
		else if (_stricmp (p, "watch4") == 0)
		{
			p = strtok (NULL, "\0");
			SetWatch( 4, p );
		}
		else if (_stricmp (p, "watch5") == 0)
		{
			p = strtok (NULL, "\0");
			SetWatch( 5, p );
		}
		else if (_stricmp (p, "flag") == 0)
		{
			unsigned char buffer [600];

			sprintf (buffer, "*Script %s", cmd_line);
			PProServices->RunCmd(buffer, "", "");
		}
		else if (_stricmp (p, "exit") == 0)
		{
			DestroyWindow(hMainWnd);
			UnregisterClass(wnd_class_name, instance);
			return 0;
		}
		else if (_stricmp (p, "get") == 0)
		{
			p = strtok (NULL, " ");
			if (p != NULL)
			{
				if (strlen(p) <= MAX_VAR_LENGTH)
				{
					if (IsKeyword(p))
					{
						WriteText (hWnd, "Error: 'Get' cannot return values of keywords.\n");
					}
					else
					{
						unsigned char * value = PProServices->GetVarAddr( p );
						if (value != NULL)
						{
							WriteText (hWnd, "Getting variable:\n");
							WriteText (hWnd, p);
							WriteText (hWnd, " = ");
							WriteText (hWnd, value);
							WriteText (hWnd, "\n");
						}
						else
						{
							WriteText (hWnd, "Error: No such variable.\n");
						}
					}
				}
				else
				{
					WriteText (hWnd, "Error: Variable name cannot be longer than 63 characters.\n");
				}
			}
			else
			{
				WriteText (hWnd, "Error: 'Get' needs one argument.\n");
			}
		}
		else if ( TRUE == IsConsoleService (p) )
		{
			WriteText (hWnd, "Error: This command is not supported.\n");
		}
		else
		{
			EnableWindow( hMainWnd, FALSE );
			if (PProServices->EvalExpr(cmd_line, cmd_result) == TRUE)
			{
				WriteText (hWnd, "Result: ");
				WriteText (hWnd, cmd_result);
				WriteText (hWnd, "\n");
			}
			else
			{
				WriteText (hWnd, "Result: Evaluation error!\n");
			}
			EnableWindow( hMainWnd, TRUE );
			SetForegroundWindow( hMainWnd );
		}
	}

	WritePrompt( );
	cmd_line_start = GetCaretPositionBySelection( );

	awaiting_input = TRUE;
	return 0;
}

//------------------------------------------------------------------------------
void SetWatch( int watch_no, unsigned char * value )
{
	unsigned char buffer [10];

	sprintf (buffer, "%d", watch_no);

	if (value != NULL)
	{
		if (IsKeyword (value))
		{
			WriteText( hEdit, "Error: Cannot have a watch for a keyword.\n" );
		}
		else
		{
			if (strlen (value) <= MAX_VAR_LENGTH)
			{
				strcpy (watches[watch_no - 1], value);
				WriteText( hEdit, "Watch " );
				WriteText( hEdit, buffer );
				WriteText( hEdit, " set to: " );
				WriteText( hEdit, watches[watch_no - 1] );
				WriteText( hEdit, "\n" );
			}
			else
			{
				WriteText( hEdit, "Error: Variable name cannot be longer than 63 characters.\n" );
			}
		}
	}
	else
	{
		strcpy (watches[watch_no - 1], "");
		WriteText( hEdit, "Watch " );
		WriteText( hEdit, buffer );
		WriteText( hEdit, " cleared.\n" );
	}
}

//------------------------------------------------------------------------------
void EnableWatches (int interval)
{
	watches_enabled = TRUE;
	SetWindowPos (hMainWnd, NULL, 0, 0, wnd_width, wnd_height + SPACE_BETWEEN_WNDS + WATCH_WND_HEIGHT, SWP_NOMOVE | SWP_NOZORDER);
	SendMessage (hMainWnd, WM_TIMER, IDT_TIMER, 0);
	SetTimer (hMainWnd, IDT_TIMER, interval * 1000, (TIMERPROC) NULL);
}

//------------------------------------------------------------------------------
void DisableWatches ()
{
	watches_enabled = FALSE;
	KillTimer (hMainWnd, IDT_TIMER);
	SetWindowPos (hMainWnd, NULL, 0, 0, wnd_width, wnd_height, SWP_NOMOVE | SWP_NOZORDER);
}

//------------------------------------------------------------------------------
BOOL IsConsoleService (const unsigned char * text)
{
	if (strlen (text) >= strlen ("console.execute") )
		if  ( 0 == _memicmp (text, "console.execute", strlen("console.execute")) )
			return TRUE;
	if (strlen (text) >= strlen ("console.print") )
		if  ( 0 == _memicmp (text, "console.print", strlen("console.print")) )
			return TRUE;
	if (strlen (text) >= strlen ("console.append") )
		if  ( 0 == _memicmp (text, "console.append", strlen("console.append")) )
			return TRUE;
	if (strlen (text) >= strlen ("console.show") )
		if  ( 0 == _memicmp (text, "console.show", strlen("console.show")) )
			return TRUE;
	if (strlen (text) >= strlen ("console.close") )
		if  ( 0 == _memicmp (text, "console.close", strlen("console.close")) )
			return TRUE;
	if (strlen (text) >= strlen ("console.unload") )
		if  ( 0 == _memicmp (text, "console.unload", strlen("console.unload")) )
			return TRUE;

	return FALSE;
}

//------------------------------------------------------------------------------
void FreeResources ()
{
	int i;

	if (hFont != NULL)
	{
		DeleteObject (hFont);
		hFont = NULL;
	}
	if (title_bar_brush != NULL)
	{
		DeleteObject (title_bar_brush);
		title_bar_brush = NULL;
	}
	if (hbClose != NULL)
	{
		DeleteObject (hbClose);
		hbClose = NULL;
	}
	if (hbMinimize != NULL)
	{
		DeleteObject (hbMinimize);
		hbMinimize = NULL;
	}
	if (hbTitleBar != NULL)
	{
		DeleteObject (hbTitleBar);
		hbTitleBar = NULL;
	}

	if (prompt != NULL)
	{
		free (prompt);
		prompt = NULL;
	}
	if (font_name != NULL)
	{
		free (font_name);
		font_name = NULL;
	}
	if (edit_ctrl_text != NULL)
	{
		free (edit_ctrl_text);
		edit_ctrl_text = NULL;
	}

	if (previous_commands != NULL)
	{
		for (i = 0; i < PREVIOUS_COMMANDS_COUNT; i++)
		{
			free(previous_commands[i]);
		}
		free (previous_commands);
		previous_commands = NULL;
	}

	if (watches != NULL)
	{
		for (i = 0; i < WATCHES_COUNT; i++)
		{
			free (watches[i]);
		}
		free (watches);
		watches = NULL;
	}
}

//------------------------------------------------------------------------------
void ErrorMessage( const unsigned char * message )
{
	MessageBox( NULL, message, "Console Plugin Error", MB_OK | MB_ICONERROR | MB_TASKMODAL	);
}

//------------------------------------------------------------------------------
BOOL SetDefaultSettings ()
{
	prompt = (unsigned char *) malloc (strlen (default_prompt) + 1);
	if (prompt == NULL)
		return FALSE;

	strcpy (prompt, default_prompt);

	font_name = (unsigned char *) malloc (strlen (default_font) + 1);
	if (font_name == NULL)
	{
		return FALSE;
	}
	strcpy (font_name, default_font);

	font_size = 9;
	font_bold = FALSE;

	wnd_height = 500;
	wnd_width = 580;
	wnd_pos_left = 50;
	wnd_pos_top = 50;
	copyright_note = TRUE;
	wnd_resizable = FALSE;

	return TRUE;
}

//------------------------------------------------------------------------------
void WriteCopyrightNote( )
{
	WriteText( hEdit , "\n          Console Plugin " CONSOLE_VERSION 
	                 "\n Copyright (C) 2003-2004 Artur Dorochowicz\n\n");	
}

//------------------------------------------------------------------------------
