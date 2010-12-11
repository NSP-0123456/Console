/*
  Console Plugin

  Copyright (C) 2003-2004 Artur Dorochowicz
  All Rights Reserved.
*/

%option noyywrap

%{
	#include <windows.h>
	#include "Parser.h"
%}

%%

^[ \t]*";".*$		;
[ \t\n]			;

\[[Ff][Oo][Nn][Tt][Ss][Ee][Tt][Tt][Ii][Nn][Gg][Ss]\]	{ return KW_FONTSETTINGS; }
\[[Aa][Pp][Pp][Ee][Aa][Rr][Aa][Nn][Cc][Ee]\]		{ return KW_APPEARANCE; }
[Cc][Oo][Pp][Yy][Rr][Ii][Gg][Hh][Tt][Nn][Oo][Tt][Ee]	{ return KW_COPYRIGHTNOTE; }
[Rr][Ee][Ss][Ii][Zz][Aa][Bb][Ll][Ee]	{ return KW_RESIZABLE; }
[Nn][Aa][Mm][Ee]		{ return KW_NAME; } 
[Ss][Ii][Zz][Ee]		{ return KW_SIZE; }
[Bb][Oo][Ll][Dd]		{ return KW_BOLD; }
[Pp][Rr][Oo][Mm][Pp][Tt]	{ return KW_PROMPT; } 
[Hh][Ee][Ii][Gg][Hh][Tt]	{ return KW_HEIGHT; }
[Ww][Ii][Dd][Tt][Hh]		{ return KW_WIDTH; }
[Tt][Oo][Pp]			{ return KW_TOP; }
[Ll][Ee][Ff][Tt]		{ return KW_LEFT; }

\".*\"		{ yyleng = ( yyleng - 2 > MAX_LENGTH - 1 ) ? MAX_LENGTH - 1 : yyleng - 2;
			  memcpy( yylval.s, &yytext[1], yyleng );
			  yylval.s[ yyleng ] = 0;			  
			  return IDENTIFIER; }

[0-9]+		{ yylval.i = atoi( yytext );
			  return NUMBER; }

.			{ return yytext[0]; }

%%
