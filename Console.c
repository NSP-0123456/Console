/*
  Console Plugin   version 1.0.2

  Copyright (C) 2003 Artur Dorochowicz
  All Rights Reserved.

  All used names are the property of their respective owners.
  PowerPro and plugin template are Copyright (C) by Bruce Switzer.
*/

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "Resources.h"

//---DEFINITIONS----------------------------------------------------------------
#define PREVIOUS_COMMANDS_COUNT	11
#define WATCHES_COUNT		5
#define MAX_VAR_LENGTH		63
#define MAX_LENGTH		531
#define WND_HEIGHT		500
#define WND_WIDTH		580
#define WND_WATCH_HEIGHT	150
#define ID_EDITBOX		200
#define ID_WATCHBOX		210
#define ID_EXITBTN		220
#define ID_MINIMIZEBTN		230
#define IDT_TIMER		300
#define ACTION_EXECUTE		1
#define ACTION_PRINT		2
#define ACTION_APPEND		3

//------------------------------------------------------------------------------
typedef struct tagPProServices
  {
   void (*ErrMessage)(LPSTR, LPSTR);
   BOOL (*MatchCaption)(HWND, LPSTR);
   HWND (*FindMatchingWindow)(LPSTR,BOOL);
   BOOL (*IsRolled)(HWND hw);
   BOOL (*IsTrayMinned)(HWND hw);
   void (*GetExeFullPath)(HWND hw, LPSTR szt);
   void (*RollUp)(HWND hw);
   void (*TrayMin)(HWND hw);
   void (*SendKeys)(LPSTR sz);
   BOOL  (*EvalExpr)(LPSTR sz, LPSTR szo);
   void  (*Debug)(LPSTR sz1, LPSTR sz2,LPSTR sz3, LPSTR sz4, LPSTR sz5, LPSTR sz6);
   LPSTR (*AllocTemp)(UINT leng);
   void (*ReturnString)(LPSTR sz, LPSTR* szargs);
   LPSTR (*GetVarAddr)(LPSTR var);
   LPSTR (*SetVar)(LPSTR var, LPSTR val);
   void (*IgnoreNextClip)();
   void (*Show)(HWND h);
   void (*RunCmd)(LPSTR szCmd, LPSTR szParam, LPSTR szWork);
   BOOL (*InsertStringForBar)( LPSTR szStr, LPSTR szCmd);
   void (*ResetFocus)();
  } PPROSERVICES;

//---GLOBAL VARIABLES-----------------------------------------------------------
const PSTR	szWindowClass = "Console Plugin 1.0.2";	// the main window class name
const PSTR	szTitle = "Console";		// The title bar text
const unsigned char	prompt [] = "?>";	// prompt

static HWND 	g_hwndPowerPro;		// PowerPro hidden control window handle
HINSTANCE 	hInst;			// current instance
PPROSERVICES * PProServices;
HWND	hMainWnd;			// main window handle
HWND	hEdit; 				// edit control window handle
HWND	hWatch;				// watch window handle
HWND	hExitBtn;			// exit button handle
HWND	hMinimizeBtn;			// minimize button handle
HFONT	hFont;				// Courier New font handle
HBITMAP hbClose;			// handle of bitmap on close button
HBITMAP hbMinimize;			// handle of bitmap on minimize button
BOOL	bReading;			// true if waiting for user input
WNDPROC	OrigEditWndProc;		// original window procedure of edit control
unsigned char * cmdline;		// command line
unsigned char * cmdresult;		// result of evaluation from PowerPro
unsigned char * variablename;		// stores names of variables
unsigned char * cmdlinetemp;		// stores cmdline for temporary use
unsigned char * editboxtext;		// buffer for edit box text
unsigned char ** watches;		// stores variables to watch
unsigned char ** previous_commands;	// stores previous command lines
int cmd_line_start;			// index of first character of current command line
int linenum;				// stores line number of currently shown line
int linenumfornext;			// stores line number for saving next line
int interval;				// interval in seconds between consecutive probing of variables in the watch list
int buffer_size;			// size of buffer storing contents of edit box

//---DECLARATIONS---------------------------------------------------------------
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	EditWndProc(HWND, UINT, WPARAM, LPARAM);
void			WritePrompt (HWND);
void			WriteText (HWND, const unsigned char *);
int			IncreaseLineNum (int number);
int			DecreaseLineNum (int number);
BOOL			AllocateMemory (void);
void			FreeMemory (void);
void			WriteWelcomeMessage (HWND);
BOOL			IsKeyword (const unsigned char *);
unsigned char *		GetNextCommand (void);
unsigned char *		GetPreviousCommand (void);
void			AddCommandToList (const unsigned char *);
int			GetPosition (HWND);
void			MoveCaretToEnd (HWND);
void			GetCmdLine (HWND, int, char * );
void			DeleteCmdLine (HWND, int);
void			MakeAction (int action, LPSTR szv, LPSTR szx, BOOL (*GetVar)(LPSTR, LPSTR), void (*SetVar)(LPSTR, LPSTR), DWORD * pFlags, UINT nargs, LPSTR * szargs, PPROSERVICES * ppsv);

//---DEFINITIONS----------------------------------------------------------------
BOOL WINAPI DllMain (HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpvReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		hInst = hinstDLL;
		g_hwndPowerPro = FindWindow("PowerProMain", NULL);
	}
	else
	{
		if (dwReason == DLL_PROCESS_DETACH)
		{
			HWND oldhWnd;

			oldhWnd = FindWindow(szWindowClass,NULL);
			if (oldhWnd)
				DestroyWindow(oldhWnd);
			UnregisterClass(szWindowClass, hInst);
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

	**szargs = '\0';
	PProServices = ppsv;

	oldhWnd = FindWindow(szWindowClass,NULL);
	if (oldhWnd != NULL) return;

	bReading = FALSE;
	linenum = 0;
	linenumfornext = 0;
	interval = 0;
	buffer_size = 100000;

        hbClose = LoadBitmap (hInst, MAKEINTRESOURCE(IDB_CLOSE));
	hbMinimize = LoadBitmap (hInst, MAKEINTRESOURCE(IDB_MINIMIZE));

        if (AllocateMemory() == FALSE)
        {
        	ppsv->ErrMessage("Error! Cannot allocate memory.", NULL);
        	FreeMemory();
        	return;
        }

	wc.style		= CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc		= (WNDPROC)WndProc;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= 0;
	wc.hInstance		= hInst;
	wc.hIcon		= LoadIcon (NULL, IDI_APPLICATION);
	wc.hCursor		= LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground	= CreateSolidBrush (RGB(236, 233, 216));
	wc.lpszMenuName		= NULL;
	wc.lpszClassName	= szWindowClass;

	if (!RegisterClass(&wc))
	{
		ppsv->ErrMessage("Error! RegisterClass() error.", NULL);
		FreeMemory();
		return;
	}

	hdc = GetDC(NULL);
	hFont = CreateFont (MulDiv(-9, GetDeviceCaps(hdc, LOGPIXELSY), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
				CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Courier New");
	ReleaseDC (NULL, hdc);
	if (!hFont)
	{
		ppsv->ErrMessage("Error! CreateFont() error.", NULL);
		FreeMemory();
		return;
	}

	hMainWnd = CreateWindow(szWindowClass, szTitle, WS_POPUPWINDOW, 50, 50, WND_WIDTH, WND_HEIGHT, NULL, NULL, hInst, NULL);
	if (!hMainWnd)
	{
		ppsv->ErrMessage("Error! CreateWindow() error.", NULL);
		FreeMemory();
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
	oldhWnd = FindWindow(szWindowClass, NULL);
	if (oldhWnd)
	{
		DestroyWindow(oldhWnd);
	}
	UnregisterClass(szWindowClass, hInst);
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

	**szargs = '\0';

	if (nargs != 1)
	{
		ppsv->ErrMessage ("The service needs exactly one argument", NULL);
		return;
	}

	oldhWnd = FindWindow (szWindowClass, NULL);
	if (oldhWnd == NULL)
		show (szv, szx, GetVar, SetVar, pFlags, nargs, szargs, ppsv);

	SetForegroundWindow (hMainWnd);

	switch (action)
	{
	case ACTION_EXECUTE:
		bReading = FALSE;
		DeleteCmdLine (hEdit, cmd_line_start);
		WriteText (hEdit, szargs[1]);
		bReading = TRUE;
		SendMessage (hEdit, WM_CHAR, '\r', 0);
		break;
	case ACTION_PRINT:
		bReading = FALSE;
		DeleteCmdLine (hEdit, cmd_line_start);
		WriteText (hEdit, szargs[1]);
		bReading = TRUE;
		break;
	case ACTION_APPEND:
		MoveCaretToEnd (hEdit);
		bReading = FALSE;
		WriteText (hEdit, szargs[1]);
		bReading = TRUE;
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
	        hExitBtn = CreateWindow ( "BUTTON", NULL, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_BITMAP | BS_FLAT, WND_WIDTH - 20, 2, 16, 16, hWnd, ( HMENU ) ID_EXITBTN, hInst, NULL );
	        SendMessage (hExitBtn, BM_SETIMAGE, (WPARAM) IMAGE_BITMAP, (LPARAM) (HANDLE) hbClose);

	        hMinimizeBtn = CreateWindow ( "BUTTON", NULL, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_BITMAP | BS_FLAT, WND_WIDTH - 37, 2, 16, 16, hWnd, ( HMENU ) ID_MINIMIZEBTN, hInst, NULL );
	        SendMessage (hMinimizeBtn, BM_SETIMAGE, (WPARAM) IMAGE_BITMAP, (LPARAM) (HANDLE) hbMinimize);

	        hEdit = CreateWindow("EDIT", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL| WS_BORDER | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                                     0, 0, 0, 0, hWnd, (HMENU) ID_EDITBOX, (HINSTANCE) GetWindowLong(hWnd, GWL_HINSTANCE), NULL);
                SendMessage (hEdit, WM_SETFONT, (WPARAM) hFont, MAKELPARAM(TRUE, 0));

                hWatch = CreateWindow ("EDIT", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                                       0, 0, 0, 0, hWnd, (HMENU) ID_WATCHBOX, hInst, NULL);
		SendMessage (hWatch, WM_SETFONT, (WPARAM) hFont, MAKELPARAM(TRUE, 0));

		WriteWelcomeMessage(hEdit);
                WritePrompt (hEdit);

		//cmd_line_start = GetPosition(hEdit);
		SendMessage (hEdit, EM_GETSEL, (WPARAM) &cmd_line_start, 0);

		OrigEditWndProc = (WNDPROC) SetWindowLong (hEdit, GWL_WNDPROC, (LONG) EditWndProc);
		SetForegroundWindow (hWnd);

		bReading = TRUE;
                break;
        case WM_SETFOCUS:
        	SetFocus (hEdit);
        	break;
        case WM_SIZE:
               	MoveWindow (hEdit, 0, 20, LOWORD(lParam), WND_HEIGHT - 20, TRUE);
               	MoveWindow (hWatch, 0, WND_HEIGHT + 20, LOWORD(lParam), WND_WATCH_HEIGHT, TRUE);
                break;
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		switch (wmId)
		{
		case ID_EXITBTN:
		        DestroyWindow(hWnd);
		        UnregisterClass(szWindowClass, hInst);
			break;
		case ID_MINIMIZEBTN:
			CloseWindow (hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		SelectObject(hdc, hFont);
		SetTextColor (hdc, RGB(0, 0, 0));
                SetBkColor (hdc, RGB(236, 233, 216));
                TextOut (hdc, 20, 2, "***   Console Plugin   ***   Console Plugin   ***   Console Plugin   ***", strlen ("***   Console Plugin   ***   Console Plugin   ***   Console Plugin   ***") );
                TextOut (hdc, 10, WND_HEIGHT + 2, "Watches:", strlen("Watches:"));
		EndPaint(hWnd, &ps);
		break;
	case WM_CLOSE:
		DestroyWindow (hWnd);
		UnregisterClass (szWindowClass, hInst);
		break;
	case WM_DESTROY:
	        SetWindowLong(hEdit, GWL_WNDPROC, (LONG) OrigEditWndProc);
		if (hFont != NULL) DeleteObject (hFont);
		if (hbClose != NULL) DeleteObject (hbClose);
		if (hbMinimize != NULL) DeleteObject (hbMinimize);
		KillTimer (hMainWnd, IDT_TIMER);
		FreeMemory();
		break;
	case WM_NCHITTEST:
	        {
		 LRESULT result;
	         RECT WndRect;

	         GetWindowRect (hWnd, &WndRect);
	         result = DefWindowProc(hWnd, message, wParam, lParam);
	         if (result == HTCLIENT && (HIWORD(lParam) - WndRect.top) < 20 && (LOWORD(lParam)-WndRect.left) < (WND_WIDTH - 37)) result = HTCAPTION;
	         return result;
	        }
	case WM_TIMER:
		switch (wParam)
		{
		case IDT_TIMER:
			{
			 unsigned char * value;
			 unsigned char num [20];
			 int i;

			 SendMessage (hWatch, WM_SETTEXT, 0, (LPARAM) NULL);
			 for (i = 0; i < WATCHES_COUNT; i++)
			 {
			 	sprintf (num, "%d", i + 1);
				WriteText (hWatch, num);
				WriteText (hWatch, ". ");
				if (strcmpi (watches[i], "") != 0)
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
			 	PProServices->EvalExpr (num, cmdresult);
			 	sprintf (num, "%2d(%c) ", i, cmdresult[0]);
			 	WriteText (hWatch, num);
			 }
			 return 0;
			}
		}
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
	        {
        		int current_position = GetPosition (hWnd);
        		SHORT ctrl_pressed = GetAsyncKeyState (VK_CONTROL) & 0x8000;
			SHORT shift_pressed = GetAsyncKeyState (VK_SHIFT) & 0x8000;

			if ( ctrl_pressed && shift_pressed )
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
		case VK_LEFT:
        	{
        		int current_position = GetPosition (hWnd);
        		SHORT ctrl_pressed = GetAsyncKeyState (VK_CONTROL) & 0x8000;
			SHORT shift_pressed = GetAsyncKeyState (VK_SHIFT) & 0x8000;

			if ( ctrl_pressed && shift_pressed )
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
	        case VK_UP:
		{
			unsigned char * p = GetPreviousCommand();

	        	if (p != NULL)
	        	{
		        	bReading = FALSE;

				DeleteCmdLine (hWnd, cmd_line_start);
        			WriteText(hWnd, p);

				bReading = TRUE;
        		}
			return 0;
        	}
	        case VK_DOWN:
		{
       			unsigned char * p = GetNextCommand();

        		if (p != NULL)
		       	{
		       		bReading = FALSE;

		       		DeleteCmdLine (hWnd, cmd_line_start);
				WriteText (hWnd, p);

				bReading = TRUE;
	    		}
			return 0;
        	}
		default:
			return CallWindowProc(OrigEditWndProc, hWnd, message, wParam, lParam);
		}
	case WM_CHAR:
		switch (wParam)
		{
		case '\r':
			if (bReading == TRUE)
	        	{
				unsigned char * p = NULL;
				bReading = FALSE;

				MoveCaretToEnd (hWnd);
				GetCmdLine (hWnd, cmd_line_start, cmdline);
				WriteText (hWnd, "\n");

				AddCommandToList (cmdline);

				strcpy (cmdlinetemp, cmdline);
				p = strtok (cmdlinetemp, " ");
				if (p != NULL)
				{
                                	if (strcmpi (p, "set") == 0)
                                	{
                                		p = strtok (NULL, " ");
                                		if (p != NULL)
                                		{
	                                		if (strlen(p) <= MAX_VAR_LENGTH)
	                                		{
		                                		strcpy (variablename, p);
		                                		p = strtok (NULL, "\0");
	        	                        		if (p != NULL)
	                	                		{
		        						PProServices->SetVar(variablename, p);
	                                				WriteText (hWnd, "Setting variable:\n");
	        							WriteText (hWnd, variablename);
	        							WriteText (hWnd, " = ");
		        						WriteText (hWnd, p);
			        					WriteText (hWnd, "\n");
			        				}
		        					else
		        					{
		        						PProServices->SetVar(variablename, "");
	                                				WriteText (hWnd, "Setting variable:\n");
	        							WriteText (hWnd, variablename);
	        							WriteText (hWnd, " =\n");
		        					}
		        				}
		        				else
		        				{
		        					WriteText (hWnd, "Error! Variable name cannot be longer than 63 characters.\n");
		        				}
		        			}
		        			else
		        			{
		        				WriteText (hWnd, "Error! 'Set' needs at least one argument.\n");
		        			}
                	                }
                	                else if (strcmpi (p, "woff") == 0)
                	                {
                	                	KillTimer (hMainWnd, IDT_TIMER);
                	                	SetWindowPos (hMainWnd, NULL, 0, 0, WND_WIDTH, WND_HEIGHT, SWP_NOMOVE | SWP_NOZORDER);
                	                	interval = 0;
                	                	WriteText (hWnd, "Watches are disabled.\n");
                	                }
                	                else if (strcmpi (p, "won") == 0)
                	                {
                	                	p = strtok (NULL, " ");
                                		if (p != NULL)
                                		{
	                                		interval = (int) strtod (p, NULL);
							if (interval > 0)
							{
								WriteText (hWnd, "Watches are enabled. Interval: ");
								WriteText (hWnd, p);
								WriteText (hWnd, "\n");
								SetWindowPos (hMainWnd, NULL, 0, 0, WND_WIDTH, WND_HEIGHT + 20 + WND_WATCH_HEIGHT, SWP_NOMOVE | SWP_NOZORDER);
								SendMessage (hMainWnd, WM_TIMER, IDT_TIMER, 0);
								SetTimer (hMainWnd, IDT_TIMER, interval * 1000, (TIMERPROC) NULL);
							}
							else
							{
								WriteText (hWnd, "Error! Given interval is not valid.\n");
							}
		        			}
		        			else
		        			{
		        				interval = 3;
		        				WriteText (hWnd, "Watches are enabled. Using default interval: 3\n");
							SetWindowPos (hMainWnd, NULL, 0, 0, WND_WIDTH, WND_HEIGHT + 20 + WND_WATCH_HEIGHT, SWP_NOMOVE | SWP_NOZORDER);
							SendMessage (hMainWnd, WM_TIMER, IDT_TIMER, 0);
							SetTimer (hMainWnd, IDT_TIMER, interval * 1000, (TIMERPROC) NULL);
		        			}
                	                }
                	                else if (strcmpi (p, "watch1") == 0)
                	                {
                	                	p = strtok (NULL, "\0");
                                		if (p != NULL)
                                		{
	                                		if (IsKeyword(p))
	                                		{
	                                			WriteText (hWnd, "Error! Cannot have a watch for a keyword.\n");
	                                		}
	                                		else
	                                		{
		                                		if (strlen(p) <= MAX_VAR_LENGTH)
		                                		{
	        	                        			strcpy (watches[0], p);
	                	                			WriteText (hWnd, "Watch 1 set to: ");
	                        	        			WriteText (hWnd, watches[0]);
	                                				WriteText (hWnd, "\n");
		        					}
		        					else
			        				{
			        					WriteText (hWnd, "Error! Variable name cannot be longer than 63 characters.\n");
			        				}
			        			}
		        			}
		        			else
		        			{
		        				strcpy (watches[0], "");
	                                		WriteText (hWnd, "Watch 1 cleared.\n");
		        			}
                	                }
                	                else if (strcmpi (p, "watch2") == 0)
                	                {
                	                	p = strtok (NULL, "\0");
                                		if (p != NULL)
                                		{
	                                		if (IsKeyword(p))
	                                		{
	                                			WriteText (hWnd, "Error! Cannot have a watch for a keyword.\n");
	                                		}
	                                		else
	                                		{
		                                		if (strlen(p) <= MAX_VAR_LENGTH)
		                                		{
	        	                        			strcpy (watches[1], p);
	                	                			WriteText (hWnd, "Watch 2 set to: ");
	                        	        			WriteText (hWnd, watches[1]);
	                                				WriteText (hWnd, "\n");
		        					}
		        					else
			        				{
			        					WriteText (hWnd, "Error! Variable name cannot be longer than 63 characters.\n");
			        				}
			        			}
		        			}
		        			else
		        			{
		        				strcpy (watches[1], "");
	                                		WriteText (hWnd, "Watch 2 cleared.\n");
		        			}
                	                }
                	                else if (strcmpi (p, "watch3") == 0)
                	                {
                	                	p = strtok (NULL, "\0");
                                		if (p != NULL)
                                		{
	                                		if (IsKeyword(p))
	                                		{
	                                			WriteText (hWnd, "Error! Cannot have a watch for a keyword.\n");
	                                		}
	                                		else
	                                		{
		                                		if (strlen(p) <= MAX_VAR_LENGTH)
		                                		{
	        	                        			strcpy (watches[2], p);
	                	                			WriteText (hWnd, "Watch 3 set to: ");
	                        	        			WriteText (hWnd, watches[2]);
	                                				WriteText (hWnd, "\n");
		        					}
		        					else
		        					{
		        						WriteText (hWnd, "Error! Variable name cannot be longer than 63 characters.\n");
		        					}
		        				}
		        			}
		        			else
		        			{
		        				strcpy (watches[2], "");
	                                		WriteText (hWnd, "Watch 3 cleared.\n");
		        			}
                	                }
                	                else if (strcmpi (p, "watch4") == 0)
                	                {
                	                	p = strtok (NULL, "\0");
                                		if (p != NULL)
                                		{
	                                		if (IsKeyword(p))
	                                		{
	                                			WriteText (hWnd, "Error! Cannot have a watch for a keyword.\n");
	                                		}
	                                		else
	                                		{
		                                		if (strlen(p) <= MAX_VAR_LENGTH)
		                                		{
	        	                        			strcpy (watches[3], p);
	                	                			WriteText (hWnd, "Watch 4 set to: ");
	                        	        			WriteText (hWnd, watches[3]);
	                                				WriteText (hWnd, "\n");
		        					}
		        					else
			        				{
			        					WriteText (hWnd, "Error! Variable name cannot be longer than 63 characters.\n");
			        				}
			        			}
		        			}
		        			else
		        			{
		        				strcpy (watches[3], "");
	                                		WriteText (hWnd, "Watch 4 cleared.\n");
		        			}
                	                }
                	                else if (strcmpi (p, "watch5") == 0)
                	                {
                	                	p = strtok (NULL, "\0");
                                		if (p != NULL)
                                		{
	                                		if (IsKeyword(p))
	                                		{
	                                			WriteText (hWnd, "Error! Cannot have a watch for a keyword.\n");
	                                		}
	                                		else
	                                		{
	                                			if (strlen(p) <= MAX_VAR_LENGTH)
		                                		{
		                                			strcpy (watches[4], p);
	        	                        			WriteText (hWnd, "Watch 5 set to: ");
	                	                			WriteText (hWnd, watches[4]);
	                        	        			WriteText (hWnd, "\n");
		        					}
		        					else
		        					{
		        						WriteText (hWnd, "Error! Variable name cannot be longer than 63 characters.\n");
		        					}
		        				}
		        			}
		        			else
		        			{
		        				strcpy (watches[4], "");
	                                		WriteText (hWnd, "Watch 5 cleared.\n");
		        			}
                	                }
                	                else if (strcmpi (p, "flag") == 0)
                	                {
                	                	unsigned char buffer [2000];

                	                	sprintf (buffer, "*Script %s", cmdline);
                	                	PProServices->RunCmd(buffer, "", "");
                	                }
                        	        else if (strcmpi (p, "exit") == 0)
                                	{
                                		DestroyWindow(hMainWnd);
                                		UnregisterClass(szWindowClass, hInst);
	                                }
        	                        else if (strcmpi (p, "get") == 0)
                	                {
	        				p = strtok (NULL, " ");
                                		if (p != NULL)
                                		{
	                                		if (strlen(p) <= MAX_VAR_LENGTH)
	                                		{
	                                			if (IsKeyword(p))
	                                			{
	                                				WriteText (hWnd, "Error! 'Get' cannot return values of keywords.\n");
	                                			}
	                                			else
	                                			{
	                                				unsigned char * value = (PProServices->GetVarAddr(p));
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
			        						WriteText (hWnd, "Error! No such variable.\n");
		        						}
		        					}
		        				}
		        				else
		        				{
		        					WriteText (hWnd, "Error! Variable name cannot be longer than 63 characters.\n");
		        				}
		        			}
		        			else
		        			{
		        				WriteText (hWnd, "Error! 'Get' needs one argument.\n");
		        			}
	                                }
					else if (strcmpi (p, "console.close") == 0 || strcmpi (p, "console.close()") == 0
						|| strcmpi (p, "console.show") == 0 || strcmpi (p, "console.show()") == 0
						|| strcmpi (p, "console.unload") == 0 || strcmpi (p, "console.unload()") == 0)
	                                {
	                                	WriteText (hWnd, "Error! This command is not supported.\n");
	                                }
					else if ((strlen (p) >= strlen("console.execute") && _memicmp (p, "console.execute", strlen("console.execute")) == 0)
						|| (strlen (p) >= strlen("console.print") && _memicmp (p, "console.print", strlen("console.print")) == 0)
						|| (strlen (p) >= strlen("console.append") && _memicmp (p, "console.append", strlen("console.append")) == 0))
					{
						WriteText (hWnd, "Error! This command is not supported.\n");
					}
        	                        else if (PProServices->EvalExpr(cmdline, cmdresult) == TRUE)
	        			{
	        				WriteText (hWnd, "Result: ");
	        				WriteText (hWnd, cmdresult);
		        			WriteText (hWnd, "\n");
		        		}
	        			else
	        			{
		        			WriteText (hWnd, "Result: EvalExpr() error!\n");
			        	}
			        }

				WritePrompt(hWnd);
	        		cmd_line_start = GetPosition(hWnd);

				bReading = TRUE;
	        		return 0;
	        	}
	        	else
				return CallWindowProc(OrigEditWndProc, hWnd, message, wParam, lParam);
		case '\b':
			if (bReading == TRUE)
	        	{
	        		if (cmd_line_start == GetPosition (hWnd) )
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
	return 0;
}

//------------------------------------------------------------------------------
void    WriteText (HWND hwnd, const unsigned char * text)
{
	int i;
        int length = strlen (text);

	for (i = 0; i < length; i++)
	{
		SendMessage (hwnd, WM_CHAR, text[i], 0);
	}
}

//------------------------------------------------------------------------------
void	WritePrompt (HWND hwnd)
{
	WriteText (hwnd, prompt);
}

//------------------------------------------------------------------------------
int IncreaseLineNum (int number)
{
	if (number == PREVIOUS_COMMANDS_COUNT - 1)
	{
		return 0;
	}
	else return number + 1;
}

//------------------------------------------------------------------------------
int DecreaseLineNum (int number)
{
	if (number == 0)
	{
		return PREVIOUS_COMMANDS_COUNT - 1;
	}
	else return number - 1;
}

//------------------------------------------------------------------------------
BOOL	AllocateMemory()
{
	int i;

	editboxtext = (unsigned char *) calloc (buffer_size, sizeof (unsigned char));
	if (editboxtext == NULL) return FALSE;

	cmdline = (unsigned char *) calloc (MAX_LENGTH + 1, sizeof(unsigned char));
	if (cmdline == NULL) return FALSE;

	cmdresult = (unsigned char *) calloc (MAX_LENGTH + 1, sizeof(unsigned char));
	if (cmdresult == NULL) return FALSE;

	cmdlinetemp = (unsigned char *) calloc (MAX_LENGTH + 1, sizeof(unsigned char));
	if (cmdlinetemp == NULL) return FALSE;

	variablename = (unsigned char *) calloc (MAX_VAR_LENGTH + 1, sizeof(unsigned char));
	if (variablename == NULL) return FALSE;

	watches = (unsigned char **) calloc (WATCHES_COUNT, sizeof(unsigned char *));
	if (watches == NULL) return FALSE;
	for (i = 0; i < WATCHES_COUNT; i++)
	{
		watches[i] = (unsigned char *) calloc (MAX_VAR_LENGTH + 1, sizeof (unsigned char));
		if (watches[i] == NULL) return FALSE;
	}

	previous_commands = (unsigned char **) calloc (PREVIOUS_COMMANDS_COUNT, sizeof(unsigned char *));
	if (previous_commands == NULL) return FALSE;
	for (i = 0; i < PREVIOUS_COMMANDS_COUNT; i++)
	{
		previous_commands[i] = (unsigned char *) calloc (MAX_LENGTH + 1, sizeof (unsigned char));
		if (previous_commands[i] == NULL) return FALSE;
	}
        return TRUE;
}

//------------------------------------------------------------------------------
void	FreeMemory()
{
	int i;

	free (editboxtext);
	free (cmdline);
	free (cmdresult);
	free (cmdlinetemp);
	free (variablename);

	for (i = 0; i < PREVIOUS_COMMANDS_COUNT; i++)
	{
		free(previous_commands[i]);
	}
	free (previous_commands);

	for (i = 0; i < WATCHES_COUNT; i++)
	{
		free (watches[i]);
	}
	free (watches);
}

//------------------------------------------------------------------------------
BOOL	IsKeyword (const unsigned char * name)
{
	if ( strcmpi ("acdc", name) == 0 ) return TRUE;
	if ( strcmpi ("allglobals", name) == 0 ) return TRUE;
	if ( strcmpi ("alt", name) == 0 ) return TRUE;
	if ( strcmpi ("batterypercent", name) == 0 ) return TRUE;
	if ( strcmpi ("browserDomain", name) == 0 ) return TRUE;
	if ( strcmpi ("browserSubdomain", name) == 0 ) return TRUE;
	if ( strcmpi ("browserURL", name) == 0 ) return TRUE;
	if ( strcmpi ("caption", name) == 0 ) return TRUE;
	if ( strcmpi ("captionunder", name) == 0 ) return TRUE;
	if ( strcmpi ("cdcurtrack", name) == 0 ) return TRUE;
	if ( strcmpi ("cdlasttrack", name) == 0 ) return TRUE;
	if ( strcmpi ("clip", name) == 0 ) return TRUE;
	if ( strcmpi ("cliptrackon", name) == 0 ) return TRUE;
	if ( strcmpi ("cpu", name) == 0 ) return TRUE;
	if ( strcmpi ("ctrl", name) == 0 ) return TRUE;
	if ( strcmpi ("currentdir", name) == 0 ) return TRUE;
	if ( strcmpi ("date", name) == 0) return TRUE;
	if ( strcmpi ("dayofweek", name) == 0 ) return TRUE;
	if ( strcmpi ("dayofyear", name) == 0 ) return TRUE;
	if ( strcmpi ("defaultprinter", name) == 0 ) return TRUE;
	if ( strcmpi ("deskempty", name) == 0 ) return TRUE;
	if ( strcmpi ("deskname", name) == 0 ) return TRUE;
	if ( strcmpi ("desknum", name) == 0 ) return TRUE;
	if ( strcmpi ("dialupname", name) == 0 ) return TRUE;
	if ( strcmpi ("disk", name) == 0 ) return TRUE;
	if ( strcmpi ("dunidle", name) == 0 ) return TRUE;
	if ( strcmpi ("dunrate", name) == 0 ) return TRUE;
	if ( strcmpi ("exefilename", name) == 0 ) return TRUE;
	if ( strcmpi ("exefullpath", name) == 0 ) return TRUE;
	if ( strcmpi ("gdi", name) == 0 ) return TRUE;
	if ( strcmpi ("inputcolor", name) == 0 ) return TRUE;
	if ( strcmpi ("inputdate", name) == 0 ) return TRUE;
	if ( strcmpi ("inputdatetime", name) == 0 ) return TRUE;
	if ( strcmpi ("inputfolder", name) == 0 ) return TRUE;
	if ( strcmpi ("inputpath", name) == 0 ) return TRUE;
	if ( strcmpi ("inputpathmulti", name) == 0 ) return TRUE;
	if ( strcmpi ("inputtext", name) == 0 ) return TRUE;
	if ( strcmpi ("keylog", name) == 0 ) return TRUE;
	if ( strcmpi ("keylogfile", name) == 0 ) return TRUE;
	if ( strcmpi ("lastactivehandle", name) == 0 ) return TRUE;
	if ( strcmpi ("lastautorunhandle", name) == 0 ) return TRUE;
	if ( strcmpi ("lastclipname", name) == 0 ) return TRUE;
	if ( strcmpi ("lastclippath", name) == 0 ) return TRUE;
	if ( strcmpi ("lastidletime", name) == 0 ) return TRUE;
	if ( strcmpi ("longdate", name) == 0 ) return TRUE;
	if ( strcmpi ("modem", name) == 0 ) return TRUE;
	if ( strcmpi ("mouseleft", name) == 0 ) return TRUE;
	if ( strcmpi ("mousemiddle", name) == 0 ) return TRUE;
	if ( strcmpi ("mouseright", name) == 0 ) return TRUE;
	if ( strcmpi ("muted", name) == 0 ) return TRUE;
	if ( strcmpi ("paper", name) == 0 ) return TRUE;
	if ( strcmpi ("pmem", name) == 0 ) return TRUE;
	if ( strcmpi ("pprofolder", name) == 0 ) return TRUE;
	if ( strcmpi ("pproversion", name) == 0 ) return TRUE;
	if ( strcmpi ("processcount", name) == 0 ) return TRUE;
	if ( strcmpi ("recycleItems", name) == 0 ) return TRUE;
	if ( strcmpi ("recycleSize", name) == 0 ) return TRUE;
	if ( strcmpi ("saver", name) == 0 ) return TRUE;
	if ( strcmpi ("saveractive", name) == 0 ) return TRUE;
	if ( strcmpi ("saverenabled", name) == 0 ) return TRUE;
	if ( strcmpi ("savertime", name) == 0 ) return TRUE;
	if ( strcmpi ("scriptfolder", name) == 0 ) return TRUE;
	if ( strcmpi ("scriptname", name) == 0 ) return TRUE;
	if ( strcmpi ("scriptpath", name) == 0 ) return TRUE;
	if ( strcmpi ("shift", name) == 0 ) return TRUE;
	if ( strcmpi ("shortdate", name) == 0) return TRUE;
	if ( strcmpi ("Subbarname", name) == 0 ) return TRUE;
	if ( strcmpi ("threadcount", name) == 0 ) return TRUE;
	if ( strcmpi ("time", name) == 0 ) return TRUE;
	if ( strcmpi ("timesec", name) == 0 ) return TRUE;
	if ( strcmpi ("timezone", name) == 0 ) return TRUE;
	if ( strcmpi ("unixdate", name) == 0 ) return TRUE;
	if ( strcmpi ("uptime", name) == 0 ) return TRUE;
	if ( strcmpi ("user", name) == 0 ) return TRUE;
	if ( strcmpi ("volume", name) == 0 ) return TRUE;
	if ( strcmpi ("win", name) == 0 ) return TRUE;
	if ( strcmpi ("xmouse", name) == 0 ) return TRUE;
	if ( strcmpi ("xscreen", name) == 0 ) return TRUE;
	if ( strcmpi ("xtime", name) == 0 ) return TRUE;
	if ( strcmpi ("ymouse", name) == 0 ) return TRUE;
	if ( strcmpi ("yscreen", name) == 0 ) return TRUE;

	return FALSE;
}

//------------------------------------------------------------------------------
unsigned char *	GetNextCommand ()
{
	int tmp;
	if (linenum == linenumfornext)
	{
		return NULL;
	}

	tmp = IncreaseLineNum (linenum);

	if (tmp == linenumfornext || strcmp(previous_commands[tmp], "") == 0)
    	{
		return NULL;
    	}
    	else
    	{
    		linenum = tmp;
    		return previous_commands [linenum];
    	}
}

//------------------------------------------------------------------------------
unsigned char *	GetPreviousCommand ()
{
	int tmp = DecreaseLineNum (linenum);

	if (tmp == linenumfornext || strcmp(previous_commands[tmp], "") == 0)
    	{
		return NULL;
    	}
	else
	{
		linenum = tmp;
		return previous_commands[linenum];
	}
}

//------------------------------------------------------------------------------
void AddCommandToList (const unsigned char * cmd)
{
	if (strcmp (cmd, "") != 0)
	{
		strcpy (previous_commands[linenumfornext], cmd);
                linenumfornext = IncreaseLineNum (linenumfornext);
	}
        linenum = linenumfornext;
}

//------------------------------------------------------------------------------
int GetPosition (HWND hwnd)
{
	/*
	int sel_start;
	SendMessage (hwnd, EM_GETSEL, (WPARAM) &sel_start, 0);

	return sel_start;
	*/

	POINT point;

	GetCaretPos(&point);
	return LOWORD(SendMessage (hwnd, EM_CHARFROMPOS, 0, MAKELPARAM (point.x, point.y)));
}

//------------------------------------------------------------------------------
void MoveCaretToEnd (HWND hwnd)
{
	LRESULT length = SendMessage (hwnd, WM_GETTEXTLENGTH, 0, 0);
	SendMessage (hwnd, EM_SETSEL, length, length);
}

//------------------------------------------------------------------------------
void GetCmdLine (HWND hwnd, int start_position, char * target)
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

	length = SendMessage (hwnd, WM_GETTEXTLENGTH, 0, 0);
	length++;	//for terminating NULL character;

	if (length > buffer_size)
	{
		unsigned char * temp;

		temp = (unsigned char *) realloc (editboxtext, 	2 * length);
		buffer_size = 2 * length;

		if (temp == NULL)
		{
			PProServices->ErrMessage("Error! Cannot increase text buffer size.\nThe console will close now.", NULL);
			strcpy (target, "exit");
			return;
		}
		else
		{
			editboxtext = temp;
		}
	}

	GetWindowText (hwnd, editboxtext, length);

	length -= start_position;
	if (length > 532) length = 532;

	memcpy (target, &(editboxtext[start_position]), length);
}

//------------------------------------------------------------------------------
void DeleteCmdLine (HWND hwnd, int start_position)
{
	LRESULT length;

	length = SendMessage (hwnd, WM_GETTEXTLENGTH, 0, 0);
	MoveCaretToEnd (hwnd);

	length -= start_position;
	while (length-- > 0)
	{
		WriteText (hwnd, "\b");
	}
}

//------------------------------------------------------------------------------
void WriteWelcomeMessage (HWND hwnd)
{
	WriteText(hwnd, "\n           Console Plugin 1.0.2\n   Copyright (C) 2003 Artur Dorochowicz\n\n");
}

//------------------------------------------------------------------------------
