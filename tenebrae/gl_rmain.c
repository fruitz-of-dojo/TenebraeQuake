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
// r_main.c

#include "quakedef.h"
#include "glATI.h"

entity_t	r_worldentity;

qboolean	r_cache_thrash;		// compatability

vec3_t		modelorg, r_entorigin;
entity_t	*currententity;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking
int			r_lightTimestamp;	// PENTA: incresed when next light is started

mplane_t	frustum[4];

int			c_brush_polys, c_alias_polys;

qboolean	envmap;				// true during envmap command capture 

int			currenttexture = -1;		// to avoid unnecessary texture sets

int			cnttextures[16] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };     // cached

int			particletexture;	// little dot for particles
int			particletexture_smoke;
int			particletexture_glow;
int			particletexture_glow2;
int			particletexture_tele;
int			particletexture_blood;
int			particletexture_dirblood;
int			playertextures;		// up to 16 color translated skins

int			mirrortexturenum;	// quake texturenum, not gltexturenum
qboolean	mirror;
qboolean	glare;
mplane_t	*mirror_plane;
mplane_t	mirror_far_plane; //far plane of the view frustum for mirrors
int			mirror_clipside;
msurface_t	*causticschain;
int			caustics_textures[8];
qboolean	busy_caustics = false;

//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

float	r_projection_matrix[16];
float	r_world_matrix[16];
float	r_base_world_matrix[16];
double	r_Dproject_matrix[16];//PENTA
double	r_Dworld_matrix[16];//PENTA
int		r_Iviewport[4];//PENTA
int		numClearsSaved;//PENTA

float color_black[4] = {0.0, 0.0, 0.0, 0.0};

//
// screen size info
//
refdef_t	r_refdef;

mleaf_t		*r_viewleaf, *r_oldviewleaf;

texture_t	*r_notexture_mip;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value


void R_MarkLeaves (void);
void R_Clear (void);

cvar_t	r_norefresh = {"r_norefresh","0"};
cvar_t	r_drawentities = {"r_drawentities","1"};
cvar_t	r_drawviewmodel = {"r_drawviewmodel","1"};
cvar_t	r_fullbright = {"r_fullbright","0"};
cvar_t	r_lightmap = {"r_lightmap","0"};
cvar_t	r_shadows = {"r_shadows","0"};
//cvar_t	r_mirroralpha = {"r_mirroralpha","1"};
cvar_t	r_wateralpha = {"r_wateralpha","0.5"};//PENTA: different default
cvar_t	r_dynamic = {"r_dynamic","1"};
cvar_t	r_novis = {"r_novis","0"};

cvar_t	gl_finish = {"gl_finish","0"};
cvar_t	gl_clear = {"gl_clear","0"};
cvar_t	gl_cull = {"gl_cull","1"};
//cvar_t	gl_texsort = {"gl_texsort","1"};
cvar_t	gl_smoothmodels = {"gl_smoothmodels","1"};
cvar_t	gl_affinemodels = {"gl_affinemodels","0"};
cvar_t	gl_polyblend = {"gl_polyblend","1"};
cvar_t	gl_flashblend = {"gl_flashblend","1"};
cvar_t	gl_playermip = {"gl_playermip","0"};
cvar_t	gl_nocolors = {"gl_nocolors","0"};
//cvar_t	gl_keeptjunctions = {"gl_keeptjunctions","0"}; PENTA: Don't remove t-junctions
cvar_t	gl_reporttjunctions = {"gl_reporttjunctions","0"};
cvar_t	gl_doubleeyes = {"gl_doubleeys", "1"};

cvar_t	gl_watershader = {"gl_watershader","1"};//PENTA: water shaders ON/OFF
cvar_t	gl_calcdepth = {"gl_calcdepth","0"};

cvar_t	sh_lightmapbright = {"sh_lightmapbright","0.5"};//PENTA: brightness of lightmaps
cvar_t	sh_radiusscale = {"sh_radiusscale","0.5"};//PENTA: brightness of lightmaps
cvar_t	sh_visiblevolumes = {"sh_visiblevolumes","0"};//PENTA: draw shadow volumes on/off
cvar_t  sh_entityshadows = {"sh_entityshadows","1"};//PENTA: entities cast shadows on/off
cvar_t  sh_worldshadows = {"sh_worldshadows","1"};//PENTA: brushes cast shadows on/off
cvar_t  sh_showlightnum = {"sh_showlightnum","0"};//PENTA: draw numer of lights used this frame
cvar_t  sh_glows = {"sh_glows","1"};//PENTA: draw glows around some light sources
cvar_t	sh_fps = {"sh_fps","0", true};	// set for running times - muff
cvar_t	sh_debuginfo = {"sh_debuginfo","0"};
cvar_t	sh_norevis = {"sh_norevis","0"};//PENTA: no recalculating the vis for light positions
cvar_t	sh_nosvbsp = {"sh_nosvbsp","0"};//PENTA: no shadow bsp
cvar_t	sh_noeclip = {"sh_noeclip","0"};//PENTA: no entity/leaf clipping for shadows
cvar_t  sh_infinitevolumes = {"sh_infinitevolumes","0", true};//PENTA: Nvidia infinite volumes 
cvar_t  sh_noscissor = {"sh_noscissor","0"};//PENTA: no scissoring
cvar_t	sh_nocleversave = {"sh_nocleversave","0"};//PENTA: don't change light drawing order to reduce stencil clears
cvar_t	sh_bumpmaps = {"sh_bumpmaps","1"};//PENTA: enable disable bump mapping
cvar_t	sh_colormaps = {"sh_colormaps","1"};//PENTA: enable disable textures on the world (light will remain)
cvar_t	sh_playershadow = {"sh_playershadow","1"};//PENTA: the player casts a shadow (the one YOU are playing with, others always cast shadows)
cvar_t	sh_nocache = {"sh_nocache","0"};
cvar_t	sh_glares = {"sh_glares","0",true};
cvar_t	sh_noefrags = {"sh_noefrags","0",true};
cvar_t	sh_showtangent = {"sh_showtangent","0"};
cvar_t	sh_noshadowpopping = {"sh_noshadowpopping","1"};

cvar_t	mir_detail = {"mir_detail","1",true};
cvar_t	mir_frameskip = {"mir_frameskip","1",true};
cvar_t	mir_forcewater = {"mir_forcewater","0"};
cvar_t	mir_distance = {"mir_distance","400",true};
cvar_t  gl_wireframe = {"gl_wireframe","0"}; 
cvar_t  gl_caustics = {"gl_caustics","1"};
cvar_t  gl_truform = {"gl_truform","0"};
cvar_t  gl_truform_tesselation = {"gl_truform_tesselation","4"};
cvar_t	gl_transformlerp = {"gl_transformlerp","0",true};//Erad - transform interpolation cvar (off by default due to bugs)

cvar_t	fog_r = {"fog_r","0.2"};
cvar_t	fog_g = {"fog_g","0.1"};
cvar_t	fog_b = {"fog_b","0.0"};
cvar_t	fog_start = {"fog_start","256"};
cvar_t	fog_end = {"fog_end","700"};
cvar_t	fog_enabled = {"fog_enabled","1"};
cvar_t  fog_waterfog = {"fog_waterfog","1"}; 
float fog_color[4];

mirrorplane_t mirrorplanes[NUM_MIRROR_PLANES];
int mirror_contents;

#ifndef GL_ATI_pn_triangles
#define GL_PN_TRIANGLES_ATI                       0x87F0
#define GL_MAX_PN_TRIANGLES_TESSELATION_LEVEL_ATI 0x87F1
#define GL_PN_TRIANGLES_POINT_MODE_ATI            0x87F2
#define GL_PN_TRIANGLES_NORMAL_MODE_ATI           0x87F3
#define GL_PN_TRIANGLES_TESSELATION_LEVEL_ATI     0x87F4
#define GL_PN_TRIANGLES_POINT_MODE_LINEAR_ATI     0x87F5
#define GL_PN_TRIANGLES_POINT_MODE_CUBIC_ATI      0x87F6
#define GL_PN_TRIANGLES_NORMAL_MODE_LINEAR_ATI    0x87F7
#define GL_PN_TRIANGLES_NORMAL_MODE_QUADRATIC_ATI 0x87F8

typedef void (APIENTRY *PFNGLPNTRIANGLESIATIPROC)(GLenum pname, GLint param);
typedef void (APIENTRY *PFNGLPNTRIANGLESFATIPROC)(GLenum pname, GLfloat param);
#endif

// actually in gl_bumpradeon (duh...)
extern qboolean	gl_pntriangles;
extern PFNGLPNTRIANGLESIATIPROC qglPNTrianglesiATI;
extern PFNGLPNTRIANGLESFATIPROC qglPNTrianglesfATI;

#define GL_INCR_WRAP_EXT                                        0x8507
#define GL_DECR_WRAP_EXT                                        0x8508


#define	MIN_PLAYER_MIRROR 48 //max size of player bounding box

//extern	cvar_t	gl_ztrick; PENTA: Removed

int SV_HullPointContents (hull_t *hull, int num, vec3_t p);

/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	int		i;

	for (i=0 ; i<4 ; i++)
		if (BoxOnPlaneSide (mins, maxs, &frustum[i]) == 2)
			return true;
	return false;
}


/*
=============
R_RotateForEntity

Erad - transform interpolation
=============
*/
void R_RotateForEntity (entity_t *e)
{
	float timepassed;
	float blend;
	vec3_t d;
	int i;

	//Hack to stop t lerping view models and player - Eradicator 
    if ((!strcmp (e->model->name, "progs/v_shot.mdl")) || (!strcmp (e->model->name, "progs/v_shot2.mdl")) 
		|| (!strcmp (e->model->name, "progs/v_nail.mdl")) || (!strcmp (e->model->name, "progs/v_nail2.mdl")) 
        || (!strcmp (e->model->name, "progs/v_rock.mdl")) || (!strcmp (e->model->name, "progs/v_rock2.mdl")) 
        || (!strcmp (e->model->name, "progs/v_axe.mdl")) || (!strcmp (e->model->name, "progs/v_light.mdl"))
		|| (!strcmp (e->model->name, "progs/players.mdl")) || (!gl_transformlerp.value)) 
    {  
		R_UnlerpedRotateForEntity(e); 
		return; 
	}

    timepassed = realtime - e->translate_start_time; 

    if (e->translate_start_time == 0 || timepassed > 1)
    {
		e->translate_start_time = realtime;
		VectorCopy (e->origin, e->origin1);
		VectorCopy (e->origin, e->origin2);
	}

	if (!VectorCompare (e->origin, e->origin2))
	{
		e->translate_start_time = realtime;
		VectorCopy (e->origin2, e->origin1);
		VectorCopy (e->origin,  e->origin2);
		blend = 0;
	}
	else
	{
		blend =  timepassed / 0.1;

		if (cl.paused || blend > 1) blend = 1;
	}

	VectorSubtract (e->origin2, e->origin1, d);

	glTranslatef (
		e->origin1[0] + (blend * d[0]),
		e->origin1[1] + (blend * d[1]),
		e->origin1[2] + (blend * d[2]));

	timepassed = realtime - e->rotate_start_time; 

	if (e->rotate_start_time == 0 || timepassed > 1)
	{
		e->rotate_start_time = realtime;
		VectorCopy (e->angles, e->angles1);
		VectorCopy (e->angles, e->angles2);
	}

	if (!VectorCompare (e->angles, e->angles2))
	{
		e->rotate_start_time = realtime;
		VectorCopy (e->angles2, e->angles1);
		VectorCopy (e->angles,  e->angles2);
		blend = 0;
	}
	else
	{
		blend = timepassed / 0.1;
 
		if (cl.paused || blend > 1) blend = 1;
		}
			VectorSubtract (e->angles2, e->angles1, d);

             for (i = 0; i < 3; i++) 
             {
                 if (d[i] > 180)
                 {
                     d[i] -= 360;
                 }
                 else if (d[i] < -180)
                 {
                     d[i] += 360;
                 }
			}
		glRotatef ( e->angles1[1] + ( blend * d[1]),  0, 0, 1);
		glRotatef (-e->angles1[0] + (-blend * d[0]),  0, 1, 0);
		glRotatef ( e->angles1[2] + ( blend * d[2]),  1, 0, 0);
	}

void R_UnlerpedRotateForEntity (entity_t *e)
{
    glTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);

    glRotatef (e->angles[1],  0, 0, 1);
    glRotatef (-e->angles[0],  0, 1, 0);
    glRotatef (e->angles[2],  1, 0, 0);
}

int CL_PointContents (vec3_t p)
{
	int		cont;

	cont = SV_HullPointContents (&cl.worldmodel->hulls[0], 0, p);
	if (cont <= CONTENTS_CURRENT_0 && cont >= CONTENTS_CURRENT_DOWN)
		cont = CONTENTS_WATER;
	return cont;
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t *R_GetSpriteFrame (entity_t *currententity)
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	int				i, numframes, frame;
	float			*pintervals, fullinterval, targettime, time;

	psprite = currententity->model->cache.data;
	frame = currententity->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		Con_Printf ("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = cl.time + currententity->syncbase;

	// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
	// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i=0 ; i<(numframes-1) ; i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}

/*
=================
R_DrawSpriteModel

=================
*/
#define VectorScalarMult(a,b,c) {c[0]=a[0]*b;c[1]=a[1]*b;c[2]=a[2]*b;}
void R_DrawSpriteModel (entity_t *e) //Oriented Sprite Fix - Eradicator
{
	vec3_t	point;
	mspriteframe_t	*frame;
	float		*up, *right;
	vec3_t		v_forward, v_right, v_up;
	msprite_t		*psprite;
	vec3_t		fixed_origin;
	vec3_t		temp;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	frame = R_GetSpriteFrame (e);
	psprite = currententity->model->cache.data;

	VectorCopy(e->origin,fixed_origin);

	if (psprite->type == SPR_ORIENTED)
	{	// bullet marks on walls
		AngleVectors (currententity->angles, v_forward, v_right, v_up);
		VectorScalarMult(v_forward,-2,temp);
		VectorAdd(temp,fixed_origin,fixed_origin);
		up = v_up;
		right = v_right;
	}
	else
	{	// normal sprite
		up = vup;
		right = vright;
	}

	GL_DisableMultitexture();

    GL_Bind(frame->gl_texturenum);

	glEnable (GL_ALPHA_TEST);
	glBegin (GL_QUADS);

	glTexCoord2f (0, 1);
	//VectorMA (e->origin, frame->down, up, point); //Old
	VectorMA (fixed_origin, frame->down, up, point); //Fixed Origin
	VectorMA (point, frame->left, right, point);
	glVertex3fv (point);

	glTexCoord2f (0, 0);
	VectorMA (fixed_origin, frame->up, up, point);
	VectorMA (point, frame->left, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 0);
	VectorMA (fixed_origin, frame->up, up, point);
	VectorMA (point, frame->right, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 1);
	VectorMA (fixed_origin, frame->down, up, point);
	VectorMA (point, frame->right, right, point);
	glVertex3fv (point);
	
	glEnd ();

	glDisable (GL_ALPHA_TEST);
}

/*
=============================================================

  ALIAS MODELS

=============================================================
*/


float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

vec3_t	shadevector;
float	shadelight, ambientlight;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anorm_dots.h"
;

float	*shadedots = r_avertexnormal_dots[0];

int	lastposenum;

void GL_DrawPentaAliasFrame (aliashdr_t *paliashdr, int posenum);
/*
=============
GL_DrawAliasFrame
=============
*/

void GL_DrawAliasFrame (aliashdr_t *paliashdr, int posenum)
{
	//Crappy
}


ftrivertx_t *apverts;
void GL_DrawPentaAliasFrame (aliashdr_t *paliashdr, int posenum) {

	mtriangle_t *ptri;
	ftrivertx_t	*verts;
	plane_t		*planes;
	int i,j;

	return;

	//verts = apverts;
	verts = (ftrivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	ptri = (mtriangle_t *)((byte *)paliashdr + paliashdr->triangles);
	planes = (plane_t *)((byte *)paliashdr + paliashdr->planes);
	planes += posenum * paliashdr->numtris;

	for (i=0 ; i<paliashdr->numtris ; i++, ptri++, planes++)
	{
		glBegin(GL_TRIANGLES);
		//Con_Printf("%i %i %i\n", (int)ptri->vertindex[0], (int)ptri->vertindex[1], (int)ptri->vertindex[2]); 
		glTexCoord2f(0.5, 0.5);
		for (j=0; j<3; j++) {
			glVertex3f(verts[ptri->vertindex[j]].v[0],
					   verts[ptri->vertindex[j]].v[1],
					   verts[ptri->vertindex[j]].v[2]);
			//Con_Printf("%i \n",ptri->vertindex[j]); 
		}
		glEnd();
	}

}

extern	vec3_t			lightspot;


int		extrudeTimeStamp;
vec3_t	extrudedVerts[MAXALIASVERTS];	//PENTA: Temp buffer for extruded vertices
int		extrudedTimestamp[MAXALIASVERTS];	//PENTA: Temp buffer for extruded vertices
qboolean	triangleVis[MAXALIASTRIS];	//PENTA: Temp buffer for light facingness of triangles

void R_CalcAliasFrameShadowVolume (aliashdr_t *paliashdr,int posenum) {

	plane_t *planes;
	ftrivertx_t	*verts;	
	mtriangle_t	*tris, *triangle;
	float d, scale;
	int i, j;
	vec3_t v2, *v1;

	planes = (plane_t *)((byte *)paliashdr + paliashdr->planes);
	planes += posenum * paliashdr->numtris;
	verts = (ftrivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	tris = (mtriangle_t *)((byte *)paliashdr + paliashdr->triangles);

	extrudeTimeStamp++;
	//calculate visibility
	for (i=0; i<paliashdr->numtris; i++) {
		d = DotProduct(planes[i].normal, currentshadowlight->origin) - planes[i].dist;
		if (d > 0) 
			triangleVis[i] = true;
		else
			triangleVis[i] = false;
	}


	//extude vertices
	triangle = tris;
	for (i=0; i<paliashdr->numtris; i++, triangle++) {

		if (triangleVis[i]) {//backfacing extrude it!
			
			for (j=0; j<3; j++) {

				int index = triangle->vertindex[j];
				if (extrudedTimestamp[index] == extrudeTimeStamp) continue;
				extrudedTimestamp[index] = extrudeTimeStamp;

				v1 = &extrudedVerts[index];

				/*
				for (k=0; k<3; k++)
					v2[k] = (verts[index].v[k] * paliashdr->scale[k]) + paliashdr->scale_origin[k];
				*/
				VectorCopy(verts[index].v,v2);

				VectorSubtract (v2, currentshadowlight->origin, (*v1));
				scale = Length ((*v1));

				if (sh_visiblevolumes.value) {
					//make them short so that we see them
					VectorScale ((*v1), (1/scale)* 70, (*v1));
				} else {
					//we don't have to be afraid they will clip with the far plane
					//since we use the infinite matrix trick
					VectorScale ((*v1), (1/scale)* currentshadowlight->radius*10, (*v1));
				}
				VectorAdd ((*v1), v2 ,(*v1));
			}
		}
	}
}

void R_DrawAliasFrameShadowVolume2 (aliashdr_t *paliashdr,aliasframeinstant_t *instant) {

	mtriangle_t	*tris, *triangle;
	int i, j;
	aliaslightinstant_t *linstant = instant->lightinstant;

	tris = (mtriangle_t *)((byte *)paliashdr + paliashdr->triangles);


	//FIXME: what's faster? less begin/ends or iterating the list multiple times?
	//extude vertices
	triangle = tris;
	for (i=0; i<paliashdr->numtris; i++, triangle++) {

		if (linstant->triangleVis[i]) {

			for (j=0; j<3; j++) {

				qboolean shadow = false;
				if (triangle->neighbours[j] == -1) {
					shadow = true;
				} else if (!linstant->triangleVis[triangle->neighbours[j]]) {
					shadow = true;
				}

				if (shadow) {
					int index = triangle->vertindex[j];
					glBegin(GL_QUAD_STRIP);
					
					/*
					glVertex3f(verts[index].v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0],
					           verts[index].v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1],
						       verts[index].v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2]
					);
					*/
					glVertex3fv(&instant->vertices[index][0]);
					glVertex3fv(&linstant->extvertices[index][0]);

					index = triangle->vertindex[(j+1)%3];
					/*glVertex3f(verts[index].v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0],
					           verts[index].v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1],
						       verts[index].v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2]
					);
					*/
					glVertex3fv(&instant->vertices[index][0]);
					glVertex3fv(&linstant->extvertices[index][0]);
					glEnd();

				}
			}


			glBegin(GL_TRIANGLES);
			for (j=0; j<3; j++) {
				int index = triangle->vertindex[j];
				/*
				glVertex3f(verts[index].v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0],
					       verts[index].v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1],
						   verts[index].v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2]
				);
				*/
				glVertex3fv(&instant->vertices[index][0]);
			}
			glEnd();
			
			glBegin(GL_TRIANGLES);
			for (j=2; j>=0; j--) {
				glVertex3fv(&linstant->extvertices[triangle->vertindex[j]][0]);
			}
			glEnd();
			
		}

		/*
		glBegin(GL_LINES);	
		for (j=0; j<3; j++) {
			v2[j] = verts[ triangle->vertindex[0]].v[j] * paliashdr->scale[j] + paliashdr->scale_origin[j];
		}
		glVertex3fv(&v2[0]);
		VectorMA(v2, 5, planes[i].normal, v2);
		glVertex3fv(&v2[0]);
		glEnd();
		*/
	}
}
/*
=============
R_DrawAliasFrameShadowVolume
=============
*/
extern	vec3_t			lightspot;

/*
void R_DrawAliasFrameShadowVolume (aliashdr_t *paliashdr, int posenum)
{
	float	s, t, l;
	int		i, j;
	int		index;
	ftrivertx_t	*v, *verts;
	int		list;
	int		*order;
	vec3_t	point,dir;
	vec3_t	*normal;
	int		count;
	vec3_t	fnormal;
	verts = (ftrivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		}
		else
			glBegin (GL_TRIANGLE_STRIP);

		do
		{
			// texture coordinates come from the draw list
			// (skipped for shadow volumes) glTexCoord2fv ((float *)order);
			order += 2;

			// normals and vertexes come from the frame list
			
			//point[0] = verts->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			//point[1] = verts->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			//point[2] = verts->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];
			

			normal = &r_avertexnormals[verts->lightnormalindex];

			//VectorSubtract(point,currentshadowlight->origin,dir);
			VectorSubtract(verts->v,currentshadowlight->origin,dir);
			VectorNormalize(dir);

			if (DotProduct(dir,(*normal)) <= 0) {
				//etrude to "infinity"
				if (sh_visiblevolumes.value) {
					VectorScale(dir,50,dir);
				} else {
					VectorScale(dir,currentshadowlight->radius*10,dir);
				}
				//VectorAdd(point,dir,point);
				VectorAdd(verts->v,dir,point);
			} else 
				VectorCopy(verts->v,point);

			glVertex3fv (point);
			verts++;
		} while (--count);

		glEnd ();
	}	
}
*/

/*
=================
R_DrawAliasShadowVolume

=================
*/

void R_DrawAliasSurfaceShadowVolume (aliashdr_t	*paliashdr, aliasframeinstant_t *aliasframeinstant)
{
#if 1
	//
	//Pass 1 increase
	//
	glCullFace(GL_BACK);
	glStencilOp(GL_KEEP, GL_INCR, GL_KEEP);
	glCullFace(GL_FRONT);

	R_DrawAliasFrameShadowVolume2 (paliashdr, aliasframeinstant);

	//
	// Second Pass. Decrease Stencil Value In The Shadow
	//
	glCullFace(GL_FRONT);
	glStencilOp(GL_KEEP, GL_DECR, GL_KEEP);
	glCullFace(GL_BACK);
	
	R_DrawAliasFrameShadowVolume2 (paliashdr, aliasframeinstant);
#else
        glDisable(GL_CULL_FACE);
	glCullFace(GL_FRONT_AND_BACK);
        checkerror();
	glStencilOp(GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);
        checkerror();
	qglStencilFuncSeparateATI(GL_ALWAYS, GL_ALWAYS, 0, ~0);
        checkerror();
	qglStencilOpSeparateATI(GL_FRONT, GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);
        checkerror();
	qglStencilOpSeparateATI(GL_BACK, GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);
        checkerror();
	R_DrawAliasFrameShadowVolume2 (paliashdr, aliasframeinstant);

        glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
#endif
}


void R_DrawAliasShadowVolume (entity_t *e)
{
    model_t		*clmodel;
    aliashdr_t	*paliashdr;
    alias3data_t    *data;
    aliasframeinstant_t *aliasframeinstant;        
    int i,maxnumsurf;
    //vec3_t		oldlightpos;

    currententity = e;
	
    clmodel = currententity->model;

    /* no shadows casting for these */
    if (clmodel->flags & EF_NOSHADOW)
		return;

    //
    // locate the proper data
    //
    if (!e->aliasframeinstant) {
		Con_Printf("no instant for ent %s\n", clmodel->name);	
		return;
    }

    /*
      Don't cull to frustum models behind you may still cast shadows
  
      if (R_CullBox (mins, maxs))
      return;
    */

    VectorCopy (currententity->origin, r_entorigin);
    VectorSubtract (r_origin, r_entorigin, modelorg);

    //
    // locate the proper data
    //
    // data = (alias3data_t *)Mod_Extradata (e->model);

    aliasframeinstant = e->aliasframeinstant;

    data = (alias3data_t *)Mod_Extradata (e->model);
    maxnumsurf = data->numSurfaces;        
        
    glPushMatrix ();
    R_RotateForEntity (e);

    for (i=0;i<maxnumsurf;++i)
    {
		paliashdr = (aliashdr_t *)((char*)data + data->ofsSurfaces[i]);
		
		if (!aliasframeinstant) {
			glPopMatrix ();
            Con_Printf("R_DrawAliasShadowVolume: missing instant for ent %s\n", e->model->name);	
			return;
		}
		
		/*  doesn't fit with new structs
		if (paliashdr != ((aliasframeinstant_t *)e->model->aliasframeinstant)->paliashdr) {
		//Sys_Error("Cache trashed");
		r_cache_thrash = true;
		((aliasframeinstant_t *)e->model->aliasframeinstant)->paliashdr = paliashdr;
		}
		*/
		
		if ((e->frame >= paliashdr->numframes) || (e->frame < 0))
		{
			glPopMatrix ();
			return;
		}
		
		//
		// draw all the triangles
		//
		R_DrawAliasSurfaceShadowVolume(paliashdr,aliasframeinstant);
		aliasframeinstant = aliasframeinstant->_next;
		//VectorCopy(oldlightpos,currentshadowlight->origin);
    } /* for paliashdr */

    glPopMatrix();
}

/*
=================
R_SetupAliasFrame

=================
*/

void R_SetupAliasFrame (aliashdr_t *paliashdr, aliasframeinstant_t *instant)
{
	float*			texcoos;
	int*			indecies;
	/*
	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	apverts = (ftrivertx_t *)((byte *)paliashdr + paliashdr->frames[frame].frame);

	if (numposes > 1)
	{
		interval = paliashdr->frames[frame].interval;
		pose += (int)(cl.time / interval) % numposes;
	}
	*/

	texcoos = (float *)((byte *)paliashdr + paliashdr->texcoords);
	indecies = (int *)((byte *)paliashdr + paliashdr->indecies);
	//GL_DrawAliasFrame (paliashdr, pose);
	glVertexPointer(3, GL_FLOAT, 0, instant->vertices);
	glEnableClientState(GL_VERTEX_ARRAY);
	glNormalPointer(GL_FLOAT, 0, instant->normals);
	glEnableClientState(GL_NORMAL_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoos);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glDrawElements(GL_TRIANGLES,paliashdr->numtris*3,GL_UNSIGNED_INT,indecies);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

/*
	Draws the tangent space of the model
*/
void R_DrawAliasTangent (aliashdr_t *paliashdr, aliasframeinstant_t *instant)
{
	float*			texcoos;
	int*			indecies;
	vec3_t			extr;
	int				i;

	texcoos = (float *)((byte *)paliashdr + paliashdr->texcoords);
	indecies = (int *)((byte *)paliashdr + paliashdr->indecies);
	//GL_DrawAliasFrame (paliashdr, pose);

	for (i=0; i<paliashdr->poseverts; i++) {

		glColor3ub(255,0,0);
		glBegin(GL_LINES);
			glVertex3fv(&instant->vertices[i][0]);
			VectorMA(instant->vertices[i],1,instant->normals[i],extr);
			glVertex3fv(&extr[0]);
		glEnd();

		glColor3ub(0,255,0);
		glBegin(GL_LINES);
			glVertex3fv(&instant->vertices[i][0]);
			VectorMA(instant->vertices[i],1,instant->tangents[i],extr);
			glVertex3fv(&extr[0]);
		glEnd();

		glColor3ub(0,0,255);
		glBegin(GL_LINES);
			glVertex3fv(&instant->vertices[i][0]);
			VectorMA(instant->vertices[i],1,instant->binomials[i],extr);
			glVertex3fv(&extr[0]);
		glEnd();
	}
}


/*
=================
R_DrawAliasSurface
DC : draw one surface from a model

=================
*/

	

void R_DrawAliasSurface (aliashdr_t *paliashdr, float bright, aliasframeinstant_t *instant)
{
        int i;
	int			anim;

	//
	// draw all the triangles
	//

	if (!busy_caustics) {
		anim = (int)(cl.time*10) & 3;
		GL_Bind(paliashdr->gl_texturenum[currententity->skinnum][anim]);

		// we can't dynamically colormap textures, so they are cached
		// seperately for the players.  Heads are just uncolored.
		if (currententity->colormap != vid.colormap && !gl_nocolors.value)
		{
			i = (int)(currententity - cl_entities);
			if (i >= 1 && i<=cl.maxclients /* && !strcmp (currententity->model->name, "progs/player.mdl") */)
				GL_Bind(playertextures - 1 + i);
		}
	}

	//XYZ
	if (gl_wireframe.value) {
		glDisable(GL_TEXTURE_2D);
	}

	if (gl_pntriangles == true && gl_truform.value )
	{
	    glEnable(GL_PN_TRIANGLES_ATI);
	    qglPNTrianglesiATI(GL_PN_TRIANGLES_POINT_MODE_ATI, GL_PN_TRIANGLES_POINT_MODE_CUBIC_ATI);
	    qglPNTrianglesiATI(GL_PN_TRIANGLES_NORMAL_MODE_ATI, GL_PN_TRIANGLES_NORMAL_MODE_QUADRATIC_ATI);
	    qglPNTrianglesiATI(GL_PN_TRIANGLES_TESSELATION_LEVEL_ATI, gl_truform_tesselation.value);
	}



	glColor3f(bright, bright, bright);
	//if (busy_caustics)
	//	glColor3f(1,1,1);
        R_SetupAliasFrame (paliashdr, instant);

        // Draw luma if present
        if ( !busy_caustics )
        {
            anim = (int)(cl.time*10) & 3;
            if ( paliashdr->gl_lumatex[currententity->skinnum][anim] != 0)
            {
                glFogfv(GL_FOG_COLOR, color_black);
                glEnable(GL_BLEND);
                glBlendFunc(GL_ONE, GL_ONE);
                GL_SelectTexture(GL_TEXTURE1_ARB);
                glDisable(GL_TEXTURE_2D);
                GL_SelectTexture(GL_TEXTURE0_ARB);
                glColor3f(1, 1, 1);

                GL_Bind( paliashdr->gl_lumatex[currententity->skinnum][anim] );

                R_SetupAliasFrame (paliashdr, instant);


                glColor3f(sh_lightmapbright.value,sh_lightmapbright.value,sh_lightmapbright.value);
                GL_SelectTexture(GL_TEXTURE1_ARB);
                glEnable(GL_TEXTURE_2D);
                GL_SelectTexture(GL_TEXTURE0_ARB);
                GL_SelectTexture(GL_TEXTURE1_ARB);
                glDisable(GL_BLEND);
                glFogfv(GL_FOG_COLOR, fog_color);
            }
        }

	if ((sh_showtangent.value) && (!busy_caustics)) {
		glDisable(GL_TEXTURE_2D);
             R_DrawAliasTangent(paliashdr, instant);
		glEnable(GL_TEXTURE_2D);
	}

	c_alias_polys += paliashdr->numtris;


	if ( gl_truform.value )
	{
	    glDisable(GL_PN_TRIANGLES_ATI);
	}
}

 
 
/*
=================
R_DrawAliasModel

=================
*/

/*
void R_PrepareEntityForDraw (float bright)
{
	float		an;

  

	model_t		*clmodel;

	clmodel = currententity->model;

	VectorAdd (currententity->origin, clmodel->mins, mins);
	VectorAdd (currententity->origin, clmodel->maxs, maxs);

	if (R_CullBox (mins, maxs))
		return;


	VectorCopy (currententity->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	shadelight = bright;
	
	an = currententity->angles[1]/180*M_PI;
	shadevector[0] = cos(-an);
	shadevector[1] = sin(-an);
	shadevector[2] = 1;
	VectorNormalize (shadevector);
   
}
*/

void R_DrawAliasModel (float bright)
{
	int			i,maxnumsurf;
	aliashdr_t	*paliashdr;
        aliasframeinstant_t *aliasframeinstant;
        alias3data_t *data;
	//vec3_t	mins,maxs;

        //R_PrepareEntityForDraw (bright);

        GL_DisableMultitexture();

        glPushMatrix ();

        R_RotateForEntity (currententity);

	data = (alias3data_t *)Mod_Extradata (currententity->model);
        maxnumsurf = data->numSurfaces;        

        aliasframeinstant = currententity->aliasframeinstant;
        
	if (gl_smoothmodels.value)
		glShadeModel (GL_SMOOTH);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

        for (i=0;i<maxnumsurf;++i){
             
             paliashdr = (aliashdr_t *)((char*)data + data->ofsSurfaces[i]);
             
             if (!aliasframeinstant) {
                  glPopMatrix();
                  Con_Printf("R_DrawAliasModel: missing instant for ent %s\n", currententity->model->name);	
                  return;
             }
             
             /* disabled for now because it doesn't work with viewent 
             VectorAdd (currententity->origin,paliashdr->mins, mins);
             VectorAdd (currententity->origin,paliashdr->maxs, maxs);

             if (!R_CullBox (mins, maxs))                  */
                  R_DrawAliasSurface (paliashdr, bright, aliasframeinstant);                          
             aliasframeinstant = aliasframeinstant->_next; 
        }

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glShadeModel (GL_FLAT);

	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glPopMatrix ();
        
}
  
//==================================================================================


void R_DrawAmbientAlias (void (*r_func)(float bright))
{     
     float	brightness;
     if (currententity->model->flags & EF_FULLBRIGHT)
     {
          r_func ( 1.0);
          //XYZ
     } else if (gl_wireframe.value) {
          r_func ( 0.0);
     }else {
          brightness = (R_LightPoint (currententity->origin)/255.0) * sh_lightmapbright.value;
          r_func (brightness);
     }
}


/*
=============
PENTA:
R_DrawAmbientEntities
=============
*/
void R_DrawAmbientEntities ()
{
	int		i;
	vec3_t	mins,maxs;

	if (!r_drawentities.value)
		return;

	//We don't draw sprites they do not cast shadows
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
                     R_DrawAmbientAlias (R_DrawAliasModel);
			break;
		case mod_brush:
			glColor3f(sh_lightmapbright.value,sh_lightmapbright.value,sh_lightmapbright.value);
			R_DrawBrushModel(currententity);
			break;

		default:
			break;
		}
	}
}

/*
=============
PENTA:
R_DrawLightEntities
-> moved to gl_bumpdriver.c
=============
*/


/*
=============
R_DrawEntitiesOnList

Post multiply the textures with the frame buff.
=============
*/
void R_DrawEntitiesOnList (void)
{
	int		i;

	if (!r_drawentities.value)
		return;

        if ( cl_numvisedicts == 0 )
            return;

	glBlendFunc(GL_ZERO,GL_SRC_COLOR);
	glEnable(GL_BLEND);

        glColor3f(1.0f, 1.0f, 1.0f);

	//Con_Printf("cl_numvisedicts %d\n",cl_numvisedicts);
	// draw sprites seperately, because of alpha blending
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		switch (currententity->model->type)
		{
		case mod_alias:
			R_DrawAliasModel (1.0);
			break;

		case mod_brush:
			//glColor3f(1.0,1.0,1.0);
			//R_DrawBrushModel (currententity);
			break;

		default:
			break;
		}
	}


	glDisable(GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//sprites are drawn the usual way
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		switch (currententity->model->type)
		{
		case mod_sprite:
			R_DrawSpriteModel (currententity);
                default:				// <AWE> added default to suppress compiler warning.
			break;
		}
	}


}


/*
=============
R_MarkEntitiesOnList
=============
*/
void R_MarkEntitiesOnList (void)
{
	int		i;
	float	angle;

	if (!r_drawentities.value)
		return;

	for (i=0 ; i<cl_numlightvisedicts ; i++)
	{
		currententity = cl_lightvisedicts[i];

		switch (currententity->model->type)
		{
		case mod_brush:
			//R_MarkBrushModelSurfaces(currententity);
			R_SetupBrushInstantForLight(currententity);
			break;
		case mod_alias:
			R_SetupInstantForLight(currententity);
			break;
		default:
			break;
		}
	}

	if (cl.viewent.model)
		R_SetupInstantForLight(&cl.viewent);
	//for player Hack: Dont let it rotate when player looks up/down this looks
	//very unrealistic

	if (mirror) return;
	if (!cl_entities[cl.viewentity].model) return;
	if (cl_entities[cl.viewentity].model->type != mod_alias) return;

	angle = cl_entities[cl.viewentity].angles[0];
	cl_entities[cl.viewentity].angles[0] = 0;
	R_SetupInstantForLight(&cl_entities[cl.viewentity]);
	cl_entities[cl.viewentity].angles[0] = 	angle;
}

/*
=============
R_DrawEntitiesShadowVolumes
=============
*/
void R_DrawEntitiesShadowVolumes (int type)
{
	int		i;
	float	colorscale;
	vec3_t	dist;
	vec3_t	angles;
	if (!r_drawentities.value)
		return;


	for (i=0 ; i<cl_numlightvisedicts ; i++)
	{
		currententity = cl_lightvisedicts[i];

		if (currententity->model->type != type) continue;

		switch (currententity->model->type)
		{
		case mod_alias:

			VectorSubtract (currententity->origin, currentshadowlight->origin,dist);
			colorscale = 1 - (Length(dist) / currentshadowlight->radius);

			//it will be a verry faint shadow
			if (colorscale < 0.1) continue;
			R_DrawAliasShadowVolume(currententity);

			break;

		case mod_brush:	
			R_DrawBrushModelVolumes(currententity);
			break;

		default:
			break;
		}
	}

	if (mirror) return;
	currententity = &cl_entities[cl.viewentity];
	VectorCopy(currententity->angles,angles);
	//during intermissions the viewent model is nil
	if (currententity->model) 
             if ((currententity->model->type == type) 
                 && (type == mod_alias)
                 && (sh_playershadow.value) 
                 && (!chase_active.value)) { //Fix for two player shadows in chase cam - Eradicator
			//for lights cast by the player don't add the player's shadow
			if (currentshadowlight->owner != currententity) {
				//HACK: only horizontal angle this looks better
				currententity->angles[0] = 0;
				currententity->angles[2] = 0;
				R_DrawAliasShadowVolume(currententity);
			}
		}
	VectorCopy(angles,currententity->angles);

}

/*
=============
R_DrawLightSprites

Draw the overriden sprites that are lit by a cube map
=============
*/
void R_DrawLightSprites (void)
{
    int		i;
    vec3_t	dist;
    float	colorscale;

    if (!r_drawentities.value)
        return;

    if ( cl_numlightvisedicts == 0 )
        return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE,GL_ONE);
    glDepthMask(0);

    if ( currentshadowlight->filtercube )
    {
        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
        GL_SetupCubeMapMatrix(true);

        GL_EnableColorShader (false);
    }
    else
    {
        GL_SelectTexture(GL_TEXTURE0_ARB);
        glEnable(GL_TEXTURE_2D);
        glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
        glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
        glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE);
        glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
    }

    for ( i = 0; i < cl_numlightvisedicts; i++ )
    {
        currententity = cl_lightvisedicts[i];

        if (currententity->model->type == mod_sprite)
        {
            if (((msprite_t *)currententity->model->cache.data)->type
                >= SPR_VP_PARALLEL_UPRIGHT_OVER)
            {
                if (currententity->light_lev)
                    continue;

                //We do attent instead of opengl since gl doesn't seem to do
                //what we want, it never really gets to zero.
                VectorSubtract (currententity->origin, currentshadowlight->origin,
                                dist);
                colorscale = 1 - (Length(dist) / currentshadowlight->radius);

                //if it's to dark we save time by not drawing it
                if (colorscale < 0.1)
                    continue;

                glColor3f(currentshadowlight->color[0]*colorscale,
                          currentshadowlight->color[1]*colorscale,
                          currentshadowlight->color[2]*colorscale);

                if ( currentshadowlight->filtercube )
                    R_DrawSpriteModelWV(currententity);
                else
                    R_DrawSpriteModel(currententity);

            }
        }
    }

    glDisable(GL_BLEND);

    if ( currentshadowlight->filtercube )
    {
        GL_DisableColorShader (false);

        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
    }
}

/*
=============

=============
*/
void R_DrawFullbrightSprites (void)
{
	int		i;

	if (!r_drawentities.value)
		return;

	if ( cl_numvisedicts == 0 )
            return;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE);
	glDepthMask(0);
	GL_DisableMultitexture();
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];
	
		if (currententity->model->type == mod_sprite) {
			if (((msprite_t *)currententity->model->cache.data)->type >= SPR_VP_PARALLEL_UPRIGHT_OVER) {
				if (currententity->light_lev) {
					
					glColor3fv(&currententity->color[0]);
					R_DrawSpriteModel(currententity);
				}
			} else {
                            glColor3f(1.0f,1.0f,1.0f);
                            R_DrawSpriteModel(currententity);
			}
		}
	}

	glDisable(GL_BLEND);
	glDepthMask(1);
}

/*
=============
R_DrawViewModel

Gunnetje

=============
*/
void R_DrawViewModel (void)
{

	/*
	if (!r_drawviewmodel.value)
		return;

	if (chase_active.value)
		return;

	if (envmap)
		return;

	if (!r_drawentities.value)
		return;

	if (cl.items & IT_INVISIBILITY)
		return;

	if (cl.stats[STAT_HEALTH] <= 0)
		return;

	if (mirror) 
		return;

	currententity = &cl.viewent;
	if (!currententity->model)
		return;

	if (currententity->model->type != mod_alias) return;
	*/

	if (!R_ShouldDrawViewModel()) return;

	// hack the depth range to prevent view model from poking into walls
	//PENTA: would this work with stencil shadows?
	if ( gl_calcdepth.value ) //Calc Depth (disables shadows on v_ models, 
							  //but they don't poke into walls) - Eradicator
		glDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));

	//Erad - don't want to lerp view models (disable this to see why ;)
	if (gl_transformlerp.value)
	{
		gl_transformlerp.value = 0;
		R_DrawAliasModel (0.1);
		gl_transformlerp.value = 1;
	}
	else
		R_DrawAliasModel (0.1);

	if ( gl_calcdepth.value ) //Calc Depth - Eradicator
		glDepthRange (gldepthmin, gldepthmax);
}

/*
=============
R_DrawViewModel

  Gunnetje
  
=============
*/
void R_DrawViewModelLight (void)
{
	float		colorscale;
	vec3_t		dist;

	/*
	if (!r_drawviewmodel.value)
		return;
	
	if (chase_active.value)
		return;
	
	if (envmap)
		return;
	
	if (!r_drawentities.value)
		return;
	
	if (cl.items & IT_INVISIBILITY)
		return;
	
	if (cl.stats[STAT_HEALTH] <= 0)
		return;
	
	if (mirror)
		return;

	currententity = &cl.viewent;
	if (!currententity->model)
		return;
	*/
	if (!R_ShouldDrawViewModel()) return;

	//We do attent instead of opengl since gl doesn't seem to do
	//what we want, it never really gets to zero.
	VectorSubtract (currententity->origin,currentshadowlight->origin,dist);
	colorscale = 1 - (Length(dist) / currentshadowlight->radius);
				
	//if it's to dark we save time by not drawing it
	if (colorscale < 0.1) return;
				
	glColor4f(colorscale,colorscale,colorscale,colorscale);
				
	R_DrawAliasObjectLight(currententity,R_DrawAliasBumped);
}

/*
	Returns true if we should draw the view model
*/
qboolean R_ShouldDrawViewModel (void)
{
	if (!r_drawviewmodel.value)
		return false;
	
	if (chase_active.value)
		return false;
	
	if (envmap)
		return false;
	
	if (!r_drawentities.value)
		return false;
	
	if (cl.items & IT_INVISIBILITY)
		return false;
	
	if (cl.stats[STAT_HEALTH] <= 0)
		return false;
	
	if (mirror)
		return false;
	currententity = &cl.viewent;
	if (!currententity->model)
		return false;

	if (currententity->model->type != mod_alias)
		return false;

	return true;
}

extern	cvar_t		v_gamma;
/*
============
R_AdjustGamma
============
*/
void R_AdjustGamma(void) //Gamma - Eradicator
{
	if (v_gamma.value < 0.2f)
		v_gamma.value = 0.2f;
	if (v_gamma.value >= 1)
	{
		v_gamma.value = 1;
		return;
	}

	glBlendFunc (GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f (1, 1, 1, v_gamma.value );
	glBegin (GL_QUADS);
	glVertex3f (10, 100, 100);
	glVertex3f (10, -100, 100);
	glVertex3f (10, -100, -100);
	glVertex3f (10, 100, -100);
	
	glVertex3f (11, 100, 100);
	glVertex3f (11, -100, 100);
	glVertex3f (11, -100, -100);
	glVertex3f (11, 100, -100);

	glEnd ();
}

/*
============
R_PolyBlend
============
*/
void R_PolyBlend (void)
{
	if (!gl_polyblend.value)
		return;

	GL_DisableMultitexture();

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable (GL_BLEND);
	glDisable (GL_DEPTH_TEST);
	glDisable (GL_TEXTURE_2D);

      	glLoadIdentity ();

      	glRotatef (-90,  1, 0, 0);	    // put Z going up
      	glRotatef (90,  0, 0, 1);	    // put Z going up

	if (v_blend[3])
	{
		glColor4fv (v_blend);

		glBegin (GL_QUADS);

		glVertex3f (10, 100, 100);
		glVertex3f (10, -100, 100);
		glVertex3f (10, -100, -100);
		glVertex3f (10, 100, -100);
		glEnd ();

	}

	if (v_gamma.value != 1) //Gamma - Eradicator
		R_AdjustGamma(); 

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable (GL_BLEND);
	glEnable (GL_TEXTURE_2D);
	glEnable (GL_ALPHA_TEST);
}


int SignbitsForPlane (mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}


void R_SetFrustum (void)
{
	int		i;

	if (r_refdef.fov_x == 90) 
	{
		// front side is visible

		VectorAdd (vpn, vright, frustum[0].normal);
		VectorSubtract (vpn, vright, frustum[1].normal);

		VectorAdd (vpn, vup, frustum[2].normal);
		VectorSubtract (vpn, vup, frustum[3].normal);
	}
	else
	{
		//Spedup Small Calculations - Eradicator
		RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_refdef.fov_x * 0.5 ) );
		RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_refdef.fov_x * 0.5 );
		RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_refdef.fov_y * 0.5 );
		RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_refdef.fov_y * 0.5 ) );
	}

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}



/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{

// don't allow cheats in multiplayer
	if (cl.maxclients > 1)
	{
		Cvar_Set ("r_fullbright", "0");
		Cvar_Set ("gl_wireframe", "0"); //Disable this is multiplayer  - Eradicator
	}

	R_AnimateLight ();

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

	V_SetContentsColor (r_viewleaf->contents);
	V_CalcBlend ();

	r_cache_thrash = false;

	c_brush_polys = 0;
	c_alias_polys = 0;

}

//PENTA: wasnt in math
//<AWE> Unused. Remove it?
GLdouble cotan(GLdouble angle) {
	return (cos(angle))/sin(angle);
}

//PENTA: Matrix with infinite far clipping plane
//<AWE> - optimized with static matrix.
//	- "nudge" is now defined as static const.
// 	- p[2][3] was "-nudge" [causing heavy z-fighting on MacOS X], but has to be -1.0.
//	  This issue indicates less precision on the Windows side.
//	- replaced the custom cotan () with 1.0 / tan () [cotan () = cos () / sin ()].

static const GLdouble nudge = 1.0 - 1.0 / ((GLdouble) (1<<23));

void	pentaGlPerspective (GLdouble fov, GLdouble aspectr, GLdouble zNear)
{
    static GLdouble	p[4][4]	= {
                                        { 0.0,    0.0,    0.0,    0.0 },
                                        { 0.0,    0.0,    0.0,    0.0 },
                                        { 0.0,    0.0,    0.0,   -1.0 },
                                        { 0.0,    0.0,    0.0,    0.0 }
                                  };

    p[0][0] = 1.0 / (aspectr * tan (fov));	// <AWE> was: p[0][0] = cotan (fov) / aspectr;
    p[1][1] = 1.0 / tan (fov);			// <AWE> was: p[1][1] = cotan (fov);
    p[2][2] = -nudge;				// PM: changed again compile error otherwise
    p[3][2] = -2.0 * zNear * nudge;

    glLoadMatrixd (&p[0][0]);
}

//PENTA: Matrix with infinite far clipping plane
//<AWE> Updated with "nudge". Unused. Remove it?
void pentaGlFrustum( GLdouble xmin, GLdouble xmax, GLdouble ymin, GLdouble ymax, GLdouble zNear)
{
    static GLdouble	p[4][4]	= {
                                        { 0.0,    0.0,    0.0,    0.0 },
                                        { 0.0,    0.0,    0.0,    0.0 },
                                        { 0.0,    0.0,	  0.0,   -1.0 },
                                        { 0.0,    0.0,    0.0,    0.0 }
                                  };
	
    p[0][0] = 2 * zNear / (xmax - xmin);
    p[1][1] = 2 * zNear / (ymax - ymin);
    p[2][2] = -nudge;				 // PM: changed again compile error otherwise
    p[2][0] = (xmax + xmin) / (xmax - xmin);
    p[2][1] = (ymax + ymin) / (ymax - ymin);
    p[3][2] = -2.0 * zNear * nudge;		// <AWE> updated with MUL "nudge".

    glLoadMatrixd(&p[0][0]);	
}

void MYgluPerspective( GLdouble fovy, GLdouble aspect,
		     GLdouble zNear, GLdouble zFar )
{
/*
<AWE> Unused. Can be removed.
   GLdouble xmin, xmax, ymin, ymax;

   ymax = zNear * tan (fovy * M_PI / 360.0);
   ymin = -ymax;

   xmin = ymin * aspect;
   xmax = ymax * aspect;

   pentaGlFrustum( xmin, xmax, ymin, ymax, zNear);
*/
   pentaGlPerspective(fovy * M_PI / 360.0, aspect, zNear);
}

/*
PENTA:
from http://www.markmorley.com/opengl/frustumculling.html
Should clean it up by using procedures.
*/
float frustumPlanes[6][4];

void	ExtractFrustum()	// <AWE> added return type.
{
   float   proj[16];
   float   modl[16];
   float   clip[16];
   float   t;

   /* Get the current PROJECTION matrix from OpenGL */
   glGetFloatv( GL_PROJECTION_MATRIX, proj );

   /* Get the current MODELVIEW matrix from OpenGL */
   glGetFloatv( GL_MODELVIEW_MATRIX, modl );

   /* Combine the two matrices (multiply projection by modelview) */
   clip[ 0] = modl[ 0] * proj[ 0] + modl[ 1] * proj[ 4] + modl[ 2] * proj[ 8] + modl[ 3] * proj[12];
   clip[ 1] = modl[ 0] * proj[ 1] + modl[ 1] * proj[ 5] + modl[ 2] * proj[ 9] + modl[ 3] * proj[13];
   clip[ 2] = modl[ 0] * proj[ 2] + modl[ 1] * proj[ 6] + modl[ 2] * proj[10] + modl[ 3] * proj[14];
   clip[ 3] = modl[ 0] * proj[ 3] + modl[ 1] * proj[ 7] + modl[ 2] * proj[11] + modl[ 3] * proj[15];

   clip[ 4] = modl[ 4] * proj[ 0] + modl[ 5] * proj[ 4] + modl[ 6] * proj[ 8] + modl[ 7] * proj[12];
   clip[ 5] = modl[ 4] * proj[ 1] + modl[ 5] * proj[ 5] + modl[ 6] * proj[ 9] + modl[ 7] * proj[13];
   clip[ 6] = modl[ 4] * proj[ 2] + modl[ 5] * proj[ 6] + modl[ 6] * proj[10] + modl[ 7] * proj[14];
   clip[ 7] = modl[ 4] * proj[ 3] + modl[ 5] * proj[ 7] + modl[ 6] * proj[11] + modl[ 7] * proj[15];

   clip[ 8] = modl[ 8] * proj[ 0] + modl[ 9] * proj[ 4] + modl[10] * proj[ 8] + modl[11] * proj[12];
   clip[ 9] = modl[ 8] * proj[ 1] + modl[ 9] * proj[ 5] + modl[10] * proj[ 9] + modl[11] * proj[13];
   clip[10] = modl[ 8] * proj[ 2] + modl[ 9] * proj[ 6] + modl[10] * proj[10] + modl[11] * proj[14];
   clip[11] = modl[ 8] * proj[ 3] + modl[ 9] * proj[ 7] + modl[10] * proj[11] + modl[11] * proj[15];

   clip[12] = modl[12] * proj[ 0] + modl[13] * proj[ 4] + modl[14] * proj[ 8] + modl[15] * proj[12];
   clip[13] = modl[12] * proj[ 1] + modl[13] * proj[ 5] + modl[14] * proj[ 9] + modl[15] * proj[13];
   clip[14] = modl[12] * proj[ 2] + modl[13] * proj[ 6] + modl[14] * proj[10] + modl[15] * proj[14];
   clip[15] = modl[12] * proj[ 3] + modl[13] * proj[ 7] + modl[14] * proj[11] + modl[15] * proj[15];

   /* Extract the numbers for the RIGHT plane */
   frustumPlanes[0][0] = clip[ 3] - clip[ 0];
   frustumPlanes[0][1] = clip[ 7] - clip[ 4];
   frustumPlanes[0][2] = clip[11] - clip[ 8];
   frustumPlanes[0][3] = clip[15] - clip[12];

   /* Normalize the result */
   t = sqrt( frustumPlanes[0][0] * frustumPlanes[0][0] + frustumPlanes[0][1] * frustumPlanes[0][1] + frustumPlanes[0][2] * frustumPlanes[0][2] );
   frustumPlanes[0][0] /= t;
   frustumPlanes[0][1] /= t;
   frustumPlanes[0][2] /= t;
   frustumPlanes[0][3] /= t;

   /* Extract the numbers for the LEFT plane */
   frustumPlanes[1][0] = clip[ 3] + clip[ 0];
   frustumPlanes[1][1] = clip[ 7] + clip[ 4];
   frustumPlanes[1][2] = clip[11] + clip[ 8];
   frustumPlanes[1][3] = clip[15] + clip[12];

   /* Normalize the result */
   t = sqrt( frustumPlanes[1][0] * frustumPlanes[1][0] + frustumPlanes[1][1] * frustumPlanes[1][1] + frustumPlanes[1][2] * frustumPlanes[1][2] );
   frustumPlanes[1][0] /= t;
   frustumPlanes[1][1] /= t;
   frustumPlanes[1][2] /= t;
   frustumPlanes[1][3] /= t;

   /* Extract the BOTTOM plane */
   frustumPlanes[2][0] = clip[ 3] + clip[ 1];
   frustumPlanes[2][1] = clip[ 7] + clip[ 5];
   frustumPlanes[2][2] = clip[11] + clip[ 9];
   frustumPlanes[2][3] = clip[15] + clip[13];

   /* Normalize the result */
   t = sqrt( frustumPlanes[2][0] * frustumPlanes[2][0] + frustumPlanes[2][1] * frustumPlanes[2][1] + frustumPlanes[2][2] * frustumPlanes[2][2] );
   frustumPlanes[2][0] /= t;
   frustumPlanes[2][1] /= t;
   frustumPlanes[2][2] /= t;
   frustumPlanes[2][3] /= t;

   /* Extract the TOP plane */
   frustumPlanes[3][0] = clip[ 3] - clip[ 1];
   frustumPlanes[3][1] = clip[ 7] - clip[ 5];
   frustumPlanes[3][2] = clip[11] - clip[ 9];
   frustumPlanes[3][3] = clip[15] - clip[13];

   /* Normalize the result */
   t = sqrt( frustumPlanes[3][0] * frustumPlanes[3][0] + frustumPlanes[3][1] * frustumPlanes[3][1] + frustumPlanes[3][2] * frustumPlanes[3][2] );
   frustumPlanes[3][0] /= t;
   frustumPlanes[3][1] /= t;
   frustumPlanes[3][2] /= t;
   frustumPlanes[3][3] /= t;

   /* Extract the FAR plane */
   frustumPlanes[4][0] = clip[ 3] - clip[ 2];
   frustumPlanes[4][1] = clip[ 7] - clip[ 6];
   frustumPlanes[4][2] = clip[11] - clip[10];
   frustumPlanes[4][3] = clip[15] - clip[14];

   /* Normalize the result */
   t = sqrt( frustumPlanes[4][0] * frustumPlanes[4][0] + frustumPlanes[4][1] * frustumPlanes[4][1] + frustumPlanes[4][2] * frustumPlanes[4][2] );
   frustumPlanes[4][0] /= t;
   frustumPlanes[4][1] /= t;
   frustumPlanes[4][2] /= t;
   frustumPlanes[4][3] /= t;

   /* Extract the NEAR plane */
   frustumPlanes[5][0] = clip[ 3] + clip[ 2];
   frustumPlanes[5][1] = clip[ 7] + clip[ 6];
   frustumPlanes[5][2] = clip[11] + clip[10];
   frustumPlanes[5][3] = clip[15] + clip[14];

   /* Normalize the result */
   t = sqrt( frustumPlanes[5][0] * frustumPlanes[5][0] + frustumPlanes[5][1] * frustumPlanes[5][1] + frustumPlanes[5][2] * frustumPlanes[5][2] );
   frustumPlanes[5][0] /= t;
   frustumPlanes[5][1] /= t;
   frustumPlanes[5][2] /= t;
   frustumPlanes[5][3] /= t;
}

/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	float	screenaspect;
	extern	int glwidth, glheight;
	int		x, x2, y2, y, w, h;

	//
	// set up viewpoint
	//
	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	x = r_refdef.vrect.x * glwidth/vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth/vid.width;
	y = (vid.height-r_refdef.vrect.y) * glheight/vid.height;
	y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight/vid.height;

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < glwidth)
		x2++;
	if (y2 < 0)
		y2--;
	if (y < glheight)
		y++;

	w = x2 - x;
	h = y - y2;

	if (envmap || mirror || glare)
	{
		x = y2 = 0;
		w = r_refdef.vrect.width;
		h = r_refdef.vrect.height;
	}

	glViewport (glx + x, gly + y2, w, h);

    screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;
	if (mirror) screenaspect = 1.0;
//	yfov = 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*180/M_PI;
	//PENTA: decreased zfar from 4096 to reduce z-fighting on alias models
	// is this ok for quake or do we need to be able to look that far??
	// seems to work, I incr. znear to 5 instead of 4
        MYgluPerspective (2.0 *atan((GLdouble) r_refdef.vrect.height / (GLdouble) r_refdef.vrect.width) * 180.0 / M_PI /* r_refdef.fov_y */,  screenaspect,  5.0,  2048.0); 

	if (mirror || glare)
	{
		//If we are mirroring we want to use the screen projection matrix
		//not the texture (=> we just calculated this) one.
		glLoadIdentity();
		glMultMatrixf(r_projection_matrix);

		if (mirror){
			if (mirror_plane->normal[2])
				glScalef (1, -1, 1);
			else
				glScalef (-1, 1, 1);
		}
	} else {
		//Store it for later mirror use
		glGetFloatv (GL_PROJECTION_MATRIX, r_projection_matrix);
	}


	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

    glRotatef (-90,  1, 0, 0);	    // put Z going up
    glRotatef (90,  0, 0, 1);	    // put Z going up
    glRotatef (-r_refdef.viewangles[2],  1, 0, 0);
    glRotatef (-r_refdef.viewangles[0],  0, 1, 0);
    glRotatef (-r_refdef.viewangles[1],  0, 0, 1);
    glTranslatef (-r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);

	glGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);
	glGetDoublev (GL_MODELVIEW_MATRIX, r_Dworld_matrix);
	glGetDoublev (GL_PROJECTION_MATRIX, r_Dproject_matrix);
	glGetIntegerv (GL_VIEWPORT, (GLint *) r_Iviewport);	// <AWE> added cast.
	ExtractFrustum();
	//
	// set drawing parms
	//
	if (gl_cull.value)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
}

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
void R_RenderScene (void)
{
	int i, j;
	shadowlight_t *l = NULL;
	
	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_MarkLeaves ();	// done here so we know if we're in water, gaat visible leafs marken

	//if (!mirror)
	R_InitShadowsForFrame ();

	R_InitDrawWorld();

	//if (!mirror) {
		//To ensure invariance we have to enable scissoring even for the ambient term
		//But in reality this doen't work (at least on my Geforce2 card) and you still
		//get (few) z-buffer fighting.
	if (!sh_noscissor.value) {
		glScissor(r_Iviewport[0], r_Iviewport[1], r_Iviewport[2], r_Iviewport[3]);
		glEnable(GL_SCISSOR_TEST);
	}

		//Ambient light drawing
		//this uses the light maps as an ambient term
		//it fills the z buffer
	R_SetupInstants();


	//XYZ - moved upwards
	if (gl_wireframe.value) {
		float old = sh_lightmapbright.value;
		sh_lightmapbright.value = 1.0;
		glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);

		R_DrawAmbientEntities();
		//glDisable(GL_TEXTURE_2D);
		R_WorldMultiplyTextures();
		glEnable(GL_TEXTURE_2D);
		sh_lightmapbright.value = old;
		glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
		return;
	}

	R_DrawViewModel();
	R_DrawAmbientEntities();
	glDepthFunc(GL_LEQUAL);

	R_WorldMultiplyTextures();

	//if (mirror) {
	//	sh_lightmapbright.value = 0.3;
	//	return;
	//}

	if (!sh_noscissor.value) {
		glClear(GL_STENCIL_BUFFER_BIT);
		R_ClearRectList();
	}

	numClearsSaved = 0;
	aliasCacheRequests = aliasFullCacheHits = aliasPartialCacheHits = 0;
	brushCacheRequests = brushFullCacheHits = brushPartialCacheHits = 0;

	glFogfv(GL_FOG_COLOR, color_black);

	for (i=0; i<numUsedShadowLights; i++) {

		//find a lights that still fits on our current screen plane
		//using the first fit method.
		//Other methods may give better results (exhaustive for example)
		//but i'm convinced you can't save more clears than those that you
		//save with this.
		if ((!sh_nocleversave.value) && (!sh_noscissor.value)) {
			qboolean foundone = false;
			for (j=0; j<numUsedShadowLights; j++) {

				if (!usedshadowlights[j]->visible) continue;

				l = usedshadowlights[j];
				currentshadowlight = l;
				if (R_CheckRectList(&l->scizz)) {
					foundone = true;
					break;
				}
			}
			
			if (!foundone) {
				R_SetTotalRect(); //Only clear dirty part
				glClear(GL_STENCIL_BUFFER_BIT);
				R_ClearRectList();
				for (j=0; j<numUsedShadowLights; j++) {
					l = usedshadowlights[j];
					currentshadowlight = l;
					if (usedshadowlights[j]->visible) break;
				}
			}

		} else {
			l = usedshadowlights[i];
			currentshadowlight = l;
		}

		//Find the polygons that cast shadows for this light
		if (!R_FillShadowChain(l))
			continue;

		if (sh_visiblevolumes.value) {
			glDepthFunc(GL_LEQUAL);
			glDepthMask(GL_TRUE);
		} else {
			glDepthMask(GL_FALSE);

			if (l->castShadow) {
  				glDepthFunc(GL_LESS);
  				glEnable(GL_STENCIL_TEST);
				//glClear(GL_STENCIL_BUFFER_BIT);
				glColorMask(false, false, false, false);
				glStencilFunc(GL_ALWAYS, 1, 0xffffffff);
				if (!sh_noscissor.value) {
					if (R_CheckRectList(&l->scizz)) {
							//we can have another go without clearing
						R_AddRectList(&l->scizz);
						numClearsSaved++;

					} else {
						R_SetTotalRect(); //Only clear dirty part
						glClear(GL_STENCIL_BUFFER_BIT);
						R_ClearRectList();
						R_AddRectList(&l->scizz);
					}
				} else {
					glClear(GL_STENCIL_BUFFER_BIT);
				}
			}
		}

		//All right now the shadow stuff

		glScissor(l->scizz.coords[0], l->scizz.coords[1],
			l->scizz.coords[2]-l->scizz.coords[0],  l->scizz.coords[3]-l->scizz.coords[1]);

		R_MarkEntitiesOnList();

		//Shadow casting is on by default
		if (l->castShadow) {
			//Calculate the shadow volume (does nothing when static)
			R_ConstructShadowVolume(l);

#if 1
			//Pass 1 increase
			glCullFace(GL_BACK);
			glStencilOp(GL_KEEP, GL_INCR, GL_KEEP);
			if (sh_worldshadows.value) R_DrawShadowVolume(l);

			// Second Pass. Decrease Stencil Value In The Shadow
			glCullFace(GL_FRONT);
			glStencilOp(GL_KEEP, GL_DECR, GL_KEEP);
  			if (sh_worldshadows.value) R_DrawShadowVolume(l);

			//PENTA: we could do the same thing for brushes as for aliasses
			//Pass 1 increase
			glCullFace(GL_BACK);
			glStencilOp(GL_KEEP, GL_INCR, GL_KEEP);
			if (sh_entityshadows.value) R_DrawEntitiesShadowVolumes(mod_brush);

			// Second Pass. Decrease Stencil Value In The Shadow
			glCullFace(GL_FRONT);
			glStencilOp(GL_KEEP, GL_DECR, GL_KEEP);
			if (sh_entityshadows.value) R_DrawEntitiesShadowVolumes(mod_brush);

#else
//                        glCullFace(GL_FRONT_AND_BACK);
                        glDisable(GL_CULL_FACE);
                        checkerror();
    	                glStencilOp(GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);
                        checkerror();
	                qglStencilFuncSeparateATI(GL_ALWAYS, GL_ALWAYS, 0, ~0);
                        checkerror();
	                qglStencilOpSeparateATI(GL_FRONT, GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);
                        checkerror();
	                qglStencilOpSeparateATI(GL_BACK, GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);
                        checkerror();

			if (sh_worldshadows.value) R_DrawShadowVolume(l);

                        //PENTA: we could do the same thing for brushes as for aliasses
			//Pass 1 increase
			if (sh_entityshadows.value) R_DrawEntitiesShadowVolumes(mod_brush);

                        glEnable(GL_CULL_FACE);
#endif
			if (sh_entityshadows.value)
			    R_DrawEntitiesShadowVolumes(mod_alias);

			//Reenable drawing
			glCullFace(GL_FRONT);
 			glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
			glDepthFunc(GL_LEQUAL);
			glStencilFunc(GL_EQUAL, 0, 0xffffffff);
		}
		if (!sh_visiblevolumes.value) {
			R_DrawWorldBumped();
			glPolygonOffset(0,-5);
			glEnable(GL_POLYGON_OFFSET_FILL);
			R_DrawLightEntities(l);
			glDisable(GL_POLYGON_OFFSET_FILL);
		}
		glDisable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_STENCIL_TEST);

		//sprites only recive "shadows" from the cubemap not the stencil

		if (!sh_visiblevolumes.value) {
			R_DrawLightSprites ();
		}

		glDepthMask(GL_TRUE);		

		if ((i%8) == 0) 
			// don't let sound get messed up if going slow
			S_ExtraUpdate ();	

		l->visible = false;
	}

	glScissor(0, 0, r_Iviewport[2], r_Iviewport[3]);
	glDisable(GL_SCISSOR_TEST);

	glDepthFunc(GL_LEQUAL);

	GL_DisableMultitexture();

	glFogfv(GL_FOG_COLOR, color_black);

	R_DrawFullbrightSprites();
	
	if (skytexturenum >= 0) {
		glColor3f(1,1,1);
		R_DrawSkyChain (cl.worldmodel->textures[skytexturenum]->texturechain);
		cl.worldmodel->textures[skytexturenum]->texturechain = NULL;
	}


	R_DrawCaustics();

	//Removed to fix particle & water bug (see R_RenderView) - Eradicator
	//R_DrawParticles (); //to fix the particles triangles showing up after water
						//put this behind the water drawing#ifdef GLTEST
	R_DrawDecals();

	glFogfv(GL_FOG_COLOR, fog_color);
}

void R_InitMirrorChains()
{
	int i;

	for (i=0; i<NUM_MIRROR_PLANES; i++) {
		mirrorplanes[i].texture_object = EasyTgaLoad("penta/mirrordummy.tga");
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	}

}


void R_NewMirrorChains()
{
	int i;

	for (i=0; i<NUM_MIRROR_PLANES; i++) {
		mirrorplanes[i].lockframe = 0;
		mirrorplanes[i].updateframe = 0;
		mirrorplanes[i].chain = NULL;
	}

}


void R_ClearMirrorChains()
{
	int i;

	for (i=0; i<NUM_MIRROR_PLANES; i++) {
		mirrorplanes[i].chain = NULL;
	}

	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		if (!cl.worldmodel->textures[i])
			continue;	
		cl.worldmodel->textures[i]->texturechain = NULL;
	}
}

void R_AllocateMirror(msurface_t *surf)
{
	int i,oldest,oindex;
	mirrorplane_t *mir = NULL;
	vec3_t	tempnormal;

	//#define NUM_MIRROR_PLANES 8
	//extern mirrorplane_t mirrorplanes[NUM_MIRROR_PLANES];

	//see if surface is part of an existing mirror
	for (i=0; i<NUM_MIRROR_PLANES; i++) {

		VectorCopy(surf->plane->normal,tempnormal);
		//if (surf->flags & SURF_PLANEBACK) {
		//	VectorInverse(tempnormal);
		//}

		if ((tempnormal[0] == mirrorplanes[i].plane.normal[0]) &&
			(tempnormal[1] == mirrorplanes[i].plane.normal[1]) &&
			(tempnormal[2] == mirrorplanes[i].plane.normal[2]) &&
			(surf->plane->dist == mirrorplanes[i].plane.dist))
		{
			mir = &mirrorplanes[i];
			//Con_Printf("reuse mir %i\n",i);
			break;
		}
	}

	//allocate new mirror
	if (!mir) {
		oldest = r_framecount;
		oindex = -1;
		for (i=0; i<NUM_MIRROR_PLANES; i++) {
			if (mirrorplanes[i].lockframe < oldest) {
				oldest = mirrorplanes[i].lockframe; 
				oindex = i;
			}
		}

		if (oindex == -1) {
			//Be silent about this
			return;
		}

		mir = &mirrorplanes[oindex];
		VectorCopy(surf->plane->normal,mir->plane.normal);
		//if (surf->flags & SURF_PLANEBACK) {
		//	VectorInverse(mir->plane.normal);
		//}
		mir->plane.dist = surf->plane->dist;
		mir->plane.type = surf->plane->type;
		mir->plane.signbits = surf->plane->signbits;
		//Con_Printf("New mirror: n: %f %f %f  d:%f\n i: %i",mir->plane.normal[0],mir->plane.normal[1],mir->plane.normal[2],mir->plane.dist,oindex);
	}

	mir->lockframe = r_framecount;
	surf->texturechain = mir->chain;
	mir->chain = surf;
}
/*
=============
R_Mirror
=============
*/
void R_Mirror (mirrorplane_t *mir)
{
	float		d, olddrawents = 0.0f;
	entity_t	*ent;
	int			ox, oy, ow, oh, ofovx, ofovy, cl_oldvisedicts;
	vec3_t		oang, oorg;
	qboolean	drawplayer;

	if (mirror) {
		Con_Printf("Warning: Resursive mirrors\n");
		return;
	}

	mirror = true;
	mirror_plane = &mir->plane;
	memcpy (r_base_world_matrix, r_world_matrix, sizeof(r_base_world_matrix));

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

	//setup mirrored view origin & direction
	VectorCopy(r_refdef.vieworg, oorg);
	d = DotProduct (r_refdef.vieworg, mir->plane.normal) - mir->plane.dist;

	//camera is to far away from mirror don't update it...
	if (abs(d) > mir_distance.value) {
		mirror = false;
		return;
	}

	//player is very close to mirror don't draw it since he interects it and this gives bad artefacts
	if (abs(d) < MIN_PLAYER_MIRROR) {
		drawplayer = false;
	} else {
		drawplayer = true;
	}

	//calculate a far plane for the mirror, stuff to far away from mirrors is not drawn anymore...
	VectorCopy(mir->plane.normal,mirror_far_plane.normal);
	//VectorInverse(mirror_far_plane.normal);
	mirror_far_plane.dist = DotProduct(r_refdef.vieworg,mir->plane.normal) + mir_distance.value;

	VectorMA (r_refdef.vieworg, -2*d, mir->plane.normal, r_refdef.vieworg);

	if (d < 0) {
		mirror_clipside = 1;
	} else {
		mirror_clipside = 2;
	}

	d = DotProduct (vpn, mir->plane.normal);
	VectorMA (vpn, -2*d, mir->plane.normal, vpn);

	VectorCopy(r_refdef.viewangles, oang);
	r_refdef.viewangles[0] = -asin (vpn[2])/M_PI*180;
	r_refdef.viewangles[1] = atan2 (vpn[1], vpn[0])/M_PI*180;
	r_refdef.viewangles[2] = -r_refdef.viewangles[2];

	//Render to a small view rectangle
	ox = r_refdef.vrect.x;
	oy = r_refdef.vrect.y;
	ow = r_refdef.vrect.width;
	oh = r_refdef.vrect.height;
	ofovx = r_refdef.fov_x;
	ofovy = r_refdef.fov_y;
	r_refdef.vrect.x = 0;
	r_refdef.vrect.y = 0;
	r_refdef.vrect.width = 256;
	r_refdef.vrect.height = 256;


//	r_refdef.fov_x = 90.0;
//	r_refdef.fov_y = 90.0;//CalcFov (90.0, r_refdef.vrect.width, r_refdef.vrect.height);
	r_refdef.fov_x = scr_fov.value;
	r_refdef.fov_y = CalcFov (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);


	//Note:  This was probably already a problem with the original quake mirrors
	//the static entities are added in all mirror passes making that by the end of
	//mirror rendering we are rendering the statics multiple times.
	//so we back up the number of entities (dynamics are in front) and later restore it
	cl_oldvisedicts = cl_numvisedicts;

	
	olddrawents = r_drawentities.value;
	if (mir_detail.value < 2) {
		//Don't draw entities
		r_drawentities.value = 0;
	} else {
		//Hack add player to back of list	
		if (drawplayer && cl_entities[cl.viewentity].model  && cl_entities[cl.viewentity].model->type == mod_alias) {
			ent = &cl_entities[cl.viewentity];
			if (cl_numvisedicts < MAX_VISEDICTS)
			{
				cl_visedicts[cl_numvisedicts] = ent;
				cl_numvisedicts++;
			}
		}		
	}	
	//r_drawentities.value = 0;

	glFrontFace(GL_CW);
	//R_RenderView ();
	//Con_Printf("Render mirror\n");

	//olddrawents = r_drawentities.value;
	//r_drawentities.value = mir_detail.value-1;
	

	R_RenderScene ();

	r_drawentities.value = olddrawents;

	Q_memcpy (mir->matrix, r_world_matrix, sizeof(r_world_matrix));

	GL_Bind(mir->texture_object);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 256, 256);
	glClear (GL_DEPTH_BUFFER_BIT);


	if (mir_detail.value < 2) {
		r_drawentities.value = olddrawents;
	}

//	r_drawentities.value = 1;

	r_refdef.vrect.x = ox;
	r_refdef.vrect.y = oy;
	r_refdef.vrect.width = ow;
	r_refdef.vrect.height = oh;
	r_refdef.fov_x = ofovx;
	r_refdef.fov_y = ofovy;
	
	cl_numvisedicts = cl_oldvisedicts;

	VectorCopy(oorg, r_refdef.vieworg);
	VectorCopy(oang, r_refdef.viewangles);

	// blend on top
	/*
	glMatrixMode(GL_PROJECTION);
	if (mir->plane.normal[2])
		glScalef (1,-1,1);
	else
		glScalef (-1,1,1);
	*/
	glFrontFace(GL_CCW);
	glMatrixMode(GL_MODELVIEW);

	//glLoadMatrixf (r_base_world_matrix);
	memcpy(r_world_matrix, r_base_world_matrix, sizeof(r_base_world_matrix));

	mirror = false;
	Sbar_Changed ();
}

void R_RenderMirrors()
{
	int i;
	for (i=0; i<NUM_MIRROR_PLANES; i++) {
		if (mirrorplanes[i].chain && ((r_framecount - mirrorplanes[i].updateframe) > mir_frameskip.value)) {
			R_Mirror(&mirrorplanes[i]);
			mirrorplanes[i].lockframe = r_framecount;
			mirrorplanes[i].updateframe = r_framecount;
		}
	}
}

void R_SetupMirrorShader(msurface_t *surf,mirrorplane_t *mir) {

	if (mir_detail.value < 1) {
		GL_Bind(newenvmap);
	} else
		GL_Bind(mir->texture_object);

	if ((surf->flags & SURF_DRAWTURB) && !(surf->flags & SURF_GLASS)) {
		//it is water
		//glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE);
		glEnable(GL_BLEND);
		return;
	} else {

		if (surf->flags & SURF_DRAWTURB) {
			glBlendFunc(GL_SRC_ALPHA,GL_ONE);
			glEnable(GL_BLEND);
			return;
		}

		//it is glass or a mirror
		//GL_EnableMultiTexture();
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
		glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
		glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE);

		GL_EnableMultitexture();
		GL_Bind(surf->texinfo->texture->gl_texturenum);

		//No colormaps: Color maps are bound on tmu 0 so disable it
		//and let tu1 modulate itself with the light map brightness
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
		glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
		glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE);
		glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, GL_TEXTURE);
		glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_INTERPOLATE_ARB);
		glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, GL_SRC_ALPHA);

		GL_Bind(surf->texinfo->texture->gl_texturenum);
	}
}

void R_DisableMirrorShader(msurface_t *surf,mirrorplane_t *mir) {


	if ((surf->flags & SURF_DRAWTURB) && !(surf->flags & SURF_GLASS)) {
		//it is water
		glDisable(GL_BLEND);
		return;
	} else {

		if (surf->flags & SURF_DRAWTURB) {
			glDisable(GL_BLEND);
			return;
		}

		glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, GL_SRC_COLOR);
		GL_DisableMultitexture();
	}
}

void R_DrawMirrorSurfaces()
{
	int i;
	msurface_t *s;

	glBlendFunc(GL_SRC_ALPHA,GL_ONE);
	for (i=0; i<NUM_MIRROR_PLANES; i++) {
		if (mirrorplanes[i].chain) {

			glMatrixMode(GL_TEXTURE);

			glPushMatrix();
			if (mir_detail.value > 0) {
				glLoadIdentity();
				glTranslatef(0.5, 0.5, 0);
				glScalef(0.5, 0.5, 0);
				glMultMatrixf (r_projection_matrix);
				glMultMatrixf (r_world_matrix);
			} else {
				//glMultMatrixf (r_projection_matrix);
				glTranslatef(r_refdef.vieworg[0]/1000,r_refdef.vieworg[1]/1000,r_refdef.vieworg[2]/1000);
				glTexGeni(GL_S,GL_TEXTURE_GEN_MODE,GL_SPHERE_MAP);
				glTexGeni(GL_T,GL_TEXTURE_GEN_MODE,GL_SPHERE_MAP);
				glEnable(GL_TEXTURE_GEN_S);
				glEnable(GL_TEXTURE_GEN_T);
			}

			R_SetupMirrorShader(mirrorplanes[i].chain,&mirrorplanes[i]);

			s = mirrorplanes[i].chain;

			glNormal3fv(mirrorplanes[i].plane.normal);
			for ( ; s ; s=s->texturechain) {
			if ((s->flags & SURF_GLASS) && (s->flags & SURF_DRAWTURB)) {
					glColor4f(1,1,1,0.5);
					EmitMirrorPolys (s);
				} else if (s->flags & SURF_DRAWTURB) {
					glColor4f(1,1,1,r_wateralpha.value);
					EmitMirrorWaterPolys (s);
				}  else {
					glColor4f(1,1,1,1);
					EmitMirrorPolys (s);
				}
			}

			R_DisableMirrorShader(mirrorplanes[i].chain,&mirrorplanes[i]);

			glPopMatrix();
        		if (mir_detail.value == 0) {
				glDisable(GL_TEXTURE_GEN_S);
				glDisable(GL_TEXTURE_GEN_T);
			}
			glMatrixMode(GL_MODELVIEW);
		}
	}
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
}

/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
	/*if (r_mirroralpha.value != 1.0)
	{
		if (gl_clear.value)
			glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			glClear (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 0.5;
		glDepthFunc (GL_LEQUAL);
	}
	else if (gl_ztrick.value) PENTA: Removed
	{
		static int trickframe;

		if (gl_clear.value)
			glClear (GL_COLOR_BUFFER_BIT);

		trickframe++;
		if (trickframe & 1)
		{
			gldepthmin = 0;
			gldepthmax = 0.49999;
			glDepthFunc (GL_LEQUAL);
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5;
			glDepthFunc (GL_GEQUAL);
		}
	}
	else*/
	
	{
		if (gl_clear.value || gl_wireframe.value)
			glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			glClear (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 1;
		glDepthFunc (GL_LEQUAL);
	}

	glDepthRange (gldepthmin, gldepthmax);
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView (void)
{
	float oldfogen;
	int	viewcont;

	if (r_norefresh.value)
		return;

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	mirror = false;


	if (gl_finish.value)
		glFinish ();

	R_Clear ();

	viewcont = CL_PointContents(r_origin);
	fog_color[3] = 1.0;
	if ((viewcont == CONTENTS_WATER) && (fog_waterfog.value)){
		glFogi(GL_FOG_MODE, GL_LINEAR);
		fog_color[0] = 64/255.0;
		fog_color[1] = 48/255.0;
		fog_color[2] = 0.0;
		glFogfv(GL_FOG_COLOR, fog_color);
		glFogf(GL_FOG_END, 512);
		glFogf(GL_FOG_START, 0);
		glEnable(GL_FOG);
		oldfogen = fog_enabled.value;
		fog_enabled.value = 1.0;
	} else 	if ((viewcont == CONTENTS_SLIME) && (fog_waterfog.value)){
		glFogi(GL_FOG_MODE, GL_LINEAR);
		fog_color[0] = 0.0;
		fog_color[1] = 128/255.0;
		fog_color[2] = 32/255.0;
		glFogfv(GL_FOG_COLOR, fog_color);
		glFogf(GL_FOG_END, 256);
		glFogf(GL_FOG_START, 0);
		glEnable(GL_FOG);
		oldfogen = fog_enabled.value;
		fog_enabled.value = 1.0;
	} else 	if ((viewcont == CONTENTS_LAVA) && (fog_waterfog.value)){
		glFogi(GL_FOG_MODE, GL_LINEAR);
		fog_color[0] = 255/255.0;
		fog_color[1] = 64/255.0;
		fog_color[2] = 0.0;
		glFogfv(GL_FOG_COLOR, fog_color);
		glFogf(GL_FOG_END, 256);
		glFogf(GL_FOG_START, 0);
		glEnable(GL_FOG);
		oldfogen = fog_enabled.value;
		fog_enabled.value = 1.0;

	} else {
		glFogi(GL_FOG_MODE, GL_LINEAR);
		fog_color[0] = fog_r.value/255.0;
		fog_color[1] = fog_g.value/255.0;
		fog_color[2] = fog_b.value/255.0;
		glFogfv(GL_FOG_COLOR, fog_color);
		glFogf(GL_FOG_END, fog_end.value);
		glFogf(GL_FOG_START, fog_start.value);
		if (fog_enabled.value && !gl_wireframe.value) 
			glEnable(GL_FOG);
	}

	if (mir_detail.value > 0) {
		R_RenderMirrors();
		//	glClear (GL_DEPTH_BUFFER_BIT);
	}

	R_ClearMirrorChains();

	R_Glare ();

	R_ClearMirrorChains();

	// render normal view

	R_RenderScene ();
	//R_DrawViewModel ();

	/*Rendering fog in black for particles is done to stop triangle effect on the 
	particles. It is done right before and fixed after each particle draw function 
	to avoid effection fog on the water. A particle draw is done after the water 
	draw to make sure particles are rendered over the surface of the water. - Eradicator*/

	R_DrawWaterSurfaces ();
	R_DrawMirrorSurfaces ();

//  More fog right here :)

	if ((viewcont == CONTENTS_WATER) && (fog_waterfog.value)){
		fog_enabled.value = oldfogen;
	}
	glDisable(GL_FOG);
//  End of all fog code...

	//glFogfv(GL_FOG_COLOR, color_black); //Stops triangle effect on particles
	R_DrawParticles (); //Fixes particle & water bug
	//glFogfv(GL_FOG_COLOR, fog_color); //Real fog colour

	//Draw a poly over the screen (underwater, slime, blood hit)
	R_DrawGlare() ;
	R_PolyBlend ();
}
