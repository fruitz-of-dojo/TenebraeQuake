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

Header for interfacing with the lexer
*/
#include "stdio.h"

//Generic tokens
#define TOK_FILE_END 257
#define TOK_STR_CONST 258
#define TOK_FLOAT_CONST 259
#define TOK_ID 260

//Particle tokens
#define TOK_EMITTER 300
#define TOK_VELOCITY 301
#define TOK_LIFETIME 302
#define TOK_FLAGS 303
#define TOK_GRAVITY 304
#define TOK_DRAG 305
#define TOK_BLENDFUNC 306
#define TOK_BOUNCES 307
#define TOK_MAP 308
#define TOK_PARTICLE 307
#define TOK_DECAL 308
#define TOK_STARTCOLOR 309
#define TOK_ENDCOLOR 310
#define TOK_ROTATION 311
#define TOK_SIZE 312
#define TOK_GROW 313
#define TOK_ORIENTATION 314
#define TOK_ONHIT 315

void SC_Start(const char *bytes, int len);
void SC_End(void);
char *SC_ParseString();
float SC_ParseFloat();
char *SC_ParseIdent();
int	SC_ParseToken();

int SC_BlendModeForName(char *name);

extern int line_num;

#define PARSERERROR(x) Con_Printf("\002Script error at line %i: %s\n",line_num,x)


