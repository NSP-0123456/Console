/*
  Console Plugin   version 1.0.1

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
#define LAST_COMMANDS_COUNT	11
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
static HWND 	g_hwndPowerPro;		// PowerPro hidden control window handle
HINSTANCE 	hInst;			// current instance
PPROSERVICES * PProServices;
HWND	hMainWnd;			// main window handle
HWND	hEdit; 				// edit control window handle
HWND	hWatch;				// watch window handle
HWND	hExitBtn;			// exit button handle
HWND	hMinimizeBtn;			// minimize button handle
HFONT	hFont = NULL;			// Courier New font handle
HBITMAP hbClose = NULL;			// handle of bitmap on close button
HBITMAP hbMinimize = NULL;		// handle of bitmap on minimize button
int	linenum = 0;			// stores line number of currently shown line
int	linenumfornext = 0;		// stores line number for saving next line
int	position = 0;			// length of current command line
int	interval = 0;			// interval in seconds between consecutive probing of variables in the watch list
BOOL	bReading = FALSE;		// true if waiting for user input
WNDPROC	OrigEditWndProc;		// original window procedure of edit control
PSTR	szTitle = "Console";		// The title bar text
PSTR	szWindowClass = "Console Plugin";	// the main window class name
unsigned char	prompt [] = "?>";		// prompt
unsigned char	* cmdline;			// command line
unsigned char	* cmdresult;			// result of evaluation from PowerPro
unsigned char	* variablename;			// stores names of variables
unsigned char	* cmdlinetemp;			// stores cmdline for temporary use
unsigned char	** watches;			// stores variables to watch
unsigned char	** lastcmdlines;		// stores previous command lines

//---DECLARATIONS---------------------------------------------------------------
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	EditWndProc(HWND, UINT, WPARAM, LPARAM);
void			WritePrompt (HWND);
void			WriteText (HWND, unsigned char *);
int			IncreaseLineNum (int number);
int			DecreaseLineNum (int number);
BOOL			AllocateMemory (void);
void			FreeMemory (void);
void			WriteWelcomeMessage (HWND);
BOOL			IsKeyword (unsigned char *);
unsigned char *		GetNextCommand (void);
unsigned char *		GetPreviousCommand (void);
void			AddCommandToList (unsigned char *);

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
	WNDCLASSEX wcex;
	HDC hdc;
	HWND oldhWnd;

	**szargs = '\0';
	PProServices = ppsv;

	oldhWnd = FindWindow(szWindowClass,NULL);
	if (oldhWnd) return;

        hbClose = LoadBitmap (hInst, MAKEINTRESOURCE(IDB_CLOSE));
	hbMinimize = LoadBitmap (hInst, MAKEINTRESOURCE(IDB_MINIMIZE));
	
        if (AllocateMemory() == FALSE)
        {
        	ppsv->ErrMessage("Error! Cannot allocate memory.", NULL);
        	FreeMemory();
        	return;
        }

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style		= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInst;
	wcex.hIcon		= LoadIcon(NULL, (LPCTSTR)IDI_APPLICATION);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH) (COLOR_BTNFACE + 1);
	wcex.lpszMenuName	= (LPCTSTR) NULL;
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(NULL, (LPCTSTR)IDI_APPLICATION);

	if (!RegisterClassEx(&wcex))
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
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_CREATE:
	        hExitBtn = CreateWindow ( "BUTTON", NULL, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_BITMAP | BS_FLAT, WND_WIDTH - 20, 2, 16, 16, hWnd, ( HMENU ) ID_EXITBTN, (HINSTANCE) GetWindowLong(hWnd, GWL_HINSTANCE), NULL );
	        SendMessage (hExitBtn, BM_SETIMAGE, (WPARAM) IMAGE_BITMAP, (LPARAM) (HANDLE) hbClose);

	        hMinimizeBtn = CreateWindow ( "BUTTON", NULL, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_BITMAP | BS_FLAT, WND_WIDTH - 37, 2, 16, 16, hWnd, ( HMENU ) ID_MINIMIZEBTN, (HINSTANCE) GetWindowLong(hWnd, GWL_HINSTANCE), NULL );
	        SendMessage (hMinimizeBtn, BM_SETIMAGE, (WPARAM) IMAGE_BITMAP, (LPARAM) (HANDLE) hbMinimize);

	        hEdit = CreateWindow("EDIT", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL| WS_BORDER | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                                     0, 0, 0, 0, hWnd, (HMENU) ID_EDITBOX, (HINSTANCE) GetWindowLong(hWnd, GWL_HINSTANCE), NULL);
                SendMessage (hEdit, WM_SETFONT, (WPARAM) hFont, MAKELPARAM(TRUE, 0));

                hWatch = CreateWindow ("EDIT", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                                       0, 0, 0, 0, hWnd, (HMENU) ID_WATCHBOX, (HINSTANCE) GetWindowLong (hWnd, GWL_HINSTANCE), NULL);
		SendMessage (hWatch, WM_SETFONT, (WPARAM) hFont, MAKELPARAM(TRUE, 0));

                WriteWelcomeMessage(hEdit);
                WritePrompt (hEdit);

                OrigEditWndProc = (WNDPROC) SetWindowLong (hEdit, GWL_WNDPROC, (LONG) EditWndProc);

                bReading = TRUE;
                
		SetForegroundWindow (hWnd);
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
		if (hFont) DeleteObject (hFont);
		if (hbClose) DeleteObject (hbClose);
		if (hbMinimize) DeleteObject (hbMinimize);
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
LRESULT CALLBACK	EditWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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
	case WM_KEYDOWN:
		switch (wParam)
		{
                case VK_DELETE:
	        	return 0;
	        case VK_UP:
	        	if (bReading == TRUE)
	        	{	
	        		unsigned char * p = GetPreviousCommand();
	        		
	        		if (p != NULL)
	        		{
		        		bReading = FALSE;
		        		while (position > 0)
	        			{
	        				SendMessage (hWnd, WM_CHAR, '\b', 1);
	        				position--;
		        		}     		
	        			WriteText(hWnd, p);
	        			position = strlen (p);
					ZeroMemory (cmdline, MAX_LENGTH + 1);
					strcpy (cmdline, p);
	        			bReading = TRUE;	        			
	        		}	        		
	        	}
	        	return 0;
	        case VK_DOWN:
	        	if (bReading == TRUE)
	        	{
	        		unsigned char * p = GetNextCommand();
	        		
	        		if (p != NULL)
	        		{
	        			bReading = FALSE;
	        			while (position > 0)
	        			{
	        				SendMessage (hWnd, WM_CHAR, '\b', 1);
	        				position--;
	        			}
	        			WriteText(hWnd, p);
	        			position = strlen (p);
	        			ZeroMemory (cmdline, MAX_LENGTH + 1);
	        			strcpy (cmdline, p);	        			
	        			bReading = TRUE;
	        		}
	        	}
	        	return 0;
	        case VK_LEFT:
	        	return 0;
	        case VK_RIGHT:
	        	return 0;
	        case VK_END:
	        	return 0;
	        case VK_HOME:
	        	return 0;
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
	        		LRESULT result = CallWindowProc(OrigEditWndProc, hWnd, message, wParam, lParam);
	        		bReading = FALSE;

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
	                                else if (strcmpi (p, "console.unload") == 0 || strcmpi (p, "console.unload()") == 0)
	                                {
	                                	WriteText (hWnd, "Error! This command is not supported.\n");
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
        	                        else if (PProServices->EvalExpr(cmdline, cmdresult) == TRUE)
	        			{
	        				WriteText (hWnd, "Result: ");
	        				WriteText (hWnd, cmdresult);
		        			WriteText (hWnd, "\n");
		        		}
	        			else
	        			{
		        			WriteText (hWnd, "Result: EvalExpr error!\n");
			        	}
			        }

	        		ZeroMemory (cmdline, MAX_LENGTH + 1);
	        		position = 0;
	        		WritePrompt(hWnd);
	        		bReading = TRUE;
	        		return result;
	        	}
	        	else
	        	{
	        		return CallWindowProc(OrigEditWndProc, hWnd, message, wParam, lParam);
	        	}
	        case '\t':
	        	if (bReading == TRUE)
	        	{
	        		return 0;
	        	}
	        	else
	        		return CallWindowProc(OrigEditWndProc, hWnd, message, wParam, lParam);
	        case '\b':
	        	if (bReading == TRUE)
	        	{
	        		if (position > 0)
	        		{
	        			position--;
	        			cmdline[position] = '\0';
	        			return CallWindowProc(OrigEditWndProc, hWnd, message, wParam, lParam);
	        		}
	        		else
	        			return 0;
	        	}
	        	else
	        		return CallWindowProc(OrigEditWndProc, hWnd, message, wParam, lParam);
	        default:
	        	if (bReading == TRUE)
	        	{                        	
                        	if (position < MAX_LENGTH)
                        	{
                        		cmdline[position] = wParam;
                        		position++;
                        	}
                        	else
                        		return 0;
                        }
	 		return CallWindowProc(OrigEditWndProc, hWnd, message, wParam, lParam);
		}
	case WM_PASTE:
		return 0;
	case WM_CUT:
		return 0;
	case WM_UNDO:
		return 0;	
	default:
	        return CallWindowProc(OrigEditWndProc, hWnd, message, wParam, lParam);
	}
	return 0;
}

//------------------------------------------------------------------------------
void    WriteText (HWND hwnd, unsigned char * text)
{
	int i;
        int length = strlen (text);

	for (i = 0; i < length; i++)
	{
		SendMessage (hwnd, WM_CHAR, text[i], 0x01000001);
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
	if (number == LAST_COMMANDS_COUNT - 1)
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
		return LAST_COMMANDS_COUNT - 1;
	}
	else return number - 1;
}

//------------------------------------------------------------------------------
BOOL	AllocateMemory()
{
	int i;

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

	lastcmdlines = (unsigned char **) calloc (LAST_COMMANDS_COUNT, sizeof(unsigned char *));
	if (lastcmdlines == NULL) return FALSE;
	for (i = 0; i < LAST_COMMANDS_COUNT; i++)
	{
		lastcmdlines[i] = (unsigned char *) calloc (MAX_LENGTH + 1, sizeof (unsigned char));
		if (lastcmdlines[i] == NULL) return FALSE;
	}
        return TRUE;
}

//------------------------------------------------------------------------------
void	FreeMemory()
{
	int i;

	free (cmdline);
	free (cmdresult);
	free (cmdlinetemp);
	free (variablename);

	for (i = 0; i < LAST_COMMANDS_COUNT; i++)
	{
		free(lastcmdlines[i]);
	}
	free (lastcmdlines);

	for (i = 0; i < WATCHES_COUNT; i++)
	{
		free (watches[i]);
	}
	free (watches);
}

//------------------------------------------------------------------------------
BOOL	IsKeyword (unsigned char * name)
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
	
	if (tmp == linenumfornext || strcmp(lastcmdlines[tmp], "") == 0)
    	{
		return NULL;
    	}
    	else
    	{
    		linenum = tmp;
    		return lastcmdlines [linenum];
    	}
}

//------------------------------------------------------------------------------
unsigned char *	GetPreviousCommand ()
{
	int tmp = DecreaseLineNum (linenum);

	if (tmp == linenumfornext || strcmp(lastcmdlines[tmp], "") == 0)
    	{			
		return NULL;
    	}
	else
	{
		linenum = tmp;
		return lastcmdlines[linenum];
	}

}

//------------------------------------------------------------------------------
void	AddCommandToList (unsigned char * cmd)
{
	if (strcmp (cmd, "") != 0)
	{
		strcpy (lastcmdlines[linenumfornext], cmd);
                linenumfornext = IncreaseLineNum (linenumfornext);
	}
        linenum = linenumfornext;
}

//------------------------------------------------------------------------------
void	WriteWelcomeMessage (HWND hwnd)
{
	WriteText(hwnd, "\n           Console Plugin 1.0.1\n   Copyright (C) 2003 Artur Dorochowicz\n\n");
}

//------------------------------------------------------------------------------
