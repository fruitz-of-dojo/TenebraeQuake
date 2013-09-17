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
// r_efrag.c

#include "quakedef.h"

mnode_t	*r_pefragtopnode;


//===========================================================================

/*
===============================================================================

					ENTITY FRAGMENT FUNCTIONS

===============================================================================
*/

efrag_t		**lastlink;

vec3_t		r_emins, r_emaxs;

entity_t	*r_addent;


/*
================
R_RemoveEfrags

Call when removing an object from the world or moving it to another position
================
*/
void R_RemoveEfrags (entity_t *ent)
{
	efrag_t		*ef, *old, *walk, **prev;
	
	ef = ent->efrag;
	
	while (ef)
	{
		prev = &ef->leaf->efrags;
		while (1)
		{
			walk = *prev;
			if (!walk)
				break;
			if (walk == ef)
			{	// remove this fragment
				*prev = ef->leafnext;
				break;
			}
			else
				prev = &walk->leafnext;
		}
				
		old = ef;
		ef = ef->entnext;
		
	// put it on the free list
		old->entnext = cl.free_efrags;
		cl.free_efrags = old;
	}
	
	ent->efrag = NULL; 
}

/*
===================
R_SplitEntityOnNode
===================
*/
void R_SplitEntityOnNode (mnode_t *node)
{
	efrag_t		*ef;
	mplane_t	*splitplane;
	mleaf_t		*leaf;
	int			sides;
	
	if (node->contents == CONTENTS_SOLID)
	{
		return;
	}
	
// add an efrag if the node is a leaf

	if ( node->contents < 0)
	{
		if (!r_pefragtopnode)
			r_pefragtopnode = node;

		leaf = (mleaf_t *)node;

// grab an efrag off the free list
		ef = cl.free_efrags;
		if (!ef)
		{
			Con_Printf ("Too many efrags!\n");
			return;		// no free fragments...
		}
		cl.free_efrags = cl.free_efrags->entnext;

		ef->entity = r_addent;
		
// add the entity link	
		*lastlink = ef;
		lastlink = &ef->entnext;
		ef->entnext = NULL;
		
// set the leaf links
		ef->leaf = leaf;
		ef->leafnext = leaf->efrags;
		leaf->efrags = ef;
			
		return;
	}
	
// NODE_MIXED

	splitplane = node->plane;
	sides = BOX_ON_PLANE_SIDE(r_emins, r_emaxs, splitplane);
	
	if (sides == 3)
	{
	// split on this plane
	// if this is the first splitter of this bmodel, remember it
		if (!r_pefragtopnode)
			r_pefragtopnode = node;
	}
	
// recurse down the contacted sides
	if (sides & 1)
		R_SplitEntityOnNode (node->children[0]);
		
	if (sides & 2)
		R_SplitEntityOnNode (node->children[1]);
}



/*
===========
R_AddEfrags
===========
*/
void R_AddEfrags (entity_t *ent)
{
	model_t		*entmodel;
	int			i;
	
	if (!ent->model) {
		Sys_Error("Ent with NULL model\n");

	} else {
		//Con_Printf("Kept ent. %s\n",ent->model->name);
	}

	r_addent = ent;
			
	lastlink = &ent->efrag;
	r_pefragtopnode = NULL;
	
	entmodel = ent->model;

	for (i=0 ; i<3 ; i++)
	{
		r_emins[i] = ent->origin[i] + entmodel->mins[i];
		r_emaxs[i] = ent->origin[i] + entmodel->maxs[i];
	}

	R_SplitEntityOnNode (cl.worldmodel->nodes);

	ent->topnode = r_pefragtopnode;
}


/*
================
R_StoreEfrags

// FIXME: a lot of this goes away with edge-based

Wat dit ook doet het voegt in ieder geval de statische ents aan
de visibility lijst voor dit frame toe.
Maar enkel de statische ents voor een leaf I suppose.
================
*/
void R_StoreEfrags (efrag_t **ppefrag)
{
	entity_t	*pent;
	model_t		*clmodel;
	efrag_t		*pefrag;


	while ((pefrag = *ppefrag) != NULL)
	{
		pent = pefrag->entity;
		clmodel = pent->model;

		//PENTA: null model? skip it!
		if (!clmodel) {
			ppefrag = &pefrag->leafnext;
		}

		switch (clmodel->type)
		{
		case mod_alias:
		case mod_brush:
		case mod_sprite:
			pent = pefrag->entity;

			//prob kunnen ents in meerdere leafs tegelijk zitten (overlappen van bbox)
			//maar we willen ze dus maar 1x tekenen
			if ((pent->visframe != r_framecount) &&
				(cl_numvisedicts < MAX_VISEDICTS))
			{
				cl_visedicts[cl_numvisedicts++] = pent;

			// mark that we've recorded this entity for this frame
				pent->visframe = r_framecount;
			}

			ppefrag = &pefrag->leafnext;
			break;

		default:	
			Sys_Error ("R_StoreEfrags: Bad entity type %d\n", clmodel->type);
		}
	}
}
/*
void R_SplitEntityOnNodePenta (mnode_t *node)
{
	efrag_t		*ef;
	mplane_t	*splitplane;
	mleaf_t		*leaf;
	int			sides;
	
	if (node->contents == CONTENTS_SOLID)
	{
		return;
	}
	
// add an efrag if the node is a leaf

	if ( node->contents < 0)
	{
		leaf = (mleaf_t *)node;
		
		//Store leaf index for vis lookup
		if (r_addent->numleafs < MAX_CLIENT_ENT_LEAFS) {
			r_addent->leafnums[r_addent->numleafs] = leaf->index;
			r_addent->numleafs++;
		} else {
			Con_Printf("Entity is in more than MAX_CLIENT_ENT_LEAFS leafs\n");
		}

		return;
	}
	
// NODE_MIXED

	splitplane = node->plane;
	sides = BOX_ON_PLANE_SIDE(r_emins, r_emaxs, splitplane);
	
	if (sides == 3)
	{
	// split on this plane
	// if this is the first splitter of this bmodel, remember it
		if (!r_pefragtopnode)
			r_pefragtopnode = node;
	}
	
// recurse down the contacted sides
	if (sides & 1)
		R_SplitEntityOnNodePenta (node->children[0]);
		
	if (sides & 2)
		R_SplitEntityOnNodePenta (node->children[1]);
}

*/
void R_SplitEntityOnNodePenta (entity_t *ent, mnode_t *node)
{
	mplane_t	*splitplane;
	mleaf_t		*leaf;
	int			sides;
	int			leafnum;

	if (node->contents == CONTENTS_SOLID)
		return;
	
// add an efrag if the node is a leaf

	if ( node->contents < 0)
	{
		if (ent->numleafs == MAX_CLIENT_ENT_LEAFS) {
			//Con_Printf("Max ent leafs reached\n");
			return;
		}

		leaf = (mleaf_t *)node;
		leafnum = (int)(leaf - cl.worldmodel->leafs - 1);

		ent->leafnums[ent->numleafs] = leafnum;
		ent->numleafs++;			
		return;
	}
	
// NODE_MIXED

	splitplane = node->plane;
	sides = BOX_ON_PLANE_SIDE(r_emins, r_emaxs, splitplane);
	
// recurse down the contacted sides
	if (sides & 1)
		R_SplitEntityOnNodePenta (ent, node->children[0]);
		
	if (sides & 2)
		R_SplitEntityOnNodePenta (ent, node->children[1]);
}


/*
===================
R_FillEntityLeafs
===================
*/
void R_FillEntityLeafs (entity_t *ent)
{
	model_t		*entmodel;
	int			i;
		
	if (!ent->model)
		return;

	if (ent == cl_entities)
		return;		// never add the world

	r_addent = ent;		
	
	entmodel = ent->model;
	
	for (i=0 ; i<3 ; i++)
	{
		r_emins[i] = ent->origin[i] + entmodel->mins[i];
		r_emaxs[i] = ent->origin[i] + entmodel->maxs[i];
	}

	//fiddle a bit with precision 
	r_emins[0] -= 5;
	r_emins[1] -= 5;
	r_emins[2] -= 5;
	r_emaxs[0] += 5;
	r_emaxs[1] += 5;
	r_emaxs[2] += 5;

	ent->numleafs = 0;
	R_SplitEntityOnNodePenta (ent, cl.worldmodel->nodes);

	ent->topnode = r_pefragtopnode;
}


