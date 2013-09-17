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

Alias instants

 There are 2 types of alias instants Frame & Light.
 -Frame is the instant that is used per frame it contains light independent information like
 vertices, tangent space, normals ....
 -Light is the instant that is used per light, vertex light directions, shadow volume vertices,
 half angle vectors ....

 The instants are in a basic cache system, every frame an entity gets an instant and is guaranteed to
 have it for the rest of the frame.
 The next frame we look if we can reuse the instant of the previous frame if this is the case we don't
 recalculate stuff otherwise we calculate everything.  So it caches interpolated
 vertices & shadowvolumes  between frames.

*/

#include "quakedef.h"

#define NUM_ALIAS_INSTANTS 64

#define NUM_ALIAS_LIGHT_INSTANTS 196

aliasframeinstant_t InstantCache[NUM_ALIAS_INSTANTS];
aliaslightinstant_t LightInstantCache[NUM_ALIAS_LIGHT_INSTANTS];

void R_SetupInstantForFrame(entity_t *e, qboolean forcevis);


#define DIST_DELTA 0.1
#define ANG_DELTA 0.5
#define RADIUS_DELTA 0.1
#define BLEND_DELTA 0.0000001 //This lets the interpolation run at ~30fps this looks verry smooth
						   //and makes our caches usefull
							//BLEND_DELTA = 1 means 10 fps means no interpolation visible

int aliasCacheRequests, aliasFullCacheHits, aliasPartialCacheHits;

/*
R_AllocateInstant
*/
aliasframeinstant_t *R_AllocateInstant(entity_t *e, aliasframeinstant_t *frameinstant, aliashdr_t *paliashdr) {

	int i, oldest, oindex;
	
        /* is this frameinstant ok ? */
        if (frameinstant)
             if ((frameinstant->paliashdr == paliashdr)
                 && (frameinstant->lastent == e))
                  return frameinstant;        

	//none found, bacause we don't want to destroy the reclaiming of other ents
	//we use the oldest used instant
	oldest = r_framecount;
	oindex = -1;
	for (i=0; i<NUM_ALIAS_INSTANTS; i++) {
		if (InstantCache[i].lockframe < oldest) {
			oldest = InstantCache[i].lockframe;
			oindex = i;
		}
	}

	if (oindex == -1) {
		//we didn't find a free one for this frame
		return NULL;
	}

	return &InstantCache[oindex];
}

/*
R_AllocateInstant
*/
aliaslightinstant_t *R_AllocateLightInstant(entity_t *e, aliashdr_t * paliashdr) {

	int i, oldest, oindex;
	/*
	if (InstantsUsed < NUM_ALIAS_INSTANTS ) {
		return &InstantCache[InstantsUsed++];
	} else {
		return NULL;
	}
	*/
	
	//try to reclaim an instant that was previously used for this surface and this light
	for (i=0; i<NUM_ALIAS_LIGHT_INSTANTS; i++) {
		if ((LightInstantCache[i].lastent == e) && (LightInstantCache[i].lastlight == currentshadowlight) && (LightInstantCache[i].lasthdr == paliashdr))
			return &LightInstantCache[i];
	}

	//none found, bacause we don't want to destroy the reclaiming of other ents
	//we use the oldest used instant
	oldest = r_framecount;
	oindex = -1;
	for (i=0; i<NUM_ALIAS_LIGHT_INSTANTS; i++) {
		if (LightInstantCache[i].lockframe < oldest) {
			oldest = LightInstantCache[i].lockframe;
			oindex = i;
		}
	}

	if (oindex == -1) {
		//find an instant of an other light
		for (i=0; i<NUM_ALIAS_LIGHT_INSTANTS; i++) {
			if (LightInstantCache[i].lastlight != currentshadowlight)
				return &LightInstantCache[i];
		}
	}

	LightInstantCache[oindex].lastent = NULL; 
	LightInstantCache[oindex].lastframeinstant = NULL; 
	LightInstantCache[oindex].lastlight = NULL; 

	return &LightInstantCache[oindex];
}

/*
R_ClearInstants
*/
void R_ClearInstantCaches() {

	int i;

	for (i=0; i<NUM_ALIAS_INSTANTS; i++) {
		InstantCache[i].lockframe = 0;
		InstantCache[i].lastent = NULL;
		InstantCache[i].paliashdr = NULL;
	}

	for (i=0; i<NUM_ALIAS_LIGHT_INSTANTS; i++) {
		LightInstantCache[i].lockframe = 0;
		LightInstantCache[i].lastent = NULL;
		LightInstantCache[i].lastlight = NULL; 
		LightInstantCache[i].lastframeinstant = NULL; 
	}
}

/*
=============
R_SetupInstants
=============
*/

void R_SetupInstants ()
{
	int		i;

	if (!r_drawentities.value)
		return;

	//return;
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];
		if (currententity->model->type == mod_alias) {
			R_SetupInstantForFrame(currententity,false);
		}
	}

	if (mirror) return;

	//interpolate gun also
	if (cl.viewent.model && cl.viewent.model->type == mod_alias)
		R_SetupInstantForFrame(&cl.viewent,true);

	//for player
	if (cl_entities[cl.viewentity].model && cl_entities[cl.viewentity].model->type == mod_alias)
		R_SetupInstantForFrame(&cl_entities[cl.viewentity],true);

}


/*************************

  Methods called per frame

**************************/

/*
R_InterpolateVerts
*/
void R_InterpolateVerts(aliashdr_t *paliashdr, aliasframeinstant_t *instant, int pose1, int pose2, float blend) {
	float 		blend1;
	ftrivertx_t	*verts1;
	ftrivertx_t	*verts2;
	int			i;

	verts1 = (ftrivertx_t *) ((byte *) paliashdr + paliashdr->posedata);
	verts2 = verts1;

	verts1 += pose1 * paliashdr->poseverts;
	verts2 += pose2 * paliashdr->poseverts;

	blend1 = 1-blend;

	for (i=0; i<paliashdr->poseverts; i++) {
		//interpolate them
		instant->vertices[i][0] = verts1[i].v[0] * blend1 + verts2[i].v[0] * blend;
		instant->vertices[i][1] = verts1[i].v[1] * blend1 + verts2[i].v[1] * blend;
		instant->vertices[i][2] = verts1[i].v[2] * blend1 + verts2[i].v[2] * blend;
	}
}

/*
R_InterpolateNormals
*/
void R_InterpolateNormals(aliashdr_t *paliashdr, aliasframeinstant_t *instant, int pose1, int pose2, float blend) {
	float 		blend1, lat, lng;
	ftrivertx_t	*verts1;
	ftrivertx_t	*verts2;
	vec3_t		norm1, norm2;
	int			i;

	verts1 = (ftrivertx_t *) ((byte *) paliashdr + paliashdr->posedata);
	verts2 = verts1;

	verts1 += pose1 * paliashdr->poseverts;
	verts2 += pose2 * paliashdr->poseverts;

	blend1 = 1-blend;

	for (i=0; i<paliashdr->poseverts; i++) {

		lat = ( verts1[i].lightnormalindex >> 8 ) & 0xff;
		lng = ( verts1[i].lightnormalindex & 0xff );
		lat *= M_PI/128;
		lng *= M_PI/128;

		norm1[0] = cos(lat) * sin(lng);
		norm1[1] = sin(lat) * sin(lng);
		norm1[2] = cos(lng);

		lat = ( verts2[i].lightnormalindex >> 8 ) & 0xff;
		lng = ( verts2[i].lightnormalindex & 0xff );
		lat *= M_PI/128;
		lng *= M_PI/128;

		norm2[0] = cos(lat) * sin(lng);
		norm2[1] = sin(lat) * sin(lng);
		norm2[2] = cos(lng);

		//interpolate them
		instant->normals[i][0] = norm1[0] * blend1 + norm2[0] * blend;
		instant->normals[i][1] = norm1[1] * blend1 + norm2[1] * blend;
		instant->normals[i][2] = norm1[2] * blend1 + norm2[2] * blend;
                VectorNormalize(instant->normals[i]);
	}
}

/*
R_InterpolateTangents
*/
void R_InterpolateTangents(aliashdr_t *paliashdr, aliasframeinstant_t *instant, int pose1, int pose2, float blend) {
	float 		blend1;
	vec3_t		*verts1;
	vec3_t		*verts2;
	float		*tang1, *tang2;
	int			i;

	verts1 = (vec3_t *) ((byte *) paliashdr + paliashdr->tangents);
	verts2 = verts1;

	verts1 += pose1 * paliashdr->poseverts;
	verts2 += pose2 * paliashdr->poseverts;

	blend1 = 1-blend;

	for (i=0; i<paliashdr->poseverts; i++) {

		tang1 = (float *)&verts1[i];
		tang2 = (float *)&verts2[i];

		//interpolate them
		instant->tangents[i][0] = tang1[0] * blend1 + tang2[0] * blend;
		instant->tangents[i][1] = tang1[1] * blend1 + tang2[1] * blend;
		instant->tangents[i][2] = tang1[2] * blend1 + tang2[2] * blend;
	}
}

/*
R_InterpolateBinomials
*/
void R_InterpolateBinomials(aliashdr_t *paliashdr, aliasframeinstant_t *instant, int pose1, int pose2, float blend) {
	float 		blend1;
	vec3_t		*verts1;
	vec3_t		*verts2;
	float		*binor1, *binor2;
	int			i;

	if (paliashdr->binormals == 0) return;

	verts1 = (vec3_t *) ((byte *) paliashdr + paliashdr->binormals);
	verts2 = verts1;

	verts1 += pose1 * paliashdr->poseverts;
	verts2 += pose2 * paliashdr->poseverts;

	blend1 = 1-blend;

	for (i=0; i<paliashdr->poseverts; i++) {

		binor1 = (float *)&verts1[i];
		binor2 = (float *)&verts2[i];

		//interpolate them
		instant->binomials[i][0] = binor1[0] * blend1 + binor2[0] * blend;
		instant->binomials[i][1] = binor1[1] * blend1 + binor2[1] * blend;
		instant->binomials[i][2] = binor1[2] * blend1 + binor2[2] * blend;
	}
}

/*
R_InterpolateTriPlanes
*/
void R_InterpolateTriPlanes(aliashdr_t *paliashdr, aliasframeinstant_t *instant, int pose1, int pose2, float blend) {
	float 		blend1;
	plane_t *planes1, *planes2;
	int i;

	planes1 = (plane_t *)((byte *)paliashdr + paliashdr->planes);
	planes2 = planes1;
	planes1 += pose1 * paliashdr->numtris;
	planes2 += pose2 * paliashdr->numtris;

	blend1 = 1-blend;

	for (i=0; i<paliashdr->numtris; i++) {
		instant->triplanes[i].normal[0] = planes1[i].normal[0] * blend1 + planes2[i].normal[0] * blend;
		instant->triplanes[i].normal[1] = planes1[i].normal[1] * blend1 + planes2[i].normal[1] * blend;
		instant->triplanes[i].normal[2] = planes1[i].normal[2] * blend1 + planes2[i].normal[2] * blend;
		instant->triplanes[i].dist = planes1[i].dist * blend1 + planes2[i].dist * blend;
	}
}

/*
R_SetupLerpPoses

Lerp frame code
From QuakeForge

This determines the two frames to interpolate inbetween an calculates the blending factor

*/
void R_SetupLerpPoses(aliashdr_t *paliashdr,entity_t *e) {
	int 	frame, pose, numposes;
	float	blend;

	frame = e->frame;

	if ((frame >= paliashdr->numframes) || (frame < 0)) {
		Con_DPrintf ("R_SetupLerpPoses: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1) {
		e->frame_interval = paliashdr->frames[frame].interval;
		pose += (int) (cl.time / e->frame_interval) % numposes;
	} else {
		/*
			One tenth of a second is a good for most Quake animations. If the
			nextthink is longer then the animation is usually meant to pause
			(e.g. check out the shambler magic animation in shambler.qc).  If
			its shorter then things will still be smoothed partly, and the
			jumps will be less noticable because of the shorter time.  So,
			this is probably a good assumption.
		*/
		e->frame_interval = 0.1;
	}

	if (e->pose2 != pose) {
		e->frame_start_time = realtime;
		if (e->pose2 == -1) {
			e->pose1 = pose;
		} else {
			e->pose1 = e->pose2;
		}
		e->pose2 = pose;
		blend = 0;
	} else {
		blend = (realtime - e->frame_start_time) / e->frame_interval;
	}

	// wierd things start happening if blend passes 1
	if (cl.paused || blend > 1)
		blend = 1;

	if ((e->pose1 >= paliashdr->numposes) || (e->pose1 < 0)) {
		Con_DPrintf ("R_SetupLerpPoses: invalid pose %d\n", e->pose1);
		e->pose1 = 0;
	}


	if ((e->pose2 >= paliashdr->numposes) || (e->pose2 < 0)) {
		Con_DPrintf ("R_SetupLerpPoses: invalid pose %d\n", e->pose2);
		e->pose2 = 0;
	}

	e->blend = blend;
}

/*
	Returns true if the status has changed enough to recalculate the interpolations.
*/

qboolean CheckUpdate(entity_t *e, aliasframeinstant_t *ins) {

	if (sh_nocache.value) return true;

	if ((ins->lastent == e) &&
		(ins->lastpose1 == e->pose1) &&
		(ins->lastpose2 == e->pose2) &&
		( (fabs(ins->lastblend - e->blend) <= BLEND_DELTA) || (e->pose1 == e->pose2) ) &&
		(ins->lastshadowonly == ins->shadowonly) &&
		(ins->lastpaliashdr == ins->paliashdr) &&
		(ins->lockframe >= r_framecount-10))//XYW Don't reuse if it has been unused for a long time
	{
		return false;
	} else {
		return true;
	}
	
	//return true;
}


void R_SetupAliasInstantForFrame(entity_t *e, qboolean forcevis, aliashdr_t *paliashdr, aliasframeinstant_t *aliasframeinstant)
{
	vec3_t mins, maxs;

	if (!forcevis) {


		if (e->angles[0] || e->angles[1] || e->angles[2])
		{
			int i;
			for (i=0 ; i<3 ; i++)
			{
				mins[i] = e->origin[i] - e->model->radius;
				maxs[i] = e->origin[i] + e->model->radius;
			}
		} else {
			VectorAdd (e->origin,e->model->mins, mins);
			VectorAdd (e->origin,e->model->maxs, maxs);
		}

		if (R_CullBox (mins, maxs))
			aliasframeinstant->shadowonly = true;
		else
			aliasframeinstant->shadowonly = false;
	} else aliasframeinstant->shadowonly = false;

	aliasframeinstant->paliashdr = paliashdr;
	R_SetupLerpPoses(paliashdr, e);

	//see if an update is needed
	if (CheckUpdate(e, aliasframeinstant)) {
		R_InterpolateVerts(paliashdr, aliasframeinstant, e->pose1, e->pose2, e->blend);
		if (!aliasframeinstant->shadowonly) {
			R_InterpolateNormals(paliashdr, aliasframeinstant, e->pose1, e->pose2, e->blend);
			R_InterpolateTangents(paliashdr, aliasframeinstant, e->pose1, e->pose2, e->blend);
			R_InterpolateBinomials(paliashdr, aliasframeinstant, e->pose1, e->pose2, e->blend);
		}
          R_InterpolateTriPlanes(paliashdr, aliasframeinstant, e->pose1, e->pose2, e->blend);
		aliasframeinstant->updateframe = r_framecount;

		//make sure that we can compare the next frame
		aliasframeinstant->lastpose1 = e->pose1;
		aliasframeinstant->lastpose2 = e->pose2;
		aliasframeinstant->lastblend = e->blend;
		aliasframeinstant->lastshadowonly = aliasframeinstant->shadowonly;
		aliasframeinstant->lastent = e;
		aliasframeinstant->lastpaliashdr = aliasframeinstant->paliashdr;
	}

	//lock it for this frame
	aliasframeinstant->lockframe = r_framecount;
}


void R_SetupInstantForFrame(entity_t *e, qboolean forcevis)
{
     alias3data_t *data;
     aliashdr_t *paliashdr;
     aliasframeinstant_t *aliasframeinstant;
     aliasframeinstant_t *nextframeinstant;
     aliasframeinstant_t *prevframeinstant;
     int numsurf,maxnumsurf;


     data = (alias3data_t *)Mod_Extradata (e->model);
     maxnumsurf = data->numSurfaces;     

     /* first surface */

     paliashdr = (aliashdr_t *)((char*)data + data->ofsSurfaces[0]);
     e->aliasframeinstant = prevframeinstant = aliasframeinstant = R_AllocateInstant (e,e->aliasframeinstant,paliashdr);

     if (aliasframeinstant)
          R_SetupAliasInstantForFrame(e,forcevis,paliashdr,aliasframeinstant);
     else 
          Con_Printf("Could Not Allocate Instant\n");


     /* the other surfaces */
     for (numsurf=1;numsurf < maxnumsurf ; ++numsurf){
          
          paliashdr = (aliashdr_t *)((char*)data + data->ofsSurfaces[numsurf]);
          // follow the instant chain
          if (aliasframeinstant)
               nextframeinstant = aliasframeinstant->_next;

          aliasframeinstant = R_AllocateInstant (e,aliasframeinstant,paliashdr);

          if (!aliasframeinstant) {
               Con_Printf("Could Not Allocate Instant\n");
               continue;
          }
          prevframeinstant->_next = aliasframeinstant;
          prevframeinstant = aliasframeinstant;

          R_SetupAliasInstantForFrame(e,forcevis,paliashdr,aliasframeinstant);

          aliasframeinstant = nextframeinstant;

     } /* for paliashdr */

     //prevframeinstant->_next = NULL;      
}


/*************************

  Methods called per light

**************************/

void R_SetupObjectSpace(entity_t *e, aliaslightinstant_t *linstant) {

	matrix_4x4		transf;
	float			org[4], res[4];

	//Put light & view origin into object space
	R_WorldToObjectMatrix(e, transf);
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

void R_SetupLightHAV(aliasframeinstant_t *instant, aliaslightinstant_t *linstant) {
	int i;
	vec3_t lightDir, H, tx, ty, tz;

	for (i=0; i<instant->paliashdr->poseverts; i++) {

		//Calc binominal
		VectorCopy(instant->normals[i],tz);
		VectorCopy(instant->tangents[i],ty);
		VectorCopy(instant->binomials[i],tx);

		//Calculate local light vector and put it into tangent space
		VectorSubtract(linstant->lightpos, instant->vertices[i], lightDir);

		linstant->tslights[i][0] = DotProduct(lightDir,tx);
		linstant->tslights[i][1] = -DotProduct(lightDir,ty);
		linstant->tslights[i][2] = DotProduct(lightDir,tz); 

		//Calculate local H vector and put it into tangent space
		VectorNormalize(lightDir); 
		VectorSubtract(linstant->vieworg, instant->vertices[i], H);
		VectorNormalize(H);
		VectorAdd(lightDir,H,H);
			
		linstant->tshalfangles[i][0] = DotProduct(H,tx);
		linstant->tshalfangles[i][1] = -DotProduct(H,ty);
		linstant->tshalfangles[i][2] = DotProduct(H,tz);			
	}	
}

extern int	extrudeTimeStamp;			// <AWE> added "extern".
extern int	extrudedTimestamp[MAXALIASVERTS];	//PENTA: Temp buffer for extruded vertices
                                                        // <AWE> added "extern".

void R_CalcVolumeVerts(aliasframeinstant_t *instant, aliaslightinstant_t *linstant) {

	mtriangle_t	*tris, *triangle;
	float d, scale;
	int i, j;
	vec3_t v2, *v1;
	aliashdr_t *paliashdr = instant->paliashdr;

	tris = (mtriangle_t *)((byte *)paliashdr + paliashdr->triangles);

	extrudeTimeStamp++;

	//calculate visibility
	for (i=0; i<paliashdr->numtris; i++) {
		d = DotProduct(instant->triplanes[i].normal, linstant->lightpos) - instant->triplanes[i].dist;
		if (d > 0) 
			linstant->triangleVis[i] = true;
		else
			linstant->triangleVis[i] = false;
	}

	if (!currentshadowlight->castShadow) return;

	//extude vertices
	triangle = tris;
	for (i=0; i<paliashdr->numtris; i++, triangle++) {

		if (linstant->triangleVis[i]) {//extrude it!
			
			//for all verts
			for (j=0; j<3; j++) {

				int index = triangle->vertindex[j];

				//vertex was already extruded for another triangle?
				if (extrudedTimestamp[index] == extrudeTimeStamp) continue;
				extrudedTimestamp[index] = extrudeTimeStamp;

				v1 = &linstant->extvertices[index];

				VectorCopy(instant->vertices[index],v2);

				VectorSubtract (v2, linstant->lightpos, (*v1));
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

void R_CalcAttenColors(aliasframeinstant_t *instant, aliaslightinstant_t *linstant)
{
	vec3_t *v,	diff;
	float		distsq, fallOff;
	float		radiussq = currentshadowlight->radius*currentshadowlight->radius;
	int			i;

	for (i=0; i<instant->paliashdr->poseverts; i++) {
		v = &instant->vertices[i];
		VectorSubtract(*v,linstant->lightpos,diff);
		distsq = DotProduct(diff,diff);
		fallOff = (radiussq - distsq) / radiussq;
		if (fallOff < 0) fallOff = 0;
		fallOff *= fallOff;
		linstant->colors[i][0] = fallOff;
		linstant->colors[i][1] = fallOff;
		linstant->colors[i][2] = fallOff;
	}
}


void R_CalcIndeciesForLight(aliasframeinstant_t *instant, aliaslightinstant_t *linstant) {

	mtriangle_t	*tris;
	int i, j;
	int		*indecies;
	aliashdr_t *paliashdr = instant->paliashdr;
	indecies = (int *)((byte *)paliashdr + paliashdr->indecies);
	tris = (mtriangle_t *)((byte *)paliashdr + paliashdr->triangles);
/*
	//calculate visibility
	linstant->numtris = paliashdr->numtris;
	for (i=0; i<paliashdr->numtris; i++) {
		linstant->indecies[i*3] = indecies[i*3];
		linstant->indecies[i*3+1] = indecies[i*3+1];
		linstant->indecies[i*3+2] = indecies[i*3+2];
	}

	return;
*/

	j = 0;
	linstant->numtris = 0;
	for (i=0; i<paliashdr->numtris; i++) {
		if (!linstant->triangleVis[i]) {
			linstant->numtris++;
			linstant->indecies[j] = indecies[i*3];
			linstant->indecies[j+1] = indecies[i*3+1];
			linstant->indecies[j+2] = indecies[i*3+2];
			j+=3;
		}
	}

	if (!sh_noshadowpopping.value) return;

	//Add backfacing tris also to the list
	//and render them separately to reduce popping artefacts
	for (i=0; i<paliashdr->numtris; i++) {
		if (linstant->triangleVis[i]) {
			linstant->indecies[j] = indecies[i*3];//tris[i].vertindex[0];
			linstant->indecies[j+1] = indecies[i*3+1];//tris[i].vertindex[1];
			linstant->indecies[j+2] = indecies[i*3+2];//tris[i].vertindex[2];
			j+=3;
		}
	}
}



float dist(vec3_t v1, vec3_t v2) {
	vec3_t diff;
	VectorSubtract(v1,v2,diff);
	return Length(diff);
}

qboolean CheckLightUpdate(entity_t *e, aliashdr_t *paliashdr, aliaslightinstant_t *ins, aliasframeinstant_t *aliasframeinstant) {

	if (sh_nocache.value) return true;

	if ((ins->lastent == e) &&
		(ins->lastlight == currentshadowlight) &&
		(dist(ins->lastlorg,currentshadowlight->origin) < DIST_DELTA) &&
		(dist(ins->lasteorg,e->origin) < DIST_DELTA) &&
		(dist(ins->lasteangles,e->angles) < ANG_DELTA) &&
		(abs(ins->lastlradius - currentshadowlight->radius) <= RADIUS_DELTA) &&
		(ins->lastframeinstant == aliasframeinstant) &&
		(aliasframeinstant->updateframe != r_framecount))
	{
		return false;
	} else {
		return true;
	}
}

qboolean CheckHalfAngle(aliaslightinstant_t *ins) 
{
	if (dist(ins->lastvorg,r_refdef.vieworg) < 0.5) 
	{
		return false;
	} else {
		return true;
	}
}


void R_SetupSurfaceInstantForLight(entity_t *e,aliashdr_t *paliashdr, aliasframeinstant_t *aliasframeinstant)
{
	aliaslightinstant_t *aliaslightinstant;
	qboolean update;

	aliaslightinstant = R_AllocateLightInstant(e, paliashdr);
	aliasframeinstant->lightinstant = aliaslightinstant;

	R_SetupObjectSpace(e, aliaslightinstant);	
	
	update = CheckLightUpdate(e,paliashdr, aliaslightinstant,aliasframeinstant);
	if  (update)
	{
		R_CalcVolumeVerts(aliasframeinstant, aliaslightinstant);

		//if (!aliasframeinstant->shadowonly) {
			if ( gl_cardtype == GENERIC || gl_cardtype == GEFORCE ) {//PA:
				R_CalcAttenColors(aliasframeinstant, aliaslightinstant);
			}
			R_CalcIndeciesForLight(aliasframeinstant, aliaslightinstant);

			//make sure that we can compare the next frame
			VectorCopy(e->origin, aliaslightinstant->lasteorg);
			VectorCopy(currentshadowlight->origin, aliaslightinstant->lastlorg);
			VectorCopy(e->angles, aliaslightinstant->lasteangles);
			aliaslightinstant->lastlradius = currentshadowlight->radius;
			aliaslightinstant->lastlight = currentshadowlight;
			aliaslightinstant->lastframeinstant = aliasframeinstant;
			aliaslightinstant->lastent = e;
			aliaslightinstant->lasthdr = paliashdr;
		//}
	}

	aliasCacheRequests++;
	//Half angles only change when the viewer changes his position
	//this happens a lot so recalculate only this.
	if ((update) || CheckHalfAngle(aliaslightinstant)) {
		//if (!aliasframeinstant->shadowonly) {
			R_SetupLightHAV(aliasframeinstant, aliaslightinstant);
		//}
		VectorCopy(r_refdef.vieworg,aliaslightinstant->lastvorg);

		if(!update) aliasPartialCacheHits++;
	} else {
		aliasFullCacheHits++;
	}

	//lock it for this frame
	aliaslightinstant->lockframe = r_framecount;
}


void R_SetupInstantForLight(entity_t *e)
{
	aliasframeinstant_t *aliasframeinstant;
        alias3data_t *data;
	aliashdr_t * paliashdr;
        int i,maxnumsurf;
        

//PENTA: guard against model removed from cache
	
	data = (alias3data_t *)Mod_Extradata (e->model);
	maxnumsurf = data->numSurfaces;        
	aliasframeinstant = e->aliasframeinstant;

	for (i=0;i<maxnumsurf;++i){
             
		paliashdr = (aliashdr_t *)((char*)data + data->ofsSurfaces[i]);
	
		if (!aliasframeinstant) {
	
			Con_Printf("R_SetupInstantForLight: missing instant for %s\n",e->model->name); 
			//r_cache_thrash = true; 
			return;                  
		}

		if (aliasframeinstant->paliashdr != paliashdr) {
			Con_Printf("R_SetupInstantForLight: Model was moved during frame, this is caused by not having enough heap memory.\n");
			aliasframeinstant->paliashdr = paliashdr;
		}


		R_SetupSurfaceInstantForLight(e, paliashdr, aliasframeinstant);
		aliasframeinstant = aliasframeinstant->_next;
	}    
}
