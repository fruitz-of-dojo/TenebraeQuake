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

Analog with aliasinstant

*/

#include "quakedef.h"


#define NUM_BRUSH_LIGHT_INSTANTS 32

brushlightinstant_t BLightInstantCache[NUM_BRUSH_LIGHT_INSTANTS];

#define DIST_DELTA 0.1
#define ANG_DELTA 0.5
#define RADIUS_DELTA 0.1

int brushCacheRequests, brushFullCacheHits, brushPartialCacheHits;

/*
R_AllocateBrushLightInstant
*/

brushlightinstant_t *R_AllocateBrushLightInstant(entity_t *e) {

	int i, oldest, oindex;
	
	//try to reclaim an instant that was previously used for this entity and this light
	for (i=0; i<NUM_BRUSH_LIGHT_INSTANTS; i++) {
		if ((BLightInstantCache[i].lastent == e) && (BLightInstantCache[i].lastlight == currentshadowlight)) {
			return &BLightInstantCache[i];
		}
	}

//	Con_Printf("legal new\n");
	//none found, bacause we don't want to destroy the reclaiming of other ents
	//we use the oldest used instant
	oldest = r_framecount;
	oindex = -1;
	for (i=0; i<NUM_BRUSH_LIGHT_INSTANTS; i++) {
		if (BLightInstantCache[i].lockframe < oldest) {
			oldest = BLightInstantCache[i].lockframe;
			oindex = i;
		}
	}

	if (oindex == -1) {
		//find an instant of an other light
		for (i=0; i<NUM_BRUSH_LIGHT_INSTANTS; i++) {
			if (BLightInstantCache[i].lastlight != currentshadowlight)
				return &BLightInstantCache[i];
		}
	}

	return &BLightInstantCache[oindex];
}


void R_ClearBrushInstantCaches() {

	int i;

	for (i=0; i<NUM_BRUSH_LIGHT_INSTANTS; i++) {
		BLightInstantCache[i].lockframe = 0;
		BLightInstantCache[i].lastent = NULL;
	}
}

void R_SetupBrushObjectSpace(entity_t *e, brushlightinstant_t *linstant) {

	matrix_4x4		transf;
	float			org[4], res[4];

	//Put light & view origin into object space

	e->angles[0] = -e->angles[0];//stupid quake bug  (ID invented it dunno what it doesn messus up angles)
	R_WorldToObjectMatrix(e, transf);
	e->angles[0] = -e->angles[0];//stupid quake bug 

	org[0] = currentshadowlight->origin[0];
	org[1] = currentshadowlight->origin[1];
	org[2] = currentshadowlight->origin[2];
	org[3] = 1;
	Mat_Mul_1x4_4x4(org,transf,res);
	linstant->lightpos[0] = res[0];
	linstant->lightpos[1] = res[1];
	linstant->lightpos[2] = res[2];

	org[0] = r_refdef.vieworg[0];
	org[1] = r_refdef.vieworg[1];
	org[2] = r_refdef.vieworg[2];
	org[3] = 1;
	Mat_Mul_1x4_4x4(org,transf,res);
	linstant->vieworg[0] = res[0];
	linstant->vieworg[1] = res[1];
	linstant->vieworg[2] = res[2];
}

int atest;

/*
I get the creeps of this if I use the dist of aliasinstant it totaly fucks up
the whole instant thing (not only brushes)
*/
float bdist(vec3_t v1, vec3_t v2) {
	vec3_t diff;
	VectorSubtract(v1,v2,diff);
	return Length(diff);
}

qboolean CheckBrushLightUpdate(entity_t *e, brushlightinstant_t *ins) {

/*
	if (ins->lastlight != currentshadowlight) Con_Printf("light\n");
	if (dist(ins->lastlorg,currentshadowlight->origin) >= DIST_DELTA) Con_Printf("lightorig\n");
	if (dist(ins->lasteorg,e->origin) >= DIST_DELTA) Con_Printf("entorig\n");
	if (dist(ins->lasteangles,e->angles) >= ANG_DELTA) Con_Printf("entangle\n");
	if (abs(ins->lastlradius - currentshadowlight->radius)  > RADIUS_DELTA) Con_Printf("lightrad\n");
	if (ins->lastshadowonly != ins->shadowonly) Con_Printf("shadowonly\n");
*/

	if (sh_nocache.value) return true;

//	return true;

	if ((ins->lastent == e) &&
		(ins->lastlight == currentshadowlight) &&
		(bdist(ins->lastlorg,currentshadowlight->origin) < DIST_DELTA) &&
		(bdist(ins->lasteorg,e->origin) < DIST_DELTA) &&
		(bdist(ins->lasteangles,e->angles) < ANG_DELTA) &&
		(fabs(ins->lastlradius - currentshadowlight->radius) <= RADIUS_DELTA) && 
		(ins->lastshadowonly == ins->shadowonly) &&
		(ins->lockframe >= r_framecount-10))//XYW Don't reuse if it has been unused for a long time

	{
		atest = atest+1;
		return false;
	} else {
		atest = atest-1;
		return true;
	}
}

qboolean CheckBrushHalfAngle(brushlightinstant_t *ins) 
{
	if (bdist(ins->lastvorg,r_refdef.vieworg) < 0.5) 
	{
		atest = atest+1;
		return false;
	} else {
		atest = atest-1;
		return true;
	}
}

/*
NOTE this is not the same as  R_MarkShadowSurf
*/
qboolean R_IsVisibleSurf(msurface_t *surf, brushlightinstant_t *ins) 
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
		//this happens with non unique brushmodels like ammoboxes that are already stamped from 
		//an previous entity with the same polygons
		poly->lightTimestamp = 0;
	}

	switch (plane->type)
	{
	case PLANE_X:
		dist = ins->lightpos[0] - plane->dist;
		break;
	case PLANE_Y:
		dist = ins->lightpos[1] - plane->dist;
		break;
	case PLANE_Z:
		dist = ins->lightpos[2] - plane->dist;
		break;
	default:
		dist = DotProduct (ins->lightpos, plane->normal) - plane->dist;
		break;
	}

	//the normals are flipped when surf_planeback is 1
	if (((surf->flags & SURF_PLANEBACK) && (dist > 0)) ||
		(!(surf->flags & SURF_PLANEBACK) && (dist < 0)))
	{
		return false;
	}

	//the normals are flipped when surf_planeback is 1
	if ( abs(dist) > currentshadowlight->radius)
	{
		return false;
	}

	poly->lightTimestamp = r_lightTimestamp;

	return true;
}

void R_CalcBrushVolumeVerts(entity_t *e, brushlightinstant_t *ins) {

	model_t	*model = e->model;
	msurface_t *surf;
	glpoly_t	*poly;
	int		extrvertcount, visneighcount;
	int		i, j;
	vec3_t	v1;
	vec3_t	*v2;
	float	scale;
	qboolean shadow;

	//mark visibility of polygons
	surf = &model->surfaces[model->firstmodelsurface];
	for (i=0; i<model->nummodelsurfaces; i++, surf++)
	{
		ins->polygonVis[i] =  R_IsVisibleSurf(surf, ins);
	}

	if (!currentshadowlight->castShadow) return;

	extrvertcount = 0;
	visneighcount = 0;

	surf = &model->surfaces[model->firstmodelsurface];
	for (i=0; i<model->nummodelsurfaces; i++, surf++)
	{
		if (!ins->polygonVis[i]) continue;

		poly = surf->polys;
		//extrude vertices and copy to buffer
		for (j=0 ; j<surf->numedges ; j++)
		{
			//v2 = (vec3_t *)&poly->verts[j];
			v2 = (vec3_t *)(&globalVertexTable[poly->firstvertex+j]);
			VectorSubtract ( (*v2) ,ins->lightpos, v1);
			scale = Length (v1);

			if (sh_visiblevolumes.value)
				VectorScale (v1, (1/scale)*50, v1);
			else
				VectorScale (v1, (1/scale)*currentshadowlight->radius*2, v1);

			VectorAdd (v1, (*v2), ins->extvertices[extrvertcount+j]);
		}

		//save visibility info of neighbours
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

			ins->neighbourVis[extrvertcount+j] = shadow;
		}
		extrvertcount+=surf->numedges;
	}
}

void R_CalcBrushAttenCoords(entity_t *e, brushlightinstant_t *ins) {
	msurface_t *psurf;
	mplane_t	*splitplane;
	int	i, j;
	vec3_t		s, t, nearToVert, nearPt;
	glpoly_t	*poly;
	int usedcolorscales, usedattencoords;
	float		dist, scale, colorscale;
	float		*v;

	psurf = &e->model->surfaces[e->model->firstmodelsurface];	

	usedcolorscales = 0;
	usedattencoords = 0;
	for (i=0 ; i<e->model->nummodelsurfaces ; i++, psurf++)
	{
		if (!ins->polygonVis[i])
			continue;

		poly = psurf->polys;

		VectorCopy(psurf->texinfo->vecs[0],s);
		VectorCopy(psurf->texinfo->vecs[1],t);
		
		splitplane = psurf->plane;
		
		switch (splitplane->type)
		{
		case PLANE_X:
			dist = ins->lightpos[0] - splitplane->dist;
			break;
		case PLANE_Y:
			dist = ins->lightpos[1] - splitplane->dist;
			break;
		case PLANE_Z:
			dist = ins->lightpos[2] - splitplane->dist;
			break;
		default:
			dist = DotProduct (ins->lightpos, splitplane->normal) - splitplane->dist;
			break;
		}
			
		dist = abs(dist);
		
		ProjectPlane(ins->lightpos,s,t,nearPt);
		
		scale = 1 /((2 * currentshadowlight->radius) - dist);
		colorscale = (1 - (dist / currentshadowlight->radius));
		
		if (colorscale <0) colorscale = 0;
		
		ins->colorscales[usedcolorscales] = colorscale;
		usedcolorscales++;

		//we could probably do this in hardware, with a vertex program!
		v = (float *)(&globalVertexTable[poly->firstvertex]);
		//v = poly->verts[0];
		for (j=0 ; j<poly->numverts ; j++, v+= VERTEXSIZE)
		{
			// Project the light image onto the face
			VectorSubtract(v,nearPt,nearToVert);
			
			// Get our texture coordinates
			ins->atencoords[usedattencoords][0] = DotProduct(nearToVert,s) * scale + 0.5;
			ins->atencoords[usedattencoords][1] = DotProduct(nearToVert,t) * scale + 0.5;
			usedattencoords++;
		}
	}
}

void R_SetupBrushLightHAV(entity_t *ent, brushlightinstant_t *ins)
{
	vec3_t lightDir,H, lightOr;
	glpoly_t *poly;
	int		i,j,usedts;
	float	*v;
	msurface_t *psurf;

	
	VectorCopy(ins->lightpos,lightOr);
	psurf = &ent->model->surfaces[ent->model->firstmodelsurface];
	usedts = 0;
	for (i=0 ; i<ent->model->nummodelsurfaces ; i++, psurf++)
	{
		if (!ins->polygonVis[i])
			continue;

		poly = psurf->polys;
		
		//v = poly->verts[0];
		v = (float *)(&globalVertexTable[poly->firstvertex]);
		for (j=0 ; j<poly->numverts ; j++, v+= VERTEXSIZE)
		{	

			VectorSubtract(lightOr,v,lightDir);

			//store tangent space light vector
			if (psurf->flags & SURF_PLANEBACK)	{
				ins->tslights[usedts][2]  = -DotProduct(lightDir,psurf->plane->normal);
			} else {
				ins->tslights[usedts][2]  = DotProduct(lightDir,psurf->plane->normal);
			}
			ins->tslights[usedts][1] = -DotProduct(lightDir,psurf->texinfo->vecs[1]);
			ins->tslights[usedts][0]  = DotProduct(lightDir,psurf->texinfo->vecs[0]);


			VectorNormalize(lightDir); 
			//object_vieworg = camera position in object space
			VectorSubtract(ins->vieworg,v,H);
			VectorNormalize(H);
			VectorAdd(lightDir,H,H);

			//store tangent space half angle
			if (psurf->flags & SURF_PLANEBACK)	{
				ins->tshalfangles[usedts][2] = -DotProduct(H,psurf->plane->normal);
			} else {
				ins->tshalfangles[usedts][2] = DotProduct(H,psurf->plane->normal);
			}

			ins->tshalfangles[usedts][1] = -DotProduct(H,psurf->texinfo->vecs[1]);
			ins->tshalfangles[usedts][0] = DotProduct(H,psurf->texinfo->vecs[0]);
			
			usedts++;
		}
	}
}

void R_SetupBrushInstantForLight(entity_t *e)
{
	brushlightinstant_t *brushlightinstant;
	vec3_t	mins, maxs;
	qboolean	update;

	brushlightinstant = R_AllocateBrushLightInstant(e);

	VectorAdd (e->origin,e->model->mins, mins);
	VectorAdd (e->origin,e->model->maxs, maxs);

	if (R_CullBox (mins, maxs))
		brushlightinstant->shadowonly = true;
	else
		brushlightinstant->shadowonly = false;

	e->brushlightinstant = brushlightinstant;

	R_SetupBrushObjectSpace(e, brushlightinstant);

	update = CheckBrushLightUpdate(e, brushlightinstant);
	if (update)
	{
		R_CalcBrushVolumeVerts(e, brushlightinstant);
		
		if (!brushlightinstant->shadowonly) {
			if ( gl_cardtype == GENERIC || gl_cardtype == GEFORCE ) {//PA:
				R_CalcBrushAttenCoords(e,  brushlightinstant);
			}
			//R_SetupBrushLightHAV(e, brushlightinstant);
		}

		//make sure that we can compare the next frame
		VectorCopy(e->origin, brushlightinstant->lasteorg);
		VectorCopy(currentshadowlight->origin, brushlightinstant->lastlorg);
		VectorCopy(e->angles, brushlightinstant->lasteangles);
		brushlightinstant->lastlradius = currentshadowlight->radius;
		brushlightinstant->lastlight = currentshadowlight;
		brushlightinstant->lastent = e;
		brushlightinstant->lastshadowonly = brushlightinstant->shadowonly;
	}

	brushCacheRequests++;
	//Half angles only change when the viewer changes his position
	//this happens a lot so recalculate only this.
	if ((update) || CheckBrushHalfAngle(brushlightinstant)) {
		if (!brushlightinstant->shadowonly)
			R_SetupBrushLightHAV(e, brushlightinstant);
		VectorCopy(r_refdef.vieworg,brushlightinstant->lastvorg);

		if(!update) brushPartialCacheHits++;
	} else {
		brushFullCacheHits++;
	}

	//lock it for this frame
	brushlightinstant->lockframe = r_framecount;
}
