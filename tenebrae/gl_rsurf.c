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
// r_surf.c: surface-related refresh code

#include "quakedef.h"

#if (defined (__APPLE__) || defined (MACOSX)) && !defined (GL_REGISTER_COMBINERS_NV)
#define GL_REGISTER_COMBINERS_NV          0x8522
#endif // (__APPLE__ || MACOSX) && !GL_REGISTER_COMBINERS_NV

#ifndef GL_RGBA4
#define	GL_RGBA4	0
#endif


int		lightmap_bytes;		// 1, 2, or 4

int		lightmap_textures;

unsigned		blocklights[18*18];

#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128

#define	MAX_LIGHTMAPS	64
int			active_lightmaps;

typedef struct glRect_s {
	unsigned char l,t,w,h;
} glRect_t;

glpoly_t	*lightmap_polys[MAX_LIGHTMAPS];
qboolean	lightmap_modified[MAX_LIGHTMAPS];
glRect_t	lightmap_rectchange[MAX_LIGHTMAPS];

int			allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
byte		lightmaps[4*MAX_LIGHTMAPS*BLOCK_WIDTH*BLOCK_HEIGHT];

// For gl_texsort 0
msurface_t  *skychain = NULL;
msurface_t  *waterchain = NULL;



/*
====================================================
PENTA: Added for brush model vertex array support...

	We first add vertices to a dynamically allocated buffer (malloc)
	at the end of level loading we copy that to a "quake" allocated
	buffer (on the Hunk)
====================================================
*/
mmvertex_t *globalVertexTable = NULL;

mmvertex_t *tempVertices = NULL;
int	tempVerticesSize = 0;
int	numTempVertices = 0;

int R_GetNextVertexIndex(void) {
	return numTempVertices;
}
/*
Returns the index of the vertex the date was copied to...
*/
int R_AllocateVertexInTemp(vec3_t pos, float texture [2], float lightmap[2]) {

	int i;
	mmvertex_t *temp;


	if (!tempVertices) {
		tempVerticesSize = 512;
		tempVertices = malloc(tempVerticesSize*sizeof(mmvertex_t));
		numTempVertices = 0;
	}

	if (numTempVertices >= tempVerticesSize) {

		tempVerticesSize+=512;
		temp = malloc(tempVerticesSize*sizeof(mmvertex_t));
		if (!temp) Sys_Error("R_AllocateVertexInTemp: malloc failed\n");
		Q_memcpy(temp,tempVertices,(tempVerticesSize-512)*sizeof(mmvertex_t));
		free(tempVertices);
		tempVertices = temp;
	}

	VectorCopy(pos,tempVertices[numTempVertices].position);
	for (i=0; i<2; i++) {
		tempVertices[numTempVertices].texture[i] = texture[i];
		tempVertices[numTempVertices].lightmap[i] = lightmap[i];
	}
	numTempVertices++;
	return numTempVertices-1;
}

void R_CopyVerticesToHunk(void)
{
	globalVertexTable = Hunk_Alloc(numTempVertices*sizeof(mmvertex_t));
	Q_memcpy(globalVertexTable,tempVertices,numTempVertices*sizeof(mmvertex_t));
	free(tempVertices);
	Con_Printf("Copied %i vertices to hunk\n",numTempVertices);

	tempVertices = NULL;
	tempVerticesSize = 0;
	numTempVertices = 0;
}

void R_EnableVertexTable(int fields) {

	glVertexPointer(3, GL_FLOAT, VERTEXSIZE*sizeof(float), globalVertexTable);
	glEnableClientState(GL_VERTEX_ARRAY);

	if (fields & VERTEX_TEXTURE) {
		qglClientActiveTextureARB(GL_TEXTURE0_ARB);
		glTexCoordPointer(2, GL_FLOAT, VERTEXSIZE*sizeof(float), (float *)(globalVertexTable)+3);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	if (fields & VERTEX_LIGHTMAP) {
		qglClientActiveTextureARB(GL_TEXTURE1_ARB);
		glTexCoordPointer(2, GL_FLOAT, VERTEXSIZE*sizeof(float), (float *)(globalVertexTable)+5);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
}

void R_DisableVertexTable(int fields) {

	glVertexPointer(3, GL_FLOAT, 0, globalVertexTable);
	glDisableClientState(GL_VERTEX_ARRAY);

	if (fields & VERTEX_TEXTURE) {
		qglClientActiveTextureARB(GL_TEXTURE0_ARB);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	if (fields & VERTEX_LIGHTMAP) {
		qglClientActiveTextureARB(GL_TEXTURE1_ARB);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
}

void R_RenderDynamicLightmaps (msurface_t *fa);

/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights (msurface_t *surf)
{
	int			lnum;
	int			sd, td;
	float		dist, rad, minlight;
	vec3_t		impact, local;
	int			s, t;
	int			i;
	int			smax, tmax;
	mtexinfo_t	*tex;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		if ( !(surf->dlightbits & (1<<lnum) ) )
			continue;		// not lit by this light

		rad = cl_dlights[lnum].radius;
		dist = DotProduct (cl_dlights[lnum].origin, surf->plane->normal) -
				surf->plane->dist;
		rad -= fabs(dist);
		minlight = cl_dlights[lnum].minlight;
		if (rad < minlight)
			continue;
		minlight = rad - minlight;

		for (i=0 ; i<3 ; i++)
		{
			impact[i] = cl_dlights[lnum].origin[i] -
					surf->plane->normal[i]*dist;
		}

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1];
		
		for (t = 0 ; t<tmax ; t++)
		{
			td = local[1] - t*16;
			if (td < 0)
				td = -td;
			for (s=0 ; s<smax ; s++)
			{
				sd = local[0] - s*16;
				if (sd < 0)
					sd = -sd;
				if (sd > td)
					dist = sd + (td>>1);
				else
					dist = td + (sd>>1);
				if (dist < minlight)
					blocklights[t*smax + s] += (rad - dist)*256;
			}
		}
	}
}


/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
void R_BuildLightMap (msurface_t *surf, byte *dest, int stride)
{
	int			smax, tmax;
	int			t;
	int			i, j, size;
	byte		*lightmap;
	unsigned	scale;
	int			maps;
	unsigned	*bl;

	surf->cached_dlight = (surf->dlightframe == r_framecount);

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	size = smax*tmax;
	lightmap = surf->samples;

// set to full bright if no light data
	if (r_fullbright.value || !cl.worldmodel->lightdata)
	{
		for (i=0 ; i<size ; i++)
			blocklights[i] = 255*256;
		goto store;
	}

// clear to no light
	for (i=0 ; i<size ; i++)
		blocklights[i] = 0;

// add all the lightmaps
	if (lightmap)
		for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
			 maps++)
		{
			scale = d_lightstylevalue[surf->styles[maps]];
			surf->cached_light[maps] = scale;	// 8.8 fraction
			for (i=0 ; i<size ; i++)
				blocklights[i] += lightmap[i] * scale;
			lightmap += size;	// skip to next lightmap
		}

// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights (surf);

// bound, invert, and shift
store:
	switch (gl_lightmap_format)
	{
	case GL_RGBA:
		stride -= (smax<<2);
		bl = blocklights;
		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{
				t = *bl++;
				t >>= 7;
				if (t > 255)
					t = 255;
				//dest[3] = 255-t;
				dest[3] = t;
				dest += 4;
			}
		}
		break;
	case GL_ALPHA:
	case GL_LUMINANCE:
	case GL_INTENSITY:
		bl = blocklights;
		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{
				t = *bl++;
				t >>= 7;
				if (t > 255)
					t = 255;
				//dest[j] = 255-t;
				dest[j] = t;
			}
		}
		break;
	default:
		Sys_Error ("Bad lightmap format");
	}
}


/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t *R_TextureAnimation (texture_t *base)
{
	int		reletive;
	int		count;

	if (currententity->frame)
	{
		if (base->alternate_anims)
			base = base->alternate_anims;
	}
	
	if (!base->anim_total)
		return base;

	reletive = (int)(cl.time*10) % base->anim_total;

	count = 0;	
	while (base->anim_min > reletive || base->anim_max <= reletive)
	{
		base = base->anim_next;
		if (!base)
			Sys_Error ("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Sys_Error ("R_TextureAnimation: infinite cycle");
	}

	return base;
}


/*
=============================================================

	BRUSH MODELS

=============================================================
*/


extern	int		solidskytexture;
extern	int		alphaskytexture;
extern	float	speedscale;		// for top sky and bottom sky

void DrawGLWaterPoly (glpoly_t *p);
void DrawGLWaterPolyLightmap (glpoly_t *p);

//lpMTexFUNC qgl MTexCoord2fSGIS = NULL;
//lpSelTexFUNC qgl SelectTextureSGIS = NULL;

/* NV_register_combiners command function pointers
PENTA: I put them here because mtex functions are above it, and I hadent't any ispiration for some other place.
*/
PFNGLCOMBINERPARAMETERFVNVPROC qglCombinerParameterfvNV = NULL;
PFNGLCOMBINERPARAMETERIVNVPROC qglCombinerParameterivNV = NULL;
PFNGLCOMBINERPARAMETERFNVPROC qglCombinerParameterfNV = NULL;
PFNGLCOMBINERPARAMETERINVPROC qglCombinerParameteriNV = NULL;
PFNGLCOMBINERINPUTNVPROC qglCombinerInputNV = NULL;
PFNGLCOMBINEROUTPUTNVPROC qglCombinerOutputNV = NULL;
PFNGLFINALCOMBINERINPUTNVPROC qglFinalCombinerInputNV = NULL;
PFNGLGETCOMBINERINPUTPARAMETERFVNVPROC qglGetCombinerInputParameterfvNV = NULL;
PFNGLGETCOMBINERINPUTPARAMETERIVNVPROC qglGetCombinerInputParameterivNV = NULL;
PFNGLGETCOMBINEROUTPUTPARAMETERFVNVPROC qglGetCombinerOutputParameterfvNV = NULL;
PFNGLGETCOMBINEROUTPUTPARAMETERIVNVPROC qglGetCombinerOutputParameterivNV = NULL;
PFNGLGETFINALCOMBINERINPUTPARAMETERFVNVPROC qglGetFinalCombinerInputParameterfvNV = NULL;
PFNGLGETFINALCOMBINERINPUTPARAMETERIVNVPROC qglGetFinalCombinerInputParameterivNV = NULL;


PFNGLTEXIMAGE3DEXT qglTexImage3DEXT = NULL;
PFNGLDEPTHBOUNDSNV qglDepthBoundsNV = NULL;

PFNGLACTIVETEXTUREARBPROC qglActiveTextureARB = NULL;
PFNGLCLIENTACTIVETEXTUREARBPROC qglClientActiveTextureARB = NULL;
PFNGLMULTITEXCOORD1FARBPROC qglMultiTexCoord1fARB = NULL;
PFNGLMULTITEXCOORD2FARBPROC qglMultiTexCoord2fARB = NULL;
PFNGLMULTITEXCOORD2FVARBPROC qglMultiTexCoord2fvARB = NULL;
PFNGLMULTITEXCOORD3FARBPROC qglMultiTexCoord3fARB = NULL;
PFNGLMULTITEXCOORD3FVARBPROC qglMultiTexCoord3fvARB = NULL;

/*
ARB_vertex_program
PFNGLBINDPROGRAMARBPROC qglBindProgramARB = NULL;
PFNGLDELETEPROGRAMSARBPROC qglDeleteProgramsARB = NULL;
PFNGLGENPROGRAMSARBPROC qglGenProgramsARB = NULL;
PFNGLGETPROGRAMIVARBPROC qglGetProgramivARB = NULL;
PFNGLGETPROGRAMSTRINGARBPROC qglGetProgramStringARB = NULL;
PFNGLGETVERTEXATTRIBDVARBPROC qglGetVertexAttribdvARB = NULL;
PFNGLGETVERTEXATTRIBFVARBPROC qglGetVertexAttribfvARB = NULL;
PFNGLGETVERTEXATTRIBIVARBPROC qglGetVertexAttribivARB = NULL;
PFNGLGETVERTEXATTRIBPOINTERVARBPROC qglGetVertexAttribPointervARB = NULL;
PFNGLISPROGRAMARBPROC qglIsProgramARB = NULL;
PFNGPROGRAMSTRINGARBPROC qglProgramStringARB = NULL;
PFNGLTRACKMATRIXNVPROC glTrackMatrixNV = NULL;
PFNGLVERTEXATTRIB1DARBPROC qglVertexAttrib1dARB = NULL;
PFNGLVERTEXATTRIB1DVARBPROC qglVertexAttrib1dvARB = NULL;
PFNGLVERTEXATTRIB1FARBPROC qglVertexAttrib1fARB = NULL;
PFNGLVERTEXATTRIB1FVARBPROC qglVertexAttrib1fvARB = NULL;
PFNGLVERTEXATTRIB1SARBPROC qglVertexAttrib1sARB = NULL;
PFNGLVERTEXATTRIB1SVARBPROC qglVertexAttrib1svARB = NULL;
PFNGLVERTEXATTRIB2DARBPROC qglVertexAttrib2dARB = NULL;
PFNGLVERTEXATTRIB2DVARBPROC qglVertexAttrib2dvARB = NULL;
PFNGLVERTEXATTRIB2FARBPROC qglVertexAttrib2fARB = NULL;
PFNGLVERTEXATTRIB2FVARBPROC qglVertexAttrib2fvARB = NULL;
PFNGLVERTEXATTRIB2SARBPROC qglVertexAttrib2sARB = NULL;
PFNGLVERTEXATTRIB2SVARBPROC qglVertexAttrib2svARB = NULL;
PFNGLVERTEXATTRIB3DARBPROC qglVertexAttrib3dARB = NULL;
PFNGLVERTEXATTRIB3DVARBPROC qglVertexAttrib3dvARB = NULL;
PFNGLVERTEXATTRIB3FARBPROC qglVertexAttrib3fARB = NULL;
PFNGLVERTEXATTRIB3FVARBPROC qglVertexAttrib3fvARB = NULL;
PFNGLVERTEXATTRIB3SARBPROC qglVertexAttrib3sARB = NULL;
PFNGLVERTEXATTRIB3SVARBPROC qglVertexAttrib3svARB = NULL;
PFNGLVERTEXATTRIB4DARBPROC qglVertexAttrib4dARB = NULL;
PFNGLVERTEXATTRIB4DVARBPROC qglVertexAttrib4dvARB = NULL;
PFNGLVERTEXATTRIB4FARBPROC qglVertexAttrib4fARB = NULL;
PFNGLVERTEXATTRIB4FVARBPROC qglVertexAttrib4fvARB = NULL;
PFNGLVERTEXATTRIB4SARBPROC qglVertexAttrib4sARB = NULL;
PFNGLVERTEXATTRIB4SVARBPROC qglVertexAttrib4svARB = NULL;
PFNGLVERTEXATTRIB4UBVARBPROC qglVertexAttrib4ubvARB = NULL;
*/

PFNGLAREPROGRAMSRESIDENTNVPROC qglAreProgramsResidentNV = NULL;
PFNGLBINDPROGRAMNVPROC qglBindProgramNV = NULL;
PFNGLDELETEPROGRAMSNVPROC qglDeleteProgramsNV = NULL;
PFNGLEXECUTEPROGRAMNVPROC qglExecuteProgramNV = NULL;
PFNGLGENPROGRAMSNVPROC qglGenProgramsNV = NULL;
PFNGLGETPROGRAMPARAMETERDVNVPROC qglGetProgramParameterdvNV = NULL;
PFNGLGETPROGRAMPARAMETERFVNVPROC qglGetProgramParameterfvNV = NULL;
PFNGLGETPROGRAMIVNVPROC qglGetProgramivNV = NULL;
PFNGLGETPROGRAMSTRINGNVPROC qglGetProgramStringNV = NULL;
PFNGLGETTRACKMATRIXIVNVPROC qglGetTrackMatrixivNV = NULL;
PFNGLGETVERTEXATTRIBDVNVPROC qglGetVertexAttribdvNV = NULL;
PFNGLGETVERTEXATTRIBFVNVPROC qglGetVertexAttribfvNV = NULL;
PFNGLGETVERTEXATTRIBIVNVPROC qglGetVertexAttribivNV = NULL;
PFNGLGETVERTEXATTRIBPOINTERVNVPROC qglGetVertexAttribPointervNV = NULL;
PFNGLISPROGRAMNVPROC qglIsProgramNV = NULL;
PFNGLLOADPROGRAMNVPROC qglLoadProgramNV = NULL;
PFNGLPROGRAMPARAMETER4DNVPROC qglProgramParameter4dNV = NULL;
PFNGLPROGRAMPARAMETER4DVNVPROC qglProgramParameter4dvNV = NULL;
PFNGLPROGRAMPARAMETER4FNVPROC qglProgramParameter4fNV = NULL;
PFNGLPROGRAMPARAMETER4FVNVPROC qglProgramParameter4fvNV = NULL;
PFNGLPROGRAMPARAMETERS4DVNVPROC qglProgramParameters4dvNV = NULL;
PFNGLPROGRAMPARAMETERS4FVNVPROC qglProgramParameters4fvNV = NULL;
PFNGLREQUESTRESIDENTPROGRAMSNVPROC qglRequestResidentProgramsNV = NULL;
PFNGLTRACKMATRIXNVPROC qglTrackMatrixNV = NULL;
PFNGLVERTEXATTRIBPOINTERNVPROC qglVertexAttribPointerNV = NULL;
PFNGLVERTEXATTRIB1DNVPROC qglVertexAttrib1dNV = NULL;
PFNGLVERTEXATTRIB1DVNVPROC qglVertexAttrib1dvNV = NULL;
PFNGLVERTEXATTRIB1FNVPROC qglVertexAttrib1fNV = NULL;
PFNGLVERTEXATTRIB1FVNVPROC qglVertexAttrib1fvNV = NULL;
PFNGLVERTEXATTRIB1SNVPROC qglVertexAttrib1sNV = NULL;
PFNGLVERTEXATTRIB1SVNVPROC qglVertexAttrib1svNV = NULL;
PFNGLVERTEXATTRIB2DNVPROC qglVertexAttrib2dNV = NULL;
PFNGLVERTEXATTRIB2DVNVPROC qglVertexAttrib2dvNV = NULL;
PFNGLVERTEXATTRIB2FNVPROC qglVertexAttrib2fNV = NULL;
PFNGLVERTEXATTRIB2FVNVPROC qglVertexAttrib2fvNV = NULL;
PFNGLVERTEXATTRIB2SNVPROC qglVertexAttrib2sNV = NULL;
PFNGLVERTEXATTRIB2SVNVPROC qglVertexAttrib2svNV = NULL;
PFNGLVERTEXATTRIB3DNVPROC qglVertexAttrib3dNV = NULL;
PFNGLVERTEXATTRIB3DVNVPROC qglVertexAttrib3dvNV = NULL;
PFNGLVERTEXATTRIB3FNVPROC qglVertexAttrib3fNV = NULL;
PFNGLVERTEXATTRIB3FVNVPROC qglVertexAttrib3fvNV = NULL;
PFNGLVERTEXATTRIB3SNVPROC qglVertexAttrib3sNV = NULL;
PFNGLVERTEXATTRIB3SVNVPROC qglVertexAttrib3svNV = NULL;
PFNGLVERTEXATTRIB4DNVPROC qglVertexAttrib4dNV = NULL;
PFNGLVERTEXATTRIB4DVNVPROC qglVertexAttrib4dvNV = NULL;
PFNGLVERTEXATTRIB4FNVPROC qglVertexAttrib4fNV = NULL;
PFNGLVERTEXATTRIB4FVNVPROC qglVertexAttrib4fvNV = NULL;
PFNGLVERTEXATTRIB4SNVPROC qglVertexAttrib4sNV = NULL;
PFNGLVERTEXATTRIB4SVNVPROC qglVertexAttrib4svNV = NULL;
PFNGLVERTEXATTRIB4UBVNVPROC qglVertexAttrib4ubvNV = NULL;
PFNGLVERTEXATTRIBS1DVNVPROC qglVertexAttribs1dvNV = NULL;
PFNGLVERTEXATTRIBS1FVNVPROC qglVertexAttribs1fvNV = NULL;
PFNGLVERTEXATTRIBS1SVNVPROC qglVertexAttribs1svNV = NULL;
PFNGLVERTEXATTRIBS2DVNVPROC qglVertexAttribs2dvNV = NULL;
PFNGLVERTEXATTRIBS2FVNVPROC qglVertexAttribs2fvNV = NULL;
PFNGLVERTEXATTRIBS2SVNVPROC qglVertexAttribs2svNV = NULL;
PFNGLVERTEXATTRIBS3DVNVPROC qglVertexAttribs3dvNV = NULL;
PFNGLVERTEXATTRIBS3FVNVPROC qglVertexAttribs3fvNV = NULL;
PFNGLVERTEXATTRIBS3SVNVPROC qglVertexAttribs3svNV = NULL;
PFNGLVERTEXATTRIBS4DVNVPROC qglVertexAttribs4dvNV = NULL;
PFNGLVERTEXATTRIBS4FVNVPROC qglVertexAttribs4fvNV = NULL;
PFNGLVERTEXATTRIBS4SVNVPROC qglVertexAttribs4svNV = NULL;
PFNGLVERTEXATTRIBS4UBVNVPROC qglVertexAttribs4ubvNV = NULL; 

// <AWE> There are some diffs with the function parameters. wgl stuff not present with MacOS X. -DC- and SDL
#if defined (__APPLE__) || defined (MACOSX) || defined (SDL) || defined (__glx__)

PFNGLFLUSHVERTEXARRAYRANGEAPPLEPROC qglFlushVertexArrayRangeAPPLE  = NULL;
PFNGLVERTEXARRAYRANGEAPPLEPROC qglVertexArrayRangeAPPLE  = NULL;

#else

PFNGLFLUSHVERTEXARRAYRANGENVPROC qglFlushVertexArrayRangeNV  = NULL;
PFNGLVERTEXARRAYRANGENVPROC glVertexArrayRangeNV  = NULL;
PFNWGLALLOCATEMEMORYNVPROC wglAllocateMemoryNV  = NULL;
PFNWGLFREEMEMORYNVPROC wglFreeMemoryNV  = NULL;

#endif /* __APPLE__ || MACOSX */

qboolean mtexenabled = false;

void GL_SelectTexture (GLenum target);

void GL_DisableMultitexture(void) 
{
	if (mtexenabled) {
		glDisable(GL_TEXTURE_2D);
		GL_SelectTexture(GL_TEXTURE0_ARB);
		mtexenabled = false;
	}
}

void GL_EnableMultitexture(void) 
{
	if (gl_mtexable) {
		GL_SelectTexture(GL_TEXTURE1_ARB);
		glEnable(GL_TEXTURE_2D);
		mtexenabled = true;
	}
}

#if 0
/*
================
R_DrawSequentialPoly

Systems that have fast state and texture changes can
just do everything as it passes with no need to sort
================
*/
void R_DrawSequentialPoly (msurface_t *s)
{
	glpoly_t	*p;
	float		*v;
	int			i;
	texture_t	*t;

	//
	// normal lightmaped poly
	//
	if (! (s->flags & (SURF_DRAWSKY|SURF_DRAWTURB|SURF_UNDERWATER) ) )
	{
		p = s->polys;

		t = R_TextureAnimation (s->texinfo->texture);
		GL_Bind (t->gl_texturenum);
		glBegin (GL_TRIANGLEFAN);
		v = p->verts[0];
		for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
		{
			glTexCoord2f (v[3], v[4]);
			glVertex3fv (v);
		}
		glEnd ();

		GL_Bind (lightmap_textures + s->lightmaptexturenum);
		glEnable (GL_BLEND);
		glBegin (GL_TRIANGLEFAN);
		v = p->verts[0];
		for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
		{
			glTexCoord2f (v[5], v[6]);
			glVertex3fv (v);
		}
		glEnd ();

		glDisable (GL_BLEND);

		return;
	}

	//
	// subdivided water surface warp
	//
	if (s->flags & SURF_DRAWTURB)
	{
		GL_Bind (s->texinfo->texture->gl_texturenum);
		EmitWaterPolys (s);
		return;
	}

	//
	// subdivided sky warp
	//
	if (s->flags & SURF_DRAWSKY)
	{
		GL_Bind (solidskytexture);
		speedscale = realtime*8;
		speedscale -= (int)speedscale;

		EmitSkyPolys (s);

		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_Bind (alphaskytexture);
		speedscale = realtime*16;
		speedscale -= (int)speedscale;
		EmitSkyPolys (s);
		if (gl_lightmap_format == GL_LUMINANCE)
			glBlendFunc (GL_ZERO, GL_ONE_MINUS_SRC_COLOR);

		glDisable (GL_BLEND);
	}

	//
	// underwater warped with lightmap
	//
	p = s->polys;

	t = R_TextureAnimation (s->texinfo->texture);
	GL_Bind (t->gl_texturenum);
	DrawGLWaterPoly (p);

	GL_Bind (lightmap_textures + s->lightmaptexturenum);
	glEnable (GL_BLEND);
	DrawGLWaterPolyLightmap (p);
	glDisable (GL_BLEND);
}
#else
/*
================
R_DrawSequentialPoly

Systems that have fast state and texture changes can
just do everything as it passes with no need to sort
PENTA: Isn't used anymore!
================
*/
void R_DrawSequentialPoly (msurface_t *s)
{
	glpoly_t	*p;
	float		*v;
	int			i;
	texture_t	*t;
	glRect_t	*theRect;

	//
	// normal lightmaped poly
	//

	if (! (s->flags & (SURF_DRAWSKY|SURF_DRAWTURB) ) )
	{
		R_RenderDynamicLightmaps (s);
		if (gl_mtexable) {
			p = s->polys;

			t = R_TextureAnimation (s->texinfo->texture);
			// Binds world to texture env 0
			GL_SelectTexture(GL_TEXTURE0_ARB);
			GL_Bind (t->gl_texturenum);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			// Binds lightmap to texenv 1
			GL_EnableMultitexture(); // Same as SelectTexture (TEXTURE1)
			GL_Bind (lightmap_textures + s->lightmaptexturenum);
			i = s->lightmaptexturenum;
			if (lightmap_modified[i])
			{
				lightmap_modified[i] = false;
				theRect = &lightmap_rectchange[i];
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
					BLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
					lightmaps+(i* BLOCK_HEIGHT + theRect->t) *BLOCK_WIDTH*lightmap_bytes);
				theRect->l = BLOCK_WIDTH;
				theRect->t = BLOCK_HEIGHT;
				theRect->h = 0;
				theRect->w = 0;
			}
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
			glBegin(GL_TRIANGLE_FAN);
			//v = p->verts[0];
			v = (float *)(&globalVertexTable[p->firstvertex]);
			for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
			{
				qglMultiTexCoord2fARB (GL_TEXTURE0_ARB, v[3], v[4]);
				qglMultiTexCoord2fARB (GL_TEXTURE1_ARB, v[5], v[6]);
				glVertex3fv (v);
			}
			glEnd ();
			return;
		} else {
			p = s->polys;

			t = R_TextureAnimation (s->texinfo->texture);
			GL_Bind (t->gl_texturenum);
			glBegin (GL_TRIANGLE_FAN);
			//v = p->verts[0];
			v = (float *)(&globalVertexTable[p->firstvertex]);
			for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
			{
				glTexCoord2f (v[3], v[4]);
				glVertex3fv (v);
			}
			glEnd ();

			GL_Bind (lightmap_textures + s->lightmaptexturenum);
			glEnable (GL_BLEND);
			glBegin (GL_TRIANGLE_FAN);
			//v = p->verts[0];
			v = (float *)(&globalVertexTable[p->firstvertex]);
			for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
			{
				glTexCoord2f (v[5], v[6]);
				glVertex3fv (v);
			}
			glEnd ();

			glDisable (GL_BLEND);
		}

		return;
	}

	//
	// subdivided water surface warp
	//

	if (s->flags & SURF_DRAWTURB)
	{
		GL_DisableMultitexture();
		GL_Bind (s->texinfo->texture->gl_texturenum);
		EmitWaterPolys (s);
		return;
	}

	//
	// subdivided sky warp
	//
	if (s->flags & SURF_DRAWSKY)
	{
		GL_DisableMultitexture();
		GL_Bind (solidskytexture);
		speedscale = realtime*8;
		speedscale -= (int)speedscale & ~127;

		EmitSkyPolys (s);

		glEnable (GL_BLEND);
		GL_Bind (alphaskytexture);
		speedscale = realtime*16;
		speedscale -= (int)speedscale & ~127;
		EmitSkyPolys (s);

		glDisable (GL_BLEND);
		return;
	}
}
#endif


/*
================
DrawGLWaterPoly

Warp the vertex coordinates
================
*/
void DrawGLWaterPoly (glpoly_t *p)
{
	int		i;
	float	*v;
	vec3_t	nv;

	GL_DisableMultitexture();

	glBegin (GL_TRIANGLE_FAN);
	//v = p->verts[0];
	v = (float *)(&globalVertexTable[p->firstvertex]);
	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
	{
		glTexCoord2f (v[3], v[4]);

		nv[0] = v[0] + 8*sin(v[1]*0.05+realtime)*sin(v[2]*0.05+realtime);
		nv[1] = v[1] + 8*sin(v[0]*0.05+realtime)*sin(v[2]*0.05+realtime);
		nv[2] = v[2];

		glVertex3fv (nv);
	}
	glEnd ();
}

void DrawGLWaterPolyLightmap (glpoly_t *p)
{
	int		i;
	float	*v;
	vec3_t	nv;

	GL_DisableMultitexture();

	glBegin (GL_TRIANGLE_FAN);
	//v = p->verts[0];
	v = (float *)(&globalVertexTable[p->firstvertex]);
	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
	{
		glTexCoord2f (v[5], v[6]);

		nv[0] = v[0] + 8*sin(v[1]*0.05+realtime)*sin(v[2]*0.05+realtime);
		nv[1] = v[1] + 8*sin(v[0]*0.05+realtime)*sin(v[2]*0.05+realtime);
		nv[2] = v[2];

		glVertex3fv (nv);
	}
	glEnd ();
}

/*
================
DrawGLPoly
================
*/
void DrawGLPoly (glpoly_t *p)
{
	int		i;
	float	*v;

	glBegin (GL_TRIANGLE_FAN);
	//v = p->verts[0];
	v = (float *)(&globalVertexTable[p->firstvertex]);
	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
	{
		glTexCoord2f (v[3], v[4]);
		glVertex3fv (v);
	}
	glEnd ();
}


/*
================
R_BlendLightmaps

verm lightmaps achteraf
elke lightmap heeft zijn lijst, teken die lijst

we hebbend dit veranderd, we tekenen nu de lightmap
zodat we de depth buffer vullen voor stencil shadows
en gaan de lightmaps heel donker renderen als vorm
van ambient light

PENTA: Modifications
================
*/
void R_BlendLightmaps (void)
{
	int			i, j;
	glpoly_t	*p;
	float		*v;

	if (r_fullbright.value)
		return;

	if (gl_lightmap_format == GL_LUMINANCE)
		glBlendFunc (GL_ZERO, GL_SRC_COLOR);
	else if (gl_lightmap_format == GL_INTENSITY)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glColor4f (0,0,0,1);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	if (!r_lightmap.value)
	{
		//glEnable (GL_BLEND); //PENTA: removed as test
	}

	glColor3f(sh_lightmapbright.value,sh_lightmapbright.value,sh_lightmapbright.value);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	for (i=0 ; i<MAX_LIGHTMAPS ; i++)
	{
		p = lightmap_polys[i];
		if (!p)
			continue;
		GL_Bind(lightmap_textures+i);

		//draw all polygons that use this lightmap
		for( ; p ; p=p->chain)
		{
			glBegin (GL_TRIANGLE_FAN);
			//v = p->verts[0];
			v = (float *)(&globalVertexTable[p->firstvertex]);
			for (j=0 ; j<p->numverts ; j++, v+= VERTEXSIZE)
			{
				glTexCoord2f (v[5], v[6]);
				glVertex3fv (v);
			}
			glEnd ();
		}
		lightmap_polys[i] = NULL;
	}

	glColor3f(1,1,1);

	glDisable (GL_BLEND);
	if (gl_lightmap_format == GL_LUMINANCE)
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	else if (gl_lightmap_format == GL_INTENSITY)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glColor4f (1,1,1,1);
	}
}

/*
================
R_RenderBrushPoly

PENTA: Modifications
================
*/
void R_RenderBrushPoly (msurface_t *fa)
{
	texture_t	*t;
	glpoly_t	*p;
	int		i;
	float	*v;

	if (fa->flags & SURF_DRAWSKY)
	{	// warp texture, no lightmaps
		EmitBothSkyLayers (fa);
		return;
	}
	
	/*if (!busy_caustics)*/ {
		t = R_TextureAnimation (fa->texinfo->texture);
		GL_SelectTexture(GL_TEXTURE0_ARB);
		GL_Bind (t->gl_texturenum);
	}

	if (fa->flags & SURF_DRAWTURB)
	{	// warp texture, no lightmaps
		EmitWaterPolys (fa);
		return;
	}

	if (fa->flags & SURF_PLANEBACK)
	{
		glNormal3f(-fa->plane->normal[0],-fa->plane->normal[1],-fa->plane->normal[2]);
	} else {
		glNormal3fv(&fa->plane->normal[0]);
	}

	/*if (!busy_caustics)*/ {
		GL_SelectTexture(GL_TEXTURE1_ARB);
		GL_Bind (lightmap_textures + fa->lightmaptexturenum);
	}

	p = fa->polys;
	glBegin (GL_TRIANGLE_FAN);
	//v = p->verts[0];
	v = (float *)(&globalVertexTable[p->firstvertex]);
	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
	{
		glTexCoord2f (v[3], v[4]);
		qglMultiTexCoord2fARB(GL_TEXTURE1_ARB, v[5], v[6]);
		glVertex3fv (v);
	}
	glEnd ();
}

/*
================
R_RenderBrushPoly

PENTA:
================
*/
void R_RenderBrushPolyLuma (msurface_t *fa)
{
	int		i;
	float	*v;
	glpoly_t *p;
	texture_t *t;

	if (fa->flags & SURF_DRAWSKY)
		return;

	if (fa->flags & SURF_DRAWTURB)
		return;

	t = R_TextureAnimation (fa->texinfo->texture);

	GL_Bind (t->gl_lumitex);

	glBegin (GL_TRIANGLE_FAN);
	p = fa->polys;
	//v = p->verts[0];
	v = (float *)(&globalVertexTable[p->firstvertex]);
	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
	{
		glTexCoord2f (v[3], v[4]);
		glVertex3fv (v);
	}
	glEnd ();
}



/*

================

R_RenderBrushPoly



PENTA:

================

*/

void R_RenderBrushPolyLightmap (msurface_t *fa)

{

	int		i;

	float	*v;

	glpoly_t *p;

	if (fa->flags & SURF_DRAWSKY)

		return;

	if (fa->flags & SURF_DRAWTURB)

		return;

	GL_Bind (lightmap_textures+fa->lightmaptexturenum);

	glBegin (GL_TRIANGLE_FAN);

	p = fa->polys;

	//v = p->verts[0];
	v = (float *)(&globalVertexTable[p->firstvertex]);

	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)

	{

		glTexCoord2f (v[5], v[6]);

		glVertex3fv (v);

	}

	glEnd ();

}


void R_RenderBrushPolyCaustics (msurface_t *fa)
{	int		i;
	float	*v;
	glpoly_t *p;

	glBegin (GL_TRIANGLE_FAN);
	p = fa->polys;
	//v = p->verts[0];
	v = (float *)(&globalVertexTable[p->firstvertex]);
	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
	{
		glTexCoord2f (v[5], v[6]);
		glVertex3fv (v);
	}

	glEnd ();
}
/*
================
R_RenderDynamicLightmaps
Multitexture
================
*/
void R_RenderDynamicLightmaps (msurface_t *fa)
{
	byte		*base;
	int			maps;
	glRect_t    *theRect;
	int smax, tmax;

	c_brush_polys++;

	if (fa->flags & ( SURF_DRAWSKY | SURF_DRAWTURB) )
		return;
		
	fa->polys->chain = lightmap_polys[fa->lightmaptexturenum];
	lightmap_polys[fa->lightmaptexturenum] = fa->polys;

	// check for lightmap modification
	for (maps = 0 ; maps < MAXLIGHTMAPS && fa->styles[maps] != 255 ;
		 maps++)
		if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps])
			goto dynamic;

	if (fa->dlightframe == r_framecount	// dynamic this frame
		|| fa->cached_dlight)			// dynamic previously
	{
dynamic:
		if (r_dynamic.value)
		{
			lightmap_modified[fa->lightmaptexturenum] = true;
			theRect = &lightmap_rectchange[fa->lightmaptexturenum];
			if (fa->light_t < theRect->t) {
				if (theRect->h)
					theRect->h += theRect->t - fa->light_t;
				theRect->t = fa->light_t;
			}
			if (fa->light_s < theRect->l) {
				if (theRect->w)
					theRect->w += theRect->l - fa->light_s;
				theRect->l = fa->light_s;
			}
			smax = (fa->extents[0]>>4)+1;
			tmax = (fa->extents[1]>>4)+1;
			if ((theRect->w + theRect->l) < (fa->light_s + smax))
				theRect->w = (fa->light_s-theRect->l)+smax;
			if ((theRect->h + theRect->t) < (fa->light_t + tmax))
				theRect->h = (fa->light_t-theRect->t)+tmax;
			base = lightmaps + fa->lightmaptexturenum*lightmap_bytes*BLOCK_WIDTH*BLOCK_HEIGHT;
			base += fa->light_t * BLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;
			R_BuildLightMap (fa, base, BLOCK_WIDTH*lightmap_bytes);
		}
	}
}

/*
================
R_MirrorChain
================
*/
void R_MirrorChain (msurface_t *s)
{
	//if (mirror)
	//	return;
	//mirror = true;
	//mirror_plane = s->plane;
}


#if 0
/*
================
R_DrawWaterSurfaces
PENTA: Modifications
================
*/
void R_DrawWaterSurfaces (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;

	//
	// go back to the world matrix
	//
    glLoadMatrixf (r_world_matrix);

	glEnable (GL_BLEND);
	glColor4f (1,1,1,r_wateralpha.value);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		t = cl.worldmodel->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;
		if ( !(s->flags & SURF_DRAWTURB) )
			continue;

		// set modulate mode explicitly
		GL_Bind (t->gl_texturenum);

		for ( ; s ; s=s->texturechain)
			R_RenderBrushPoly (s);

		t->texturechain = NULL;
	}

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glColor4f (1,1,1,1);
	glDisable (GL_BLEND);
}
#else
/*
================
R_DrawWaterSurfaces

PENTA: Modifications
================
*/
void R_DrawWaterSurfaces (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;

	/*
	PENTA:  we always draw water at the end of the frame
	if (r_wateralpha.value == 1.0 && gl_texsort.value)
		return;
	*/

	//
	// go back to the world matrix
	//

    glLoadMatrixf (r_world_matrix);

	if (r_wateralpha.value < 1.0) {
		if (gl_watershader.value < 1.0) {
			glEnable (GL_BLEND);
			glColor4f (1,1,1,r_wateralpha.value);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		} else {
			glEnable (GL_BLEND);
			glColor4f (1,1,1,r_wateralpha.value);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			//glBlendFunc(GL_DST_COLOR,GL_ONE);

			glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
			glMatrixMode(GL_TEXTURE);
			glLoadIdentity();
		}
	} else {
		glColor4f (1,1,1,1);
	}

	/*if (!gl_texsort.value) {
		if (!waterchain)
			return;

		for ( s = waterchain ; s ; s=s->texturechain) {
			GL_Bind (s->texinfo->texture->gl_texturenum);
			EmitWaterPolys (s);
		}
		
		waterchain = NULL;
	} else */{

		for (i=0 ; i<cl.worldmodel->numtextures ; i++)
		{
			t = cl.worldmodel->textures[i];
			if (!t)
				continue;
			s = t->texturechain;
			if (!s)
				continue;
			if ( !(s->flags & SURF_DRAWTURB ) )
				continue;

			if ( (s->flags & SURF_MIRROR ) )
				continue;

			// set modulate mode explicitly
		
			if (!OverrideFluidTex(t->name)) {
				GL_Bind (t->gl_texturenum);
			}

			for ( ; s ; s=s->texturechain)
				EmitWaterPolys (s);
			
			t->texturechain = NULL;

			//PENTA: only translate back if we use shaders
			if ((gl_watershader.value == 1) && (r_wateralpha.value < 1) ) {
				glLoadIdentity();
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				GL_DisableMultitexture() ;
				glLoadIdentity();
				glDisable(GL_TEXTURE_GEN_S);
				glDisable(GL_TEXTURE_GEN_T);
				glFogfv(GL_FOG_COLOR, fog_color);
			}
		}
	}

	if (r_wateralpha.value < 1.0) {

		if (gl_watershader.value < 1.0) {
			glColor4f (1,1,1,r_wateralpha.value);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glDisable(GL_BLEND);

		} else {
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

			glColor4f (1,1,1,1);
			glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
			glDisable (GL_BLEND);
			glMatrixMode(GL_MODELVIEW);
		}
	} else {
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	}

}

#endif

/*
================
DrawTextureChains
PENTA: Modifications

We fill the causistics chain here!
================
*/
void DrawTextureChains (void)
{
	int		i;
	msurface_t	*s;
	texture_t	*t, *tani;
	qboolean	found = false;

	//glBlendFunc(GL_ZERO,GL_SRC_COLOR);
	//glEnable(GL_BLEND);
	glDepthMask (1);
	GL_DisableMultitexture();
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glColor3f(sh_lightmapbright.value,sh_lightmapbright.value,sh_lightmapbright.value);

	GL_EnableMultitexture();
	if (sh_colormaps.value) {
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	} else {
		//No colormaps: Color maps are bound on tmu 0 so disable it
		//and let tu1 modulate itself with the light map brightness

        if ((gl_cardtype == GEFORCE) || (gl_cardtype == GEFORCE3))
        {
            glDisable(GL_REGISTER_COMBINERS_NV);
        }
        
		GL_SelectTexture(GL_TEXTURE0_ARB);		
		glDisable(GL_TEXTURE_2D);
		GL_SelectTexture(GL_TEXTURE1_ARB);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
		glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PRIMARY_COLOR_ARB);
		glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE);
		glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
	}



	if (gl_wireframe.value) {
		GL_SelectTexture(GL_TEXTURE0_ARB);
		glDisable(GL_TEXTURE_2D);
		GL_SelectTexture(GL_TEXTURE1_ARB);
		glDisable(GL_TEXTURE_2D);
	}

	causticschain = NULL; //clear chain here

	R_EnableVertexTable(VERTEX_TEXTURE | VERTEX_LIGHTMAP);

	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		t = cl.worldmodel->textures[i];
		if (!t)
			continue;

		s = t->texturechain;
		if (!s)
			continue;

		found = true;

		if (i == skytexturenum) {
			continue;
			//R_DrawSkyChain (s);
		}
		/*
		else if (i == mirrortexturenum && r_mirroralpha.value != 1.0)
		{
			R_MirrorChain (s);
			continue;
		}
		*/
		//else
		{

			//PENTA: water at end of frame
			if (s->flags & SURF_DRAWTURB)
				continue;

			GL_SelectTexture(GL_TEXTURE0_ARB);			
			tani = R_TextureAnimation (s->texinfo->texture);
			GL_Bind (tani->gl_texturenum);

			//Do the ambient pass
			//now with arrays!
			GL_SelectTexture(GL_TEXTURE1_ARB);
			while (s) {
				//R_RenderBrushPoly (s);
				GL_Bind (lightmap_textures + s->lightmaptexturenum);
				glDrawArrays(GL_TRIANGLE_FAN,s->polys->firstvertex,s->polys->numverts);
				s=s->texturechain;
				c_brush_polys ++;
			}
			GL_SelectTexture(GL_TEXTURE0_ARB);
			
			//Has this texture a luma texture then add it

			if (t->gl_lumitex) {
				//vec3_t color_black = {0.0, 0.0, 0.0};
				glFogfv(GL_FOG_COLOR, color_black);
				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE);
				GL_SelectTexture(GL_TEXTURE1_ARB);
				glDisable(GL_TEXTURE_2D);
				GL_SelectTexture(GL_TEXTURE0_ARB);
				glColor3f(1, 1, 1);

				s = t->texturechain;
				GL_Bind (t->gl_lumitex);

				for ( ; s ; s=s->texturechain) {
					//R_RenderBrushPolyLuma (s);
					glDrawArrays(GL_TRIANGLE_FAN,s->polys->firstvertex,s->polys->numverts);
				}

				glColor3f(sh_lightmapbright.value,sh_lightmapbright.value,sh_lightmapbright.value);
				GL_SelectTexture(GL_TEXTURE1_ARB);
				glEnable(GL_TEXTURE_2D);
				GL_SelectTexture(GL_TEXTURE0_ARB);
				GL_SelectTexture(GL_TEXTURE1_ARB);
				glDisable(GL_BLEND);
				glFogfv(GL_FOG_COLOR, fog_color);
			}

			s = t->texturechain;
			//Make cauistics list
			while (s) {
				msurface_t *olds;
				olds = s;
				s=s->texturechain;
				//attach the surface for caustics drawing (post multipyied later)
				if (olds->flags & SURF_UNDERWATER) {
					olds->texturechain = causticschain;
					causticschain = olds;
				}
			}

		}

		t->texturechain = NULL;
	}
	R_DisableVertexTable(VERTEX_TEXTURE | VERTEX_LIGHTMAP);
	GL_SelectTexture(GL_TEXTURE1_ARB);
	GL_DisableMultitexture();
	//glDisable(GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask (1);
}

void R_DrawBrushModelCaustics (entity_t *e);

/*
=============
R_DrawCaustics

Tenebrae does "real cauistics", projected textures on all things including ents
(Other mods seem to just add an extra, not properly projected layer to the polygons in the world.)
=============
*/
void R_DrawCaustics(void) {

	msurface_t *s;
	int			i;
	vec3_t		mins, maxs;

	GLfloat sPlane[4] = {0.01, 0.005, 0.0, 0.0 };
	GLfloat tPlane[4] = {0.0, 0.01, 0.005, 0.0 };

	if (!gl_caustics.value) return;

	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGenfv(GL_S, GL_OBJECT_PLANE, sPlane);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, tPlane);
	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);

	glEnable(GL_BLEND);
	glBlendFunc(GL_DST_COLOR, GL_ONE);

	GL_Bind(caustics_textures[(int)(cl.time*16)&7]);
	busy_caustics = true;
	s = causticschain;

	R_EnableVertexTable(0);
	while (s) {
		//R_RenderBrushPolyCaustics (s);
		glDrawArrays(GL_TRIANGLE_FAN,s->polys->firstvertex,s->polys->numverts);			
		s = s->texturechain;
	}
	R_DisableVertexTable(0);

	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		if (currententity->angles[0] || currententity->angles[1] || currententity->angles[2])
		{
			int i;
			for (i=0 ; i<3 ; i++)
			{
				mins[i] = currententity->origin[i] - currententity->model->radius;
				maxs[i] = currententity->origin[i] + currententity->model->radius;
			}
		} else {
			VectorAdd (currententity->origin,currententity->model->mins, mins);
			VectorAdd (currententity->origin,currententity->model->maxs, maxs);
		}

		if (R_CullBox (mins, maxs))
			continue;

		//quick hack! check if ent is below water
		if ((CL_PointContents(mins) != CONTENTS_WATER) && (CL_PointContents(maxs) != CONTENTS_WATER))
			continue;

		if (mirror) {
			if (mirror_clipside == BoxOnPlaneSide(mins, maxs, mirror_plane)) {
				continue;
			}


			if ( BoxOnPlaneSide(mins, maxs, &mirror_far_plane) == 1) {
				return;
			}
		}

		switch (currententity->model->type)
		{
		case mod_alias:
			R_DrawAliasModel (1.0);
			break;

		case mod_brush:
			R_DrawBrushModelCaustics(currententity);
			break;

		default:
			break;
		}
	}

	busy_caustics = false;
	glDisable(GL_BLEND);

	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);	
}


/*
=================
R_DrawBrushModel

PENTA: Modifications
=================
*/
void R_DrawBrushModel (entity_t *e)
{
    vec3_t		mins, maxs;
    int			i;
    msurface_t	*psurf;
    mplane_t	*pplane;
    model_t		*clmodel;
    qboolean	rotated;
    texture_t *t;

    //bright = 1;

    currententity = e;
    currenttexture = -1;

    clmodel = e->model;

    if (e->angles[0] || e->angles[1] || e->angles[2])
    {
	rotated = true;
	for (i=0 ; i<3 ; i++)
	{
	    mins[i] = e->origin[i] - clmodel->radius;
	    maxs[i] = e->origin[i] + clmodel->radius;
	}
    }
    else
    {
	rotated = false;
	VectorAdd (e->origin, clmodel->mins, mins);
	VectorAdd (e->origin, clmodel->maxs, maxs);
    }

    if (R_CullBox (mins, maxs))
	return;

    memset (lightmap_polys, 0, sizeof(lightmap_polys));

    VectorSubtract (r_refdef.vieworg, e->origin, modelorg);
    if (rotated)
    {
	vec3_t	temp;
	vec3_t	forward, right, up;

	VectorCopy (modelorg, temp);
	AngleVectors (e->angles, forward, right, up);
	modelorg[0] = DotProduct (temp, forward);
	modelorg[1] = -DotProduct (temp, right);
	modelorg[2] = DotProduct (temp, up);
    }

    psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

    glPushMatrix ();
    e->angles[0] = -e->angles[0];	// stupid quake bug
    R_RotateForEntity (e);
    e->angles[0] = -e->angles[0];	// stupid quake bug

    //Draw model with specified ambient color
    GL_DisableMultitexture();
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    if (!busy_caustics) {
	glColor3f(sh_lightmapbright.value,sh_lightmapbright.value,sh_lightmapbright.value);

	GL_EnableMultitexture();
	if (sh_colormaps.value) {
	    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	} else {
	    //No colormaps: Color maps are bound on tmu 0 so disable it
	    //and let tu1 modulate itself with the light map brightness
        if ((gl_cardtype == GEFORCE) || (gl_cardtype == GEFORCE3))
        {
            glDisable(GL_REGISTER_COMBINERS_NV);
        }
	    GL_SelectTexture(GL_TEXTURE0_ARB);		
	    glDisable(GL_TEXTURE_2D);
	    GL_SelectTexture(GL_TEXTURE1_ARB);
	    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PRIMARY_COLOR_ARB);
	    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE);
	    glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
	}
    } else {
	glColor3f(1,1,1);
    }



    //XYZ

    if (gl_wireframe.value) {

	GL_SelectTexture(GL_TEXTURE0_ARB);		

	glDisable(GL_TEXTURE_2D);

	GL_SelectTexture(GL_TEXTURE1_ARB);

	glDisable(GL_TEXTURE_2D);

    }


    c_brush_polys += clmodel->nummodelsurfaces;
    //
    // draw texture
    //
    for (i=0 ; i<clmodel->nummodelsurfaces ; i++, psurf++)
    {
	// find which side of the node we are on
	pplane = psurf->plane;

	//dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

	// draw the polygon
	//if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
	//	(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
	//{
	R_RenderBrushPoly (psurf);
	//}
    }

    // if luma, draw it too
    psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
    //vec3_t color_black = {0.0, 0.0, 0.0};
    glFogfv(GL_FOG_COLOR, color_black);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    GL_SelectTexture(GL_TEXTURE1_ARB);
    glDisable(GL_TEXTURE_2D);
    GL_SelectTexture(GL_TEXTURE0_ARB);
    glColor3f(1, 1, 1);

    for (i=0 ; i<clmodel->nummodelsurfaces ; i++, psurf++)
    {
	t = R_TextureAnimation (psurf->texinfo->texture);
	if ( t->gl_lumitex )
	    R_RenderBrushPolyLuma (psurf);
    }

    glColor3f(sh_lightmapbright.value,sh_lightmapbright.value,sh_lightmapbright.value);
    GL_SelectTexture(GL_TEXTURE1_ARB);
    glEnable(GL_TEXTURE_2D);
    GL_SelectTexture(GL_TEXTURE0_ARB);
    GL_SelectTexture(GL_TEXTURE1_ARB);
    glDisable(GL_BLEND);
    glFogfv(GL_FOG_COLOR, fog_color);

    GL_DisableMultitexture();
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glPopMatrix ();
}

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModelCaustics (entity_t *e)
{
	vec3_t		mins, maxs;
	int			i;
	msurface_t	*psurf;
	float		dot;
	mplane_t	*pplane;
	model_t		*clmodel;
	qboolean	rotated;

	//bright = 1;

	currententity = e;
	currenttexture = -1;

	clmodel = e->model;

	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		rotated = true;
		for (i=0 ; i<3 ; i++)
		{
			mins[i] = e->origin[i] - clmodel->radius;
			maxs[i] = e->origin[i] + clmodel->radius;
		}
	}
	else
	{
		rotated = false;
		VectorAdd (e->origin, clmodel->mins, mins);
		VectorAdd (e->origin, clmodel->maxs, maxs);
	}

	if (R_CullBox (mins, maxs))
		return;

	VectorSubtract (r_refdef.vieworg, e->origin, modelorg);
	if (rotated)
	{
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

    glPushMatrix ();
e->angles[0] = -e->angles[0];	// stupid quake bug
	R_RotateForEntity (e);
e->angles[0] = -e->angles[0];	// stupid quake bug

	//Draw model with specified ambient color
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	//
	// draw texture
	//
	for (i=0 ; i<clmodel->nummodelsurfaces ; i++, psurf++)
	{
	// find which side of the node we are on
		pplane = psurf->plane;

		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

	// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
				R_RenderBrushPolyCaustics (psurf);
		}
	}

	//R_BlendLightmaps (); nope no lightmaps 
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glPopMatrix ();
}

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModelAmbient (entity_t *e)
{
	vec3_t		mins, maxs;
	int			i;
	msurface_t	*psurf;
	float		dot;
	mplane_t	*pplane;
	model_t		*clmodel;
	qboolean	rotated;

	//bright = 1;

	currententity = e;
	currenttexture = -1;

	clmodel = e->model;

	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		rotated = true;
		for (i=0 ; i<3 ; i++)
		{
			mins[i] = e->origin[i] - clmodel->radius;
			maxs[i] = e->origin[i] + clmodel->radius;
		}
	}
	else
	{
		rotated = false;
		VectorAdd (e->origin, clmodel->mins, mins);
		VectorAdd (e->origin, clmodel->maxs, maxs);
	}

	if (R_CullBox (mins, maxs))
		return;

	VectorSubtract (r_refdef.vieworg, e->origin, modelorg);
	if (rotated)
	{
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

    glPushMatrix ();
e->angles[0] = -e->angles[0];	// stupid quake bug
	R_RotateForEntity (e);
e->angles[0] = -e->angles[0];	// stupid quake bug

	//Draw model with specified ambient color
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	//
	// draw texture
	//
	for (i=0 ; i<clmodel->nummodelsurfaces ; i++, psurf++)
	{
	// find which side of the node we are on
		pplane = psurf->plane;

		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

	// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
				R_RenderBrushPolyLightmap (psurf);
		}
	}

	//R_BlendLightmaps (); nope no lightmaps 
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glPopMatrix ();
}


/*
=============================================================

	WORLD MODEL

=============================================================
*/

/*
================
R_RecursiveWorldNode

PENTA: Modifications
================
*/
qboolean didnode;

void R_RecursiveWorldNode (mnode_t *node)
{
	int			c, side;
	mplane_t	*plane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	double		dot;

	if (node->contents == CONTENTS_SOLID)
		return;		// solid

	if (node->visframe != r_visframecount)
		return;

	if (R_CullBox (node->minmaxs, node->minmaxs+3))
		return;

	
		if (mirror) {

			int side = BoxOnPlaneSide(node->minmaxs, node->minmaxs+3, mirror_plane); 
			if (mirror_clipside == side) {
				return;
			}

			if ( BoxOnPlaneSide(node->minmaxs, node->minmaxs+3, &mirror_far_plane) == 1) {
				return;
			}
		}


// if a leaf node, draw stuff
	if (node->contents < 0)
	{

/*		
		if (mirror) {
			if (mirror_clipside == BoxOnPlaneSide(node->minmaxs, node->minmaxs+3, mirror_plane)) {
				return;
			}
		}
	
*/
		pleaf = (mleaf_t *)node;
		
		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
/*
				if ((*mark)->flags & SURF_MIRROR) {
					if (!mirror)
						R_AllocateMirror((*mark));	
					mark++;
					continue;
				}*/

				(*mark)->visframe = r_framecount;
				mark++;
			} while (--c);
		}

	// deal with model fragments in this leaf
	// steek de statishe moddellen voor dit leafy in de lijst
	// waarom zo ingewikkeld doen via efrags is nog niet helemaal duidelijk
		if (pleaf->efrags)
			R_StoreEfrags (&pleaf->efrags);

		return;
	}

// node is just a decision point, so go down the apropriate sides

// find which side of the node we are on
	plane = node->plane;

	switch (plane->type)
	{
	case PLANE_X:
		dot = modelorg[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = modelorg[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = modelorg[2] - plane->dist;
		break;
	default:
		dot = DotProduct (modelorg, plane->normal) - plane->dist;
		break;
	}

	if (dot >= 0)
		side = 0;
	else
		side = 1;

// recurse down the children, front side first
	R_RecursiveWorldNode (node->children[side]);

// draw stuff
	c = node->numsurfaces;

	if (c)
	{
		surf = cl.worldmodel->surfaces + node->firstsurface;

		if (dot < 0 -BACKFACE_EPSILON)
			side = SURF_PLANEBACK;
		else if (dot > BACKFACE_EPSILON)
			side = 0;
		{
			for ( ; c ; c--, surf++)
			{
				if (surf->visframe != r_framecount)
					continue;

				// don't backface underwater surfaces, because they warp
				
				if ( (dot < 0) ^ !!(surf->flags & SURF_PLANEBACK))
					continue;		// wrong side
				

				
				if (surf->flags & SURF_MIRROR) {

					//PENTA the  SURF_UNDERWATER check is needed so that we dont draw glass
					//twice since we design it as a water block we would render two poly's with the
					//mirror otherwice
					if(  (! ((surf->flags & SURF_UNDERWATER) && (surf->flags & SURF_GLASS)) )|| (!(surf->flags & SURF_DRAWTURB)))

						if (!mirror) R_AllocateMirror(surf);
					//destroy visframe
					surf->visframe = 0;
					continue;
				}

				if ((surf->flags & SURF_DRAWTURB) && (surf->flags & SURF_MIRROR)) {
					continue;
				}
				
				if ((surf->flags & SURF_DRAWTURB) && (mirror)) {
					continue;
				}

				// if sorting by texture, just store it out
				/*if (gl_texsort.value)
				{*/

					// add the surface to the proper texture chain 
					//if (!mirror
					//|| surf->texinfo->texture != cl.worldmodel->textures[mirrortexturenum])
					//{
						surf->texturechain = surf->texinfo->texture->texturechain;
						surf->texinfo->texture->texturechain = surf;
					//}
					

					// add the poly to the proper lightmap chain 
					//steek hem in de lijst en vermenigvuldig de lightmaps er achteraf over via
					//r_blendlightmaps
					//we steken geen lucht/water polys in de lijst omdat deze geen
					//lightmaps hebben
					if (!(surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))) {
						surf->polys->chain = lightmap_polys[surf->lightmaptexturenum];
						lightmap_polys[surf->lightmaptexturenum] = surf->polys;
					}

				/*
				} else if (surf->flags & SURF_DRAWSKY) {
					surf->texturechain = skychain;
					skychain = surf;
				} else if (surf->flags & SURF_DRAWTURB) {
					surf->texturechain = waterchain;
					waterchain = surf;
				} else
					R_DrawSequentialPoly (surf);
				*/

			}
		}

	}

// recurse down the back side
	R_RecursiveWorldNode (node->children[!side]);
}



/*
=============
R_InitDrawWorld

Make sure everyting is set up correctly to draw
the world model.

PENTA: Modifications
=============
*/
void R_InitDrawWorld (void)
{
	entity_t	ent;

	memset (&ent, 0, sizeof(ent));
	ent.model = cl.worldmodel;

	VectorCopy (r_refdef.vieworg, modelorg);

	currententity = &ent;
	currenttexture = -1;

	glColor3f (1,1,1);
	memset (lightmap_polys, 0, sizeof(lightmap_polys));
//#ifdef QUAKE2

	R_ClearSkyBox ();
//#endif
	//if (mirror) Con_Printf("RWorldn\n");
	//mark visible polygons/ents
	R_RecursiveWorldNode (cl.worldmodel->nodes);
//#ifdef QUAKE2

	if (gl_wireframe.value) return;
	R_DrawSkyBox ();
	//if (fog_enabled.value) glEnable(GL_FOG);
//#endif
}

/*
=============
R_DrawWorldAmbient

Draw the ambient lighting of the world
the scene should be drawn (even if we want black ambient) since
we need the depthbuffer to do correct
stencil shadows.
PENTA: New
=============
*/
void R_DrawWorldAmbient (void) {
	R_BlendLightmaps ();
}

/*
=============
R_WorldMultiplyTextures

Once we have drawn the lightmap in the depth buffer multiply the textures over it
PENTA: New
=============
*/
void R_WorldMultiplyTextures(void) {
	DrawTextureChains ();
	//R_RenderDlights (); //render "fake" dynamic lights als die aan staan
}

/*
===============
R_MarkLeaves


Markeer visible leafs voor dit frame
aangeroepen vanuit render scene voor alle 
ander render code
(hmm handig hiernaa kunnen we dus al van de vis voor de camera
gebruik maken en toch nog stuff doen)

PENTA: Modifications

===============
*/
void R_MarkLeaves (void)
{
	byte	*vis;
	mnode_t	*node;
	int		i;
	byte	solid[4096];

	//zelfde leaf als vorige frame vermijd van vis opnieuw te calculaten
//	if (r_oldviewleaf == r_viewleaf && !r_novis.value)
//		return;
	
	//	return;

	r_visframecount++;//begin nieuwe timestamp
	r_oldviewleaf = r_viewleaf;

	if (r_novis.value)
	{
		vis = solid;
		memset (solid, 0xff, (cl.worldmodel->numleafs+7)>>3);
	}
	else
		vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);//vraag visible leafs vanuit dit leaf aan model
		
	for (i=0 ; i<cl.worldmodel->numleafs ; i++)//overloop alle leafs
	{
		if (vis[i>>3] & (1<<(i&7))) // staat leaf i's vis bit op true ?
		{
			node = (mnode_t *)&cl.worldmodel->leafs[i+1];
			do
			{
				if (node->visframe == r_visframecount) //node al op true gezet dit frame, exit
					break;
				node->visframe = r_visframecount;//zet node zijn timestamp voor dit frame
				node = node->parent;//mark parents as vis
			} while (node);
		}
	}
}



/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

// returns a texture number and the position inside it
int AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		texnum;

	for (texnum=0 ; texnum<MAX_LIGHTMAPS ; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i=0 ; i<BLOCK_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (allocated[texnum][i+j] >= best)
					break;
				if (allocated[texnum][i+j] > best2)
					best2 = allocated[texnum][i+j];
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
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error ("AllocBlock: full");
	return 0;
}


mvertex_t	*r_pcurrentvertbase;
model_t		*currentmodel;

int	nColinElim;

//PENTA: temporaly storage for polygons that use an edge
typedef struct {
	int used;				//how many polygons use this edge
	glpoly_t	*poly[2];	//pointer to the polygons who use this edge
} temp_connect_t;

temp_connect_t *tempEdges;


/*
================
PENTA:
SetupSurfaceConnectivity

Setup the neighour pointers of this surface's polygon.
================
*/
void SetupSurfaceConnectivity(msurface_t *surf)
{
	int i,j,lindex;
	temp_connect_t *tempEdge;

	if (surf->numedges > 50) {
		Con_Printf ("to many edges %i\n", surf->numedges);
	}

	for (i=0 ; i<surf->numedges ; i++)
	{
		lindex = currentmodel->surfedges[surf->firstedge + i];
		tempEdge = tempEdges+abs(lindex);

		surf->polys->neighbours[i] = NULL;
		for (j=0; j<tempEdge->used; j++) {
			if (tempEdge->poly[j] != surf->polys) {				
				surf->polys->neighbours[i] = tempEdge->poly[j]; 
			}
		}
	}
}

/*
================
PENTA:
PrintTempEdges

================
*/
void PrintTempEdges()
{
	int i;
	temp_connect_t *tempEdge;

	for (i=0 ; i<currentmodel->numedges ; i++)
	{
		tempEdge = tempEdges+i;

		Con_Printf("moord en brand %d\n",tempEdge->used);
	}
}

/*
================
BuildPolyFromSurface

Creer polygons van de lijst van surfaces om
gemakkelijk aan opengl te geven
================
*/
void BuildPolyFromSurface (msurface_t *fa)
{
	int			i, lindex, lnumverts;
	medge_t		*pedges, *r_pedge;
	int			vertpage;
	float		*vec;
//	float		s, t;
	glpoly_t	*poly;
	temp_connect_t *tempEdge;
	float		tex[2];
	float		light[2];

// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	//
	// draw texture <= wat mag dit comment me betekenen?
	//
	poly = Hunk_Alloc (sizeof(glpoly_t));
	//PENTA: reserve space for neighbour pointers
	//PENTA: FIXME: pointers don't need to be 4 bytes
	poly->neighbours = Hunk_Alloc (lnumverts*sizeof (glpoly_t*));

	poly->next = fa->polys;
	poly->flags = fa->flags;
	fa->polys = poly;
	poly->numverts = lnumverts;
	poly->firstvertex = R_GetNextVertexIndex();
	for (i=0 ; i<lnumverts ; i++)
	{
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = r_pcurrentvertbase[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = r_pcurrentvertbase[r_pedge->v[1]].position;
		}
		tex[0] = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		tex[0] /= fa->texinfo->texture->width;

		tex[1] = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		tex[1] /= fa->texinfo->texture->height;

		//
		// lightmap texture coordinates
		//
		light[0] = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		light[0] -= fa->texturemins[0];
		light[0] += fa->light_s*16;
		light[0] += 8;
		light[0] /= BLOCK_WIDTH*16; //fa->texinfo->texture->width;

		light[1] = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		light[1] -= fa->texturemins[1];
		light[1] += fa->light_t*16;
		light[1] += 8;
		light[1] /= BLOCK_HEIGHT*16; //fa->texinfo->texture->height;

		//Push back the new vertex
		R_AllocateVertexInTemp(vec,tex,light);

		//PENTA: Store in the tempedges table that this polygon uses the edge
		tempEdge = tempEdges+abs(lindex);

		if (tempEdge->used < 2) {
			tempEdge->poly[tempEdge->used]  = poly;
			tempEdge->used++;
		} else {
			Con_Printf ("BuildSurfaceDisplayList: Edge used by more than 2 surfaces\n");
		}
	}

	poly->numverts = lnumverts;

}

/*
========================
GL_CreateSurfaceLightmap
========================
*/
void GL_CreateSurfaceLightmap (msurface_t *surf)
{
	int		smax, tmax;
	byte	*base;

	if (surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
		return;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	surf->lightmaptexturenum = AllocBlock (smax, tmax, &surf->light_s, &surf->light_t);
	base = lightmaps + surf->lightmaptexturenum*lightmap_bytes*BLOCK_WIDTH*BLOCK_HEIGHT;
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * lightmap_bytes;
	R_BuildLightMap (surf, base, BLOCK_WIDTH*lightmap_bytes);
}


/*
==================
GL_BuildLightmaps

Builds the lightmap texture
with all the surfaces from all brush models
==================
*/
void GL_BuildLightmaps (void)
{
	int		i, j;
	model_t	*m;
	extern qboolean isPermedia;
#if defined(__APPLE__) || defined(MACOSX)
	extern qboolean	gl_luminace_lightmaps;
#endif /* __APPLE__ || MACOSX */

	memset (allocated, 0, sizeof(allocated));

	r_framecount = 1;		// no dlightcache

	if (!lightmap_textures)
	{
		lightmap_textures = texture_extension_number;
		texture_extension_number += MAX_LIGHTMAPS;
	}

#if defined(__APPLE__) || defined(MACOSX)

	// <AWE> MacOS X v10.1, GLQuake v1.0.2:
	// Using GL_LUMINACE causes frame drops in rooms with many lightmap textures.
	// Since with GL_INTENSITY, GL_ALPHA will result in totaly dark surfaces, we have to use GL_RGBA as default.
        
	if (gl_luminace_lightmaps == false)
		gl_lightmap_format = GL_RGBA;
	else

#endif /* __APPLE__ || MACOSX */
		gl_lightmap_format = GL_LUMINANCE;

	// default differently on the Permedia
	if (isPermedia)
		gl_lightmap_format = GL_RGBA;

	if (COM_CheckParm ("-lm_1"))
		gl_lightmap_format = GL_LUMINANCE;
	if (COM_CheckParm ("-lm_a"))
		gl_lightmap_format = GL_ALPHA;
	if (COM_CheckParm ("-lm_i"))
		gl_lightmap_format = GL_INTENSITY;
	if (COM_CheckParm ("-lm_2"))
		gl_lightmap_format = GL_RGBA4;
	if (COM_CheckParm ("-lm_4"))
		gl_lightmap_format = GL_RGBA;

	switch (gl_lightmap_format)
	{
	case GL_RGBA:
		lightmap_bytes = 4;
		break;
	case GL_RGBA4:
		lightmap_bytes = 2;
		break;
	case GL_LUMINANCE:
	case GL_INTENSITY:
	case GL_ALPHA:
		lightmap_bytes = 1;
		break;
	}

	for (j=1 ; j<MAX_MODELS ; j++)
	{
		m = cl.model_precache[j];
		if (!m)
			break;
		if (m->name[0] == '*')
			continue;

		r_pcurrentvertbase = m->vertexes;
		currentmodel = m;
		
		//PENTA: Allocate storage for our edge table
		tempEdges = Hunk_TempAlloc(m->numedges * sizeof(temp_connect_t));

		//clear tempedges
		for (i=0; i<m->numedges; i++) {
			tempEdges[i].used = 0;
			tempEdges[i].poly[0] = NULL;
			tempEdges[i].poly[1] = NULL;
		}



		for (i=0 ; i<m->numsurfaces ; i++)
		{
			GL_CreateSurfaceLightmap (m->surfaces + i);
			if ( m->surfaces[i].flags & SURF_DRAWTURB )
				continue;
/*#ifndef QUAKE2
			if ( m->surfaces[i].flags & SURF_DRAWSKY )
				continue;
#endif*/
			BuildPolyFromSurface (m->surfaces + i);
		}

		//PENTA: we now have the connectivity in tempEdges now store it in the polygons
		for (i=0 ; i<m->numsurfaces ; i++)
		{
			if ( m->surfaces[i].flags & SURF_DRAWTURB )
				continue;
#ifndef QUAKE2
			if ( m->surfaces[i].flags & SURF_DRAWSKY )
				continue;
#endif
			SetupSurfaceConnectivity (m->surfaces + i);
		}

	}
	Con_Printf("Connectivity calculated\n");


	/*
	PENTA: Normalize texture coordinate s/t's since we now use them as tangent space
	*/
	for (j=1 ; j<MAX_MODELS ; j++)
	{
		mtexinfo_t *texinfos;

		m = cl.model_precache[j];
		if (!m)
			break;
		if (m->name[0] == '*')
			continue;

		texinfos = m->texinfo;
		for (i=0; i<m->numtexinfo; i++) {
			VectorNormalize(texinfos[i].vecs[0]);
			VectorNormalize(texinfos[i].vecs[1]);
		}
	}


	/*
 	if (!gl_texsort.value)
 		GL_SelectTexture(GL_TEXTURE1_ARB);
	*/

	//
	// upload all lightmaps that were filled
	//
	for (i=0 ; i<MAX_LIGHTMAPS ; i++)
	{
		if (!allocated[i][0])
			break;		// no more used
		lightmap_modified[i] = false;
		lightmap_rectchange[i].l = BLOCK_WIDTH;
		lightmap_rectchange[i].t = BLOCK_HEIGHT;
		lightmap_rectchange[i].w = 0;
		lightmap_rectchange[i].h = 0;
		GL_Bind(lightmap_textures + i);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		GL_TexImage2D (GL_TEXTURE_2D, 0, lightmap_bytes
		, BLOCK_WIDTH, BLOCK_HEIGHT, 0, 
		gl_lightmap_format, GL_UNSIGNED_BYTE, lightmaps+i*BLOCK_WIDTH*BLOCK_HEIGHT*lightmap_bytes);
	}

	/*
 	if (!gl_texsort.value)
 		GL_SelectTexture(GL_TEXTURE0_ARB);
	*/

}

