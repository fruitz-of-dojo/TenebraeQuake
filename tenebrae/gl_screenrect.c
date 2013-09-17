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

Stencil clear / fillrate saving by using opengl scisorring.

The basic idea is that we seach for lights whose scissor rects do not overlap,
if we find such lights we don't clear the stencil buffer between passes.

*/

#include "quakedef.h"

/*
Copy one rectangle to another.
*/
void R_RectCopy(screenrect_t *src, screenrect_t *dst) {
	int i;
	for (i=0; i<4; i++) {
		dst->coords[i] = src->coords[i];
	}
}

/*
Copy one rectangle to another.
*/
void R_RectPosCopy(screenrect_t *src, screenrect_t *dst) {
	int i;
	for (i=0; i<4; i++) {
		dst->coords[i] = src->coords[i]+50000;
	}
}

/*
Extend the rectangle in res by the rectangle in add
(make it contain both rectangles)
*/

void R_RectsAdd(screenrect_t *add, screenrect_t *res) {
	res->coords[0] = min (res->coords[0],add->coords[0]);
	res->coords[1] = min (res->coords[1],add->coords[1]);
	res->coords[2] = max (res->coords[2],add->coords[2]);
	res->coords[3] = max (res->coords[3],add->coords[3]);
}

/*
Calculate the surface of the given rect
*/
int R_RectSurf(screenrect_t *rect) {
	return (rect->coords[2]-rect->coords[0])*(rect->coords[3]-rect->coords[1]);
}

/*
qboolean R_RectOverlap(screenrect_t *r1, screenrect_t *r2) {
	screenrect_t rr1,rr2;
	R_RectPosCopy(r1,&rr1);
	R_RectPosCopy(r2,&rr2);
	return R_RectOverlap_Broken(&rr1,&rr2);
}

*/
/*
Check if rectangles overlap
returns true if they overlap
*/
qboolean R_RectOverlap(screenrect_t *r1, screenrect_t *r2) {
	//r1 is rightof r2
	if (r1->coords[0] > r2->coords[0]) {
		//overlap in x dir?
		if ((r1->coords[0] - r2->coords[2]) < 0)	{
			//check y dir

			//r1 below r2
			if (r1->coords[1] > r2->coords[1]) {
				if ((r1->coords[1]-r2->coords[3]) < 0)
					return true;
				else
					return false;
			//r1 above r2
			} else {
				if ((r2->coords[1]-r1->coords[3]) < 0) 
					return true;
				else 
					return false;
			}
		//no overlap in x- they don't overlap at all
		} else {
			return false;
		}
	//r2 is rightof r1
	} else {
		//overlap in x dir
		if ((r2->coords[0] - r1->coords[2]) < 0)	{
			//check y dir

			//r1 below r2
			if (r1->coords[1] > r2->coords[1]) {
				if ((r1->coords[1]-r2->coords[3]) < 0)
					return true;
				else
					return false;
			//r1 above r2
			} else {
				if ((r2->coords[1]-r1->coords[3]) < 0) 
					return true;
				else 
					return false;
			}
		//no overlap in x- they don't overlap at all
		} else {
			return false;
		}
	}
}

screenrect_t *recList;	//first rectangle of the list
screenrect_t totalRect;	//rectangle that holds all rectangles in the list
int recsUsed;


void R_ClearRectList() {
	recList = NULL;
	recsUsed = 0;
}

qboolean R_CheckRectList(screenrect_t *rec) {
	screenrect_t *r;
	r = recList;
	while (r) {
		if (R_RectOverlap(rec,r)) return false;
		r = r->next;
	}
	return true;
}

void R_AddRectList(screenrect_t *rec) {
	
	//Extend bounding rectangle
	if (!recList) {
		R_RectCopy(rec,&totalRect);
	} else {
		R_RectsAdd(rec,&totalRect);
	}
	//Add it to the list
	rec->next = recList;
	recList = rec;
}

void R_SetTotalRect() {
	glScissor(totalRect.coords[0], totalRect.coords[1],
		totalRect.coords[2]-totalRect.coords[0],  totalRect.coords[3]-totalRect.coords[1]);	
}