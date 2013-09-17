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


Adds decal support...
Decals are similar to particles currently...
The code to generate them is based on the article "Applying decals to arbitrary surfaces"
by "Eric Lengyel" in "Game Programming Gems 2"
*/

#include "quakedef.h"
#include "r_local.h"

#define MAX_DECALS			1024	// default max # of decals at one
										//  time
#define ABSOLUTE_MIN_DECALS	0		// no fewer than this no matter what's
										//  on the command line


decal_t	*active_decals, *free_decals;

decal_t	*decals;
int			r_numdecals;

void DecalClipLeaf(decal_t *dec, mleaf_t *leaf);
void DecalWalkBsp_R(decal_t *dec, mnode_t *node);
int DecalClipPolygonAgainstPlane(plane_t *plane, int vertexCount, vec3_t *vertex, vec3_t *newVertex);
int DecalClipPolygon(int vertexCount, vec3_t *vertices, vec3_t *newVertex);

/*
===============
R_InitDecals
===============
*/
void R_InitDecals (void)
{
	int		i;

	i = COM_CheckParm ("-decals");

	if (i)
	{
		r_numdecals = (int)(Q_atoi(com_argv[i+1]));
		if (r_numdecals < ABSOLUTE_MIN_DECALS)
			r_numdecals = ABSOLUTE_MIN_DECALS;
	}
	else
	{
		r_numdecals = MAX_DECALS;
	}

	decals = (decal_t *)
			Hunk_AllocName (r_numdecals * sizeof(decal_t), "decals");
}

/*
===============
R_ClearDecals
===============
*/
void R_ClearDecals (void)
{
	int		i;
	
	free_decals = &decals[0];
	active_decals = NULL;

	for (i=0 ;i<r_numdecals ; i++)
		decals[i].next = &decals[i+1];
	decals[r_numdecals-1].next = NULL;
}

static plane_t leftPlane, rightPlane, bottomPlane, topPlane, backPlane, frontPlane;

void R_SpawnDecal(vec3_t center, vec3_t normal, vec3_t tangent,  ParticleEffect_t *effect)
{

	float width, height, depth, d;
	vec3_t binormal;
	decal_t	*dec;
	float one_over_w, one_over_h;
	int	a;
	vec3_t test = {0.5, 0.5, 0.5};
//	vec3_t t;

	//allocate decal
	if (!free_decals)
		return;
	dec = free_decals;
	free_decals = dec->next;
	dec->next = active_decals;
	active_decals = dec;

	//VectorNormalize(tangent);

	VectorNormalize(test);
	CrossProduct(normal,test,tangent);

	//Con_Printf("StartDecail\n");
	VectorCopy(center, dec->origin);
	VectorCopy(tangent, dec->tangent);
	VectorCopy(normal, dec->normal);

	VectorNormalize(tangent);
	VectorNormalize(normal);
	CrossProduct(normal, tangent, binormal);
	VectorNormalize(binormal);
	
	width = RandomMinMax(effect->sizemin, effect->sizemax);
	height = width;
	depth = width*0.5;
	dec->radius = max(max(width,height),depth);
	dec->texture = effect->texture;
	dec->lifetime = RandomMinMax(effect->lifemin, effect->lifemax);
	dec->die = cl.time + dec->lifetime;
	
	dec->startcolor[0] = RandomMinMax(effect->startcolormin[0], effect->startcolormax[0]);
	dec->startcolor[1] = RandomMinMax(effect->startcolormin[1], effect->startcolormax[1]);
	dec->startcolor[2] = RandomMinMax(effect->startcolormin[2], effect->startcolormax[2]);
	dec->endcolor[0] = RandomMinMax(effect->endcolormin[0], effect->endcolormax[0]);
	dec->endcolor[1] = RandomMinMax(effect->endcolormin[1], effect->endcolormax[1]);
	dec->endcolor[2] = RandomMinMax(effect->endcolormin[2], effect->endcolormax[2]);

	dec->srcblend = effect->srcblend;
	dec->dstblend = effect->dstblend;

/*	
	-= Hack: don't clip decal just draw a quad =-	

	VectorScale(binormal,10,binormal);
	VectorScale(tangent,10,tangent);
	VectorCopy(center,dec->vertexArray[0]);
	VectorAdd(center,tangent,t);
	VectorCopy(t,dec->vertexArray[1]);
	VectorAdd(t,binormal,t);
	VectorCopy(t,dec->vertexArray[2]);
	VectorAdd(center,binormal,t);
	VectorCopy(t,dec->vertexArray[3]);

	dec->vertexCount = 4;
	dec->triangleCount = 2;

	dec->triangleArray[0][0] = 0;
	dec->triangleArray[0][1] = 1;
	dec->triangleArray[0][2] = 2;

	dec->triangleArray[1][0] = 0;
	dec->triangleArray[1][1] = 2;
	dec->triangleArray[1][2] = 3;	
*/

	// Calculate boundary planes
	d = DotProduct(center, tangent);
	VectorCopy(tangent,leftPlane.normal);
	leftPlane.dist = -(width * 0.5 - d);
	VectorInverse(tangent);
	VectorCopy(tangent,rightPlane.normal);
	VectorInverse(tangent);
	rightPlane.dist = -(width * 0.5 + d);
	
	d = DotProduct(center, binormal);
	VectorCopy(binormal,bottomPlane.normal);
	bottomPlane.dist = -(height * 0.5 - d);
	VectorInverse(binormal);
	VectorCopy(binormal,topPlane.normal);
	VectorInverse(binormal);
	topPlane.dist = -(height * 0.5 + d);
	
	d = DotProduct(center, normal);
	VectorCopy(normal,backPlane.normal);
	backPlane.dist = -(depth - d);
	VectorInverse(normal);
	VectorCopy(normal,frontPlane.normal);
	VectorInverse(normal);
	frontPlane.dist = -(depth + d);

	// Begin with empty mesh
	dec->vertexCount = 0;
	dec->triangleCount = 0;
	
	//Clip decal to bsp
	DecalWalkBsp_R(dec, cl.worldmodel->nodes);

	//This happens when a decal is to far from any surface or the surface is to steeply sloped
	if (dec->triangleCount == 0) {
		//deallocate decal
		active_decals = dec->next;
		dec->next = free_decals;
		free_decals = dec;	
		return;
	}

	//Assign texture mapping coordinates
	one_over_w  = 1.0F / width;
	one_over_h = 1.0F / height;
	for (a = 0; a < dec->vertexCount; a++)
	{
		float s, t;
		vec3_t v;
		VectorSubtract(dec->vertexArray[a],center,v);
		s = DotProduct(v, tangent) * one_over_w + 0.5F;
		t = DotProduct(v, binormal) * one_over_h + 0.5F;
		dec->texcoordArray[a][0] = s;
		dec->texcoordArray[a][1] = t;
	}
}

void DecalWalkBsp_R(decal_t *dec, mnode_t *node)
{
	mplane_t	*plane;
	float		dist;
	mleaf_t		*leaf;

	if (node->contents < 0) {

		//we are in a leaf
		leaf = (mleaf_t *)node;
		DecalClipLeaf(dec, leaf);
		return;
	}

	plane = node->plane;
	dist = DotProduct (dec->origin, plane->normal) - plane->dist;
	
	if (dist > dec->radius)
	{
		DecalWalkBsp_R (dec, node->children[0]);
		return;
	}
	if (dist < -dec->radius)
	{
		DecalWalkBsp_R (dec, node->children[1]);
		return;
	}

	DecalWalkBsp_R (dec, node->children[0]);
	DecalWalkBsp_R (dec, node->children[1]);
}

qboolean DecalAddPolygon(decal_t *dec, int vertcount, vec3_t *vertices)
{
	int *triangle;
	int count;
	int a, b;
//	float f;

	//Con_Printf("AddPolygon %i %i\n",vertcount, dec->vertexCount);
	count = dec->vertexCount;
	if (count + vertcount >= MAX_DECAL_VERTICES)
		return false;
	
	if (dec->triangleCount + vertcount-2 >= MAX_DECAL_TRIANGLES)
		return false;

	// Add polygon as a triangle fan
	triangle = &dec->triangleArray[dec->triangleCount][0];
	for (a = 2; a < vertcount; a++)
	{
		dec->triangleArray[dec->triangleCount][0] = count;
		dec->triangleArray[dec->triangleCount][1] = (count + a - 1);
		dec->triangleArray[dec->triangleCount][2] = (count + a );
		dec->triangleCount++;
	}
	
	// Assign vertex colors
	for (b = 0; b < vertcount; b++)
	{
		VectorCopy(vertices[b],dec->vertexArray[count]);
		count++;
	}
	
	dec->vertexCount = count;
	return true;
}

const float decalEpsilon = 0.001;

void DecalClipLeaf(decal_t *dec, mleaf_t *leaf)
{
//	int a;
	vec3_t		newVertex[64] /* , t1, t2 */, t3;
	int	c;
	msurface_t **surf;

	//Con_Printf("Clipleaf\n");
	c = leaf->nummarksurfaces;
	surf = leaf->firstmarksurface;

	//for all surfaces in the leaf
	for (c=0; c<leaf->nummarksurfaces; c++, surf++) {

		glpoly_t *poly;
		int	i,count;
		float *v;
	
		poly = (*surf)->polys;

		v = (float *)(&globalVertexTable[poly->firstvertex]);
		for (i=0 ; i<poly->numverts ; i++, v+= VERTEXSIZE)
		{
			newVertex[i][0] = v[0];
			newVertex[i][1] = v[1];
			newVertex[i][2] = v[2];
		}

		VectorCopy((*surf)->plane->normal,t3);		
		
		if ((*surf)->flags & SURF_PLANEBACK) {
			VectorInverse(t3);
		}
		

		//avoid backfacing and ortogonal facing faces to recieve decal parts
		if (DotProduct(dec->normal, t3) > decalEpsilon)
		{
			count = DecalClipPolygon(poly->numverts, newVertex, newVertex);
			if ((count != 0) && (!DecalAddPolygon(dec, count, newVertex))) break;
		}
	}
}

int DecalClipPolygon(int vertexCount, vec3_t *vertices, vec3_t *newVertex)
{
	vec3_t		tempVertex[64];
	
	// Clip against all six planes
	int count = DecalClipPolygonAgainstPlane(&leftPlane, vertexCount, vertices, tempVertex);
	if (count != 0)
	{
		count = DecalClipPolygonAgainstPlane(&rightPlane, count, tempVertex, newVertex);
		if (count != 0)
		{
			count = DecalClipPolygonAgainstPlane(&bottomPlane, count, newVertex, tempVertex);
			if (count != 0)
			{
				count = DecalClipPolygonAgainstPlane(&topPlane, count, tempVertex, newVertex);
				if (count != 0)
				{
					count = DecalClipPolygonAgainstPlane(&backPlane, count, newVertex, tempVertex);
					if (count != 0)
					{
						count = DecalClipPolygonAgainstPlane(&frontPlane, count, tempVertex, newVertex);
					}
				}
			}
		}
	}
	
	return count;
}

int DecalClipPolygonAgainstPlane(plane_t *plane, int vertexCount, vec3_t *vertex, vec3_t *newVertex)
{
	qboolean	negative[65];
	int	a, count, b, c;
//	float sect;
	float t;
//	vec3_t v;
	vec3_t v1, v2;

	// Classify vertices
	int negativeCount = 0;
	for (a = 0; a < vertexCount; a++)
	{
		qboolean neg = ((DotProduct(plane->normal, vertex[a]) - plane->dist) < 0.0);
		negative[a] = neg;
		negativeCount += neg;
	}
	
	// Discard this polygon if it's completely culled
	if (negativeCount == vertexCount) {
		return (0);
	}

	count = 0;
	for (b = 0; b < vertexCount; b++)
	{
		// c is the index of the previous vertex
		c = (b != 0) ? b - 1 : vertexCount - 1;
		
		if (negative[b])
		{
			if (!negative[c])
			{
				// Current vertex is on negative side of plane,
				// but previous vertex is on positive side.
				VectorCopy(vertex[c],v1);
				VectorCopy(vertex[b],v2);

				t = (DotProduct(plane->normal, v1) - plane->dist) / (
					  plane->normal[0] * (v1[0] - v2[0])
					+ plane->normal[1] * (v1[1] - v2[1]) 
					+ plane->normal[2] * (v1[2] - v2[2]));
				
				VectorScale(v1,(1.0 - t),newVertex[count]);
				VectorMA(newVertex[count],t,v2,newVertex[count]);
				
				count++;
			}
		}
		else
		{
			if (negative[c])
			{
				// Current vertex is on positive side of plane,
				// but previous vertex is on negative side.
				VectorCopy(vertex[b],v1);
				VectorCopy(vertex[c],v2);

				t = (DotProduct(plane->normal, v1) - plane->dist) / (
					  plane->normal[0] * (v1[0] - v2[0])
					+ plane->normal[1] * (v1[1] - v2[1])
					+ plane->normal[2] * (v1[2] - v2[2]));

				
				VectorScale(v1,(1.0 - t),newVertex[count]);
				VectorMA(newVertex[count],t,v2,newVertex[count]);
				
				count++;
			}
			
			// Include current vertex
			VectorCopy(vertex[b],newVertex[count]);
			count++;
		}
	}
	
	// Return number of vertices in clipped polygon
	return count;
}

/*
===============
R_DrawDecails
===============
*/
extern	cvar_t	sv_gravity;

void R_DrawDecals (void)
{
	decal_t		*p, *kill;
	int				i;
	float			blend, blend1;
	
	glFogfv(GL_FOG_COLOR, color_black);
	glEnable (GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER,0.000);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDepthMask(0);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1,-1);

	for ( ;; ) 
	{
		kill = active_decals;
		if (kill && kill->die < cl.time)
		{
			active_decals = kill->next;
			kill->next = free_decals;
			free_decals = kill;
			continue;
		}
		break;
	}

	for (p=active_decals ; p ; p=p->next)
	{
		for ( ;; )
		{
			kill = p->next;
			//XYZ
			if (kill && (kill->die < cl.time))
			{
				p->next = kill->next;
				kill->next = free_decals;
				free_decals = kill;
				continue;
			}
			break;
		}

		GL_Bind(p->texture);
		glBlendFunc (p->srcblend, p->dstblend);
		


		//calculate color based on life ...
		blend = (p->die-cl.time)/p->lifetime;
		blend1 = 1-blend;
		for (i=0; i<3; i++) {
			p->color[i] = p->startcolor[i] * blend + p->endcolor[i] * blend1;
		}

		if ((p->die - cl.time) < 0.5) {
			float scale = 2*(p->die - cl.time);
			glColor4f((p->color[0]*scale), (p->color[1]*scale), (p->color[2]*scale), scale);
		} else {
			glColor3fv(&p->color[0]);
		}


		//Con_Printf("Drawdec %i\n",p->triangleCount);
		for (i=0; i<p->triangleCount; i++) {
			int i1, i2, i3;
			i1 = p->triangleArray[i][0];
			i2 = p->triangleArray[i][1];
			i3 = p->triangleArray[i][2];

			glBegin(GL_TRIANGLES);
			glTexCoord2fv(&p->texcoordArray[i1][0]);
			glVertex3fv(&p->vertexArray[i1][0]);

			glTexCoord2fv(&p->texcoordArray[i2][0]);
			glVertex3fv(&p->vertexArray[i2][0]);

			glTexCoord2fv(&p->texcoordArray[i3][0]);
			glVertex3fv(&p->vertexArray[i3][0]);
			glEnd();
		}

	
	}
	glDisable(GL_POLYGON_OFFSET_FILL);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(1);
	glDisable (GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER,0.666);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glFogfv(GL_FOG_COLOR, fog_color);
}

