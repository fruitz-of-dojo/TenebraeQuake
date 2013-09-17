
%option noyy_scan_string
%option 8bit

%{

/*
Copyright (C) 2001-2002 Charles Hollemeersch

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

PENTA: the whole file is freakin penta...

This file defines the lexical analzer for the tenebrae shader/particle/decail scripts...
Basic lexing stuff returning tokens for keywords and storing semantics in global vars...


*/

#include <stdio.h>
#include "te_scripts.h"

#define MAX_STR_CONST 512

/* Semantics are stored in here */

static char string_buf[MAX_STR_CONST];
static char *string_buf_ptr, *string_end_buf;
static float float_value;

int line_num; /* current line number */

float Q_atof (char *str);

%}

%x str comment comment2 foo

DIGIT    [0-9]
ID       [a-z][a-z0-9_]*

%%

        
  /***************************************************************************
  Parse C-like strings, this means including stuff like escape sequences....
  ****************************************************************************/

\"      { BEGIN(str); string_buf_ptr = string_buf; string_end_buf = string_buf_ptr+MAX_STR_CONST; }

<str>\"        { /* saw closing quote - all done */
        BEGIN(INITIAL);
        *string_buf_ptr = '\0';
        /* return string constant token type and
         * value to parser
         */
         return TOK_STR_CONST;
        }

<str>\n        {
        /* error - unterminated string constant */
        /* generate error message */
		Con_Printf("\002Parse error at line %i: Newline in constant string", line_num);
        line_num++; /*for error messages*/
		return TOK_STR_CONST; //more of a mock up this will generate lots of errors probably
        }

<str>\\[0-7]{1,3} {
        /* octal escape sequence */
        int result;

        (void) sscanf( yytext + 1, "%o", &result );

        if ( result > 0xff )
                /* error, constant is out-of-bounds */

	if (string_buf_ptr < string_end_buf)
        	*string_buf_ptr++ = result;
        }

<str>\\[0-9]+ {
        /* generate error - bad escape sequence; something
         * like '\48' or '\0777777'
         */
        }

<str>\\n  *string_buf_ptr++ = '\n';
<str>\\t  *string_buf_ptr++ = '\t';
<str>\\r  *string_buf_ptr++ = '\r';
<str>\\b  *string_buf_ptr++ = '\b';
<str>\\f  *string_buf_ptr++ = '\f';

<str>\\(.|\n)  {
		if (string_buf_ptr < string_end_buf)
			*string_buf_ptr++ = yytext[1];
		}
		

<str>[^\\\n\"]+        {
        char *yptr = yytext;

        while ( (*yptr) && (string_buf_ptr < string_end_buf) )
                *string_buf_ptr++ = *yptr++;
        }
        
  /****************************************************************************
  Skip the / *  * / comments
  *****************************************************************************/
        
"/*"         BEGIN(comment);

<comment>[^*\n]*        /* eat anything that's not a '*' */
<comment>"*"+[^*/\n]*   /* eat up '*'s not followed by '/'s */
<comment>\n		{ line_num++; /*for error messages*/ }
<comment>"*"+"/"        BEGIN(INITIAL);

  /****************************************************************************
   Skip the //
  *****************************************************************************/

"//"         BEGIN(comment2);
<comment2>\n        { BEGIN(INITIAL); line_num++; } /* end of comment */
<comment2>.	/*eat up the rest */

  /****************************************************************************
  Parse the numeric constants
  *****************************************************************************/

-?{DIGIT}+       {
		float_value = Q_atof(yytext);
		//Con_Printf("tok: =%s=%f=\n",yytext,float_value);
		return TOK_FLOAT_CONST;
            }

-?{DIGIT}+"."{DIGIT}*        {
		float_value = Q_atof(yytext);
		//Con_Printf("tok: =%s=%f=\n",yytext,float_value);
		return TOK_FLOAT_CONST;
            }
            
emitter	{ return TOK_EMITTER; }
velocity	{  return TOK_VELOCITY; }
lifetime	{  return TOK_LIFETIME; }
flags		{  return TOK_FLAGS; }
gravity	{  return TOK_GRAVITY; }
drag	{  return TOK_DRAG; }
blendfunc	{  return TOK_BLENDFUNC; }
bounces	{  return TOK_BOUNCES; }
map	{  return TOK_MAP; }
particle	{  return TOK_PARTICLE; }
decal	{  return TOK_DECAL; }
startcolor		{  return TOK_STARTCOLOR; }
endcolor		{  return TOK_ENDCOLOR; }
grow			{  return TOK_GROW; }
size			{  return TOK_SIZE; }
rotation		{  return TOK_ROTATION; }
orientation		{  return TOK_ORIENTATION; }
onhit			{  return TOK_ONHIT; }

{ID}        {
		strcpy(string_buf,yytext);
		return TOK_ID;
	    }
	    
<<EOF>>  {  BEGIN(INITIAL); return TOK_FILE_END; }
\000     {  BEGIN(INITIAL); return TOK_FILE_END; }

[ \t\x0D] 	 { /* eat whitespace */ }

[\n]	 { line_num++; /*for error messages*/ }
	    
.	    {
		strcpy(string_buf,yytext);
		//return a 1 karakter token... (e.g. },{,],...)
		return yytext[0];
	    }
	    	  	    
%%

int yywrap(void) {
  return 1;
}

/*

	Engine communication with parser

*/

char *SC_ParseString() {
	int token = yylex();
	if (token == TOK_STR_CONST) {
		return string_buf;
	} else {
		Con_Printf("\002Parse error at line %i: Expected string constant (found id%i)\n", line_num, token);
		return string_buf;
	}
}

char *SC_ParseIdent() {
	int token = yylex();
	if (token == TOK_ID) {
		return string_buf;
	} else {
		Con_Printf("\002Parse error at line %i: Expected identifier (found id%i)\n", line_num, token);
		return string_buf;
	}
}

float SC_ParseFloat() {
	int token = yylex();
	if (token == TOK_FLOAT_CONST) {
		return float_value;
	} else {
		Con_Printf("\002Parse error at line %i: Expected float constant (found id%i)\n", line_num, token);
		return float_value;
	}
}

int SC_ParseToken() {
	return yylex();
}

static void *buffHandle = NULL;

/*
Parse from the given block of bytes
*/
void SC_Start(const char *bytes,int len) {
	line_num = 1;
	strcpy(string_buf,"newfile");
	
	buffHandle = yy_scan_bytes(bytes,len);
	
	//Con_Printf("new file\n");
	/*
	if (YY_CURRENT_BUFFER)
		yy_delete_buffer( YY_CURRENT_BUFFER );
    yy_switch_to_buffer(yy_create_buffer( yyin, YY_BUF_SIZE));

	yyrestart(fin);
	*/
}

void SC_End() {
	yy_delete_buffer( buffHandle );	
	buffHandle = NULL;
}