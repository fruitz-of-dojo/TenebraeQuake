/*
Copyright (C) 1996-1997 Id Software, Inc.

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

*/

// draw.c -- this is the only file outside the refresh that touches the
// vid buffer

#include "quakedef.h"
#include <stdlib.h>

#define GL_COLOR_INDEX8_EXT     0x80E5

extern unsigned char d_15to8table[65536];

cvar_t		gl_nobind = {"gl_nobind", "0"};
cvar_t		gl_max_size = {"gl_max_size", "1024"};
cvar_t		gl_picmip = {"gl_picmip", "0"};
cvar_t          gl_gloss = {"gl_gloss", "0.3"};
cvar_t          gl_compress_textures = {"gl_compress_textures", "0"};
cvar_t          willi_gray_colormaps = {"willi_gray_colormaps", "0"};
cvar_t          con_clock = {"con_clock", "1", true};

byte		*draw_chars;				// 8*8 graphic characters
qpic_t		*draw_disc;
qpic_t		*draw_backtile;

int			translate_texture;
int			char_texture;
int			glow_texture_object;	//PENTA: gl texture object of the glow texture
int			normcube_texture_object; //PENTA: normalization cubemap
int			atten1d_texture_object;
int			atten2d_texture_object;
int			atten3d_texture_object;
int			halo_texture_object;


typedef struct
{
	int		texnum;
	float	sl, tl, sh, th;
} glpic_t;

byte		conback_buffer[sizeof(qpic_t) + sizeof(glpic_t)];
qpic_t		*conback = (qpic_t *)&conback_buffer;

int		gl_lightmap_format = 4;
int		gl_solid_format = 3;
int		gl_alpha_format = 4;

int		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int		gl_filter_max = GL_LINEAR;


int		texels;

typedef struct
{
	int		texnum;
	char	identifier[64];
	int		width, height;
	qboolean	mipmap;
} gltexture_t;

#define	MAX_GLTEXTURES	1024
gltexture_t	gltextures[MAX_GLTEXTURES];
int			numgltextures;

qboolean is_overriden;

// <AWE> added local prototypes.
int		GL_Load2DAttenTexture (void);
int		GL_Load3DAttenTexture (void);
int		GL_LoadBumpTexture (void);
int		GL_LoadNormalizationCubemap (void);
int		GL_LoadPicTexture (qpic_t *pic);

void GL_Bind (int texnum)
{
	if (gl_nobind.value)
		texnum = char_texture;
	if (currenttexture == texnum)
		return;
	currenttexture = texnum;
//#ifdef _WIN32
//	bindTexFunc (GL_TEXTURE_2D, texnum);
//#else
	glBindTexture(GL_TEXTURE_2D, texnum);
//#endif

#if defined(__APPLE__) || defined (MACOSX)

	if (gl_texturefilteranisotropic)	// <AWE> anisotropic texture filtering
	{
		glTexParameterfv (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &gl_textureanisotropylevel);
	}

#endif // __APPLE__ || MACOSX
}


/*
=============================================================================

  scrap allocation

  Allocate all the little status bar obejcts into a single texture
  to crutch up stupid hardware / drivers

=============================================================================
*/

#define	MAX_SCRAPS		2
#define	BLOCK_WIDTH		256
#define	BLOCK_HEIGHT	256

int			scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];
byte		scrap_texels[MAX_SCRAPS][BLOCK_WIDTH*BLOCK_HEIGHT*4];
qboolean	scrap_dirty;
int			scrap_texnum;

// returns a texture number and the position inside it
int Scrap_AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		texnum;

	for (texnum=0 ; texnum<MAX_SCRAPS ; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i=0 ; i<BLOCK_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (scrap_allocated[texnum][i+j] >= best)
					break;
				if (scrap_allocated[texnum][i+j] > best2)
					best2 = scrap_allocated[texnum][i+j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i=0 ; i<w ; i++)
			scrap_allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error ("Scrap_AllocBlock: full");
	return 0;
}

int	scrap_uploads;

void Scrap_Upload (void)
{
	int		texnum;

	scrap_uploads++;

	for (texnum=0 ; texnum<MAX_SCRAPS ; texnum++) {
		GL_Bind(scrap_texnum + texnum);
		GL_Upload8 (scrap_texels[texnum], "scrap", BLOCK_WIDTH, BLOCK_HEIGHT, false, true, false);
	}
	scrap_dirty = false;
}

//=============================================================================
/* Support Routines */

typedef struct cachepic_s
{
	char		name[MAX_QPATH];
	qpic_t		pic;
	byte		padding[32];	// for appended glpic
} cachepic_t;

#define	MAX_CACHED_PICS		128
cachepic_t	menu_cachepics[MAX_CACHED_PICS];
int			menu_numcachepics;

byte		menuplyr_pixels[4096];

int		pic_texels;
int		pic_count;

qpic_t *Draw_PicFromWad (char *name)
{
	qpic_t	*p;
	glpic_t	*gl;

	p = W_GetLumpName (name);
	gl = (glpic_t *)p->data;

	// load little ones into the scrap
	if (p->width < 64 && p->height < 64)
	{
		int		x, y;
		int		i, j, k;
		int		texnum;

		texnum = Scrap_AllocBlock (p->width, p->height, &x, &y);
		scrap_dirty = true;
		k = 0;
		for (i=0 ; i<p->height ; i++)
			for (j=0 ; j<p->width ; j++, k++)
				scrap_texels[texnum][(y+i)*BLOCK_WIDTH + x + j] = p->data[k];
		texnum += scrap_texnum;
		gl->texnum = texnum;
		gl->sl = (x+0.01)/(float)BLOCK_WIDTH;
		gl->sh = (x+p->width-0.01)/(float)BLOCK_WIDTH;
		gl->tl = (y+0.01)/(float)BLOCK_WIDTH;
		gl->th = (y+p->height-0.01)/(float)BLOCK_WIDTH;

		pic_count++;
		pic_texels += p->width*p->height;
	}
	else
	{
		gl->texnum = GL_LoadPicTexture (p);
		gl->sl = 0;
		gl->sh = 1;
		gl->tl = 0;
		gl->th = 1;
	}
	return p;
}


/*
================
Draw_CachePic
================
*/
qpic_t	*Draw_CachePic (char *path)
{
	cachepic_t	*pic;
	int			i;
	qpic_t		*dat;
	glpic_t		*gl;

	for (pic=menu_cachepics, i=0 ; i<menu_numcachepics ; pic++, i++)
		if (!strcmp (path, pic->name))
			return &pic->pic;

	if (menu_numcachepics == MAX_CACHED_PICS)
		Sys_Error ("menu_numcachepics == MAX_CACHED_PICS");
	menu_numcachepics++;
	strncpy (pic->name, path,sizeof(pic->name));

//
// load the pic from disk
//
	dat = (qpic_t *)COM_LoadTempFile (path);	
	if (!dat)
		Sys_Error ("Draw_CachePic: failed to load %s", path);
	SwapPic (dat);

	// HACK HACK HACK --- we need to keep the bytes for
	// the translatable player picture just for the menu
	// configuration dialog
	if (!strcmp (path, "gfx/menuplyr.lmp"))
		memcpy (menuplyr_pixels, dat->data, dat->width*dat->height);

	pic->pic.width = dat->width;
	pic->pic.height = dat->height;

	gl = (glpic_t *)pic->pic.data;
	gl->texnum = GL_LoadPicTexture (dat);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return &pic->pic;
}


void Draw_CharToConback (int num, byte *dest)
{
	int		row, col;
	byte	*source;
	int		drawline;
	int		x;

	row = num>>4;
	col = num&15;
	source = draw_chars + (row<<10) + (col<<3);

	drawline = 8;

	while (drawline--)
	{
		for (x=0 ; x<8 ; x++)
			if (source[x] != 255)
				dest[x] = 0x60 + source[x];
		source += 128;
		dest += 320;
	}

}

typedef struct
{
	char *name;
	int	minimize, maximize;
} glmode_t;

glmode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

/*
===============
Draw_TextureMode_f
===============
*/
void Draw_TextureMode_f (void)
{
	int		i;
	gltexture_t	*glt;

	if (Cmd_Argc() == 1)
	{
		for (i=0 ; i< 6 ; i++)
			if (gl_filter_min == modes[i].minimize)
			{
				Con_Printf ("%s\n", modes[i].name);
				return;
			}
		Con_Printf ("current filter is unknown???\n");
		return;
	}

	for (i=0 ; i< 6 ; i++)
	{
		if (!Q_strcasecmp (modes[i].name, Cmd_Argv(1) ) )
			break;
	}
	if (i == 6)
	{
		Con_Printf ("bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if (glt->mipmap)
		{
			GL_Bind (glt->texnum);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
			//PENTA: also update bump maps
			GL_Bind (glt->texnum+1);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
}

/*
===============
Draw_Init
===============
*/
void Draw_Init (void)
{
	int		i;
	qpic_t	*cb;
	byte	*dest /*, *src*/;	// <AWE> unused because of "#if 0".
	int		x, y;
	char	ver[40];
	glpic_t	*gl;
	int		start;
	byte	*ncdata;
//	int		fstep;		// <AWE> unused because of "#if 0".


	Cvar_RegisterVariable (&gl_nobind);
	Cvar_RegisterVariable (&gl_max_size);
	Cvar_RegisterVariable (&gl_picmip);
	Cvar_RegisterVariable (&gl_gloss);
	Cvar_RegisterVariable (&gl_compress_textures);
	Cvar_RegisterVariable (&willi_gray_colormaps);
	Cvar_RegisterVariable (&con_clock);

	// 3dfx can only handle 256 wide textures
	if (!Q_strncasecmp ((char *)gl_renderer, "3dfx",4) ||
		strstr((char *)gl_renderer, "Glide"))
		Cvar_Set ("gl_max_size", "256");

	Cmd_AddCommand ("gl_texturemode", &Draw_TextureMode_f);

	// load the console background and the charset
	// by hand, because we need to write the version
	// string into the background before turning
	// it into a texture
	draw_chars = W_GetLumpName ("conchars");
	for (i=0 ; i<256*64 ; i++)
		if (draw_chars[i] == 0)
			draw_chars[i] = 255;	// proper transparent color

	// now turn them into textures
	char_texture = GL_LoadTexture ("charset", 128, 128, draw_chars, false, true, false);

	start = Hunk_LowMark();

	cb = (qpic_t *)COM_LoadTempFile ("gfx/conback.lmp");	
	if (!cb)
		Sys_Error ("Couldn't load gfx/conback.lmp");
	SwapPic (cb);

	// hack the version number directly into the pic
#if defined(__linux__)
	sprintf (ver, "(Linux %2.2f, gl %4.2f) %4.2f", (float)LINUX_VERSION, (float)GLQUAKE_VERSION, (float)VERSION);
#elif defined (__APPLE__) || defined (MACOSX)
	sprintf (ver, "(MACOS X %2.2f, gl %4.2f) %4.2f", (float)MACOSX_VERSION, (float)GLQUAKE_VERSION, (float)VERSION);
#else
	sprintf (ver, "(st %4.2f) %4.2f", (float)STQUAKE_VERSION, (float)VERSION);
#endif
	dest = cb->data + 320*186 + 320 - 11 - 8*strlen(ver);
	y = (int) strlen(ver);
	for (x=0 ; x<y ; x++)
		Draw_CharToConback (ver[x], dest+(x<<3));

#if 0
	conback->width = vid.conwidth;
	conback->height = vid.conheight;

 	// scale console to vid size
 	dest = ncdata = Hunk_AllocName(vid.conwidth * vid.conheight, "conback");
 
 	for (y=0 ; y<vid.conheight ; y++, dest += vid.conwidth)
 	{
 		src = cb->data + cb->width * (y*cb->height/vid.conheight);
 		if (vid.conwidth == cb->width)
 			memcpy (dest, src, vid.conwidth);
 		else
 		{
 			f = 0;
 			fstep = cb->width*0x10000/vid.conwidth;
 			for (x=0 ; x<vid.conwidth ; x+=4)
 			{
 				dest[x] = src[f>>16];
 				f += fstep;
 				dest[x+1] = src[f>>16];
 				f += fstep;
 				dest[x+2] = src[f>>16];
 				f += fstep;
 				dest[x+3] = src[f>>16];
 				f += fstep;
 			}
 		}
 	}
#else
	conback->width = cb->width;
	conback->height = cb->height;
	ncdata = cb->data;
#endif

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	gl = (glpic_t *)conback->data;
	gl->texnum = GL_LoadTexture ("conback", conback->width, conback->height, ncdata, false, false, false);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;
	conback->width = vid.width;
	conback->height = vid.height;

	// free loaded console
	Hunk_FreeToLowMark(start);

	// save a texture slot for translated picture
	translate_texture = texture_extension_number++;

	// save slots for scraps
	scrap_texnum = texture_extension_number;
	texture_extension_number += MAX_SCRAPS;

	//
	// get the other pics we need
	//
	draw_disc = Draw_PicFromWad ("disc");
	draw_backtile = Draw_PicFromWad ("backtile");

	//PENTA: load fallof glow
	glow_texture_object = GL_Load2DAttenTexture();
	atten3d_texture_object = GL_Load3DAttenTexture();

	//Load nomalization cube map
	normcube_texture_object = GL_LoadNormalizationCubemap();

	//load diffuse vertex program
	R_LoadVertexProgram();

	halo_texture_object = EasyTgaLoad("penta/utflare5.tga");

	for (i=0; i<8; i++) {
		char name[32];
		sprintf(name,"penta/caust%02.2i.tga",i*4);
		caustics_textures[i] = EasyTgaLoad(name);
	}

	//Load water shader textures
	InitShaderTex();
	//load mirror dummys
	R_InitMirrorChains();

	R_InitGlare();
}



/*
================
Draw_Character

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void Draw_Character (int x, int y, int num)
{
	int		row, col;
	float		frow, fcol, size;

	if (num == 32)
		return;		// space

	num &= 255;
	
	if (y <= -8)
		return;			// totally off screen

	row = num>>4;
	col = num&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

	glEnable(GL_BLEND);
	GL_Bind (char_texture);

	glBegin (GL_QUADS);
	glTexCoord2f (fcol, frow);
	glVertex2f (x, y);
	glTexCoord2f (fcol + size, frow);
	glVertex2f (x+8, y);
	glTexCoord2f (fcol + size, frow + size);
	glVertex2f (x+8, y+8);
	glTexCoord2f (fcol, frow + size);
	glVertex2f (x, y+8);
	glEnd ();
	glDisable(GL_BLEND);
}

/*
================
Draw_String
================
*/
void Draw_String (int x, int y, char *str)
{
	while (*str)
	{
		Draw_Character (x, y, *str);
		str++;
		x += 8;
	}
}

/*
================
Draw_DebugChar

Draws a single character directly to the upper right corner of the screen.
This is for debugging lockups by drawing different chars in different parts
of the code.
================
*/
void Draw_DebugChar (char num)
{
}

/*
=============
Draw_AlphaPic
=============
*/
void Draw_AlphaPic (int x, int y, qpic_t *pic, float alpha)
{
	glpic_t			*gl;

	if (scrap_dirty)
		Scrap_Upload ();
	gl = (glpic_t *)pic->data;
	glDisable(GL_ALPHA_TEST);
	glEnable (GL_BLEND);
//	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//	glCullFace(GL_FRONT);
	glColor4f (1,1,1,alpha);
	GL_Bind (gl->texnum);
	glBegin (GL_QUADS);
	glTexCoord2f (gl->sl, gl->tl);
	glVertex2f (x, y);
	glTexCoord2f (gl->sh, gl->tl);
	glVertex2f (x+pic->width, y);
	glTexCoord2f (gl->sh, gl->th);
	glVertex2f (x+pic->width, y+pic->height);
	glTexCoord2f (gl->sl, gl->th);
	glVertex2f (x, y+pic->height);
	glEnd ();
	glColor4f (1,1,1,1);
	glEnable(GL_ALPHA_TEST);
	glDisable (GL_BLEND);
}


/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, qpic_t *pic)
{
	glpic_t			*gl;

	if (scrap_dirty)
		Scrap_Upload ();
	gl = (glpic_t *)pic->data;
	glColor4f (1,1,1,1);
	GL_Bind (gl->texnum);
	glBegin (GL_QUADS);
	glTexCoord2f (gl->sl, gl->tl);
	glVertex2f (x, y);
	glTexCoord2f (gl->sh, gl->tl);
	glVertex2f (x+pic->width, y);
	glTexCoord2f (gl->sh, gl->th);
	glVertex2f (x+pic->width, y+pic->height);
	glTexCoord2f (gl->sl, gl->th);
	glVertex2f (x, y+pic->height);
	glEnd ();
}


/*
=============
Draw_TransPic
=============
*/
void Draw_TransPic (int x, int y, qpic_t *pic)
{
	if (x < 0 || (unsigned)(x + pic->width) > vid.width || y < 0 ||
		 (unsigned)(y + pic->height) > vid.height)
	{
		Sys_Error ("Draw_TransPic: bad coordinates");
	}
		
	Draw_Pic (x, y, pic);
}


/*
=============
Draw_TransPicTranslate

Only used for the player color selection menu
=============
*/
void Draw_TransPicTranslate (int x, int y, qpic_t *pic, byte *translation)
{
	int				v, u, c;
	unsigned		trans[64*64], *dest;
	byte			*src;
	int				p;

	GL_Bind (translate_texture);

	c = pic->width * pic->height;

	dest = trans;
	for (v=0 ; v<64 ; v++, dest += 64)
	{
		src = &menuplyr_pixels[ ((v*pic->height)>>6) *pic->width];
		for (u=0 ; u<64 ; u++)
		{
			p = src[(u*pic->width)>>6];
			if (p == 255)
				dest[u] = p;
			else
				dest[u] =  d_8to24table[translation[p]];
		}
	}

	GL_TexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glColor3f (1,1,1);
	glBegin (GL_QUADS);
	glTexCoord2f (0, 0);
	glVertex2f (x, y);
	glTexCoord2f (1, 0);
	glVertex2f (x+pic->width, y);
	glTexCoord2f (1, 1);
	glVertex2f (x+pic->width, y+pic->height);
	glTexCoord2f (0, 1);
	glVertex2f (x, y+pic->height);
	glEnd ();
}


/*
================
Draw_ConsoleBackground

================
*/
void Draw_ConsoleBackground (int lines)
{
	int y = (vid.height * 3) >> 2;
	int x, i; 

#if defined(__APPLE__) || defined (MACOSX)

	conback->width	= vid.width;
	conback->height	= vid.height;
	
#endif // __APPLE__ || MACOSX

	char tl[80]; //Console Clock - Eradicator
	char timebuf[20];

	if (lines > y)
		Draw_Pic(0, lines - vid.height, conback);
	else
		Draw_AlphaPic (0, lines - vid.height, conback, (float)(1.2 * lines)/y);

	if ( con_clock.value )
	{
                Sys_Strtime( timebuf );
		y = lines-14; 
		sprintf (tl, "Time: %s",timebuf); //Console Clock - Eradicator
		x = vid.conwidth - (vid.conwidth*12/vid.width*12) + 30; 
		for (i=0 ; i < strlen(tl) ; i++) 
		   Draw_Character (x + i * 8, y, tl[i] | 0x80);
	}
}

void Draw_SpiralConsoleBackground (int lines) //Spiral Console - Eradicator
{ 
   int x, i; 
   int y; 
   static float xangle = 0, xfactor = .3f, xstep = .01f; 
   
   char tl[80]; //Console Clock - Eradicator
   char timebuf[20];
   Sys_Strtime( timebuf );


   glPushMatrix(); 
   glMatrixMode(GL_TEXTURE); 
   glPushMatrix(); 
   glLoadIdentity(); 
   xangle += 1.0f; 
   xfactor += xstep; 
   if (xfactor > 8 || xfactor < .3f) 
      xstep = -xstep; 
   glRotatef(xangle, 0, 0, 1); 
   glScalef(xfactor, xfactor, xfactor); 
   y = (vid.height * 3) >> 2;  
   if (lines > y) 
      Draw_Pic(0, lines-vid.height, conback); 
   else 
      Draw_AlphaPic (0, lines - vid.height, conback, (float)(1.2 * lines)/y); 
   glPopMatrix(); 
   glMatrixMode(GL_MODELVIEW); 
   glPopMatrix(); 

   	if ( con_clock.value )
	{
		y = lines-14; 
		sprintf (tl, "Time: %s",timebuf); //Console Clock - Eradicator
		x = vid.conwidth - (vid.conwidth*12/vid.width*12) + 30; 
		for (i=0 ; i < strlen(tl) ; i++) 
			Draw_Character (x + i * 8, y, tl[i] | 0x80);
	}
}


/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h)
{
	glColor3f (1,1,1);
	GL_Bind (*(int *)draw_backtile->data);
	glBegin (GL_QUADS);
	glTexCoord2f (x/64.0, y/64.0);
	glVertex2f (x, y);
	glTexCoord2f ( (x+w)/64.0, y/64.0);
	glVertex2f (x+w, y);
	glTexCoord2f ( (x+w)/64.0, (y+h)/64.0);
	glVertex2f (x+w, y+h);
	glTexCoord2f ( x/64.0, (y+h)/64.0 );
	glVertex2f (x, y+h);
	glEnd ();
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, int c)
{
	glDisable (GL_TEXTURE_2D);
	glColor3f (host_basepal[c*3]/255.0,
		host_basepal[c*3+1]/255.0,
		host_basepal[c*3+2]/255.0);

	glBegin (GL_QUADS);

	glVertex2f (x,y);
	glVertex2f (x+w, y);
	glVertex2f (x+w, y+h);
	glVertex2f (x, y+h);

	glEnd ();
	glColor3f (1,1,1);
	glEnable (GL_TEXTURE_2D);
}
//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
	glEnable (GL_BLEND);
	glDisable (GL_TEXTURE_2D);
	glColor4f (0, 0, 0, 0.8);
	glBegin (GL_QUADS);

	glVertex2f (0,0);
	glVertex2f (vid.width, 0);
	glVertex2f (vid.width, vid.height);
	glVertex2f (0, vid.height);

	glEnd ();
	glColor4f (1,1,1,1);
	glEnable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);

	Sbar_Changed();
}

//=============================================================================

/*
================
Draw_BeginDisc

Draws the little blue disc in the corner of the screen.
Call before beginning any disc IO.
================
*/
void Draw_BeginDisc (void)
{
	if (!draw_disc)
		return;
    
#if defined (__APPLE__) || defined (MACOSX)
    GLboolean multiSampleEnabled = glIsEnabled (GL_MULTISAMPLE);
    
    if (multiSampleEnabled == GL_FALSE)
#endif /* __APPLE__ || MACOSX */
    {
        glDrawBuffer  (GL_FRONT);
    }
    
	Draw_Pic (vid.width - 24, 0, draw_disc);
    
#if defined (__APPLE__) || defined (MACOSX)
    if (multiSampleEnabled == GL_FALSE)
#endif /* __APPLE__ || MACOSX */
    {
        glDrawBuffer  (GL_BACK);
    }
}


/*
================
Draw_EndDisc

Erases the disc icon.
Call after completing any disc IO
================
*/
void Draw_EndDisc (void)
{
}

/*
================
GL_Set2D

Setup as if the screen was 320*200
================
*/
void GL_Set2D (void)
{
	glViewport (glx, gly, glwidth, glheight);

	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	glOrtho  (0, vid.width, vid.height, 0, -99999, 99999);

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
//	glDisable (GL_BLEND);
	glEnable (GL_ALPHA_TEST);

	glEnable(GL_BLEND);
	glAlphaFunc(GL_GREATER,0.666);
//	glDisable (GL_ALPHA_TEST);

	glColor4f (1,1,1,1);
}

//====================================================================

/*
================
GL_FindTexture
================
*/
int GL_FindTexture (char *identifier)
{
	int		i;
	gltexture_t	*glt;

	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if (!strcmp (identifier, glt->identifier))
			return gltextures[i].texnum;
	}

	return -1;
}

/*
================
GL_ResampleTextureLerpLine

Interpolates between pixels on line - Eradicator
================
*/
void GL_ResampleTextureLerpLine (byte *in, byte *out, int inwidth, int outwidth)
{
	int		j, xi, oldx = 0, f, fstep, endx;
	fstep = (int) (inwidth*65536.0f/outwidth);
	endx = (inwidth-1);
	for (j = 0,f = 0;j < outwidth;j++, f += fstep)
	{
		xi = (int) f >> 16;
		if (xi != oldx)
		{
			in += (xi - oldx) * 4;
			oldx = xi;
		}
		if (xi < endx)
		{
			int lerp = f & 0xFFFF;
			*out++ = (byte) ((((in[4] - in[0]) * lerp) >> 16) + in[0]);
			*out++ = (byte) ((((in[5] - in[1]) * lerp) >> 16) + in[1]);
			*out++ = (byte) ((((in[6] - in[2]) * lerp) >> 16) + in[2]);
			*out++ = (byte) ((((in[7] - in[3]) * lerp) >> 16) + in[3]);
		}
		else
		{
			*out++ = in[0];
			*out++ = in[1];
			*out++ = in[2];
			*out++ = in[3];
		}
	}
}

/*
================
GL_ResampleTexture
================
*/
void GL_ResampleTexture (void *indata, int inwidth, int inheight, void *outdata,  int outwidth, int outheight)
{
	//New Interpolated Texture Code - Eradicator
	int		i, j, yi, oldy, f, fstep, endy = (inheight-1);
	byte	*inrow, *out, *row1, *row2;
	out = outdata;
	fstep = (int) (inheight*65536.0f/outheight);

	row1 = malloc(outwidth*4);
	row2 = malloc(outwidth*4);
	inrow = indata;
	oldy = 0;
	GL_ResampleTextureLerpLine (inrow, row1, inwidth, outwidth);
	GL_ResampleTextureLerpLine (inrow + inwidth*4, row2, inwidth, outwidth);
	for (i = 0, f = 0;i < outheight;i++,f += fstep)
	{
		yi = f >> 16;
		if (yi < endy)
		{
			int lerp = f & 0xFFFF;
			if (yi != oldy)
			{
				inrow = (byte *)indata + inwidth*4*yi;
				if (yi == oldy+1)
					memcpy(row1, row2, outwidth*4);
				else
					GL_ResampleTextureLerpLine (inrow, row1, inwidth, outwidth);
				GL_ResampleTextureLerpLine (inrow + inwidth*4, row2, inwidth, outwidth);
				oldy = yi;
			}
			j = outwidth - 4;
			while(j >= 0)
			{
				out[ 0] = (byte) ((((row2[ 0] - row1[ 0]) * lerp) >> 16) + row1[ 0]);
				out[ 1] = (byte) ((((row2[ 1] - row1[ 1]) * lerp) >> 16) + row1[ 1]);
				out[ 2] = (byte) ((((row2[ 2] - row1[ 2]) * lerp) >> 16) + row1[ 2]);
				out[ 3] = (byte) ((((row2[ 3] - row1[ 3]) * lerp) >> 16) + row1[ 3]);
				out[ 4] = (byte) ((((row2[ 4] - row1[ 4]) * lerp) >> 16) + row1[ 4]);
				out[ 5] = (byte) ((((row2[ 5] - row1[ 5]) * lerp) >> 16) + row1[ 5]);
				out[ 6] = (byte) ((((row2[ 6] - row1[ 6]) * lerp) >> 16) + row1[ 6]);
				out[ 7] = (byte) ((((row2[ 7] - row1[ 7]) * lerp) >> 16) + row1[ 7]);
				out[ 8] = (byte) ((((row2[ 8] - row1[ 8]) * lerp) >> 16) + row1[ 8]);
				out[ 9] = (byte) ((((row2[ 9] - row1[ 9]) * lerp) >> 16) + row1[ 9]);
				out[10] = (byte) ((((row2[10] - row1[10]) * lerp) >> 16) + row1[10]);
				out[11] = (byte) ((((row2[11] - row1[11]) * lerp) >> 16) + row1[11]);
				out[12] = (byte) ((((row2[12] - row1[12]) * lerp) >> 16) + row1[12]);
				out[13] = (byte) ((((row2[13] - row1[13]) * lerp) >> 16) + row1[13]);
				out[14] = (byte) ((((row2[14] - row1[14]) * lerp) >> 16) + row1[14]);
				out[15] = (byte) ((((row2[15] - row1[15]) * lerp) >> 16) + row1[15]);
				out += 16;
				row1 += 16;
				row2 += 16;
				j -= 4;
			}
			if (j & 2)
			{
				out[ 0] = (byte) ((((row2[ 0] - row1[ 0]) * lerp) >> 16) + row1[ 0]);
				out[ 1] = (byte) ((((row2[ 1] - row1[ 1]) * lerp) >> 16) + row1[ 1]);
				out[ 2] = (byte) ((((row2[ 2] - row1[ 2]) * lerp) >> 16) + row1[ 2]);
				out[ 3] = (byte) ((((row2[ 3] - row1[ 3]) * lerp) >> 16) + row1[ 3]);
				out[ 4] = (byte) ((((row2[ 4] - row1[ 4]) * lerp) >> 16) + row1[ 4]);
				out[ 5] = (byte) ((((row2[ 5] - row1[ 5]) * lerp) >> 16) + row1[ 5]);
				out[ 6] = (byte) ((((row2[ 6] - row1[ 6]) * lerp) >> 16) + row1[ 6]);
				out[ 7] = (byte) ((((row2[ 7] - row1[ 7]) * lerp) >> 16) + row1[ 7]);
				out += 8;
				row1 += 8;
				row2 += 8;
			}
			if (j & 1)
			{
				out[ 0] = (byte) ((((row2[ 0] - row1[ 0]) * lerp) >> 16) + row1[ 0]);
				out[ 1] = (byte) ((((row2[ 1] - row1[ 1]) * lerp) >> 16) + row1[ 1]);
				out[ 2] = (byte) ((((row2[ 2] - row1[ 2]) * lerp) >> 16) + row1[ 2]);
				out[ 3] = (byte) ((((row2[ 3] - row1[ 3]) * lerp) >> 16) + row1[ 3]);
				out += 4;
				row1 += 4;
				row2 += 4;
			}
			row1 -= outwidth*4;
			row2 -= outwidth*4;
		}
		else
		{
			if (yi != oldy)
			{
				inrow = (byte *)indata + inwidth*4*yi;
				if (yi == oldy+1)
					memcpy(row1, row2, outwidth*4);
				else
					GL_ResampleTextureLerpLine (inrow, row1, inwidth, outwidth);
				oldy = yi;
			}
			memcpy(out, row1, outwidth * 4);
		}
	}
	free(row1);
	free(row2);

	//Old Code - Eradicator
	/*int		i, j;
	unsigned	*inrow;
	unsigned	frac, fracstep;

	fracstep = inwidth*0x10000/outwidth;
	for (i=0 ; i<outheight ; i++, out += outwidth)
	{
		inrow = in + inwidth*(i*inheight/outheight);
		frac = fracstep >> 1;
		for (j=0 ; j<outwidth ; j+=4)
		{
			out[j] = inrow[frac>>16];
			frac += fracstep;
			out[j+1] = inrow[frac>>16];
			frac += fracstep;
			out[j+2] = inrow[frac>>16];
			frac += fracstep;
			out[j+3] = inrow[frac>>16];
			frac += fracstep;
		}
	}*/
}


/*
================
GL_Resample8BitTexture -- JACK
================
*/
void GL_Resample8BitTexture (unsigned char *in, int inwidth, int inheight, unsigned char *out,  int outwidth, int outheight)
{
	int		i, j;
	unsigned	char *inrow;
	unsigned	frac, fracstep;

	fracstep = inwidth*0x10000/outwidth;
	for (i=0 ; i<outheight ; i++, out += outwidth)
	{
		inrow = in + inwidth*(i*inheight/outheight);
		frac = fracstep >> 1;
		for (j=0 ; j<outwidth ; j+=4)
		{
			out[j] = inrow[frac>>16];
			frac += fracstep;
			out[j+1] = inrow[frac>>16];
			frac += fracstep;
			out[j+2] = inrow[frac>>16];
			frac += fracstep;
			out[j+3] = inrow[frac>>16];
			frac += fracstep;
		}
	}
}



/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
void GL_MipMap (byte *in, int width, int height)
{
	int		i, j;
	byte	*out;

	width <<=2;
	height >>= 1;
	out = in;
	for (i=0 ; i<height ; i++, in+=width)
	{
		for (j=0 ; j<width ; j+=8, out+=4, in+=8)
		{
			out[0] = (in[0] + in[4] + in[width+0] + in[width+4])>>2;
			out[1] = (in[1] + in[5] + in[width+1] + in[width+5])>>2;
			out[2] = (in[2] + in[6] + in[width+2] + in[width+6])>>2;
			out[3] = (in[3] + in[7] + in[width+3] + in[width+7])>>2;
		}
	}
}

/*
================
PENTA:
GL_MipMapGray

Operates in place, quartering the size of the texture
================
*/
void GL_MipMapGray (byte *in, int width, int height)
{
	int		i, j;
	byte	*out;

	width <<=2;
	height >>= 1;
	out = in;
	for (i=0 ; i<height ; i++, in+=width)
	{
		for (j=0 ; j<width ; j+=2, out+=1, in+=2)
		{
			out[0] = (in[0] + in[1] + in[width+0] + in[width+1])>>2;
		}
	}
}

/*
================
GL_MipMapNormal

Operates in place, quartering the size of the texture
Works on normal maps instead of rgb
================
*/
void GL_MipMapNormal (byte *in, int width, int height)
{
    int		i, j;
    byte	*out;
    float	inv255	= 1.0f/255.0f;
    float	inv127	= 1.0f/127.0f;
    float	x,y,z,l,g;


    width <<=2;
    height >>= 1;
    out = in;
    for (i=0 ; i<height ; i++, in+=width)
    {
	for (j=0 ; j<width ; j+=8, out+=4, in+=8)
	{
	    x = (inv127*in[0]-1.0)+
		(inv127*in[4]-1.0)+
		(inv127*in[width+0]-1.0)+
		(inv127*in[width+4]-1.0);
	    y = (inv127*in[1]-1.0)+
		(inv127*in[5]-1.0)+
		(inv127*in[width+1]-1.0)+
		(inv127*in[width+5]-1.0);
	    z = (inv127*in[2]-1.0)+
		(inv127*in[6]-1.0)+
		(inv127*in[width+2]-1.0)+
		(inv127*in[width+6]-1.0);

	    g = (inv255*in[3])+

		(inv255*in[7])+

		(inv255*in[width+3])+

		(inv255*in[width+7]);


            l = sqrt(x*x+y*y+z*z);
	    if (l == 0.0) {
		x = 0.0;
		y = 0.0;
		z = 1.0;
	    } else {
		//normalize it.
		l=1/l;
		x *=l;
		y *=l;
		z *=l;
	    }
	    out[0] = (unsigned char)128 + 127*x;
	    out[1] = (unsigned char)128 + 127*y;
	    out[2] = (unsigned char)128 + 127*z;
	    out[3] = (byte)(g * 255.0/4.0);
	}
    }

}

void GL_Normalize(byte *in, int width, int height)
{
    int		i, j;
    byte	*out;
    float	inv127	= 1.0f/127.0f;
    float	x,y,z,l;

    width <<=2;
    out = in;
    for (i=0 ; i<height ; i++)
    {
	for (j=0 ; j<width ; j+=4, out+=4, in+=4)
	{
	    x = (inv127*in[0]-1.0);
	    y = (inv127*in[1]-1.0);
            z = (inv127*in[2]-1.0);

            l = sqrt(x*x+y*y+z*z);
	    if (l == 0.0) {
		x = 0.0;
		y = 0.0;
		z = 1.0;
	    } else {
		//normalize it.
		l=1/l;
		x *=l;
		y *=l;
		z *=l;
	    }
	    out[0] = (unsigned char)128 + 127*x;
	    out[1] = (unsigned char)128 + 127*y;
	    out[2] = (unsigned char)128 + 127*z;
	}
    }

}

/*
================
GL_MipMap8Bit

Mipping for 8 bit textures
================
*/
void GL_MipMap8Bit (byte *in, int width, int height)
{
	int		i, j;
	unsigned short     r,g,b;
	byte	*out, *at1, *at2, *at3, *at4;

//	width <<=2;
	height >>= 1;
	out = in;
	for (i=0 ; i<height ; i++, in+=width)
	{
		for (j=0 ; j<width ; j+=2, out+=1, in+=2)
		{
			at1 = (byte *) (d_8to24table + in[0]);
			at2 = (byte *) (d_8to24table + in[1]);
			at3 = (byte *) (d_8to24table + in[width+0]);
			at4 = (byte *) (d_8to24table + in[width+1]);

 			r = (at1[0]+at2[0]+at3[0]+at4[0]); r>>=5;
 			g = (at1[1]+at2[1]+at3[1]+at4[1]); g>>=5;
 			b = (at1[2]+at2[2]+at3[2]+at4[2]); b>>=5;

			out[0] = d_15to8table[(r<<0) + (g<<5) + (b<<10)];
		}
	}
}

/*
===============
PENTA:
Packs the byte values in gloss in the alpha channel of dest
<AWE> : added "void" as return type.
===============
*/
void	GL_PackGloss(byte *gloss,unsigned *dest,int length)
{
    int i;

    for (i=0; i<length; i++, gloss++, dest++) 

    {
        *dest = *dest & LittleLong (0x00FFFFFF);	// <AWE> Added support for big endian.
        *dest = *dest | LittleLong (*gloss << 24);	// <AWE> Added support for big endian.
    }
}

/*
===============
GL_Upload32
===============
*/
void GL_Upload32 (unsigned *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int			texturemode;
	//XYZ
static	unsigned	scaled[1024*1024];	// [512*256];
	int			scaled_width, scaled_height;

        if ( willi_gray_colormaps.value )
        {
	    Q_memset(data, 0x7f, width*height*4);
        }

	for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
		;
	for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
		;

	scaled_width >>= (int)gl_picmip.value;
	scaled_height >>= (int)gl_picmip.value;

	if (scaled_width > gl_max_size.value)
		scaled_width = gl_max_size.value;
	if (scaled_height > gl_max_size.value)
		scaled_height = gl_max_size.value;

	if (scaled_width * scaled_height > sizeof(scaled)/4)
		Sys_Error ("GL_LoadTexture: too big");

        if ( gl_texcomp && ((int)gl_compress_textures.value) & 1 )
        {
            texturemode = GL_COMPRESSED_RGBA_ARB;
        }
        else
        {
            texturemode = GL_RGBA;//PENTA: Always upload rgb it doesn't make any difference for nvidia cards (& others)
	    //texturemode = alpha ? gl_alpha_format : gl_solid_format;
        }
#if 0
	if (mipmap)
		gluBuild2DMipmaps (GL_TEXTURE_2D, texturemode, width, height, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	else if (scaled_width == width && scaled_height == height)
		GL_TexImage2D (GL_TEXTURE_2D, 0, texturemode, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	else
	{
		gluScaleImage (GL_RGBA, width, height, GL_UNSIGNED_BYTE, trans,
			scaled_width, scaled_height, GL_UNSIGNED_BYTE, scaled);
		GL_TexImage2D (GL_TEXTURE_2D, 0, texturemode, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	}
#else
texels += scaled_width * scaled_height;

	if (scaled_width == width && scaled_height == height)
	{
		if (!mipmap)
		{
			GL_TexImage2D (GL_TEXTURE_2D, 0, texturemode, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			goto done;
		}
		memcpy (scaled, data, width*height*4);
	}
	else
		GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);

	GL_TexImage2D (GL_TEXTURE_2D, 0, texturemode, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	if (mipmap)
	{
		int		miplevel;

		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1)
		{
			GL_MipMap ((byte *)scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			if (scaled_width < 1)
				scaled_width = 1;
			if (scaled_height < 1)
				scaled_height = 1;
			miplevel++;
			GL_TexImage2D (GL_TEXTURE_2D, miplevel, texturemode, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
                }
	}
done: ;
#endif


	if (mipmap)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}

	if (gl_texturefilteranisotropic)
		glTexParameterfv (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &gl_textureanisotropylevel);
}

void GL_Upload8_EXT (byte *data, int width, int height,  qboolean mipmap, qboolean alpha) 
{
	int			i, s;
	qboolean		noalpha;
	int			samples;
        static unsigned char 	scaled[1024*512];	// [512*256];
	int			scaled_width, scaled_height;

	s = width*height;

        // if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (alpha)
	{
		noalpha = true;
		for (i=0 ; i<s ; i++)
		{
			if (data[i] == 255)
				noalpha = false;
		}

		if (alpha && noalpha)
			alpha = false;
	}
	for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
		;
	for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
		;

	scaled_width >>= (int)gl_picmip.value;
	scaled_height >>= (int)gl_picmip.value;

	if (scaled_width > gl_max_size.value)
		scaled_width = gl_max_size.value;
	if (scaled_height > gl_max_size.value)
		scaled_height = gl_max_size.value;

	if (scaled_width * scaled_height > sizeof(scaled))
		Sys_Error ("GL_LoadTexture: too big");

	samples = 1; // alpha ? gl_alpha_format : gl_solid_format;

	texels += scaled_width * scaled_height;

	if (scaled_width == width && scaled_height == height)
	{
		if (!mipmap)
		{
			GL_TexImage2D (GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, scaled_width, scaled_height, 0, GL_COLOR_INDEX , GL_UNSIGNED_BYTE, data);
			goto done;
		}
		memcpy (scaled, data, width*height);
	}
	else
		GL_Resample8BitTexture (data, width, height, scaled, scaled_width, scaled_height);

	GL_TexImage2D (GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, scaled_width, scaled_height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, scaled);
	if (mipmap)
	{
		int		miplevel;

		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1)
		{
			GL_MipMap8Bit ((byte *)scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			if (scaled_width < 1)
				scaled_width = 1;
			if (scaled_height < 1)
				scaled_height = 1;
			miplevel++;
			GL_TexImage2D (GL_TEXTURE_2D, miplevel, GL_COLOR_INDEX8_EXT, scaled_width, scaled_height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, scaled);
		}
	}
done: ;


	if (mipmap)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}

	if (gl_texturefilteranisotropic)
		glTexParameterfv (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &gl_textureanisotropylevel);

}

//PENTA

unsigned int * genNormalMap(byte *pixels, int w, int h, float scale)
{
  int i, j, wr, hr;
  unsigned char r, g, b;
  static unsigned nmap[512*512];
  float sqlen, reciplen, nx, ny, nz;

  const float oneOver255 = 1.0f/255.0f;

  float c, cx, cy, dcx, dcy;

  wr = w;
  hr = h;

  for (i=0; i<h; i++) {
    for (j=0; j<w; j++) {
      /* Expand [0,255] texel values to the [0,1] range. */
      c = pixels[i*wr + j] * oneOver255;
      /* Expand the texel to its right. */
      cx = pixels[i*wr + (j+1)%wr] * oneOver255;
      /* Expand the texel one up. */
      cy = pixels[((i+1)%hr)*wr + j] * oneOver255;
      dcx = scale * (c - cx);
      dcy = scale * (c - cy);

      /* Normalize the vector. */
      sqlen = dcx*dcx + dcy*dcy + 1;
      reciplen = 1.0f/(float)sqrt(sqlen);
      nx = dcx*reciplen;
      ny = -dcy*reciplen;
      nz = reciplen;

      /* Repack the normalized vector into an RGB unsigned byte
         vector in the normal map image. */
      r = (byte) (128 + 127*nx);
      g = (byte) (128 + 127*ny);
      b = (byte) (128 + 127*nz);

      /* The highest resolution mipmap level always has a
         unit length magnitude. */
      nmap[i*w+j] = LittleLong ((255 << 24)|(b << 16)|(g << 8)|(r));	// <AWE> Added support for big endian.
    }
  }

  return &nmap[0];
}

//PENTA
void GL_UploadBump(byte *data, int width, int height, qboolean mipmap, byte* gloss)
{
    static unsigned char	scaled[1024*1024];	// [512*256];
    static unsigned char	scaledgloss[1024*1024];	// [512*256];
    int			scaled_width, scaled_height;
    byte			*nmap;
    int			texturemode;

    if ( gl_texcomp && ((int)gl_compress_textures.value) & 2 )
    {
	texturemode = GL_COMPRESSED_RGBA_ARB;
    }
    else
    {
	texturemode = GL_RGBA;
    }

    //Resize to power of 2 and maximum texture size
    for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
	;
    for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
	;

    scaled_width >>= (int)gl_picmip.value;
    scaled_height >>= (int)gl_picmip.value;

    if (scaled_width > gl_max_size.value)
	scaled_width = gl_max_size.value;
    if (scaled_height > gl_max_size.value)
	scaled_height = gl_max_size.value;

    if (scaled_width * scaled_height > sizeof(scaled))
	Sys_Error ("GL_LoadTexture: too big");

    //To resize or not to resize
    if (scaled_width == width && scaled_height == height)
    {
	memcpy (scaled, data, width*height);
	memcpy (scaledgloss, gloss, width*height);
	scaled_width = width;
	scaled_height = height;
    }
    else
    {
	//Just picks pixels so grayscale is equivalent with 8 bit.
	GL_Resample8BitTexture (data, width, height, scaled, scaled_width, scaled_height);
	GL_Resample8BitTexture (gloss, width, height, scaledgloss, scaled_width, scaled_height);
    }

    if (is_overriden)
	nmap = (byte *)genNormalMap(scaled,scaled_width,scaled_height,10.0f); 
    else
	nmap = (byte *)genNormalMap(scaled,scaled_width,scaled_height,4.0f);

    GL_PackGloss(scaledgloss, (unsigned int*)nmap, scaled_width*scaled_height);

    GL_TexImage2D (GL_TEXTURE_2D, 0, texturemode, scaled_width, scaled_height, 0,
		  GL_RGBA, GL_UNSIGNED_BYTE, nmap);

    //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (mipmap)
    {
	int		miplevel;

	miplevel = 0;
	while (scaled_width > 1 || scaled_height > 1)
	{
	    GL_MipMapNormal(nmap,scaled_width,scaled_height);
	    //GL_MipMapGray((byte *)scaled, scaled_width, scaled_height);
	    scaled_width >>= 1;
	    scaled_height >>= 1;
	    if (scaled_width < 1)
		scaled_width = 1;
	    if (scaled_height < 1)
		scaled_height = 1;
	    miplevel++;

	    GL_TexImage2D (GL_TEXTURE_2D, miplevel, texturemode, scaled_width, scaled_height, 0, GL_RGBA,
			  GL_UNSIGNED_BYTE, nmap);
	}
    }

    if (mipmap)
    {
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
    }
    else
    {
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
    }

        
    if (gl_texturefilteranisotropic)
	glTexParameterfv (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &gl_textureanisotropylevel);

}


//PENTA
void GL_UploadNormal(unsigned int *data, int width, int height, qboolean mipmap)
{
    static unsigned int	scaled[1024*1024];	// [512*256];
    int			scaled_width, scaled_height;
    int			texturemode;

    if ( gl_texcomp && ((int)gl_compress_textures.value) & 2 )
    {
        texturemode = GL_COMPRESSED_RGBA_ARB;
    }
    else
    {
        texturemode = GL_RGBA;
    }

    //Resize to power of 2 and maximum texture size
    for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
	;
    for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
	;

    scaled_width >>= (int)gl_picmip.value;
    scaled_height >>= (int)gl_picmip.value;

    if (scaled_width > gl_max_size.value)
		scaled_width = gl_max_size.value;
    if (scaled_height > gl_max_size.value)
		scaled_height = gl_max_size.value;

    if (scaled_width * scaled_height > sizeof(scaled))
		Sys_Error ("GL_LoadTexture: too big");

    //To resize or not to resize
    if (scaled_width == width && scaled_height == height)
    {
		memcpy (scaled, data, width*height*4);
		scaled_width = width;
		scaled_height = height;
    }
    else {
		GL_ResampleTexture ((unsigned*)data, width, height, scaled, scaled_width, scaled_height);
    }

    GL_Normalize((byte*)scaled, scaled_width, scaled_height);
    GL_TexImage2D (GL_TEXTURE_2D, 0, texturemode, scaled_width, scaled_height, 0,
		  GL_RGBA, GL_UNSIGNED_BYTE, scaled);

    //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (mipmap)
    {
		int		miplevel;

		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1)
		{
			GL_MipMapNormal((byte*)scaled,scaled_width,scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			if (scaled_width < 1)
			scaled_width = 1;
			if (scaled_height < 1)
			scaled_height = 1;
			miplevel++;

			GL_TexImage2D (GL_TEXTURE_2D, miplevel, texturemode, scaled_width, scaled_height, 0, GL_RGBA,
				  GL_UNSIGNED_BYTE, scaled);
		}
    }

    if (mipmap)
    {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
    }
    else
    {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
    }

    if (gl_texturefilteranisotropic)
		glTexParameterfv (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &gl_textureanisotropylevel);

}

/*
===============
PENTA:
get the name of an overriden file
<AWE> : added "void" as return type.
===============
*/
void	GL_GetOverrideName(char *identifier,char *tail, char *dest) {
	int i;

	sprintf(dest,"override/%s%s.tga", identifier, tail);
	for (i=0; i<strlen(dest); i++) {
		if (dest[i] == '*')
			dest[i] = '%';
	}

}



/*
===============
GL_Upload8
===============
*/
	//XYZ
static	unsigned	trans[1024*1024];		// FIXME, temporary

#define RED_MASK 0x00FF0000
#define GREEN_MASK 0x0000FF00
#define BLUE_MASK 0x000000FF

void GL_Upload8 (byte *data,char *identifier, int width, int height,  qboolean mipmap, qboolean alpha, qboolean bump)
{
	//XYZ
static	unsigned char	bumppix[1024*1024];	// PENTA: bumped texture (it seems the previous one never got fixed anyway)
static	unsigned char	glosspix[1024*1024];	// PENTA: bumped texture (it seems the previous one never got fixed anyway)

	int			i, s;
	qboolean	noalpha;
	int			p;
	char filename[MAX_OSPATH];
	byte		r, g, b;
	s = width*height;

	//try to load an overrided texture

	GL_GetOverrideName(identifier,"",filename);	
	if ( LoadTextureInPlace(filename, 4, (unsigned char*)&trans[0], &width, &height) )
	{
		Con_DPrintf("Overriding colormap for %s\n",identifier);

		is_overriden = true;
		//force it to upload a 32 bit texture
		alpha = true;

		for (i=0; i<width*height; i++) {
			r = (trans[i] & RED_MASK) >> 16;
			g = (trans[i] & GREEN_MASK) >> 8;
			b = (trans[i] & BLUE_MASK);
			bumppix[i] = (r+g+b)/3;
		}
	}
	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	else if (alpha)
	{
		is_overriden = false;

		noalpha = true;
		for (i=0 ; i<s ; i++)
		{
			p = data[i];
			if (p == 255)
				noalpha = false;
			trans[i] = d_8to24table[p];
			//PENTA: Grayscale for autobumps
			bumppix[i] = d_8to8graytable[p];
		}

		if (alpha && noalpha)
			alpha = false;
	}
	else
	{
		is_overriden = false;

		if (s&3)
			Sys_Error ("GL_Upload8: s&3");
		for (i=0 ; i<s ; i+=4)
		{
			trans[i] = d_8to24table[data[i]];
			trans[i+1] = d_8to24table[data[i+1]];
			trans[i+2] = d_8to24table[data[i+2]];
			trans[i+3] = d_8to24table[data[i+3]];
			//PENTA: Grayscale for autobumps
			bumppix[i] = d_8to8graytable[data[i]];
			bumppix[i+1] = d_8to8graytable[data[i+1]];
			bumppix[i+2] = d_8to8graytable[data[i+2]];
			bumppix[i+3] = d_8to8graytable[data[i+3]];
		}
	}

	if (!strcmp("scrap",identifier)) {
		GL_Upload32 (trans, width, height, mipmap, alpha);
		return;
	}

	//upload color map
	GL_Bind(texture_extension_number);

	GL_Upload32 (trans, width, height, mipmap, alpha);

	texture_extension_number++;

	//Disable Bumpmapping parameter and now bumpmap cvar slightly
	//works. this only prevents textures from loading, but if it
	//has already loaded bumpmaps will not turn off) - Eradicator
	if (!COM_CheckParm("-nobumpmaps") && ( sh_bumpmaps.value ) && bump )
	{
	    //upload bump map (if any)
	    char filename[MAX_OSPATH];

	    GL_Bind(texture_extension_number);
	    // first check for normal map
	    GL_GetOverrideName(identifier,"_norm",filename);
	    if ( LoadTextureInPlace(filename, 4, (unsigned char*)&trans[0], &width, &height) )
	    {
		char filename[MAX_OSPATH];
		int gloss_width, gloss_height;
		Con_DPrintf("Overriding normal map for %s\n",identifier);

		GL_GetOverrideName(identifier,"_gloss",filename);
		if ( LoadTextureInPlace(filename, 1, &glosspix[0], &gloss_width, &gloss_height) )
		{
		    Con_DPrintf("Using gloss map for %s\n",identifier);
		}
		else
		{
		    //set some gloss by default
		    gloss_width = width;
		    gloss_height = height;
		    Q_memset(&glosspix[0], 255*gl_gloss.value, width*height);
		}
		
		if ((gloss_width == width) && (gloss_height == height))
		{
		    GL_PackGloss(glosspix, trans, width*height);
		}
		else
		{
		    Con_SafePrintf("Error loading gloss map %s: Gloss map bust be the same size as normal map\n", identifier);
		}
		GL_UploadNormal (trans, width, height, mipmap);
	    }
	    else
	    {
		//See if we can override the bump map
		GL_GetOverrideName(identifier,"_bump",filename);
		//See if we can override the bump map
		if ( LoadTextureInPlace(filename, 1, &bumppix[0], &width, &height) )
		{
		    char filename[MAX_OSPATH];
		    int gloss_width, gloss_height;
		    Con_DPrintf("Overriding bumpmap for %s\n",identifier);
		    GL_GetOverrideName(identifier,"_gloss",filename);
		    if ( LoadTextureInPlace(filename, 1, &glosspix[0], &gloss_width, &gloss_height) )
		    {
			Con_DPrintf("Using gloss map for %s\n",identifier);
		    }
		    else
		    {
			//set some gloss by default
			gloss_width = width;
			gloss_height = height;
			Q_memset(&glosspix[0], 255*gl_gloss.value, width*height);
		    }
		
		    if ((gloss_width == width) && (gloss_height == height))
		    {
		    }
		    else
		    {
			Con_SafePrintf("Error loading gloss map %s: Gloss map bust be the same size as bump map\n", identifier);
                        Q_memset(&glosspix[0], 255*gl_gloss.value, width*height);
		    }
		}
                else
                {
                    Q_memset(&glosspix[0], 255*gl_gloss.value, width*height);

                }
		GL_UploadBump (bumppix, width, height, mipmap, &glosspix[0]);
	    }
	}
	texture_extension_number++;
}

/*
================
GL_LoadTexture
================
*/
int GL_LoadTexture (char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha, qboolean bump)
{
	int			i;
	gltexture_t	*glt;

	// see if the texture is allready present
	if (identifier[0])
	{
		for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
		{
			if (!strcmp (identifier, glt->identifier))
			{
				if (width != glt->width || height != glt->height)
					Con_Printf ("%s: warning: cache mismatch\n",identifier);
				//Con_Printf("reuse: %s\n",identifier);
				return gltextures[i].texnum;
			}
		}
	}
	//else {
		glt = &gltextures[numgltextures];
		numgltextures++;
	//}

	//Con_Printf("Load texture: %s %i %i\n",identifier,width,height);

	strncpy (glt->identifier, identifier, sizeof(glt->identifier));
	glt->texnum = texture_extension_number;
	glt->width = width;
	glt->height = height;
	glt->mipmap = mipmap;

	GL_Bind(texture_extension_number );

	GL_Upload8 (data, identifier, width, height, mipmap, alpha, bump);

	//texture_extension_number++;

	return texture_extension_number-2;
}


int GL_LoadLuma(char *identifier, qboolean mipmap)
{
	qboolean	alpha;
	char filename[MAX_OSPATH];
	int			width, height;

        if ( willi_gray_colormaps.value )
            return 0;

	//try to load an overrided texture
	GL_GetOverrideName(identifier,"_luma",filename);	
	if ( LoadTextureInPlace(filename, 4, (unsigned char*)&trans[0], &width, &height) )
	{

		Con_DPrintf("Using luma map for %s\n",identifier);

		is_overriden = true;
		//force it to upload a 32 bit texture
		alpha = true;

		GL_Bind(texture_extension_number);
		GL_Upload32 (trans, width, height, mipmap, alpha);
		texture_extension_number++;
		return texture_extension_number-1;
	} else {
		return 0;
	}
}


char *face_names[] =
{
	"posx",
	"negx",
	"posy",
	"negy",
	"posz",
	"negz",
};

/*
================
GL_LoadCubeMap
================
*/
int GL_LoadCubeMap (int identifier)
{
	int			i, width, height;
	gltexture_t	*glt;
	char filename[MAX_OSPATH];
        int			texturemode;

        if ( gl_texcomp && ((int)gl_compress_textures.value) & 4 )
        {   
            texturemode = GL_COMPRESSED_RGBA_ARB;
        }
        else
        {
            texturemode = GL_RGBA;
        }

	width = 0;
	height = 0;
	// see if the texture is allready present
	//Con_Printf("Texnum %i\n",numgltextures);
	sprintf(filename,"cubemaps/%i", identifier);

	if (filename[0])
	{
		for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
		{
			if (!strcmp (filename, glt->identifier))
			{
				//Con_Printf("Found cubemap: %s %i\n",identifier,i);
				return gltextures[i].texnum;
			}
		}
	}
	//else {
		glt = &gltextures[numgltextures];
		numgltextures++;
	strncpy (glt->identifier, filename,sizeof(glt->identifier));

		//Con_Printf("Texnum2 %i\n",numgltextures);
	//}

//	Con_Printf("Loading cubemap from: %i %i\n",identifier,numgltextures);
	glEnable(GL_TEXTURE_CUBE_MAP_ARB);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, texture_extension_number);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	
	for (i = 0; i < 6; i++) 
	{
		sprintf(filename,"cubemaps/%i%s.tga", identifier, face_names[i]);	
		LoadTextureInPlace(filename, 4, (unsigned char*)&trans[0], &width, &height);
		GL_TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB+i, 0, texturemode, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &trans[0]);
	}
	
	//glEnable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_CUBE_MAP_ARB);

	glt->texnum = texture_extension_number;
	glt->width = width;
	glt->height = height;
	glt->mipmap = false;

	texture_extension_number++;

	return texture_extension_number-1;
}

/*
================
GL_LoadPicTexture
================
*/
int GL_LoadPicTexture (qpic_t *pic)
{
	return GL_LoadTexture ("", pic->width, pic->height, pic->data, false, true, false);
}

/****************************************/

static GLenum oldtarget = GL_TEXTURE0_ARB;

void GL_SelectTexture (GLenum target) 
{
	if (!gl_mtexable)
		return;
	qglActiveTextureARB(target);
	if (target == oldtarget) 
		return;
	cnttextures[oldtarget-GL_TEXTURE0_ARB] = currenttexture;
	currenttexture = cnttextures[target-GL_TEXTURE0_ARB];
	oldtarget = target;
}

#define ATTEN_VOLUME_SIZE 64

float sqr(float p) {
	return p*p;
}

/*
===============
PENTA:

Load texture for 2d atten
===============
*/
int GL_Load2DAttenTexture(void)
{

	float centerx, centery, radiussq;
	int s,t, err;
	byte *data;

	data = malloc(ATTEN_VOLUME_SIZE*ATTEN_VOLUME_SIZE);
        if (!data) return 0;							// <AWE> check memory here!

	centerx = ATTEN_VOLUME_SIZE/2.0;
	centery = ATTEN_VOLUME_SIZE/2.0;
	radiussq = ATTEN_VOLUME_SIZE/2.0;
	radiussq = radiussq*radiussq;

	for (s = 0; s < ATTEN_VOLUME_SIZE; s++) {
		for (t = 0; t < ATTEN_VOLUME_SIZE; t++) {
			float DistSq = sqr(s-centerx)+sqr(t-centery);
			if (DistSq < radiussq) {
				byte value;
				float FallOff = (radiussq - DistSq) / radiussq;
				FallOff *= FallOff;
				value = FallOff*255.0;
				data[t * ATTEN_VOLUME_SIZE + s] = value;
			} else {
				data[t * ATTEN_VOLUME_SIZE + s] = 0;
			}
		}
	}

	GL_Bind(texture_extension_number);

	GL_TexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, ATTEN_VOLUME_SIZE, ATTEN_VOLUME_SIZE,
					0, GL_ALPHA, GL_UNSIGNED_BYTE, data);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);	

	free(data);	
	texture_extension_number++;

	err  = glGetError();

	if (err != GL_NO_ERROR) {
		Con_Printf("%s",gluErrorString(err));
	}

	return texture_extension_number-1;
}

/*
===============
PENTA:

Load texture for 3d atten (on gf3)
===============
*/
int GL_Load3DAttenTexture(void)
{

	float centerx, centery, centerz, radiussq;
	int s,t,r, err;
	byte *data;

	if (gl_cardtype == GENERIC || gl_cardtype == GEFORCE) return 0;//PA:

	data = malloc(ATTEN_VOLUME_SIZE*ATTEN_VOLUME_SIZE*ATTEN_VOLUME_SIZE*4);
        if (!data) return 0;							// <AWE> check memory here!
        
	centerx = ATTEN_VOLUME_SIZE/2.0;
	centery = ATTEN_VOLUME_SIZE/2.0;
	centerz = ATTEN_VOLUME_SIZE/2.0;
	radiussq = ATTEN_VOLUME_SIZE/2.0;
	radiussq = radiussq*radiussq;

	for (s = 0; s < ATTEN_VOLUME_SIZE; s++) {
		for (t = 0; t < ATTEN_VOLUME_SIZE; t++) {
			for (r = 0; r < ATTEN_VOLUME_SIZE; r++) {
				float DistSq = sqr(s-centerx)+sqr(t-centery)+sqr(r-centerz);
				if (DistSq < radiussq) {
					byte value;
					float FallOff = (radiussq - DistSq) / radiussq;
					FallOff *= FallOff;
					value = FallOff*255.0;
					data[r * ATTEN_VOLUME_SIZE * ATTEN_VOLUME_SIZE *4+ t * ATTEN_VOLUME_SIZE *4+ s *4+ 0] = value;
					data[r * ATTEN_VOLUME_SIZE * ATTEN_VOLUME_SIZE *4+ t * ATTEN_VOLUME_SIZE *4+ s *4+ 1] = value;
					data[r * ATTEN_VOLUME_SIZE * ATTEN_VOLUME_SIZE *4+ t * ATTEN_VOLUME_SIZE *4+ s *4+ 2] = value;
					data[r * ATTEN_VOLUME_SIZE * ATTEN_VOLUME_SIZE *4+ t * ATTEN_VOLUME_SIZE *4+ s *4+ 3] = value;
				} else {
					data[r * ATTEN_VOLUME_SIZE * ATTEN_VOLUME_SIZE *4+ t * ATTEN_VOLUME_SIZE *4+ s *4+ 0] = 0;
					data[r * ATTEN_VOLUME_SIZE * ATTEN_VOLUME_SIZE *4+ t * ATTEN_VOLUME_SIZE *4+ s *4+ 1] = 0;
					data[r * ATTEN_VOLUME_SIZE * ATTEN_VOLUME_SIZE *4+ t * ATTEN_VOLUME_SIZE *4+ s *4+ 2] = 0;
					data[r * ATTEN_VOLUME_SIZE * ATTEN_VOLUME_SIZE *4+ t * ATTEN_VOLUME_SIZE *4+ s *4+ 3] = 0;
				}

			}
		}
	}

	glBindTexture(GL_TEXTURE_3D, texture_extension_number);

	GL_TexImage3D(GL_TEXTURE_3D, 0, 4,
					ATTEN_VOLUME_SIZE, ATTEN_VOLUME_SIZE, ATTEN_VOLUME_SIZE,
					0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);	

	free(data);	
	texture_extension_number++;

	err  = glGetError();

	if (err != GL_NO_ERROR) {
		Con_Printf("%s",gluErrorString(err));
	}

	return texture_extension_number-1;
}

/* 
Code from:

bumpdemo.c - glamour demo of "normal perturbation" mapping
by Nvidia

*/

void getCubeVector(int i, int cubesize, int x, int y, float *vector)
{
  float s, t, sc, tc, mag;

  s = ((float)x + 0.5f) / (float)cubesize;
  t = ((float)y + 0.5f) / (float)cubesize;
  sc = s*2.0f - 1.0f;
  tc = t*2.0f - 1.0f;

  switch (i) {
  case 0:
    vector[0] = 1.0f;
    vector[1] = -tc;
    vector[2] = -sc;
    break;
  case 1:
    vector[0] = -1.0;
    vector[1] = -tc;
    vector[2] = sc;
    break;
  case 2:
    vector[0] = sc;
    vector[1] = 1.0;
    vector[2] = tc;
    break;
  case 3:
    vector[0] = sc;
    vector[1] = -1.0;
    vector[2] = -tc;
    break;
  case 4:
    vector[0] = sc;
    vector[1] = -tc;
    vector[2] = 1.0;
    break;
  case 5:
    vector[0] = -sc;
    vector[1] = -tc;
    vector[2] = -1.0;
    break;
  }

  mag = 1.0f/sqrt(vector[0]*vector[0] + vector[1]*vector[1] + vector[2]*vector[2]);
  vector[0] *= mag;
  vector[1] *= mag;
  vector[2] *= mag;
}

#define NC_SIZE 128//Size of the cube faces

int GL_LoadNormalizationCubemap()
{
	vec3_t vec;
	int i, x, y;
	char pixels [NC_SIZE*NC_SIZE*3];

	glEnable(GL_TEXTURE_CUBE_MAP_ARB);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, texture_extension_number);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	for (i = 0; i < 6; i++) {
		for (y = 0; y < NC_SIZE; y++) {
			for (x = 0; x < NC_SIZE; x++) 
			{
				getCubeVector(i, NC_SIZE, x, y, vec);
				pixels[3*(y*NC_SIZE+x) + 0] = (unsigned char)(128 + 127*vec[0]);
				pixels[3*(y*NC_SIZE+x) + 1] = (unsigned char)(128 + 127*vec[1]);
				pixels[3*(y*NC_SIZE+x) + 2] = (unsigned char)(128 + 127*vec[2]);
			}
		}
		GL_TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB+i, 0, GL_RGB8, NC_SIZE, NC_SIZE, 0, GL_RGB, GL_UNSIGNED_BYTE, &pixels);
	}
	
	glDisable(GL_TEXTURE_CUBE_MAP_ARB);
	texture_extension_number++;
	return texture_extension_number-1;
}
