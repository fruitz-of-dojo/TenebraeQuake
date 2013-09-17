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
// gl_warp.c -- sky and water polygons

//#define NO_PNG
#ifndef NO_PNG
#if defined (__APPLE__) || defined (MACOSX)
#include "png.h"
#else
#include <png.h>
#endif // __APPLE__ || MACOSX
#endif

#include "quakedef.h"

extern	model_t	*loadmodel;

int		skytexturenum;

int		solidskytexture;
int		alphaskytexture;
float	speedscale;		// for top sky and bottom sky

int		newwatertex,newslimetex,newlavatex,newteletex,newenvmap; //PENTA: texture id's for the new fluid textures

msurface_t	*warpface;

extern cvar_t gl_subdivide_size;


void BoundPoly (int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
    int		i, j;
    float	*v;

    mins[0] = mins[1] = mins[2] = 9999;
    maxs[0] = maxs[1] = maxs[2] = -9999;
    v = verts;
    for (i=0 ; i<numverts ; i++)
	for (j=0 ; j<3 ; j++, v++)
	{
	    if (*v < mins[j])
		mins[j] = *v;
	    if (*v > maxs[j])
		maxs[j] = *v;
	}
}

void SubdividePolygon (int numverts, float *verts)
{
    int		i, j, k;
    vec3_t	mins, maxs;
    float	m;
    float	*v;
    vec3_t	front[64], back[64];
    int		f, b;
    float	dist[64];
    float	frac;
    glpoly_t	*poly;
//    float	s, t;
    float	tex[2];

    if (numverts > 60)
	Sys_Error ("numverts = %i", numverts);

    BoundPoly (numverts, verts, mins, maxs);

    for (i=0 ; i<3 ; i++)
    {
	m = (mins[i] + maxs[i]) * 0.5;
	m = gl_subdivide_size.value * floor (m/gl_subdivide_size.value + 0.5);
	if (maxs[i] - m < 8)
	    continue;
	if (m - mins[i] < 8)
	    continue;

	// cut it
	v = verts + i;
	for (j=0 ; j<numverts ; j++, v+= 3)
	    dist[j] = *v - m;

	// wrap cases
	dist[j] = dist[0];
	v-=i;
	VectorCopy (verts, v);

	f = b = 0;
	v = verts;
	for (j=0 ; j<numverts ; j++, v+= 3)
	{
	    if (dist[j] >= 0)
	    {
		VectorCopy (v, front[f]);
		f++;
	    }
	    if (dist[j] <= 0)
	    {
		VectorCopy (v, back[b]);
		b++;
	    }
	    if (dist[j] == 0 || dist[j+1] == 0)
		continue;
	    if ( (dist[j] > 0) != (dist[j+1] > 0) )
	    {
		// clip point
		frac = dist[j] / (dist[j] - dist[j+1]);
		for (k=0 ; k<3 ; k++)
		    front[f][k] = back[b][k] = v[k] + frac*(v[3+k] - v[k]);
		f++;
		b++;
	    }
	}

	SubdividePolygon (f, front[0]);
	SubdividePolygon (b, back[0]);
	return;
    }

    poly = Hunk_Alloc(sizeof(glpoly_t));
						
    poly->next = warpface->polys;
    warpface->polys = poly;
    poly->numverts = numverts;
    poly->firstvertex = R_GetNextVertexIndex();
    for (i=0 ; i<numverts ; i++, verts+= 3)
    {
	tex[0] = DotProduct (verts, warpface->texinfo->vecs[0]);
	tex[1] = DotProduct (verts, warpface->texinfo->vecs[1]);
	//Penta: lighmap coords are ignored...
	R_AllocateVertexInTemp(verts,tex,tex);
    }
}

/*
================
GL_SubdivideSurface

Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
void GL_SubdivideSurface (msurface_t *fa)
{
    vec3_t		verts[64];
    int			numverts;
    int			i;
    int			lindex;
    float		*vec;

    warpface = fa;

    //
    // convert edges back to a normal polygon
    //
    numverts = 0;
    for (i=0 ; i<fa->numedges ; i++)
    {
	lindex = loadmodel->surfedges[fa->firstedge + i];

	if (lindex > 0)
	    vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
	else
	    vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
	VectorCopy (vec, verts[numverts]);
	numverts++;
    }

    SubdividePolygon (numverts, verts[0]);
}

//=========================================================



// speed up sin calculations - Ed
float	turbsin[] =
{
#include "gl_warp_sin.h"
};
#define TURBSCALE (256.0 / (2 * M_PI))

/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
=============
*/
void EmitWaterPolys (msurface_t *fa)
{
    glpoly_t	*p;
    float		*v;
    int			i;
    float		s, t, os, ot;


    for (p=fa->polys ; p ; p=p->next)
    {
	glBegin (GL_TRIANGLE_FAN);
	for (i=0,v=(float *)(&globalVertexTable[p->firstvertex]) ; i<p->numverts ; i++, v+=VERTEXSIZE)
	{
	    os = v[3];
	    ot = v[4];

			
	    s = os + (turbsin[(int)((ot*0.125+realtime) * TURBSCALE) & 255])*0.25;
	    s *= (1.0/64);

	    t = ot + (turbsin[(int)((os*0.125+realtime) * TURBSCALE) & 255])*0.25;
	    t *= (1.0/64);

	    glTexCoord2f (s, t);

	    if (gl_mtexable)
		qglMultiTexCoord2fARB (GL_TEXTURE1_ARB, s, t);

	    glVertex3f (v[0],v[1],v[2]);
	}
	glEnd ();
    }
}

float RandomXY(float x, float y, int seedlike)
{
    float ret;
    int n = (int)x+(int)y*seedlike;
    n = (n<<13)^n;
    ret = (1- ( (n * (n * n * 19417 + 189851) + 4967243) & 4945007) / 3354521.0);
    return ret;
}

void EmitMirrorWaterPolys (msurface_t *fa)
{
    glpoly_t	*p;
    float		*v;
    int			i;
    float		s, t, os, ot;

    for (p=fa->polys ; p ; p=p->next)
    {
	glBegin (GL_TRIANGLE_FAN);
	for (i=0,v=(float *)(&globalVertexTable[p->firstvertex]) ; i<p->numverts ; i++, v+=VERTEXSIZE)
	{
	    os = v[0];
	    ot = v[1];

	    //Fake the tex coords a bit so it looks a bit like water

	    s = RandomXY(os,ot,57)*turbsin[(int)((ot*0.125+realtime) * TURBSCALE) & 255];
	    s *=0.25;
	    t = RandomXY(os,ot,63)*turbsin[(int)((ot*0.125+realtime) * TURBSCALE) & 255];
	    t *=0.25;

	    glTexCoord3f (v[0]+s, v[1]+t, v[2]);
	    glVertex3f (v[0],v[1],v[2]);
	}
	glEnd ();
    }
}

void EmitMirrorPolys (msurface_t *fa)
{
    glpoly_t	*p;
    float		*v;
    int			i;

    for (p=fa->polys ; p ; p=p->next)
    {
	glBegin (GL_TRIANGLE_FAN);
	for (i=0,v=(float *)(&globalVertexTable[p->firstvertex]); i<p->numverts ; i++, v+=VERTEXSIZE)
	{
	    qglMultiTexCoord2fARB(GL_TEXTURE1_ARB, v[3]/64, v[4]/64);
	    glTexCoord3fv(v);
	    glVertex3fv(v);
	}
	glEnd ();
    }
}


/*
=============
PENTA:

Binds a water shader
=============
*/
qboolean OverrideFluidTex(char *name)
{
    if (gl_watershader.value < 1) return false;
    if (r_wateralpha.value == 1) return false;

    if (strstr (name, "water")) {
	glEnable(GL_BLEND);
	//glBlendFunc(GL_DST_COLOR,GL_ONE);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	GL_Bind(newwatertex);
	glScalef(0.5,0.5,1);
	glTranslatef(-0.025*realtime,-0.025*realtime,0);

	GL_EnableMultitexture() ;
	GL_Bind(newwatertex);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
	glTranslatef(0.05*realtime,0.05*realtime,0);
	glScalef(0.25,0.25,1);
	return true;

    }
	
    if (strstr (name, "tele")) {
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE,GL_ONE);
	GL_Bind(newteletex);
	glTranslatef(realtime,0.5*realtime,0);
	//glScalef(0.5,0.25,1);
			
	GL_EnableMultitexture() ;
	GL_Bind(newteletex);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
	glTranslatef(-realtime,0.5*realtime,0);
	//glScalef(0.5,0.25,1);
	glFogfv(GL_FOG_COLOR, color_black);
	return true;

    }

    if (strstr (name, "lava")) {
	//lava is never transparent
	glDisable(GL_BLEND);
	GL_Bind(newlavatex);
	glColor4f(1,1,1,1);
	return true;

    }

    if (strstr (name, "slime")) {
	glEnable(GL_BLEND);
	//glBlendFunc(GL_DST_COLOR,GL_ONE);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	GL_Bind(newslimetex);
	glScalef(0.5,0.5,1);
	glTranslatef(-0.05*realtime,-0.05*realtime,0);

	GL_EnableMultitexture() ;
	GL_Bind(newslimetex);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
	glScalef(0.25,0.25,1);
	glTranslatef(0.05*realtime,0.05*realtime,0);
	return true;

    }

    if (strstr (name, "glass")) {
	glEnable(GL_BLEND);
	glBlendFunc(GL_DST_COLOR,GL_ONE);
	GL_Bind(newenvmap);
	glTexGeni(GL_S,GL_TEXTURE_GEN_MODE,GL_SPHERE_MAP);
	glTexGeni(GL_T,GL_TEXTURE_GEN_MODE,GL_SPHERE_MAP);
	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glFogfv(GL_FOG_COLOR, color_black);
	return true;

    }
	
    return false;
}

/*
==================
PENTA:
Loads textures needed for the fluid shaders
==================
*/
void InitShaderTex(void)
{
    newwatertex = EasyTgaLoad("penta/q3water.tga");
    newslimetex = EasyTgaLoad("penta/q3slime.tga");
    newlavatex = EasyTgaLoad("penta/q3lava.tga");
    newteletex = EasyTgaLoad("penta/newtele.tga"); 
    newenvmap = EasyTgaLoad("penta/q3envmap.tga");
}

/*
=============
EmitSkyPolys
=============
*/
void EmitSkyPolys (msurface_t *fa)
{
    glpoly_t	*p;
    float		*v;
    int			i;
    float	s, t;
    vec3_t	dir;
    float	length;

    for (p=fa->polys ; p ; p=p->next)
    {
	glBegin (GL_TRIANGLE_FAN);
	for (i=0,v=(float *)(&globalVertexTable[p->firstvertex]) ; i<p->numverts ; i++, v+=VERTEXSIZE)
	{
	    VectorSubtract (v, r_origin, dir);
	    dir[2] *= 3;	// flatten the sphere

	    length = dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2];
	    length = sqrt (length);
	    length = 6*63/length;

	    dir[0] *= length;
	    dir[1] *= length;

	    s = (speedscale + dir[0]) * (1.0/128);
	    t = (speedscale + dir[1]) * (1.0/128);

	    glTexCoord2f (s, t);
	    glVertex3fv (v);
	}
	glEnd ();
    }
}

/*
===============
EmitBothSkyLayers

Does a sky warp on the pre-fragmented glpoly_t chain
This will be called for brushmodels, the world
will have them chained together.
===============
*/
void EmitBothSkyLayers (msurface_t *fa)
{
    GL_DisableMultitexture();

    GL_Bind (solidskytexture);
    speedscale = realtime*8;
    speedscale -= (int)speedscale & ~127 ;

    EmitSkyPolys (fa);

    glEnable (GL_BLEND);
    GL_Bind (alphaskytexture);
    speedscale = realtime*16;
    speedscale -= (int)speedscale & ~127 ;

    EmitSkyPolys (fa);

//	glDisable (GL_BLEND);
}

//#ifndef QUAKE2
/*
=================
R_DrawSkyChain
=================
*/
/*
void R_DrawSkyChain (msurface_t *s)
{
    msurface_t	*fa;

    GL_DisableMultitexture();

    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    // used when gl_texsort is on
    GL_Bind(solidskytexture);
    speedscale = realtime*8;
    speedscale -= (int)speedscale & ~127 ;

    for (fa=s ; fa ; fa=fa->texturechain)
	EmitSkyPolys (fa);

    glEnable (GL_BLEND);


    GL_Bind (alphaskytexture);
    speedscale = realtime*16;
    speedscale -= (int)speedscale & ~127 ;

    for (fa=s ; fa ; fa=fa->texturechain)
	EmitSkyPolys (fa);

    //glBlendFunc(GL_ZERO,GL_SRC_COLOR);
    glDisable (GL_BLEND);
}

#endif
*/

/*
=================================================================

  Quake 2 environment sky

=================================================================
*/

#ifdef QUAKE2


#define	SKY_TEX		2000

/*
=================================================================

  PCX Loading

=================================================================
*/

typedef struct
{
    char	manufacturer;
    char	version;
    char	encoding;
    char	bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hres,vres;
    unsigned char	palette[48];
    char	reserved;
    char	color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char	filler[58];
    unsigned 	data;			// unbounded
} pcx_t;

byte	*pcx_rgb;

/*
============
LoadPCX
============
*/
void LoadPCX (FILE *f)
{
    pcx_t	*pcx, pcxbuf;
    byte	palette[768];
    byte	*pix;
    int		x, y;
    int		dataByte, runLength;
    int		count;

//
// parse the PCX file
//
    fread (&pcxbuf, 1, sizeof(pcxbuf), f);

    pcx = &pcxbuf;

    if (pcx->manufacturer != 0x0a
	|| pcx->version != 5
	|| pcx->encoding != 1
	|| pcx->bits_per_pixel != 8
	|| pcx->xmax >= 320
	|| pcx->ymax >= 256)
    {
	Con_Printf ("Bad pcx file\n");
	return;
    }

    // seek to palette
    fseek (f, -768, SEEK_END);
    fread (palette, 1, 768, f);

    fseek (f, sizeof(pcxbuf) - 4, SEEK_SET);

    count = (pcx->xmax+1) * (pcx->ymax+1);
    pcx_rgb = malloc( count * 4);

    for (y=0 ; y<=pcx->ymax ; y++)
    {
	pix = pcx_rgb + 4*y*(pcx->xmax+1);
	for (x=0 ; x<=pcx->ymax ; )
	{
	    dataByte = fgetc(f);

	    if((dataByte & 0xC0) == 0xC0)
	    {
		runLength = dataByte & 0x3F;
		dataByte = fgetc(f);
	    }
	    else
		runLength = 1;

	    while(runLength-- > 0)
	    {
		pix[0] = palette[dataByte*3];
		pix[1] = palette[dataByte*3+1];
		pix[2] = palette[dataByte*3+2];
		pix[3] = 255;
		pix += 4;
		x++;
	    }
	}
    }
}

#endif

//PENTA: removed this from the ifdef QUAKE2
/*
=========================================================

TARGA LOADING

=========================================================
*/

typedef struct _TargaHeader
{
    unsigned char 	id_length, colormap_type, image_type;
    unsigned short	colormap_index, colormap_length;
    unsigned char	colormap_size;
    unsigned short	x_origin, y_origin, width, height;
    unsigned char	pixel_size, attributes;
} TargaHeader;


TargaHeader		targa_header;
byte			*targa_rgba;

int fgetLittleShort (FILE *f)
{
    byte	b1, b2;

    b1 = fgetc(f);
    b2 = fgetc(f);

    return (short)(b1 + b2*256);
}

int fgetLittleLong (FILE *f)
{
    byte	b1, b2, b3, b4;

    b1 = fgetc(f);
    b2 = fgetc(f);
    b3 = fgetc(f);
    b4 = fgetc(f);

    return b1 + (b2<<8) + (b3<<16) + (b4<<24);
}

static char argh[MAX_OSPATH];

// Proper replacement for LoadTGA and friends
int LoadTexture(char* filename, int size)
{
    FILE	*f;
    char* tmp;

    // Check for *.png first, then *.jpg, fall back to *.tga if not found...
    // first replace the last three letters with png (yeah, hack)
    strncpy(argh, filename,sizeof(argh));
    tmp = argh + strlen(filename) - 3;
#ifndef NO_PNG
    tmp[0] = 'p';
    tmp[1] = 'n';
    tmp[2] = 'g';
    COM_FOpenFile (argh, &f);
    if ( f )
    {
	png_infop info_ptr;
	png_structp png_ptr;
        char* mem;
        unsigned long width, height;
        
	int bit_depth;
	int color_type;
	unsigned char** rows;
        int i;
//        png_color_16 background = {0, 0, 0};
//        png_color_16p image_background;
	
	Con_DPrintf("Loading %s\n", argh);
	
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	info_ptr = png_create_info_struct(png_ptr);
	png_init_io(png_ptr, f);
        png_set_sig_bytes(png_ptr, 0);
	
	png_read_info(png_ptr, info_ptr);
	png_get_IHDR(png_ptr, info_ptr, &width, &height,
		     &bit_depth, &color_type, 0, 0, 0);
	
	
	// Allocate memory and get data there
	mem = malloc(size*height*width);
	rows = malloc(height*sizeof(char*));

        if ( bit_depth == 16)
            png_set_strip_16(png_ptr);
        
        if ( color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8 )
            png_set_gray_1_2_4_to_8(png_ptr);

        png_set_gamma(png_ptr, 1.0, 1.0);



        if ( size == 4 )
	{
            if ( color_type & PNG_COLOR_MASK_PALETTE )
                png_set_palette_to_rgb(png_ptr);

            if ( png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS) )
                png_set_tRNS_to_alpha(png_ptr);

            if ( (color_type & PNG_COLOR_MASK_COLOR) == 0 )
                png_set_gray_to_rgb(png_ptr);

            if ( (color_type & PNG_COLOR_MASK_ALPHA) == 0 )
                png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
	}
	else
	{          
            if ( color_type & PNG_COLOR_MASK_ALPHA )
                png_set_strip_alpha(png_ptr);

            if ( color_type & PNG_COLOR_MASK_PALETTE )
            {
                png_set_palette_to_rgb(png_ptr);   

            }

            if ( color_type & PNG_COLOR_MASK_COLOR )
                png_set_rgb_to_gray_fixed(png_ptr, 1, -1, -1);
        }
	
	for ( i = 0; i < height; i++ )
	{
	    rows[i] = ((unsigned char*) mem) + size * width * i;
	}
	png_read_image(png_ptr, rows);
	
	png_destroy_read_struct(&png_ptr, &info_ptr, 0);
	free(rows);        
	targa_header.width = width;
	targa_header.height = height;
        targa_rgba = (byte*) mem;
        fclose(f);
	return 1;
    }
#endif

    tmp[0] = 't';
    tmp[1] = 'g';
    tmp[2] = 'a';
    COM_FOpenFile (argh, &f);
    if (!f)
    {
	return 0;
    }
    LoadTGA(f);
    return 1;
}


int LoadTextureInPlace(char* filename, int size, byte* mem, int* width, int* height)
{
    FILE	*f;
    char* tmp;

    // Check for *.png first, then *.jpg, fall back to *.tga if not found...
    // first replace the last three letters with png (yeah, hack)
    strncpy(argh, filename,sizeof(argh));
    tmp = argh + strlen(filename) - 3;
#ifndef NO_PNG
    tmp[0] = 'p';
    tmp[1] = 'n';
    tmp[2] = 'g';
    COM_FOpenFile (argh, &f);
    if ( f )
    {
	png_infop info_ptr;
	png_structp png_ptr;
	int bit_depth;
	int color_type;
        unsigned long mywidth, myheight;
        
	unsigned char** rows;
        int i;
//        png_color_16 background = {0, 0, 0};
//        png_color_16p image_background;
	
	Con_DPrintf("Loading %s\n", argh);
	
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	info_ptr = png_create_info_struct(png_ptr);
	png_init_io(png_ptr, f);
        png_set_sig_bytes(png_ptr, 0);
	
	png_read_info(png_ptr, info_ptr);
	png_get_IHDR(png_ptr, info_ptr, &mywidth, &myheight,
		     &bit_depth, &color_type, 0, 0, 0);
	*width=(int)mywidth;
    *height=(int)myheight;
        
	
	// Allocate memory and get data there
	rows = malloc(*height*sizeof(char*));

        if ( bit_depth == 16)
            png_set_strip_16(png_ptr);
        
        if ( color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8 )
            png_set_gray_1_2_4_to_8(png_ptr);

        png_set_gamma(png_ptr, 1.0, 1.0);



        if ( size == 4 )
	{
            if ( color_type & PNG_COLOR_MASK_PALETTE )
                png_set_palette_to_rgb(png_ptr);

            if ( png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS) )
                png_set_tRNS_to_alpha(png_ptr);

            if ( (color_type & PNG_COLOR_MASK_COLOR) == 0 )
                png_set_gray_to_rgb(png_ptr);

            if ( (color_type & PNG_COLOR_MASK_ALPHA) == 0 )
                png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
	}
	else
	{          
            if ( color_type & PNG_COLOR_MASK_ALPHA )
                png_set_strip_alpha(png_ptr);

            if ( color_type & PNG_COLOR_MASK_PALETTE )
            {
                png_set_palette_to_rgb(png_ptr);   

            }

            if ( color_type & PNG_COLOR_MASK_COLOR )
                png_set_rgb_to_gray_fixed(png_ptr, 1, -1, -1);
        }
	
	for ( i = 0; i < *height; i++ )
	{
	    rows[i] = mem + size * *width * i;
	}
	png_read_image(png_ptr, rows);
	
	png_destroy_read_struct(&png_ptr, &info_ptr, 0);
	free(rows);        
        fclose(f);
	return 1;
    }
#endif
    tmp[0] = 't';
    tmp[1] = 'g';
    tmp[2] = 'a';
    COM_FOpenFile (argh, &f);
    if (!f)
    {
//		Con_SafePrintf ("Couldn't load %s\n", argh);
	return 0;
    }
    if ( size == 4 )
	LoadColorTGA(f, mem, width, height, argh);
    else
	LoadGrayTGA(f, mem, width, height, argh);
    return 1;
}

/*
=============
LoadTGA
=============
*/
void LoadTGA (FILE *fin)
{
    int				columns, rows, numPixels;
    byte			*pixbuf;
    int				row, column;
	int				row_inc;

    targa_header.id_length = fgetc(fin);
    targa_header.colormap_type = fgetc(fin);
    targa_header.image_type = fgetc(fin);
	
    targa_header.colormap_index = fgetLittleShort(fin);
    targa_header.colormap_length = fgetLittleShort(fin);
    targa_header.colormap_size = fgetc(fin);
    targa_header.x_origin = fgetLittleShort(fin);
    targa_header.y_origin = fgetLittleShort(fin);
    targa_header.width = fgetLittleShort(fin);
    targa_header.height = fgetLittleShort(fin);
    targa_header.pixel_size = fgetc(fin);
    targa_header.attributes = fgetc(fin);

    if (targa_header.image_type!=2 
	&& targa_header.image_type!=10) 
	Sys_Error ("LoadTGA: Only type 2 and 10 targa RGB images supported\n");

    if (targa_header.colormap_type !=0 
	|| (targa_header.pixel_size!=32 && targa_header.pixel_size!=24))
	Sys_Error ("Texture_LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n");

    columns = targa_header.width;
    rows = targa_header.height;
    numPixels = columns * rows;

    targa_rgba = malloc (numPixels*4);
	
    if (targa_header.id_length != 0)
	fseek(fin, targa_header.id_length, SEEK_CUR);  // skip TARGA image comment
	
	// If bit 5 of attributes isn't set, the image has been stored from bottom to top
	if ((targa_header.attributes & 0x20) == 0)
	{
		pixbuf = targa_rgba + (rows - 1)*columns*4;
		row_inc = -columns*4*2;
	}
	else
	{
		pixbuf = targa_rgba;
		row_inc = 0;
	}

    if (targa_header.image_type==2) {  // Uncompressed, RGB images
	for(row=rows-1; row>=0; row--, pixbuf += row_inc) {
	    for(column=0; column<columns; column++) {
		unsigned char red,green,blue,alphabyte;
		switch (targa_header.pixel_size) {
		case 24:
							
		    blue = getc(fin);
		    green = getc(fin);
		    red = getc(fin);
		    *pixbuf++ = red;
		    *pixbuf++ = green;
		    *pixbuf++ = blue;
		    *pixbuf++ = 255;
		    break;
		case 32:
		    blue = getc(fin);
		    green = getc(fin);
		    red = getc(fin);
		    alphabyte = getc(fin);
		    *pixbuf++ = red;
		    *pixbuf++ = green;
		    *pixbuf++ = blue;
		    *pixbuf++ = alphabyte;
		    break;
		}
	    }
	}
    }
    else if (targa_header.image_type==10) {   // Runlength encoded RGB images
	unsigned char red = 0x00,green = 0x00,blue = 0x00,alphabyte = 0x00,packetHeader,packetSize,j;
	for(row=rows-1; row>=0; row--, pixbuf += row_inc) {
	    for(column=0; column<columns; ) {
		packetHeader=getc(fin);
		packetSize = 1 + (packetHeader & 0x7f);
		if (packetHeader & 0x80) {        // run-length packet
		    switch (targa_header.pixel_size) {
		    case 24:
			blue = getc(fin);
			green = getc(fin);
			red = getc(fin);
			alphabyte = 255;
			break;
		    case 32:
			blue = getc(fin);
			green = getc(fin);
			red = getc(fin);
			alphabyte = getc(fin);
			break;
		    }
	
		    for(j=0;j<packetSize;j++) {
			*pixbuf++=red;
			*pixbuf++=green;
			*pixbuf++=blue;
			*pixbuf++=alphabyte;
			column++;
			if (column==columns) { // run spans across rows
			    column=0;
			    if (row>0) {
				row--;
				pixbuf += row_inc;
				}
			    else
				goto breakOut;
			}
		    }
		}
		else {                            // non run-length packet
		    for(j=0;j<packetSize;j++) {
			switch (targa_header.pixel_size) {
			case 24:
			    blue = getc(fin);
			    green = getc(fin);
			    red = getc(fin);
			    *pixbuf++ = red;
			    *pixbuf++ = green;
			    *pixbuf++ = blue;
			    *pixbuf++ = 255;
			    break;
			case 32:
			    blue = getc(fin);
			    green = getc(fin);
			    red = getc(fin);
			    alphabyte = getc(fin);
			    *pixbuf++ = red;
			    *pixbuf++ = green;
			    *pixbuf++ = blue;
			    *pixbuf++ = alphabyte;
			    break;
			}
			column++;
			if (column==columns) { // pixel packet run spans across rows
			    column=0;
			    if (row>0) {
				row--;
				pixbuf += row_inc;
				}
			    else
				goto breakOut;
			}						
		    }
		}
	    }
	breakOut:;
	}
    }
	
    fclose(fin);
}

/*
=============
LoadColorTGA
PENTA: loads a color tga (rgb/rgba/or quake palette)
returns width & height in the references.
=============
*/
void LoadColorTGA (FILE *fin, byte *pixels, int *width, int *height, char* fname)
{
    int				columns, rows, numPixels;
    byte			*pixbuf;
    int				row, column;
	int				row_inc;

    targa_header.id_length = fgetc(fin);
    targa_header.colormap_type = fgetc(fin);
    targa_header.image_type = fgetc(fin);
	
    targa_header.colormap_index = fgetLittleShort(fin);
    targa_header.colormap_length = fgetLittleShort(fin);
    targa_header.colormap_size = fgetc(fin);
    targa_header.x_origin = fgetLittleShort(fin);
    targa_header.y_origin = fgetLittleShort(fin);
    targa_header.width = fgetLittleShort(fin);
    targa_header.height = fgetLittleShort(fin);
    targa_header.pixel_size = fgetc(fin);
    targa_header.attributes = fgetc(fin);

    if (targa_header.image_type!=2 
	&& targa_header.image_type!=10 && targa_header.image_type!=1) 
	Sys_Error ("LoadTGA: Only type 1, 2 and 10 targa images supported\n%s\n",fname);

    /*
      if (targa_header.colormap_type !=0 
      || (targa_header.pixel_size!=32 && targa_header.pixel_size!=24))
      Sys_Error ("Texture_LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n");
    */
    if (targa_header.image_type==1 && targa_header.pixel_size != 8 && 
	targa_header.colormap_size != 24 && targa_header.colormap_length != 256)
	Sys_Error ("LoadGrayTGA: Strange palette type\n");

    columns = targa_header.width;
    rows = targa_header.height;
    numPixels = columns * rows;

    targa_rgba = pixels;
	
    if (targa_header.id_length != 0)
	fseek(fin, targa_header.id_length, SEEK_CUR);  // skip TARGA image comment
	
	// If bit 5 of attributes isn't set, the image has been stored from bottom to top
	if ((targa_header.attributes & 0x20) == 0)
	{
		pixbuf = targa_rgba + (rows - 1)*columns*4;
		row_inc = -columns*4*2;
	}
	else
	{
		pixbuf = targa_rgba;
		row_inc = 0;
	}

    if (targa_header.image_type==1) {  // Color mapped
	fseek(fin, 768, SEEK_CUR);  // skip palette
	for(row=rows-1; row>=0; row--, pixbuf += row_inc) {
	    for(column=0; column<columns; column++) {
		unsigned char index;
		index = getc(fin);
		*(int *)pixbuf = d_8to24table[index];
		pixbuf+=4;
	    }
	}
    } else if (targa_header.image_type==2) {  // Uncompressed, RGB images
	for(row=rows-1; row>=0; row--, pixbuf += row_inc) {
	    for(column=0; column<columns; column++) {
		unsigned char red,green,blue,alphabyte;
		switch (targa_header.pixel_size) {
		case 24:
							
		    blue = getc(fin);
		    green = getc(fin);
		    red = getc(fin);
		    *pixbuf++ = red;
		    *pixbuf++ = green;
		    *pixbuf++ = blue;
		    *pixbuf++ = 255;
		    break;
		case 32:
		    blue = getc(fin);
		    green = getc(fin);
		    red = getc(fin);
		    alphabyte = getc(fin);
		    *pixbuf++ = red;
		    *pixbuf++ = green;
		    *pixbuf++ = blue;
		    *pixbuf++ = alphabyte;
		    break;
		}
	    }
	}
    }
    else if (targa_header.image_type==10) {   // Runlength encoded RGB images
	unsigned char red = 0x00,green = 0x00,blue = 0x00,alphabyte = 0x00,packetHeader,packetSize,j;
	for(row=rows-1; row>=0; row--, pixbuf += row_inc) {
	    for(column=0; column<columns; ) {
		packetHeader=getc(fin);
		packetSize = 1 + (packetHeader & 0x7f);
		if (packetHeader & 0x80) {        // run-length packet
		    switch (targa_header.pixel_size) {
		    case 24:
			blue = getc(fin);
			green = getc(fin);
			red = getc(fin);
			alphabyte = 255;
			break;
		    case 32:
			blue = getc(fin);
			green = getc(fin);
			red = getc(fin);
			alphabyte = getc(fin);
			break;
		    }
	
		    for(j=0;j<packetSize;j++) {
			*pixbuf++=red;
			*pixbuf++=green;
			*pixbuf++=blue;
			*pixbuf++=alphabyte;
			column++;
			if (column==columns) { // run spans across rows
			    column=0;
			    if (row>0) {
				row--;
				pixbuf += row_inc;
				}
			    else
				goto breakOut;
			}
		    }
		}
		else {                            // non run-length packet
		    for(j=0;j<packetSize;j++) {
			switch (targa_header.pixel_size) {
			case 24:
			    blue = getc(fin);
			    green = getc(fin);
			    red = getc(fin);
			    *pixbuf++ = red;
			    *pixbuf++ = green;
			    *pixbuf++ = blue;
			    *pixbuf++ = 255;
			    break;
			case 32:
			    blue = getc(fin);
			    green = getc(fin);
			    red = getc(fin);
			    alphabyte = getc(fin);
			    *pixbuf++ = red;
			    *pixbuf++ = green;
			    *pixbuf++ = blue;
			    *pixbuf++ = alphabyte;
			    break;
			}
			column++;
			if (column==columns) { // pixel packet run spans across rows
			    column=0;
			    if (row>0) {
				row--;
				pixbuf += row_inc;
				}
			    else
				goto breakOut;
			}						
		    }
		}
	    }
	breakOut:;
	}
    }
	
    //we didn't load a paletted image so we still have to fix the gamma
    /*
      Just let the uses specify the gamma in the image editor
      if (targa_header.image_type != 1) {
      int i;
      float f, inf;
      for (i=0 ; i<numPixels*4; i++)
      {
      f = pow ( (targa_rgba[i]+1)/256.0 , 0.7 );
      inf = f*255 + 0.5;
      if (inf < 0)
      inf = 0;
      if (inf > 255)
      inf = 255;
      targa_rgba[i] = inf;
      }
      }
    */
    fclose(fin);
    *width = targa_header.width;
    *height = targa_header.height;
}

/*
=============
LoadGrayTGA
PENTA: Load a grayscale tga for bump maps
Copies the result to pixbuf (bytes not rgb) and puts the width & height in the references.
=============
*/
void LoadGrayTGA (FILE *fin,byte *pixels,int *width, int *height, char *fname)
{
    int				columns, rows, numPixels;
    int				row, column;
    byte			*pixbuf;
	int				row_inc;

    targa_header.id_length = fgetc(fin);
    targa_header.colormap_type = fgetc(fin);
    targa_header.image_type = fgetc(fin);
	
    targa_header.colormap_index = fgetLittleShort(fin);
    targa_header.colormap_length = fgetLittleShort(fin);
    targa_header.colormap_size = fgetc(fin);
    targa_header.x_origin = fgetLittleShort(fin);
    targa_header.y_origin = fgetLittleShort(fin);
    targa_header.width = fgetLittleShort(fin);
    targa_header.height = fgetLittleShort(fin);
    targa_header.pixel_size = fgetc(fin);
    targa_header.attributes = fgetc(fin);

    if (targa_header.image_type!=1 
	&& targa_header.image_type!=3) 
	Sys_Error ("LoadGrayTGA: Only type 1 and 3 targa images supported for bump maps.\n%s\n",fname);

    if (targa_header.image_type==1 && targa_header.pixel_size != 8 && 
	targa_header.colormap_size != 24 && targa_header.colormap_length != 256)
	Sys_Error ("LoadGrayTGA: Strange palette type\n");

    columns = targa_header.width;
    rows = targa_header.height;
    numPixels = columns * rows;

	// If bit 5 of attributes isn't set, the image has been stored from bottom to top
	if ((targa_header.attributes & 0x20) == 0)
	{
		pixbuf = pixels + (rows - 1)*columns;
		row_inc = -columns*2;
	}
	else
	{
		pixbuf = pixels;
		row_inc = 0;
	}
	
    if (targa_header.id_length != 0)
	fseek(fin, targa_header.id_length, SEEK_CUR);  // skip TARGA image comment
	
    if (targa_header.image_type == 1) {
	fseek(fin, 768, SEEK_CUR);//skip palette 
    }

    //fread(pixbuf,1,numPixels,fin);	
    for(row=rows-1; row>=0; row--, pixbuf += row_inc) {
	for(column=0; column<columns; column++) {
	    *pixbuf++= getc(fin);
	}
    }

    fclose(fin);
    *width = targa_header.width;
    *height = targa_header.height;
}
/*
==================
PENTA:
Loads the tga in filename and returns the texture object
in gl texture object.
==================
*/
int EasyTgaLoad(char *filename)
{
    FILE	*f;
    int			texturemode;
    unsigned long width, height;
    char* tmp;
    char* mem;
        
    if ( gl_texcomp && ((int)gl_compress_textures.value) & 1 )
    {
	texturemode = GL_COMPRESSED_RGBA_ARB;
    }
    else
    {
	texturemode = GL_RGBA8;
    }

    GL_Bind (texture_extension_number);
    // Check for *.png first, then *.jpg, fall back to *.tga if not found...
    // first replace the last three letters with png (yeah, hack)
    strncpy(argh, filename,sizeof(argh));
    tmp = argh + strlen(filename) - 3;
#ifndef NO_PNG
    tmp[0] = 'p';
    tmp[1] = 'n';
    tmp[2] = 'g';
    COM_FOpenFile (argh, &f);
    if ( f )
    {
        png_infop info_ptr;
        png_structp png_ptr;
        int bit_depth;
        int color_type;
        unsigned char** rows;
        int i;
//        png_color_16 background = {0, 0, 0};
//        png_color_16p image_background;

        Con_DPrintf("Loading %s\n", argh);
	
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	info_ptr = png_create_info_struct(png_ptr);
	png_init_io(png_ptr, f);
        png_set_sig_bytes(png_ptr, 0);
	
	png_read_info(png_ptr, info_ptr);
	png_get_IHDR(png_ptr, info_ptr, &width, &height,
		     &bit_depth, &color_type, 0, 0, 0);
	
	
	// Allocate memory and get data there
	mem = malloc(4*height*width);
	rows = malloc(height*sizeof(char*));

        if ( bit_depth == 16)
            png_set_strip_16(png_ptr);
        
        if ( color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8 )
            png_set_gray_1_2_4_to_8(png_ptr);

        png_set_gamma(png_ptr, 1.0, 1.0);



        if ( color_type & PNG_COLOR_MASK_PALETTE )
            png_set_palette_to_rgb(png_ptr);

        if ( png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS) )
            png_set_tRNS_to_alpha(png_ptr);

        if ( (color_type & PNG_COLOR_MASK_COLOR) == 0 )
            png_set_gray_to_rgb(png_ptr);

        if ( (color_type & PNG_COLOR_MASK_ALPHA) == 0 )
            png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);


	for ( i = 0; i < height; i++ )
	{
	    rows[i] = ((unsigned char*) mem) + 4 * width * i;
	}
	png_read_image(png_ptr, rows);
	
	png_destroy_read_struct(&png_ptr, &info_ptr, 0);
	free(rows);        
        fclose(f);
    }
    else
#endif
    {
        tmp[0] = 't';
        tmp[1] = 'g';
        tmp[2] = 'a';
	COM_FOpenFile (argh, &f);
	if (!f)
	{
	    Con_SafePrintf ("Couldn't load %s\n", argh);
	    return 0;
	}
	LoadTGA (f);
        width = targa_header.width;
        height = targa_header.height;
        mem = (char*) targa_rgba;
    } 
    GL_TexImage2D (GL_TEXTURE_2D, 0, texturemode, (GLsizei)width, (GLsizei)height, 0, GL_RGBA, GL_UNSIGNED_BYTE, mem);
    free (mem);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    texture_extension_number++;
    return texture_extension_number-1;
}

//#ifdef QUAKE2 PENTA: enable skyboxes again

#define SKY_TEX 10000

char	skybox_name[64];
float skybox_cloudspeed;
qboolean skybox_hasclouds;

/*
==================
R_LoadSkys
==================
*/
char	*suf[7] = {"rt", "bk", "lf", "ft", "up", "dn","tile"};
void R_LoadSkys (void)
{
    int		i;
//    FILE	*f;
    char	name[64];

    skybox_hasclouds = true;

    for (i=0 ; i<7 ; i++)
    {
	sprintf (name, "env/%s%s.tga", skybox_name, suf[i]);
//		Con_Printf("Loading file: %s\n",name);

	if (!LoadTexture(name, 4))
	{
	    if (i == 6) {
		skybox_hasclouds = false;
	    } else {
		Con_Printf ("Couldn't load %s\n", name);
	    }
	    continue;
	}
	else
	{
	    GL_Bind (SKY_TEX + i);
	    GL_TexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, targa_header.width, targa_header.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, targa_rgba);

	    free (targa_rgba);

	    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
    }
}


vec3_t	skyclip[6] =
{
    {1,1,0},
    {1,-1,0},
    {0,-1,1},
    {0,1,1},
    {1,0,1},
    {-1,0,1} 
};
int	c_sky;

// 1 = s, 2 = t, 3 = 2048
int	st_to_vec[6][3] =
{
    {3,-1,2},
    {-3,1,2},

    {1,3,2},
    {-1,-3,2},

    {-2,-1,3},		// 0 degrees yaw, look straight up
    {2,-1,-3}		// look straight down

//	{-1,2,3},
//	{1,2,-3}
};

// s = [0]/[2], t = [1]/[2]
int	vec_to_st[6][3] =
{
    {-2,3,1},
    {2,3,-1},

    {1,3,2},
    {-1,3,-2},

    {-2,-1,3},
    {-2,1,-3}

//	{-1,2,3},
//	{1,2,-3}
};

float	skymins[2][6], skymaxs[2][6];

void DrawSkyPolygon (int nump, vec3_t vecs)
{
    int		i,j;
    vec3_t	v, av;
    float	s, t, dv;
    int		axis;
    float	*vp;

    c_sky++;
#if 0
    glBegin (GL_POLYGON);
    for (i=0 ; i<nump ; i++, vecs+=3)
    {
	VectorAdd(vecs, r_origin, v);
	glVertex3fv (v);
    }
    glEnd();
    return;
#endif
    // decide which face it maps to
    VectorCopy (vec3_origin, v);
    for (i=0, vp=vecs ; i<nump ; i++, vp+=3)
    {
	VectorAdd (vp, v, v);
    }
    av[0] = fabs(v[0]);
    av[1] = fabs(v[1]);
    av[2] = fabs(v[2]);
    if (av[0] > av[1] && av[0] > av[2])
    {
	if (v[0] < 0)
	    axis = 1;
	else
	    axis = 0;
    }
    else if (av[1] > av[2] && av[1] > av[0])
    {
	if (v[1] < 0)
	    axis = 3;
	else
	    axis = 2;
    }
    else
    {
	if (v[2] < 0)
	    axis = 5;
	else
	    axis = 4;
    }

    // project new texture coords
    for (i=0 ; i<nump ; i++, vecs+=3)
    {
	j = vec_to_st[axis][2];
	if (j > 0)
	    dv = vecs[j - 1];
	else
	    dv = -vecs[-j - 1];

	j = vec_to_st[axis][0];
	if (j < 0)
	    s = -vecs[-j -1] / dv;
	else
	    s = vecs[j-1] / dv;
	j = vec_to_st[axis][1];
	if (j < 0)
	    t = -vecs[-j -1] / dv;
	else
	    t = vecs[j-1] / dv;

	if (s < skymins[0][axis])
	    skymins[0][axis] = s;
	if (t < skymins[1][axis])
	    skymins[1][axis] = t;
	if (s > skymaxs[0][axis])
	    skymaxs[0][axis] = s;
	if (t > skymaxs[1][axis])
	    skymaxs[1][axis] = t;
    }
}

#define	MAX_CLIP_VERTS	64
void ClipSkyPolygon (int nump, vec3_t vecs, int stage)
{
    float	*norm;
    float	*v;
    qboolean	front, back;
    float	d, e;
    float	dists[MAX_CLIP_VERTS];
    int		sides[MAX_CLIP_VERTS];
    vec3_t	newv[2][MAX_CLIP_VERTS];
    int		newc[2];
    int		i, j;

    if (nump > MAX_CLIP_VERTS-2)
	Sys_Error ("ClipSkyPolygon: MAX_CLIP_VERTS");
    if (stage == 6)
    {	// fully clipped, so draw it
	DrawSkyPolygon (nump, vecs);
	return;
    }

    front = back = false;
    norm = skyclip[stage];
    for (i=0, v = vecs ; i<nump ; i++, v+=3)
    {
	d = DotProduct (v, norm);
	if (d > ON_EPSILON)
	{
	    front = true;
	    sides[i] = SIDE_FRONT;
	}
	else if (d < ON_EPSILON)
	{
	    back = true;
	    sides[i] = SIDE_BACK;
	}
	else
	    sides[i] = SIDE_ON;
	dists[i] = d;
    }

    if (!front || !back)
    {	// not clipped
	ClipSkyPolygon (nump, vecs, stage+1);
	return;
    }

    // clip it
    sides[i] = sides[0];
    dists[i] = dists[0];
    VectorCopy (vecs, (vecs+(i*3)) );
    newc[0] = newc[1] = 0;

    for (i=0, v = vecs ; i<nump ; i++, v+=3)
    {
	switch (sides[i])
	{
	case SIDE_FRONT:
	    VectorCopy (v, newv[0][newc[0]]);
	    newc[0]++;
	    break;
	case SIDE_BACK:
	    VectorCopy (v, newv[1][newc[1]]);
	    newc[1]++;
	    break;
	case SIDE_ON:
	    VectorCopy (v, newv[0][newc[0]]);
	    newc[0]++;
	    VectorCopy (v, newv[1][newc[1]]);
	    newc[1]++;
	    break;
	}

	if (sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
	    continue;

	d = dists[i] / (dists[i] - dists[i+1]);
	for (j=0 ; j<3 ; j++)
	{
	    e = v[j] + d*(v[j+3] - v[j]);
	    newv[0][newc[0]][j] = e;
	    newv[1][newc[1]][j] = e;
	}
	newc[0]++;
	newc[1]++;
    }

    // continue
    ClipSkyPolygon (newc[0], newv[0][0], stage+1);
    ClipSkyPolygon (newc[1], newv[1][0], stage+1);
}

/*
=================
R_DrawSkyChain
=================
*/
void R_DrawSkyChain (msurface_t *s)
{
    msurface_t	*fa;

    int		i;
    vec3_t	verts[MAX_CLIP_VERTS];
    glpoly_t	*p;
    float		*v;

    c_sky = 0;
    GL_Bind(solidskytexture);

    // calculate vertex values for sky box


    for (fa=s ; fa ; fa=fa->texturechain)
    {
	for (p=fa->polys ; p ; p=p->next)
	{
	    v = (float *)(&globalVertexTable[p->firstvertex]);

	    for (i=0 ; i<p->numverts ; i++, v+=VERTEXSIZE)
	    {
		VectorSubtract (v, r_origin, verts[i]);
	    }
	    ClipSkyPolygon (p->numverts, v, 0);
	}
    }
}


/*
==============
R_ClearSkyBox
==============
*/
void R_ClearSkyBox (void)
{
    int		i;

    for (i=0 ; i<6 ; i++)
    {
	skymins[0][i] = skymins[1][i] = 9999;
	skymaxs[0][i] = skymaxs[1][i] = -9999;
    }
}


void MakeSkyVec (float s, float t, int axis)
{
    vec3_t		v, b;
    int			j, k;

    b[0] = s*1024;
    b[1] = t*1024;
    b[2] = 1024;

    for (j=0 ; j<3 ; j++)
    {
	k = st_to_vec[axis][j];
	if (k < 0)
	    v[j] = -b[-k - 1];
	else
	    v[j] = b[k - 1];
	v[j] += r_origin[j];
    }

    // avoid bilerp seam
    s = (s+1)*0.5;
    t = (t+1)*0.5;

    if (s < 1.0/512)
	s = 1.0/512;
    else if (s > 511.0/512)
	s = 511.0/512;
    if (t < 1.0/512)
	t = 1.0/512;
    else if (t > 511.0/512)
	t = 511.0/512;

    t = 1.0 - t;
    glTexCoord2f (s, t);
    glVertex3fv (v);
}

/*
==============
R_DrawSkyBox
==============
*/
int	skytexorder[6] = {0,2,1,3,4,5};
void R_DrawSkyBox (void)
{
    int		i; //, j, k;
//    vec3_t	v;
//    float	s, t;

    if (skytexturenum >= 0) {
	//	glColor3f(1,1,1);
	if (!cl.worldmodel->textures[skytexturenum]->texturechain) return;
	//	R_DrawSkyChain (cl.worldmodel->textures[skytexturenum]->texturechain);
	cl.worldmodel->textures[skytexturenum]->texturechain = NULL;
    }

    glDepthMask(0);

    if (fog_enabled.value && !gl_wireframe.value) 
	glDisable(GL_FOG);

    for (i=0 ; i<6 ; i++)
    {
	/*
	  if (skymins[0][i] >= skymaxs[0][i]
	  || skymins[1][i] >= skymaxs[1][i])
	  continue;
	*/
	GL_Bind (SKY_TEX+skytexorder[i]);

	skymins[0][i] = -1;
	skymins[1][i] = -1;
	skymaxs[0][i] = 1;
	skymaxs[1][i] = 1;

	glBegin (GL_QUADS);
	MakeSkyVec (skymins[0][i], skymins[1][i], i);
	MakeSkyVec (skymins[0][i], skymaxs[1][i], i);
	MakeSkyVec (skymaxs[0][i], skymaxs[1][i], i);
	MakeSkyVec (skymaxs[0][i], skymins[1][i], i);
	glEnd ();
    }

    if (fog_enabled.value && !gl_wireframe.value) 
	glEnable(GL_FOG);

    if (!skybox_hasclouds) {
	glDepthMask(1);		
	return;
    }

    GL_Bind (SKY_TEX+6);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);

    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glTranslatef(cl.time*skybox_cloudspeed,0,0);
    glColor4ub(255,255,255,255);
    glBegin(GL_TRIANGLE_FAN);
    glTexCoord2f(0,0);
    glVertex3f(-800.0 + r_origin[0], -800.0 + r_origin[1], 20.0 + r_origin[2]);
		
    glTexCoord2f(0,40);
    glVertex3f(800.0 + r_origin[0], -800.0 + r_origin[1], 20.0 + r_origin[2]);

    glTexCoord2f(40,40);
    glVertex3f(800.0 + r_origin[0], 800.0 + r_origin[1], 20.0 + r_origin[2]);

    glTexCoord2f(40,0);
    glVertex3f(-800.0 + r_origin[0], 800.0 + r_origin[1], 20.0 + r_origin[2]);

    glEnd();
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glDisable(GL_ALPHA_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (i=0 ; i<6 ; i++)
    {
	/*
	  if (skymins[0][i] >= skymaxs[0][i]
	  || skymins[1][i] >= skymaxs[1][i])
	  continue;
	*/
	GL_Bind (SKY_TEX+skytexorder[i]);


	skymins[0][i] = -1;
	skymins[1][i] = -1;
	skymaxs[0][i] = 1;
	skymaxs[1][i] = 1;

	glBegin (GL_QUADS);
	MakeSkyVec (skymins[0][i], skymins[1][i], i);
	MakeSkyVec (skymins[0][i], skymaxs[1][i], i);
	MakeSkyVec (skymaxs[0][i], skymaxs[1][i], i);
	MakeSkyVec (skymaxs[0][i], skymins[1][i], i);
	glEnd ();
    }

    glDisable(GL_BLEND);
    glDepthMask(1);
}


//#endif

//===============================================================

/*
=============
R_InitSky

A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void R_InitSky (texture_t *mt)
{
    int		i, j, p;
    byte		*src;
    unsigned	trans[128*128];
    unsigned	transpix;
    int		r, g, b;
    unsigned	*rgba;

    src = (byte *)mt + mt->offsets[0];

    // make an average value for the back to avoid
    // a fringe on the top level

    r = g = b = 0;
    for (i=0 ; i<128 ; i++)
	for (j=0 ; j<128 ; j++)
	{
	    p = src[i*256 + j + 128];
	    rgba = &d_8to24table[p];
	    trans[(i*128) + j] = *rgba;
	    r += ((byte *)rgba)[0];
	    g += ((byte *)rgba)[1];
	    b += ((byte *)rgba)[2];
	}

    ((byte *)&transpix)[0] = r/(128*128);
    ((byte *)&transpix)[1] = g/(128*128);
    ((byte *)&transpix)[2] = b/(128*128);
    ((byte *)&transpix)[3] = 0;


    if (!solidskytexture)
	solidskytexture = texture_extension_number++;
    GL_Bind (solidskytexture );
    GL_TexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


    for (i=0 ; i<128 ; i++)
	for (j=0 ; j<128 ; j++)
	{
	    p = src[i*256 + j];
	    if (p == 0)
		trans[(i*128) + j] = transpix;
	    else
		trans[(i*128) + j] = d_8to24table[p];
	}

    if (!alphaskytexture)
	alphaskytexture = texture_extension_number++;
    GL_Bind(alphaskytexture);
    GL_TexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

