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

*/

// gl_shadow.c stencil shadow support for quake

#include "quakedef.h"

int numShadowLights;
int numStaticShadowLights;
int numUsedShadowLights; //number of shadow lights acutally drawn this frame

shadowlight_t shadowlights[MAXSHADOWLIGHTS];
shadowlight_t *usedshadowlights[MAXUSEDSHADOWLIGHS];
shadowlight_t *currentshadowlight;


int volumeCmdsBuff[MAX_VOLUME_COMMANDS+128]; //Hack protect against slight overflows
float volumeVertsBuff[MAX_VOLUME_VERTS+128];
lightcmd_t	lightCmdsBuff[MAX_LIGHT_COMMANDS+128];
int numVolumeCmds;
int numLightCmds;
int numVolumeVerts;

msurface_t *shadowchain; //linked list of polygons that are shadowed
byte *lightvis;
byte worldvis[MAX_MAP_LEAFS/8];

/* -DC- isn't that volumeVertsBuff ?
vec3_t volumevertices[MAX_VOLUME_VERTICES];//buffer for the vertices of the shadow volume
int usedvolumevertices;
*/

void DrawVolumeFromCmds(int *volumeCmds, lightcmd_t *lightCmds, float *volumeVerts);
void DrawAttentFromCmds(lightcmd_t *lightCmds);
void DrawBumpFromCmds(lightcmd_t *lightCmds);
void DrawSpecularBumpFromCmds(lightcmd_t *lightCmds);
void PrecalcVolumesForLight(model_t *model);
int getVertexIndexFromSurf(msurface_t *surf, int index, model_t *model);
qboolean R_ContributeFrame(shadowlight_t *light);

/*
=============
AllocShadowLight
=============
*/
shadowlight_t* AllocShadowLight(void) {

	if (numShadowLights >= MAXSHADOWLIGHTS) {
		return NULL;
	}

	shadowlights[numShadowLights].owner = NULL;
	numShadowLights++;
	return &shadowlights[numShadowLights-1];
}

/*
=============
R_ShadowFromDlight
=============
*/
void R_ShadowFromDlight(dlight_t *light) {

	shadowlight_t *l;

	l = AllocShadowLight();
	if (!l) return;

	VectorCopy(light->origin,l->origin);
	l->radius = light->radius;
	l->color[0] = light->color[0];
	l->color[1] = light->color[1];
	l->color[2] = light->color[2];
	l->baseColor[0] = light->color[0];
	l->baseColor[1] = light->color[1];
	l->baseColor[2] = light->color[2];
	l->style = 0;
	l->brightness = 1;
	l->isStatic = false;
	l->numVisSurf = 0;
	l->visSurf = NULL;
	l->style = light->style;
	l->owner = light->owner;

	//VectorCopy(light->angles,l->angles);
	
	//We use some different angle convention
	l->angles[1] = light->angles[0];
	l->angles[0] = light->angles[2];
	l->angles[2] = light->angles[1];

	l->filtercube = light->filtercube;
	l->rspeed = 0;
	l->cubescale = 1;
	l->castShadow = true;

	//Some people will be instulted by the mere existence of this flag.
	if (light->pflags & PFLAG_NOSHADOW) {
		l->castShadow = false;
	} else {
		l->castShadow = true;
	}

	if (light->pflags & PFLAG_HALO) {
		l->halo = true;
	} else {
		l->halo = false;
	}
}


#define NUM_CUBEMAPS 64
int cubemap_tex_obj [NUM_CUBEMAPS];


/*
=============
R_CubeMapLookup
=============
*/
int R_CubeMapLookup(int i) {

	if (i > NUM_CUBEMAPS) {
		return 0;
	} else {
		if (!cubemap_tex_obj[i]) {
			cubemap_tex_obj[i] = GL_LoadCubeMap(i);
		}
		return cubemap_tex_obj[i];
	}
}

/*
=============
R_ShadowFromEntity
=============
*/
void R_ShadowFromEntity(entity_t *ent) {

	shadowlight_t *l;

	l = AllocShadowLight();
	if (!l) return;

	VectorCopy(ent->origin,l->origin);
	l->radius = 350;
	l->color[0] = 1;
	l->color[1] = 1;
	l->color[2] = 1;
	l->style = 0;
	l->brightness = 1;
	l->isStatic = false;
	l->numVisSurf = 0;
	l->visSurf = NULL;
	l->style = 0;
	l->owner = ent;

	l->style = ent->style;

	if (ent->light_lev != 0) {
		l->radius = ent->light_lev;
	} else {
		l->radius = 350;
	}

	VectorCopy(ent->color,l->baseColor);

	if ((l->baseColor[0] == 0) && (l->baseColor[1] == 0) && (l->baseColor[2] == 0)) {
		l->baseColor[0] = 1;
		l->baseColor[1] = 1;
		l->baseColor[2] = 1;
	}

	if (ent->skinnum >= 16) {
		l->filtercube = R_CubeMapLookup(ent->skinnum);
	} else {
		l->filtercube = 0;
	}

	//We use some different angle convention
	l->angles[1] = ent->angles[0];
	l->angles[0] = ent->angles[2];
	l->angles[2] = ent->angles[1];


	l->rspeed = ent->alpha*512;
	l->cubescale = 1;

	//Some people will be instulted by the mere existence of this flag.
	if (ent->pflags & PFLAG_NOSHADOW) {
		l->castShadow = false;
	} else {
		l->castShadow = true;
	}

	if (ent->pflags & PFLAG_HALO) {
		l->halo = true;
	} else {
		l->halo = false;
	}

}

/*
=============
R_MarkDLights

Adds dynamic lights to the shadow light list
=============
*/
void R_MarkDlights (void)
{
	int		i;
	dlight_t	*l;

	l = cl_dlights;
	for (i=0 ; i<MAX_DLIGHTS ; i++, l++)
	{
		if (l->die < cl.time || !l->radius)
			continue;
		R_ShadowFromDlight(l);
	}
}


/*
=============
R_MarkEntities

Adds entities that have a lightsource attched to
the shadow light list.
(used for static ents)
We just check if they have a torch/flame model or not.
=============
*/
void R_MarkEntities (void)
{
	int		i;
	entity_t *current;

	for (i=0 ; i<cl.num_statics; i++)
	{
		current = &cl_static_entities[i];

		if (!strcmp (current->model->name, "progs/flame2.mdl")
		|| !strcmp (current->model->name, "progs/flame.mdl") )
		{
			R_ShadowFromEntity(current);
		}
	}
}

int cut_ent;

/*
=============
R_InitShadowsForFrame

Do per frame intitialization for the shadows
=============
*/
void R_InitShadowsForFrame(void) {

	byte *vis;
	int i;

	numShadowLights = numStaticShadowLights;

	R_MarkDlights (); //add dynamic lights to the list
//	R_ShadowFromPlayer();//give the player some sort of torch

	numUsedShadowLights = 0;

	//if (cut_ent) Con_Printf("cut ents: %i\n",cut_ent);
	cut_ent = 0;
	Q_memset (&worldvis, 0, MAX_MAP_LEAFS/8); //all invisible

	vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);
	Q_memcpy(&worldvis, vis, MAX_MAP_LEAFS/8);

	for (i=0; i<numShadowLights; i++) {
		currentshadowlight = &shadowlights[i];
		if (R_ContributeFrame(currentshadowlight)) {
			currentshadowlight->visible = true;
			if (numUsedShadowLights < MAXUSEDSHADOWLIGHS) {
				usedshadowlights[numUsedShadowLights] = currentshadowlight;
				numUsedShadowLights++;
			} else {
				Con_Printf("R_InitShadowsForFrame: More than MAXUSEDSHADOWLIGHS lights for frame\n");
			}
		} else {
			currentshadowlight->visible = false;
		}
	}
}


qboolean R_MarkShadowSurf(msurface_t *surf, shadowlight_t *light) 
{
	mplane_t	*plane;
	float		dist;
	glpoly_t	*poly;

	//we don't cast shadows with water
	if (( surf->flags & SURF_DRAWTURB ) || ( surf->flags & SURF_DRAWSKY )) {
		return false;
	}

	plane = surf->plane;

	poly = surf->polys;

	if (poly->lightTimestamp == r_lightTimestamp) {
		return false;
	}

	switch (plane->type)
	{
	case PLANE_X:
		dist = light->origin[0] - plane->dist;
		break;
	case PLANE_Y:
		dist = light->origin[1] - plane->dist;
		break;
	case PLANE_Z:
		dist = light->origin[2] - plane->dist;
		break;
	default:
		dist = DotProduct (light->origin, plane->normal) - plane->dist;
		break;
	}

	//the normals are flipped when surf_planeback is 1
	if (((surf->flags & SURF_PLANEBACK) && (dist > 0)) ||
		(!(surf->flags & SURF_PLANEBACK) && (dist < 0)))
	{
		return false;
	}

	//the normals are flipped when surf_planeback is 1
	if ( abs(dist) > light->radius)
	{
		return false;
	}

	poly->lightTimestamp = r_lightTimestamp;

	return true;
}

#define MAX_LEAF_LIST 32
mleaf_t *leafList[MAX_LEAF_LIST];
int numLeafList;
extern vec3_t		r_emins, r_emaxs;	// <AWE> added "extern".

/*
=============

InShadowEntity

Some efrag based sceme may cut even more ents!
=============
*/

qboolean InShadowEntity(entity_t *ent) {
	
	int i, leafindex;

	model_t	*entmodel = ent->model;
	vec3_t dst;
	float radius, d;

	radius  = entmodel->radius;
	VectorSubtract (currentshadowlight->origin, ent->origin, dst);
	d = Length (dst);

	if (d < (currentshadowlight->radius + radius)) {
		
		if (sh_noefrags.value) return true;
		
		for (i=0; i<ent->numleafs;i++) {
			leafindex = ent->leafnums[i];
			//leaf ent is in is visible from light
			if (currentshadowlight->entvis[leafindex>>3] & (1<<(leafindex&7)))
			{
				return true;
			}
		}
		if (ent->numleafs == 0) {
			//Con_Printf("Ent with no leafs");
			return true;
		}
		return false;
	}
	return false;
}

/*
=============

R_VisibleEntity

Adds shadow casting ents to the list
=============
*/
void MarkShadowEntities() {

	int		i;
	vec3_t	mins, maxs;

	if (!r_drawentities.value)
		return;

	cl_numlightvisedicts = 0;
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		if ((currententity->model->flags & EF_NOSHADOW) && (currententity->model->flags & EF_FULLBRIGHT)) {
			continue;
		}

		if (mirror) {
			VectorAdd (currententity->origin,currententity->model->mins, mins);
			VectorAdd (currententity->origin,currententity->model->maxs, maxs);
			if (mirror_clipside == BoxOnPlaneSide(mins, maxs, mirror_plane)) {
				continue;
			}

			if ( BoxOnPlaneSide(mins, maxs, &mirror_far_plane) == 1) {
				return;
			}
		}

		//Dont cast shadows with the ent this light is attached to because
		//when the light is partially in the model shadows will look weird.
		//FIXME: Model is not lit by its own light.
		if (currententity != currentshadowlight->owner)
		{
			if (InShadowEntity(currententity)) {
				currententity->lightTimestamp = r_lightTimestamp;
				cl_lightvisedicts[cl_numlightvisedicts] = currententity;
				cl_numlightvisedicts++;
			} else cut_ent++;
		}
	}
}

/*
=============
R_ProjectSphere

Returns the rectangle the sphere will be in when it is drawn.
FIXME: This is crappy code we draw a "sprite" and project those points
it should be possible to analytically derive a eq.

=============
*/
void R_ProjectSphere (shadowlight_t *light, int *rect)
{
	int		i, j;
	float	a;
	vec3_t	v, vp2;
	float	rad;
	double minx, maxx, miny, maxy;
	double px, py, pz;

	rad = light->radius;
	/*
	rect[0] = 100;
	rect[1] = 100;
	rect[2] = 300;
	rect[3] = 300;

	return;
	*/
	VectorSubtract (light->origin, r_origin, v);

	//dave - slight fix
	VectorSubtract (light->origin, r_origin, vp2);
	VectorNormalize(vp2);

	minx = 1000000;
	miny = 1000000;
	maxx = -1000000;
	maxy = -1000000;

	for (i=16 ; i>=0 ; i--)
	{
		a = i/16.0 * M_PI*2;
		for (j=0 ; j<3 ; j++)
			v[j] = light->origin[j] + vright[j]*cos(a)*rad + vup[j]*sin(a)*rad;

		gluProject(v[0], v[1], v[2], r_Dworld_matrix, r_Dproject_matrix,
                           (GLint *) r_Iviewport, &px, &py, &pz);			// <AWE> added cast.

		if (px > maxx) maxx = px;
		if (px < minx) minx = px;
		if (py > maxy) maxy = py;
		if (py < miny) miny = py;

	}

	rect[0] = (int)minx;
	rect[1] = (int)miny;
	rect[2] = (int)maxx;
	rect[3] = (int)maxy;	
}

/**************************************************************

	World model shadow volumes

***************************************************************/


/*
=============

HasSharedLeaves

Returns true if both vis arrays have shared leafs visible.
FIXME: compare bytes at a time (what does quake fill the unused bits with??)
=============
*/
qboolean HasSharedLeafs(byte *v1, byte *v2) {

	int i;

	for (i=0 ; i<cl.worldmodel->numleafs ; i++)
	{
		if (v1[i>>3] & (1<<(i&7)))
		{
			if (v2[i>>3] & (1<<(i&7))) 
				return true;
		}
	}
	return false;
}

/*
=============

R_MarkShadowCasting

Fills the shadow chain with polygons we should consider.

Polygons that will be added are:
1. In the light volume. (sphere)
2. "Visible" to the light.

Visible is:
a. facing the light (dotprod > 0)
b. in a leaf that is visible from the light's leaf. (based on vis data)

This is crude for satic lights we use extra tricks (svbsp / revis) to
reduce the number of polyons.
=============
*/
void R_MarkShadowCasting (shadowlight_t *light, mnode_t *node)
{
	mplane_t	*plane;
	float		dist;
	msurface_t	**surf;
	mleaf_t		*leaf;
	int			c,leafindex;

	if (node->contents < 0) {
		//we are in a leaf
		leaf = (mleaf_t *)node;
		leafindex = leaf->index-1;

		//is this leaf visible from the light
		if (!(lightvis[leafindex>>3] & (1<<(leafindex&7)))) {
			return;
		}

		c = leaf->nummarksurfaces;
		surf = leaf->firstmarksurface;

		for (c=0; c<leaf->nummarksurfaces; c++, surf++) {

			if (R_MarkShadowSurf ((*surf), light)) {
				(*surf)->shadowchain = shadowchain;
				shadowchain = (*surf);				
				//svBsp_NumKeptPolys++;
			}
		}

		return;
	}

	plane = node->plane;
	dist = DotProduct (light->origin, plane->normal) - plane->dist;
	
	if (dist > light->radius)
	{
		R_MarkShadowCasting (light, node->children[0]);
		return;
	}
	if (dist < -light->radius)
	{
		R_MarkShadowCasting (light, node->children[1]);
		return;
	}

	R_MarkShadowCasting (light, node->children[0]);
	R_MarkShadowCasting (light, node->children[1]);
}

float SphereInFrustum( vec3_t o, float radius )
{
   int p;
   float d;

   for( p = 0; p < 6; p++ )
   {
      d = frustumPlanes[p][0] * o[0] + frustumPlanes[p][1] * o[1] + frustumPlanes[p][2] * o[2] + frustumPlanes[p][3];
      if( d <= -radius )
         return 0;
   }
   return d + radius;
}

/*
=============
R_ContributeFrame

Returns true if the light should be rendered.

=============
*/
qboolean R_ContributeFrame (shadowlight_t *light)
{
	mleaf_t *lightleaf;
	float dist;

	float b = d_lightstylevalue[light->style]/255.0;

	light->color[0] = light->baseColor[0] * b;
	light->color[1] = light->baseColor[1] * b;
	light->color[2] = light->baseColor[2] * b;

	//verry soft light, don't bother.
	if (b < 0.1) return false;

	//frustum scissor testing
	dist = SphereInFrustum(light->origin, light->radius);
	if (dist == 0)  {
		//whole sphere is out ouf frustum so cut it.
		return false;
	}

	//fully/partially in frustum
	
	if (!sh_noscissor.value) {
		vec3_t dst;
		float d;
		VectorSubtract (light->origin, r_refdef.vieworg, dst);
		d = Length (dst);
		
		if (d > light->radius) {
			
			R_ProjectSphere (light, light->scizz.coords);
			//glScissor(light->scizz.coords[0], light->scizz.coords[1],
			//		light->scizz.coords[2]-light->scizz.coords[0],  light->scizz.coords[3]-light->scizz.coords[1]);
		} else {
			//viewport is ofs/width based
			light->scizz.coords[0] = r_Iviewport[0];
			light->scizz.coords[1] = r_Iviewport[1];
			light->scizz.coords[2] = r_Iviewport[0]+r_Iviewport[2];
			light->scizz.coords[3] = r_Iviewport[1]+r_Iviewport[3];
			//glScissor(r_Iviewport[0], r_Iviewport[1], r_Iviewport[2], r_Iviewport[3]);
		}
	}

	//r_lightTimestamp++;

	shadowchain = NULL;
	if (light->isStatic) {
		lightvis = &light->vis[0];
	} else {
		lightleaf = Mod_PointInLeaf (light->origin, cl.worldmodel);
		lightvis = Mod_LeafPVS (lightleaf, cl.worldmodel);
		Q_memcpy(&light->vis,lightvis,MAX_MAP_LEAFS/8);
		Q_memcpy(&light->entvis, lightvis, MAX_MAP_LEAFS/8);
	}

	if (HasSharedLeafs(lightvis,&worldvis[0])) {

		//light->visible = true;
		//numUsedShadowLights++;

		//mark shadow casting ents
		//MarkShadowEntities();

		//mark shadow casting polygons
		//if (!light->isStatic) {
		//	R_MarkShadowCasting ( light, cl.worldmodel->nodes);
		//} else {
		//	return true;
		//}
		//return (shadowchain) ? true : false;
		return true;
	} else {
		return false;
	}


}

/*
=============
R_FillShadowChain 

Returns true if the light should be rendered.

=============
*/
qboolean R_FillShadowChain (shadowlight_t *light)
{

	r_lightTimestamp++;

	shadowchain = NULL;

	lightvis = &light->vis[0];
	//numUsedShadowLights++;
	
	//mark shadow casting ents
	MarkShadowEntities();
	
	//mark shadow casting polygons
	if (!light->isStatic) {
	       R_MarkShadowCasting ( light, cl.worldmodel->nodes);
	} else {
	       return true;
	}
	
	return (shadowchain) ? true : false;
}

void *VolumeVertsPointer;
/*
=============
R_ConstructShadowVolume

	Calculate the shadow volume commands for the light.
	(only if dynamic)
=============
*/
void R_ConstructShadowVolume(shadowlight_t *light) {

	if (!light->isStatic) {
		PrecalcVolumesForLight(cl.worldmodel);
		light->volumeCmds = &volumeCmdsBuff[0];
		light->volumeVerts = &volumeVertsBuff[0];
		light->lightCmds = &lightCmdsBuff[0];
	}

	VolumeVertsPointer = light->volumeVerts;

	if (gl_var) {
		if (light->numVolumeVerts < AGP_BUFFER_SIZE/sizeof(float)) {
			memcpy(AGP_Buffer,light->volumeVerts,light->numVolumeVerts*sizeof(float));
			VolumeVertsPointer = AGP_Buffer;
		}
	}
}

/*
=============
R_DrawShadowVolume

Draws the shadow volume, for statics this is the precalc one
for dynamics this is the R_ConstructShadowVolume one.

=============
*/
void R_DrawShadowVolume(shadowlight_t *light) {


	glDisable(GL_TEXTURE_2D);
	
/*	if (!light->isStatic) {
		glColor3f(0,1,0);
		DrawVolumeFromCmds (&volumeCmdsBuff[0], &lightCmdsBuff[0]);
	} else {
*/
		glColor3f(1,0,0);
		DrawVolumeFromCmds (light->volumeCmds, light->lightCmds, VolumeVertsPointer);
//	}

	glColor3f(1,1,1);
	glEnable(GL_TEXTURE_2D);
}


/**************************************************************


	Brush model shadow volume support


***************************************************************/

/*
=============
R_MarkBrushModelSurfaces

	Set the light timestamps of the brush model.
=============
*/
void R_MarkBrushModelSurfaces(entity_t *e) {

	vec3_t		mins, maxs;
	int			i;
	msurface_t	*psurf;
	model_t		*clmodel;
	qboolean	rotated;

	vec3_t oldlightorigin;
	//backup light origin since we will have to translate
	//light into model space
	VectorCopy (currentshadowlight->origin, oldlightorigin);

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

	VectorSubtract (r_refdef.vieworg, e->origin, modelorg);
	VectorSubtract (currentshadowlight->origin, e->origin, currentshadowlight->origin);
	if (rotated)
	{
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);

		VectorCopy (currentshadowlight->origin, temp);
		currentshadowlight->origin[0] = DotProduct (temp, forward);
		currentshadowlight->origin[1] = -DotProduct (temp, right);
		currentshadowlight->origin[2] = DotProduct (temp, up);

	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

    glPushMatrix ();
e->angles[0] = -e->angles[0];	// stupid quake bug
	R_RotateForEntity (e);
e->angles[0] = -e->angles[0];	// stupid quake bug


	for (i=0 ; i<clmodel->nummodelsurfaces ; i++, psurf++)
	{
		R_MarkShadowSurf(psurf, currentshadowlight);
	}

	VectorCopy(oldlightorigin,currentshadowlight->origin);
	glPopMatrix ();
}


/*
=============
R_DrawBrushModelVolumes

	Draw the shadow volumes of the brush model.
	They are dynamically calculated.
=============
*/
/*
void R_DrawBrushModelVolumes(entity_t *e) {

	int			j, k;
	vec3_t		mins, maxs;
	int			i, numsurfaces;
	msurface_t	*surf;
	float		dot;
	mplane_t	*pplane;
	model_t		*clmodel;
	qboolean	rotated;
	glpoly_t	*poly;
	vec3_t v1,*v2;
	float scale;
	qboolean shadow;
	vec3_t		temp[32];


	vec3_t oldlightorigin;

	//backup light origin since we will have to translate
	//light into model space
	VectorCopy (currentshadowlight->origin, oldlightorigin);

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

	VectorSubtract (r_refdef.vieworg, e->origin, modelorg);
	VectorSubtract (currentshadowlight->origin, e->origin, currentshadowlight->origin);
	if (rotated)
	{
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);

		VectorCopy (currentshadowlight->origin, temp);
		currentshadowlight->origin[0] = DotProduct (temp, forward);
		currentshadowlight->origin[1] = -DotProduct (temp, right);
		currentshadowlight->origin[2] = DotProduct (temp, up);

	}

	surf = &clmodel->surfaces[clmodel->firstmodelsurface];

    glPushMatrix ();
e->angles[0] = -e->angles[0];	// stupid quake bug
	R_RotateForEntity (e);
e->angles[0] = -e->angles[0];	// stupid quake bug

	for (i=0 ; i<clmodel->nummodelsurfaces ; i++, surf++)
	{
		if (surf->polys->lightTimestamp != r_lightTimestamp) continue;

		poly = surf->polys;

		for (j=0 ; j<surf->numedges ; j++)
		{
			v2 = (vec3_t *)&poly->verts[j];
			VectorSubtract ( (*v2) ,currentshadowlight->origin, v1);
			scale = Length (v1);

			if (sh_visiblevolumes.value)
				VectorScale (v1, (1/scale)*50, v1);
			else
				VectorScale (v1, (1/scale)*currentshadowlight->radius*2, v1);

			VectorAdd (v1, (*v2), temp[j]);
		}

		//check if neighbouring polygons are shadowed
		for (j=0 ; j<surf->numedges ; j++)
		{
			shadow = false;

			if (poly->neighbours[j] != NULL) {
				if ( poly->neighbours[j]->lightTimestamp != poly->lightTimestamp) {
					shadow = true;
				}
			} else {
				shadow = true;
			}

			if (shadow) {
				
				//we extend the shadow volumes by projecting them on the
				//light's sphere.
				//This sometimes gives problems when the light is verry close to a big
				//polygon.  But further extending the volume wastes fill rate.
				//So ill have to fix it.

				glBegin(GL_QUAD_STRIP);
					glVertex3fv(&poly->verts[j][0]);
					glVertex3fv(&temp[j][0]);
					glVertex3fv(&poly->verts[((j+1)% poly->numverts)][0]);
					glVertex3fv(&temp[((j+1)% poly->numverts)][0]);
				glEnd();
			}
		}

		//Draw near light cap
		glBegin(GL_POLYGON);
		for (j=0; j<surf->numedges ; j++)
		{
				glVertex3fv(&poly->verts[j][0]);
		}
		glEnd();

		//Draw extruded cap
		glBegin(GL_POLYGON);
		for (j=surf->numedges-1; j>=0 ; j--)
		{
				glVertex3fv(&temp[j][0]);
		}
		glEnd();
	}

	//PrecalcVolumesForLight(clmodel);

	VectorCopy(oldlightorigin,currentshadowlight->origin);
	glPopMatrix ();
}

*/
/*
=============
R_DrawBrushModelVolumes

	Draw the shadow volumes of the brush model.
	They are dynamically calculated.
=============
*/

void R_DrawBrushModelVolumes(entity_t *e) {

	model_t	*model = e->model;
	msurface_t *surf;
	glpoly_t	*poly;
	int		i, j, count;
	brushlightinstant_t *ins = e->brushlightinstant;

	count = 0;


    glPushMatrix ();
	e->angles[0] = -e->angles[0];	// stupid quake bug
	R_RotateForEntity (e);
	e->angles[0] = -e->angles[0];	// stupid quake bug


	surf = &model->surfaces[model->firstmodelsurface];
	for (i=0; i<model->nummodelsurfaces; i++, surf++)
	{
		if (!ins->polygonVis[i]) continue;

		poly = surf->polys;
		//extrude edges
		for (j=0 ; j<surf->numedges ; j++)
		{
			if (ins->neighbourVis[count+j]) {
				glBegin(GL_QUAD_STRIP);
					//glVertex3fv(&poly->verts[j][0]);
					glVertex3fv((float *)(&globalVertexTable[surf->polys->firstvertex+j]));
					glVertex3fv(&ins->extvertices[count+j][0]);
					//glVertex3fv(&poly->verts[((j+1)% poly->numverts)][0]);
					glVertex3fv((float *)(&globalVertexTable[surf->polys->firstvertex+((j+1)% poly->numverts)]));
					glVertex3fv(&ins->extvertices[count+((j+1)% poly->numverts) ][0]);
				glEnd();			
			}
		}

		//Draw near light cap
		glBegin(GL_TRIANGLE_FAN);
		for (j=0; j<surf->numedges ; j++)
		{
			//glVertex3fv(&poly->verts[j][0]);
			glVertex3fv((float *)(&globalVertexTable[surf->polys->firstvertex+j]));
		}
		glEnd();

		//Draw extruded cap
		glBegin(GL_TRIANGLE_FAN);
		for (j=surf->numedges-1; j>=0 ; j--)
		{
			glVertex3fv(&ins->extvertices[count+j][0]);
		}
		glEnd();

		count+=surf->numedges;
	}	

	glPopMatrix ();
}


/**************************************************************

	Shadow volume precalculation & storing

***************************************************************/


/*
=============

getVertexIndexFromSurf

Gets index of the i'th vertex of the surface in the models vertex array
=============
*/
int getVertexIndexFromSurf(msurface_t *surf, int index, model_t *model) {

		int lindex = model->surfedges[surf->firstedge + index];
		medge_t	*r_pedge;

		if (lindex > 0)
		{
			r_pedge = &model->edges[lindex];
			//if (r_pedge->v[0] == 0) Con_Printf("moord en brand");
			return r_pedge->v[0];
		}
		else
		{
			r_pedge = &model->edges[-lindex];
			//if (r_pedge->v[1] == 0) Con_Printf("moord en brand");
			return r_pedge->v[1];
		}
}

/*
=============

PrecalcVolumesForLight

This will create arrays with gl-commands that define the shadow volumes
(something similar to the mesh arrays that are created.)
They are stored for static lights and recalculated every frame for dynamic ones.
Non calculated vertices are not saved in the list but the index in the vertex array
of the model is saved.

We store them in volumeCmdsBuff and lightCmdsBuff

=============
*/
void PrecalcVolumesForLight(model_t *model) {

	msurface_t *surf;

	int *volumeCmds = &volumeCmdsBuff[0];
	lightcmd_t *lightCmds = &lightCmdsBuff[0];
	float *volumeVerts = &volumeVertsBuff[0];
	int volumePos = 0;
	int lightPos = 0;
	int vertPos = 0;
	int startVerts, startNearVerts, numPos = 0, stripLen = 0, i;
	glpoly_t *poly;
	qboolean lastshadow;
	vec3_t v1, *v2, vert1;
	float scale;
	qboolean shadow;

	vec3_t *s, *t, nearPt, nearToVert;
	float dist, colorscale;
	mplane_t *splitplane;
	int		j;
	float	*v;

	surf = shadowchain;

	//1. Calculate shadow volumes

	while (surf)
	{

		poly = surf->polys;

		if (( surf->flags & SURF_DRAWTURB ) || ( surf->flags & SURF_DRAWSKY )) {
			surf = surf->shadowchain;
			Con_Printf ("Water/Sky in shadow chain!!");
			continue;
		}
	
		
		//a. far cap
//		volumeCmds[volumePos++] = GL_POLYGON;
		volumeCmds[volumePos++] = GL_TRIANGLE_FAN;
		volumeCmds[volumePos++] = surf->numedges;

		startVerts = (int)vertPos/3;
		for (i=0 ; i<surf->numedges ; i++)
		{
			//v2 = (vec3_t *)&poly->verts[i];
			v2 = (vec3_t *)(&globalVertexTable[surf->polys->firstvertex+i]);
			VectorSubtract ( (*v2), currentshadowlight->origin, v1);

			scale = Length (v1);
			if (sh_visiblevolumes.value) {
				//make them short so that we see them
				VectorScale (v1, (1.0f/scale)*50.0f, v1);
			} else {
				//we don't have to be afraid they will clip with the far plane
				//since we use the infinite matrix trick
				VectorScale (v1, (1.0f/scale)* currentshadowlight->radius*10.0f, v1);
			}


			VectorAdd (v1, (*v2) ,vert1);
			VectorCopy (vert1, ((vec3_t *)(volumeVerts+vertPos))[0]);
			vertPos+=3;

			volumeCmds[volumePos++] = startVerts+(surf->numedges-(i+1));
		}

		//copy vertices
		startNearVerts = (int)vertPos/3;
		for (i=0 ; i<surf->numedges ; i++)
		{
			//v2 = (vec3_t *)&poly->verts[i];
			v2 = (vec3_t *)(&globalVertexTable[surf->polys->firstvertex+i]);
			/*(float)*/volumeVerts[vertPos++] = (*v2)[0];	// <AWE> lvalue cast. let's get rid off it!
			/*(float)*/volumeVerts[vertPos++] = (*v2)[1];	// <AWE> a float is a float is a...
			/*(float)*/volumeVerts[vertPos++] = (*v2)[2];
		}


		if (vertPos >  MAX_VOLUME_VERTS) {
			Con_Printf ("More than MAX_VOLUME_VERTS vetices! %i\n", volumePos);
			break;
		}
		
		
		//b. borders of volume
		//we make quad strips if we have continuous borders
		lastshadow = false;

		for (i=0 ; i<surf->numedges ; i++)
		{

			shadow = false;

			if (poly->neighbours[i] != NULL) {
				if ( poly->neighbours[i]->lightTimestamp != poly->lightTimestamp) {
					shadow = true;
				}
			} else {
				shadow = true;
			}

			if (shadow) {

				if (!lastshadow) {
					//begin new strip
					volumeCmds[volumePos++] = GL_QUAD_STRIP;
					numPos = volumePos;
					volumeCmds[volumePos++] = 4;
					stripLen = 2;

					//copy vertices
					volumeCmds[volumePos++] = startNearVerts+i;//-getVertexIndexFromSurf(surf, i, model);
					volumeCmds[volumePos++] = startVerts+i;

				}

				volumeCmds[volumePos++] = startNearVerts+((i+1)%poly->numverts);//-getVertexIndexFromSurf(surf, (i+1)%poly->numverts, model);
				volumeCmds[volumePos++] = startVerts+((i+1)%poly->numverts);

				stripLen+=2;
				
			} else {
				if (lastshadow) {
					//close list up
					volumeCmds[numPos] = stripLen;
				}
			}
			lastshadow = shadow;
		}
		
		if (lastshadow) {
			//close list up
			volumeCmds[numPos] = stripLen;
		}
		
		if (volumePos >  MAX_VOLUME_COMMANDS) {
			Con_Printf ("More than MAX_VOLUME_COMMANDS commands! %i\n", volumePos);
			break;
		}
		
		//c. glow surfaces/texture coordinates
			//leftright vectors of plane
			s = (vec3_t *)&surf->texinfo->vecs[0];
			t = (vec3_t *)&surf->texinfo->vecs[1];

			/*
			VectorNormalize(*s);
			VectorNormalize(*t);
			*/

			splitplane = surf->plane;

			dist = DotProduct (currentshadowlight->origin, splitplane->normal) - splitplane->dist;


			dist = abs(dist);

			if (dist > currentshadowlight->radius) Con_Printf("Polygon to far\n");
			ProjectPlane (currentshadowlight->origin, (*s), (*t), nearPt);

			scale = 1.0f /((2.0f * currentshadowlight->radius) - dist);
			colorscale = (1.0f - (dist / currentshadowlight->radius));

			if (colorscale <0) colorscale = 0;

			lightCmds[lightPos++].asInt = GL_TRIANGLE_FAN;

            // <AWE> HACK: Fix for 64-bit pointers in lightcmd buffer
			//lightCmds[lightPos++].asVoid = surf;
            lightCmds[lightPos++].asOffset = (int)(((byte*) surf) - hunk_base);
            lightCmds[lightPos++].asFloat = currentshadowlight->color[0]*colorscale; 
			lightCmds[lightPos++].asFloat = currentshadowlight->color[1]*colorscale;
			lightCmds[lightPos++].asFloat = currentshadowlight->color[2]*colorscale;
			lightCmds[lightPos++].asFloat = colorscale;

			//v = poly->verts[0];
			v = (float *)(&globalVertexTable[surf->polys->firstvertex]);
			for (j=0 ; j<poly->numverts ; j++, v+= VERTEXSIZE)
			{
				// Project the light image onto the face
				VectorSubtract (v, nearPt, nearToVert);

				// Get our texture coordinates, transform into tangent plane
				lightCmds[lightPos++].asVec = DotProduct (nearToVert, (*s)) * scale + 0.5;
				lightCmds[lightPos++].asVec = DotProduct (nearToVert, (*t)) * scale + 0.5;
				
				//calculate local light vector and put it into tangent space
				{
					vec3_t lightDir, tsLightDir;

					VectorSubtract( currentshadowlight->origin,v,lightDir);

					if (surf->flags & SURF_PLANEBACK)	{
						tsLightDir[2] = -DotProduct(lightDir,surf->plane->normal);
					} else {
						tsLightDir[2] = DotProduct(lightDir,surf->plane->normal);
					}

					tsLightDir[1] = -DotProduct(lightDir,(*t));
					tsLightDir[0] = DotProduct(lightDir,(*s));
					lightCmds[lightPos++].asVec = tsLightDir[0];
					lightCmds[lightPos++].asVec = tsLightDir[1];
					lightCmds[lightPos++].asVec = tsLightDir[2];
				}
			}
		if (lightPos >  MAX_LIGHT_COMMANDS) {
			Con_Printf ("More than MAX_LIGHT_COMMANDS commands %i\n", lightPos);
			break;
		}
		
		surf = surf->shadowchain;
	}

	//Con_Printf("used %i\n",volumePos);
	//finish them off with 0
	lightCmds[lightPos++].asInt = 0;
	volumeCmds[volumePos++] = 0;

	numLightCmds = lightPos;
	numVolumeCmds = volumePos;
	numVolumeVerts = vertPos;
}
/*
=============
DrawVolumeFromCmds

Draws the generated commands as shadow volumes
=============
*/
void DrawVolumeFromCmds(int *volumeCmds, lightcmd_t *lightCmds, float *volumeVerts) {

	int command, num, i;
	int volumePos = 0;
	int lightPos = 0;
//	int count = 0;						// <AWE> no longer required.
	msurface_t *surf;
	float		*v;

	glVertexPointer(3, GL_FLOAT, 0, volumeVerts);
	glEnableClientState(GL_VERTEX_ARRAY);

	while (1) {
		command = volumeCmds[volumePos++];
		if (command == 0) break; //end of list
		num = volumeCmds[volumePos++];

		glDrawElements(command,num,GL_UNSIGNED_INT,&volumeCmds[volumePos]);
		volumePos+=num;

                /*glBegin(command);

		if ((command == GL_QUAD_STRIP) || (command == GL_QUADS)) {
			for (i=0; i<num; i++) {
				ind = volumeCmds[volumePos++];
				//	glVertex3fv((float *)(&volumeCmds[ind]));
				glVertex3fv(&volumeVerts[ind*3]);
			}
		} else {
			//caps point inwards
			//volumePos+=num*3;
			for (i=0; i<num; i++) {
				ind = volumeCmds[volumePos++];
				//extuded verts have w component
				//glVertex3fv((float *)(volumeCmds+(volumePos-(i+1)*3)));
				glVertex3fv(&volumeVerts[ind*3]);
			}
			//volumePos+=num*3;
		}

		glEnd();*/
//		count++;			// <AWE> no longer required.
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	
	//Con_Printf("%i objects drawn\n",count);
	if (sh_visiblevolumes.value) return;

	while (1) {
		
		command = lightCmds[lightPos++].asInt;
		if (command == 0) break; //end of list

        // <AWE> HACK: Fix for 64-bit pointers in lightcmd buffer
		//surf = lightCmds[lightPos++].asVoid;
        surf = (msurface_t*) (hunk_base + lightCmds[lightPos++].asOffset);
		lightPos+=4;  //skip color
		num = surf->polys->numverts; 

		glBegin(command);
		//v = surf->polys->verts[0];
		v = (float *)(&globalVertexTable[surf->polys->firstvertex]);
		for (i=0; i<num; i++, v+= VERTEXSIZE) {
			//skip attent texture coord.
			lightPos+=2;
			//skip tangent space light vector
			lightPos+=3; 
			glVertex3fv(&v[0]);
		}
		glEnd();
	}
	
}

/*
=============
R_RenderGlow

Render a halo around a light.
The idea is similar to the UT2003 lensflares.
=============
*/

//speed at wich halo grows fainter when it gets occluded
#define HALO_FALLOF 2
//the scale the alpa is multipied with (none is verry bright)
#define HALO_ALPHA_SCALE 0.7
//the maximum radius (in pixels) of the halo
#define HALO_SIZE 30
//the distance of the eye at wich the halo stops shrinking
#define HALO_MIN_DIST 300
void R_RenderGlow (shadowlight_t *light)
{
	vec3_t	hit = {0, 0, 0};
	double	x,y,z;
	float	fdist,realz;
	int		ofsx, ofsy;
	qboolean hitWorld;

	if (!light->halo || gl_wireframe.value) 
		return;

	//trace a from the eye to the light source
	TraceLine (r_refdef.vieworg, light->origin, hit);

	//if it's not visible anymore slowly fade it
	if (Length(hit) != 0) {
		light->haloalpha = light->haloalpha - (cl.time - cl.oldtime)*HALO_FALLOF;
		if (light->haloalpha < 0) return;
		hitWorld = true;
	} else
		hitWorld = false;

	/*
	VectorSubtract(r_refdef.vieworg, light->origin, dist);
	fdist = Length(dist);
	if (fdist < HALO_MIN_DIST) fdist = HALO_MIN_DIST;
	fdist = (1-1/fdist)*HALO_SIZE;
	*/
	fdist = HALO_SIZE;
	
	gluProject(light->origin[0], light->origin[1], light->origin[2], r_Dworld_matrix,
                   r_Dproject_matrix, (GLint *) r_Iviewport, &x, &y, &z); // <AWE> added cast.

	//Con_Printf("Viewp %i %i %i %i\n",r_Iviewport[0],r_Iviewport[1],r_Iviewport[2],r_Iviewport[3]);
	if (!hitWorld) {
		//we didn't hit any bsp try to read the z buffer (wich is totally utterly freakingly
		// über evil!)
		glReadPixels((int)x,(int)y,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&realz);
		if (realz < z) {
			light->haloalpha = light->haloalpha - (cl.time - cl.oldtime)*HALO_FALLOF;
			if (light->haloalpha < 0) return;	
		} else {
			//nothing in the way make it fully bright
			light->haloalpha = 1.0;
		}
	}

	ofsx = r_Iviewport[0];
	ofsy = r_Iviewport[1];
	x = x;
	y = glheight-y;

	x = (x/(float)glwidth)*vid.width;
	y = (y/(float)glheight)*vid.height;

	//glDisable (GL_TEXTURE_2D)
	GL_Bind(halo_texture_object);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glShadeModel (GL_SMOOTH);
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE);
	glDisable(GL_ALPHA_TEST);



	glBegin (GL_QUADS);

	glColor4f(light->color[0],light->color[1],light->color[2],light->haloalpha*HALO_ALPHA_SCALE);
	//glColor4f(1,1,1,light->haloalpha);
	glTexCoord2f (0, 0);
	glVertex2f (x-fdist, y-fdist);
	glTexCoord2f (1, 0);
	glVertex2f (x+fdist, y-fdist);
	glTexCoord2f (1, 1);
	glVertex2f (x+fdist, y+fdist);
	glTexCoord2f (0, 1);
	glVertex2f (x-fdist, y+fdist);
	
	glEnd ();
/*
	rad = 40;//light->radius * 0.35;

	VectorSubtract (light->origin, r_origin, v);

	//dave - slight fix
	VectorSubtract (light->origin, r_origin, vp2);
	VectorNormalize(vp2);


	glBegin (GL_TRIANGLE_FAN);
	glColor3f (light->color[0]*0.2,light->color[1]*0.2,light->color[2]*0.2);
	for (i=0 ; i<3 ; i++)
		v[i] = light->origin[i] - vp2[i]*rad;
	glVertex3fv (v);
	glColor3f (0,0,0);
	for (i=16 ; i>=0 ; i--)
	{
		a = i/16.0 * M_PI*2;
		for (j=0 ; j<3 ; j++)
			v[j] = light->origin[j] + vright[j]*cos(a)*rad
				+ vup[j]*sin(a)*rad;
		glVertex3fv (v);
	}
	glEnd ();
*/
	glEnable(GL_ALPHA_TEST);
	glColor3f (1,1,1);
	glDisable (GL_BLEND);
	//glEnable (GL_TEXTURE_2D);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask (1);
	glColor3f (1,1,1);
}

/**************************************************************

	Shadow volume bsp's

	Some of this suff should be put in gl_svbsp.c

***************************************************************/

svnode_t *currentlightroot;
vec3_t testvect = {10,10,10};

/*
================
  CutLeafs

  Removes leaves that were cut by using the svbsp from the light's
  visibility list.
  This gains some speed in certain cases.
================
*/
void CutLeafs(byte *vis) {

	int c, i;
	msurface_t **surf;
	mleaf_t *leaf;
	qboolean found;
	int removed = 0;

	if (sh_norevis.value) return;

	for (i=0 ; i<cl.worldmodel->numleafs ; i++)//overloop alle leafs
	{

		if (vis[i>>3] & (1<<(i&7)))
		{
			//this leaf is visible from the leaf the current light (in a brute force way)
			//now check if we entirely cut this leaf by means of the svbsp
			leaf = &cl.worldmodel->leafs[i];
			c = leaf->nummarksurfaces;
			surf = leaf->firstmarksurface;
			
			if (leaf->index != i) Con_Printf("Weird leaf index %i, %i\n",i,leaf->index);
			found = false;
			for (c=0; c<leaf->nummarksurfaces; c++, surf++) {			
				if ((*surf)->polys->lightTimestamp == r_lightTimestamp) {
					found = true;
					break;
				}
			}

			if (!found) {
				//set vis bit on false
				vis[i>>3] &= ~(1<<(i&7));	
				removed++;
			}
		}
	}

	//Con_Printf("  Removed leafs: %i\n", removed);
}

/*
================
AddToShadowBsp

Add a surface as potential shadow caster to the svbsp, it can be
cut when it is occluded by other surfaces.
================
*/
void AddToShadowBsp(msurface_t *surf) {
	vec3_t surfvects[32];
	int numsurfvects;
	int i;
	float *v;

	/*
	PENTA: removed we checked this twice

	//we don't cast shadows with water
	if (( surf->flags & SURF_DRAWTURB ) || ( surf->flags & SURF_DRAWSKY )) {
		return;
	}

	plane = surf->plane;

	switch (plane->type)
	{
		case PLANE_X:
			dist = currentshadowlight->origin[0] - plane->dist;
			break;
		case PLANE_Y:
			dist = currentshadowlight->origin[1] - plane->dist;
			break;
		case PLANE_Z:
			dist = currentshadowlight->origin[2] - plane->dist;
			break;
		default:
			dist = DotProduct (currentshadowlight->origin, plane->normal) - plane->dist;
			break;
	}

	//the normals are flipped when surf_planeback is 1
	if (((surf->flags & SURF_PLANEBACK) && (dist > 0)) ||
		(!(surf->flags & SURF_PLANEBACK) && (dist < 0)))
	{
		return;
	}
	
	//the normals are flipped when surf_planeback is 1
	if ( abs(dist) > currentshadowlight->radius)
	{
		return;
	}
	
	*/

	//FIXME: use constant instead of 32
	if (surf->numedges > 32) {
		Con_Printf("Error: to many edges");
		return;
	}

	surf->visframe = 0;
	//Make temp copy of suface polygon
	numsurfvects = surf->numedges;
	for (i=0, v=(float *)(&globalVertexTable[surf->polys->firstvertex]); i<numsurfvects; i++, v+=VERTEXSIZE) {
		VectorCopy(v,surfvects[i]);
	}

	if (!sh_nosvbsp.value) {
		svBsp_NumAddedPolys++;
		R_AddShadowCaster(currentlightroot,surfvects,numsurfvects,surf,0);
	} else {
		surf->shadowchain = shadowchain;
		surf->polys->lightTimestamp = r_lightTimestamp;
		shadowchain = surf;
		svBsp_NumKeptPolys++;
	}
}

/*
================
R_RecursiveShadowAdd

Add surfaces front to back to the shadow bsp.
================
*/
void R_RecursiveShadowAdd(mnode_t *node)
{
	int			c, side;
	mplane_t	*plane;
	msurface_t	*surf;
	double		dot;

	if (node->contents == CONTENTS_SOLID) {
		return;		// solid
	}

	if (node->contents < 0) {
		return;		// leaf
	}

	//find which side of the node we are on

	plane = node->plane;

	switch (plane->type)
	{
	case PLANE_X:
		dot = currentshadowlight->origin[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = currentshadowlight->origin[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = currentshadowlight->origin[2] - plane->dist;
		break;
	default:
		dot = DotProduct (currentshadowlight->origin, plane->normal) - plane->dist;
		break;
	}

	if (dot >= 0)
		side = 0;
	else
		side = 1;

	//recurse down the children, front side first
	R_RecursiveShadowAdd (node->children[side]);


	//draw stuff
	c = node->numsurfaces;

	if (c)
	{
		
		surf = cl.worldmodel->surfaces + node->firstsurface;
		do
		{
			if (surf->polys) {
				if (surf->polys->lightTimestamp == r_lightTimestamp)
				{
					AddToShadowBsp (surf);
				}
			}
			surf++;
		} while (--c);
	}

	//recurse down the back side
	R_RecursiveShadowAdd (node->children[!side]);
}

/*
===============
R_MarkLightLeaves

Marks nodes from the light, this is used for
gross culling during svbsp creation.
===============
*/

void R_MarkLightLeaves (void)
{
	byte	solid[4096];
	mleaf_t	*lightleaf;

	//we use the same timestamp as for rendering (may cause errors maybe)
	r_visframecount++;
	lightleaf = Mod_PointInLeaf (currentshadowlight->origin, cl.worldmodel);

	if (r_novis.value)
	{
		lightvis = solid;
		memset (solid, 0xff, (cl.worldmodel->numleafs+7)>>3);
	}
	else
		lightvis = Mod_LeafPVS (lightleaf, cl.worldmodel);
}


/*
================
ShadowVolumeBsp

Create the shadow volume bsp for currentshadowlight
================
*/
void ShadowVolumeBsp() {
	currentlightroot = R_CreateEmptyTree();
	R_MarkLightLeaves();
	R_MarkShadowCasting (currentshadowlight,cl.worldmodel->nodes);
	shadowchain = NULL;
	svBsp_NumKeptPolys = 0;
	R_RecursiveShadowAdd(cl.worldmodel->nodes);
}

int done = 0;

/*
================
R_CalcSvBsp

Called for every static ent during spawning of the client
================
*/
void R_CalcSvBsp(entity_t *ent) {
	
	int i;
	msurface_t *surf;
	msurface_t	*s;
	texture_t	*t;

	//Con_Printf("Shadow volumes start\n");

	if (ent->model == NULL) {
		Con_Printf("null model");
		return;
	}

	if (!strcmp (ent->model->name, "progs/flame2.mdl")
		|| !strcmp (ent->model->name, "progs/flame.mdl")
		|| !strcmp (ent->model->name, "progs/s_light.spr")
		|| !strcmp (ent->model->name, "progs/b_light.spr")
		|| !strcmp (ent->model->name, "progs/w_light.spr"))
	{
		shadowchain = NULL;
		done++;
		
		//Con_Printf("->Light %i\n",done);
		

		r_lightTimestamp++;
		r_framecount++;

		svBsp_NumCutPolys = 0;
		svBsp_NumKeptPolys = 0;
		svBsp_NumAddedPolys = 0;

		//Create a light and make it static
		R_ShadowFromEntity(ent);
		numStaticShadowLights++;

		if (numShadowLights >= MAXSHADOWLIGHTS)  {
			Con_Printf("R_CalcSvBsp: More than MAXSHADOWLIGHTS lights");
			return;
		}

		currentshadowlight = &shadowlights[numShadowLights-1];

		//Hack: support quake light_* entities
		if (!strcmp (ent->model->name, "progs/flame2.mdl")) {
			currentshadowlight->baseColor[0] = 1;
			currentshadowlight->baseColor[1] = 0.9;
			currentshadowlight->baseColor[2] = 0.75;	
		} else if (!strcmp (ent->model->name, "progs/flame.mdl")) {
			currentshadowlight->baseColor[0] = 1;
			currentshadowlight->baseColor[1] = 0.9;
			currentshadowlight->baseColor[2] = 0.75;
		} else if (!strcmp (ent->model->name, "progs/s_light.spr")) {
			currentshadowlight->baseColor[0] = 1;
			currentshadowlight->baseColor[1] = 0.9;
			currentshadowlight->baseColor[2] = 0.75;
		}

		currentshadowlight->isStatic = true;

		//Calculate visible polygons
		ShadowVolumeBsp();

		//Print stats
		/*
		Con_Printf("  Thrown away: %i\n",svBsp_NumAddedPolys-svBsp_NumKeptPolys);
		Con_Printf("  Total in volume: %i\n",svBsp_NumKeptPolys);
		*/

		currentshadowlight->visSurf = Hunk_Alloc(sizeof(msurface_t*)*svBsp_NumKeptPolys);
		currentshadowlight->numVisSurf = svBsp_NumKeptPolys;
		surf = shadowchain;
	
		//Clear texture chains
		for (i=0 ; i<cl.worldmodel->numtextures ; i++)
		{
			if (!cl.worldmodel->textures[i]) continue;
 			cl.worldmodel->textures[i]->texturechain = NULL;
		}

		//Remark polys since polygons may have been removed since the last time stamp
		r_lightTimestamp++;
		for (i=0; i<svBsp_NumKeptPolys; i++,surf = surf->shadowchain) {
			surf->polys->lightTimestamp = r_lightTimestamp;
			currentshadowlight->visSurf[i] = surf;
			//put it in the correct texture chain
			surf->texturechain = surf->texinfo->texture->texturechain;
			surf->texinfo->texture->texturechain = surf;
		}

		//Sort surfs in our list per texture
		shadowchain = NULL;
		for (i=0 ; i<cl.worldmodel->numtextures ; i++)
		{
			t = cl.worldmodel->textures[i];
			if (!t)
				continue;
			s = t->texturechain;
			if (!s)
				continue;
			else
			{
				for ( ; s ; s=s->texturechain) {
					s->shadowchain = shadowchain;
					shadowchain = s;
				}
			}
			t->texturechain = NULL;
		}

		//Recalculate vis for this light
		currentshadowlight->leaf = Mod_PointInLeaf (currentshadowlight->origin, cl.worldmodel);
		lightvis = Mod_LeafPVS (currentshadowlight->leaf, cl.worldmodel);
		Q_memcpy(&currentshadowlight->vis[0], lightvis, MAX_MAP_LEAFS/8);
		Q_memcpy(&currentshadowlight->entvis[0], lightvis, MAX_MAP_LEAFS/8);
		CutLeafs(currentshadowlight->vis);

		//Precalculate the shadow volume / glow-texcoords
		PrecalcVolumesForLight(cl.worldmodel);
		currentshadowlight->volumeCmds = Hunk_Alloc(sizeof(int)*numVolumeCmds);
		Q_memcpy(currentshadowlight->volumeCmds, &volumeCmdsBuff, sizeof(int)*numVolumeCmds);

		currentshadowlight->volumeVerts = Hunk_Alloc(sizeof(float)*numVolumeVerts);
		currentshadowlight->numVolumeVerts = numVolumeVerts;
		Q_memcpy(currentshadowlight->volumeVerts, &volumeVertsBuff, sizeof(float)*numVolumeVerts);

		currentshadowlight->lightCmds = Hunk_Alloc(sizeof(lightcmd_t)*numLightCmds);
		Q_memcpy(currentshadowlight->lightCmds, &lightCmdsBuff, sizeof(lightcmd_t)*numLightCmds);
		//Con_Printf("light done\n");
	} else {
		//Con_Printf("thrown away");
	}

}

#define surfaceLightRadius 350.0

/*
	Not used places lights in front of things that look like lights in a map
*/
void LightFromSurface(msurface_t *surf) {
	vec3_t center, orig, normal;
	glpoly_t *poly;
	float	*v, invnum;
	int i;
	shadowlight_t *light;
	qboolean	tooClose;
	entity_t	fakeEnt;

	poly = surf->polys;
	invnum = 1.0/poly->numverts;

	//Calculate origin for the light we are possibly going to spawn
	//v = poly->verts[0];
	v = (float *)(&globalVertexTable[poly->firstvertex]);
	center[0] = center[1] = center[2] = 0;
	for (i=0 ; i<poly->numverts ; i++, v+= VERTEXSIZE)
	{
		VectorAdd(center,v,center);
	}
	VectorScale(center,invnum,center);

	if (surf->flags & SURF_PLANEBACK)	{
		VectorScale(surf->plane->normal,-1,normal);
	} else {
		VectorCopy(surf->plane->normal,normal);
	}

	VectorMA(center,16,normal,orig);


	//not to close to other lights then add it

	tooClose = false;
	for (i=0; i<numShadowLights; i++) {
		vec3_t dist;
		float length;
		light = &shadowlights[i];
		VectorSubtract(orig,light->origin,dist);
		length = Length(dist);

		if (length < (surfaceLightRadius*sh_radiusscale.value + light->radius*sh_radiusscale.value)) {
			tooClose = true;
		}
	}


	if (!tooClose) {
		Q_memset(&fakeEnt,0,sizeof(entity_t));
		fakeEnt.light_lev = surfaceLightRadius;
		VectorCopy(orig,fakeEnt.origin);
		fakeEnt.model = Mod_ForName("progs/w_light.spr",true);
		R_CalcSvBsp(&fakeEnt);
		Con_Printf("Added surface light");
	}

}

/**

Hacked entitiy loading code, this parses the entities client side to find lights in it

**/

void LightFromFile(vec3_t orig) {
	int i;
	shadowlight_t *light;
	qboolean	tooClose;
	entity_t	fakeEnt;




	//not to close to other lights then add it

	tooClose = false;
	for (i=0; i<numShadowLights; i++) {
		vec3_t dist;
		float length;
		light = &shadowlights[i];
		VectorSubtract(orig,light->origin,dist);
		length = Length(dist);

		if (length < (surfaceLightRadius*sh_radiusscale.value + light->radius*sh_radiusscale.value)) {
			tooClose = true;
		}
	}


	if (!tooClose) {
		Q_memset(&fakeEnt,0,sizeof(entity_t));
		fakeEnt.light_lev = surfaceLightRadius;
		VectorCopy(orig,fakeEnt.origin);
		fakeEnt.model = Mod_ForName("progs/w_light.spr",true);
		R_CalcSvBsp(&fakeEnt);
		//Con_Printf("Added file light");
	}


}


void ParseVector (char *s, float *d)
{
	int		i;
	char	string[128];
	char	*v, *w;
		
	strncpy (string, s,sizeof(string));
	v = string;
	w = string;
	for (i=0 ; i<3 ; i++)
	{
		while (*v && *v != ' ')
			v++;
		*v = 0;
		d[i] = atof (w);
		w = v = v+1;
	}
}



char *ParseEnt (char *data, qboolean *isLight, vec3_t origin)
{
	qboolean	init;
	char		keyname[256];
	qboolean	foundworld = false;
	init = false;

// go through all the dictionary pairs
	while (1)
	{	
	// parse key
		data = COM_Parse (data);
		if (com_token[0] == '}')
			break;
		if (!data)
			Sys_Error ("ParseEnt: EOF without closing brace");

		strncpy (keyname, com_token,sizeof(keyname));

	// parse value	
		data = COM_Parse (data);
		if (!data)
			Sys_Error ("ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			Sys_Error ("ED_ParseEntity: closing brace without data");

		init = true;	


		if (!strcmp(keyname, "classname")) {
			if (!strcmp(com_token, "light")) {
				*isLight = true;
			} else {
				*isLight = false;
			}
		} else if (!strcmp(keyname, "origin"))  {
			ParseVector(com_token, origin);	
		} else if (!strcmp(keyname, "_noautolight")) {
			Con_Printf("Automatic light gen disabled\n");//XYW \n
			foundworld = true;
		} else if (!strcmp(keyname, "_skybox")) {
			strncpy(skybox_name,com_token,sizeof(skybox_name));
		} else if (!strcmp(keyname, "_cloudspeed")) {
			skybox_cloudspeed = atof(com_token);
		} else if (!strcmp(keyname, "_lightmapbright")) {
			Cvar_Set("sh_lightmapbright",com_token);
			Con_Printf("Lightmap brightness set to %f\n",sh_lightmapbright.value);
		} else if (!strcmp(keyname, "_fog_color")) {
			ParseVector(com_token, origin);	
			Cvar_SetValue("fog_r",origin[0]);
			Cvar_SetValue("fog_g",origin[1]);
			Cvar_SetValue("fog_b",origin[2]);
		} else if (!strcmp(keyname, "_fog_start")) {
			Cvar_Set("fog_start",com_token);
		} else if (!strcmp(keyname, "_fog_end")) {
			Cvar_Set("fog_end",com_token);
		} else {

			//just do nothing

		}
	}



	if (foundworld) return NULL;
	return data;
}

void LoadLightsFromFile (char *data)
{	
	qboolean	isLight;
	vec3_t		origin;

	Cvar_SetValue ("fog_start",0.0);
	Cvar_SetValue ("fog_end",0.0);

// parse ents
	while (1)
	{
// parse the opening brace	
		data = COM_Parse (data);
		if (!data)
			break;
		if (com_token[0] != '{')
			Sys_Error ("ED_LoadFromFile: found %s when expecting {",com_token);

		isLight = false;
		data = ParseEnt (data, &isLight, origin);
		if (isLight) {
			LightFromFile(origin);
			//Con_Printf("found light in file");
		}
	}
	
	if ((!fog_start.value) && (!fog_end.value)) {
		Cvar_SetValue ("fog_enabled",0.0);
	}
}

void R_AutomaticLightPos() {

/*
	for (i=0; i<m->numsurfaces; i++) {
		surf = &m->surfaces[i];
		if (strstr(surf->texinfo->texture->name,"light")) {
			LightFromSurface(surf);
		}
	}*/

	LoadLightsFromFile(cl.worldmodel->entities);
}
