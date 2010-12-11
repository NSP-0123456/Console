/*
  Console Plugin

  Copyright (C) 2003-2004 Artur Dorochowicz
  All Rights Reserved.
*/

%{
	#include <windows.h>
	
	/* needed by Visual C++ */
	#ifdef _MSC_VER
		#include <malloc.h>
	#endif

	extern unsigned char * font_name;
	extern unsigned char * prompt;
	extern int font_size;
	extern BOOL font_bold;

	extern BOOL copyright_note;
	extern BOOL wnd_resizable;
	extern int wnd_height;
	extern int wnd_width;
	extern int wnd_pos_left;
	extern int wnd_pos_top;
%}

%union
{
	#define MAX_LENGTH 1000
	int i;
	char s [MAX_LENGTH];
}

%start INI_FILE

%token KW_FONTSETTINGS KW_APPEARANCE
%token NUMBER IDENTIFIER
%token KW_NAME KW_SIZE KW_BOLD
%token KW_PROMPT KW_COPYRIGHTNOTE KW_HEIGHT KW_WIDTH KW_RESIZABLE KW_LEFT KW_TOP

%%

INI_FILE
	: /* empty */
	| INI_FILE KW_FONTSETTINGS LIST_FONTSETTINGS
	| INI_FILE KW_APPEARANCE LIST_APPEARANCE
	;

LIST_FONTSETTINGS
	: SETTING_FONT
	| LIST_FONTSETTINGS SETTING_FONT
	;

SETTING_FONT
	: KW_NAME '=' IDENTIFIER
	{
		if (font_name != NULL)
		{
			free (font_name);
		}
		font_name = (unsigned char *) malloc (strlen (yylval.s) + 1);
		if (font_name == NULL)
		{
			/* this will unfortunately misinform user that file has wrong format */
			YYABORT;
		}
		strcpy (font_name, yylval.s);
	}
	| KW_SIZE '=' NUMBER
	{
		font_size = yylval.i;
	}
	| KW_BOLD '=' NUMBER
	{
		font_bold = yylval.i;
	}
	;

LIST_APPEARANCE
	: SETTING_APPEARANCE
	| LIST_APPEARANCE SETTING_APPEARANCE
	;

SETTING_APPEARANCE
	: KW_PROMPT '=' IDENTIFIER
	{
		if ( prompt != NULL )
		{
			free( prompt );
		}
		prompt = ( unsigned char * ) malloc( strlen( yylval.s ) + 1 );
		if ( prompt == NULL )
		{
			/* this will unfortunately misinform user that the file has wrong format */
			YYABORT;	
		}
		strcpy( prompt, yylval.s );
	}
	| KW_COPYRIGHTNOTE '=' NUMBER
	{
		copyright_note = yylval.i;
	}
	| KW_HEIGHT '=' NUMBER
	{
		wnd_height = yylval.i;
	}
	| KW_WIDTH '=' NUMBER
	{
		wnd_width = yylval.i;
	}
	| KW_RESIZABLE '=' NUMBER
	{
		wnd_resizable = yylval.i;
	}
	| KW_LEFT '=' NUMBER
	{
		wnd_pos_left = yylval.i;
	}
	| KW_TOP '=' NUMBER
	{
		wnd_pos_top = yylval.i;
	}
	;

%%

int yyerror ()
{
	return 0;
}
