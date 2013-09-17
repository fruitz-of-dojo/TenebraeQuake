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

// gl_svbsp.c shadow volume bsp

#include "quakedef.h"

int svBsp_NumCutPolys;
int svBsp_NumKeptPolys;
int svBsp_NumAddedPolys;
int svBsp_NumMissedPlanes;
int svBsp_NumMissedNodes;

//FIXME: Decent allocation

//1Meg plane pool :)
#define MAX_PLANE_POOL 65536/2

#define MAX_NODE_POOL 65536

plane_t PlanePool[MAX_PLANE_POOL];
int planesusedpool;

svnode_t NodePool[MAX_NODE_POOL];
int nodesusedpool;

//Returns an plane, tries to find the same one as tryplane, if a match if found it return the found one.
plane_t *AllocPlane(plane_t *tryplane) {

	if (tryplane) {
		int i;
		//try to fit one that fits
		for (i=0; i<planesusedpool; i++) {
			if ((PlanePool[i].normal[0] == tryplane->normal[0]) &&
				(PlanePool[i].normal[1] == tryplane->normal[1]) &&
				(PlanePool[i].normal[2] == tryplane->normal[2]) &&
				(PlanePool[i].dist == tryplane->dist)) {
				return &PlanePool[i];
			}
		}
	}

	if (planesusedpool >= MAX_PLANE_POOL) {
		Con_Printf("Too many planes...");
		svBsp_NumMissedPlanes++;
		return NULL;
	}
	planesusedpool++;
	return &PlanePool[planesusedpool-1];
}

svnode_t *AllocNode(void) {
	if (nodesusedpool >= MAX_NODE_POOL) {
		Con_Printf("Too many nodes...");
		svBsp_NumMissedNodes++;
		return NULL;
	}
	nodesusedpool++;
	return &NodePool[nodesusedpool-1];
}


int Epsilon_Sign(float value) {
	if (value < -ON_EPSILON) return 1;
	else if (value > ON_EPSILON) return 2;
	else return 0; 
}

/*
=============
R_CreateEmptyTree

Do some tricks to make sure we have a 360 deg. field of view.
This routine destroys the last svbsp the program has generated.

         plane
         /   \ 
      -plane  open
       /  \
   solid   open

=============
*/

svnode_t *R_CreateEmptyTree(void) {

	plane_t *plane1;
	plane_t *plane2;
	svnode_t *node1;
	svnode_t *node2;

	planesusedpool = 0;
	nodesusedpool = 0;
	svBsp_NumMissedPlanes = 0;
	svBsp_NumMissedNodes = 0;

	plane1 = AllocPlane (NULL);
	plane2 = AllocPlane (NULL);
	node1 = AllocNode ();
	node2 = AllocNode ();

	//all planes go trough the light origin
	plane1->normal[0] = 1;
	plane1->normal[1] = 0;
	plane1->normal[2] = 0;
	plane1->dist = DotProduct (currentshadowlight->origin, plane1->normal);

	plane2->normal[0] = -1;
	plane2->normal[1] = 0;
	plane2->normal[2] = 0;
	plane2->dist = DotProduct (currentshadowlight->origin, plane2->normal);


	node1->splitplane = plane1;
	node1->children[0] = node2;
	node1->children[1] = NULL;

	node2->splitplane = plane2;
	node2->children[0] = NULL;
	node2->children[1] = NULL;

	return node1;
}

#define MAX_POLY_VERT 32

void SplitPolygon(vec3_t *polygon,int *signs, int vnum, plane_t *plane, vec3_t *inpts, int *innum, vec3_t *outpts, int *outnum) {

 	int out_c = 0;
   	int in_c = 0;
   		
   	vec3_t *ptA = &polygon[vnum-1];
	vec3_t *ptB, v, newVert;
   	int sideA = signs[vnum-1];
	int sideB;
	int i;
	float sect;

	for (i=0;  i<vnum; i++) {

		ptB = &polygon[i];
		sideB = signs[i];

		//is b on "right side"
		if (sideB == 2) {

			if (sideA == 1) {
	
				// compute the intersection point of the line
				// from point A to point B with the partition
				// plane. This is a simple ray-plane intersection.
				VectorSubtract ((*ptB), (*ptA), v);
				sect = - (DotProduct (plane->normal, (*ptA) )-plane->dist) / 
								DotProduct (plane->normal, v);
				VectorScale (v,sect,v);
            
				//add a new vertex
				VectorAdd ((*ptA), v, newVert);
				VectorCopy (newVert, inpts[in_c]);
				VectorCopy (newVert, outpts[out_c]);

				out_c++;
				in_c++;
			}
			VectorCopy (polygon[i], outpts[out_c]);
			out_c++;
		}
		//b is on "left" side
		else if (sideB ==1) {

			if (sideA == 2) {

				// compute the intersection point of the line
				// from point A to point B with the partition
				// plane. This is a simple ray-plane intersection.
				VectorSubtract ((*ptB), (*ptA), v);
				sect = - (DotProduct (plane->normal, (*ptA) )-plane->dist) /
								DotProduct (plane->normal, v);
				VectorScale (v,sect,v);
            
				//add a new vertex
				VectorAdd ((*ptA), v, newVert);
				VectorCopy (newVert, inpts[in_c]);
				VectorCopy (newVert, outpts[out_c]);

				out_c++;
				in_c++;
			}
			VectorCopy (polygon[i], inpts[in_c]);
			in_c++;
		}
		//b is almost on plane
		else {
      
			VectorCopy (polygon[i], inpts[in_c]);
			VectorCopy (inpts[in_c], outpts[out_c]);
			in_c++;
			out_c++;
		}
		ptA = ptB;
		sideA = sideB;

		if ((out_c > MAX_POLY_VERT) || (in_c > MAX_POLY_VERT)) {
			Con_Printf ("MAX_POLY_VERT exceeded: %i %i\n", in_c, out_c);

			//just return what we've got
			(*innum) = in_c;
			(*outnum) = out_c;
			return;
		}
	}

	(*innum) = in_c;
	(*outnum) = out_c;
}

/*
=============
R_AddShadowCaster

Polygons must be added in front to back order!
=============
*/
svnode_t *R_AddShadowCaster(svnode_t *node, vec3_t *v, int vnum, msurface_t *surf,int depth) {

	int sign;
	int	signs[MAX_POLY_VERT],signs2[MAX_POLY_VERT];
	vec3_t v1[MAX_POLY_VERT],v2[MAX_POLY_VERT];
	int vnum1,vnum2;
	int i;

	if (depth > 1500) {
		Con_Printf("to deep\n");
		return NULL;
	}

	if (vnum == 0) return NULL;


	sign = 0;
	for (i=0; i<vnum; i++) {
		sign |= signs[i] = Epsilon_Sign (DotProduct (v[i], node->splitplane->normal)- node->splitplane->dist);
	}

	if (sign == 1) {

		if (node->children[0] != NULL) {

			R_AddShadowCaster (node->children[0], v, vnum, surf, depth+1); 
		} else {

			svBsp_NumCutPolys++;
		}


	} else if (sign == 2) {


		if (node->children[1] != NULL) {


			R_AddShadowCaster (node->children[1], v, vnum, surf, depth+1); 
		} else {

			node->children[1] = ExpandVolume(v, signs, vnum, surf);

			if (surf->visframe != r_lightTimestamp) {
				//Store it out as visible
				surf->shadowchain = shadowchain;
				surf->visframe = r_lightTimestamp;
				surf->polys->lightTimestamp = r_lightTimestamp;
				shadowchain = surf;
				svBsp_NumKeptPolys++;
			}
		}

	} else if (sign == 3) {


		SplitPolygon(&v[0], &signs[0], vnum, node->splitplane, &v1[0], &vnum1, &v2[0], &vnum2);

		if (node->children[0] != NULL) {

			R_AddShadowCaster (node->children[0], v1, vnum1, surf, depth+1);

		} else {

			svBsp_NumCutPolys++;

		}


		if (vnum2 == 0) return NULL;

		if (node->children[1] != NULL) {

			R_AddShadowCaster (node->children[1], v2, vnum2, surf, depth+1);
		} else {

			node->children[1] = ExpandVolume (v2, signs2, vnum2, surf);

			if (surf->visframe != r_lightTimestamp) {
				//Store it out as visible
				surf->shadowchain = shadowchain;
				surf->visframe = r_lightTimestamp;
				surf->polys->lightTimestamp = r_lightTimestamp;
				shadowchain = surf;
				svBsp_NumKeptPolys++;
			}
		}
	}
	return NULL;
}

/*
=============
ExpandVolume

=============
*/
svnode_t *ExpandVolume(vec3_t *v,int *sign, int vnum, msurface_t *surf) {

	svnode_t	*res, *currnode;
	int		i;

#ifdef SHADOW_DEBUG
	Con_Printf ("expand volume %i\n",vnum);
#endif

	if (vnum == 0) return NULL;

	res = NodeFromEdge (v, vnum, 0);
	if (!res) return NULL;

	currnode = res;
	for (i=1; i<vnum; i++) {
		currnode->children[0] = NodeFromEdge (v, vnum, i);
		if (currnode->children[0] == NULL) break;
		currnode = currnode->children[0];
	}
	//Con_Printf ("expand done");

	return res;
}

/*
=============
NodeFromEdge
=============
*/

svnode_t *NodeFromEdge(vec3_t *v, int vnum, int edgeindex) {

	vec3_t		*v1,*v2;
	vec3_t		vert1,vert2,normal;
	vec_t		dist;
	plane_t		*plane, tryplane;
	svnode_t	*res;

	//Extract one vertex so we can calultate the normal of this shadow plane
	v1 = &v[edgeindex];
	v2 = &v[(edgeindex+1)% vnum];
	VectorSubtract ((*v1), currentshadowlight->origin, vert1);

	VectorSubtract ((*v1), (*v2), vert2);

	CrossProduct (vert1, vert2, normal);
	VectorNormalize (normal);

	//VectorInverse(normal);
	dist = DotProduct(normal, currentshadowlight->origin);

	//make a node with it
	VectorCopy (normal, tryplane.normal);
	tryplane.dist = dist;

	plane = AllocPlane (&tryplane);
	if (!plane) return NULL;

	VectorCopy (normal, plane->normal);
	plane->dist = dist;

	res = AllocNode ();
	if (!res) {
		return res;
	}

	res->splitplane = plane;
	
	res->children[0] = NULL;
	res->children[1] = NULL;

	return res;
}





















/*

  Code scrap yard

*/


/*
==================
FaceSide

PENTA: From qbsp code in "solidbsp.c" and adapted a bit.
==================
*/
/*
int FaceSide (msurface_t *in, mplane_t *split)
{
	int		frontcount, backcount;
	vec_t	dot;
	int		i;
	vec_t	*p;
	
	frontcount = backcount = 0;
	
// axial planes are fast
	if (split->type < 3)
		for (i=0, p = in->polys->verts[0]+split->type ; i<in->numedges ; i++, p+=VERTEXSIZE)
		{
			if (*p > split->dist + ON_EPSILON)
			{
				if (backcount)
					return SIDE_ON;
				frontcount = 1;
			}
			else if (*p < split->dist - ON_EPSILON)
			{
				if (frontcount)
					return SIDE_ON;
				backcount = 1;
			}
		}
	else	
// sloping planes take longer
		for (i=0, p = in->polys->verts[0] ; i<in->numedges ; i++, p+=VERTEXSIZE)
		{
			dot = DotProduct (p, split->normal);
			dot -= split->dist;
			if (dot > ON_EPSILON)
			{
				if (backcount)
					return SIDE_ON;
				frontcount = 1;
			}
			else if (dot < -ON_EPSILON)
			{
				if (frontcount)
					return SIDE_ON;
				backcount = 1;
			}
		}
	
	if (!frontcount)
		return SIDE_BACK;
	if (!backcount)
		return SIDE_FRONT;
	
	return SIDE_ON;
}

*/


/*svnode_t *R_AddShadowCaster(svnode_t *node, msurface_t *surf) {

	int side;
	
	if (node == &OUT_NODE) {
		//Polygon casts a shadow add the volume to the svbsp
		return ExpandVolumeBsp(surf);

	} else if (node == &IN_NODE) {
		//Polygons is obscured by others
		return node;

	} else {

		side = FaceSide(surf,node->splitplane);

		if (side == SIDE_FRONT) {

			node->children[0] = R_AddShadowCaster(node->children[0],surf);
		}
		else if (side == SIDE_BACK) {

			node->children[1] = R_AddShadowCaster(node->children[1],surf);
		}
		else {

			node->children[0] = R_AddShadowCaster(node->children[0],surf);
			node->children[1] = R_AddShadowCaster(node->children[1],surf);
		}
	}

}
*/

	/*

int GetLeftVerts(int *signs,vec3_t *v,int num,vec3_d *dest,int *destnum) {

	//find first 0 or 1
	while (!((signs[i] == 0) || (signs[i] == 1))) {
		i++;
		//none found
		if (i == num) return 0;
	}

	//find consecutive 0's and 1's
	j = i;
	while ((signs[j] == 0) || (signs[j] == 1)) {
		VectorCopy(v[j],dest[j]);
		j++;
		//all found
		if (j == num) return num;
	}

	//include final 2's
	if (signs[j-1] == 1) {
		if (signs[j] == 2) {
			VectorCopy(v[j],dest[j]);
		} else Con_Printf("strange stuff...");
	}

	if (signs[i] == 1) {
		if (signs[i-1] == 2) {
			VectorCopy(v[i-1],dest[j+1]);
		} else Con_Printf("strange stuff...");
	}
	*/