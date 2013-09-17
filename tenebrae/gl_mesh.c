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
// gl_mesh.c: triangle model functions

#include "quakedef.h"

model_t		*aliasmodel;
aliashdr_t	*paliashdr;

/*
Yet another hack.  Some models seem to have edges shared between three triangles, this is obviously
a strange thing to have, we resolve it simply by throwing away that shared egde and giving all
triangles a "-1" neighbour for that edge.  This will give some unneeded fins for some edges of some models
but this number is generally verry low (< 3 edges per model) and only on a few models.
*/
int findneighbour(int index, int edgei, int numtris) {
	int i, j, v1, v0, found,foundj = 0;
	mtriangle_t *current = &triangles[index];
	mtriangle_t *t;
	qboolean	dup;

	v0 = current->vertindex[edgei];
	v1 = current->vertindex[(edgei+1)%3];

	//XYZ
	found = -1;
	dup = false;
	for (i=0; i<numtris; i++) {
		if (i == index) continue;
		t = &triangles[i];

		for (j=0; j<3; j++) {      
			if (((current->vertindex[edgei] == triangles[i].vertindex[j]) 
			    && (current->vertindex[(edgei+1)%3] == triangles[i].vertindex[(j+1)%3]))
				||
			   ((current->vertindex[edgei] == triangles[i].vertindex[(j+1)%3]) 
			    && (current->vertindex[(edgei+1)%3] == triangles[i].vertindex[j])))
			{
				//no edge for this model found yet?
				if (found == -1) {
					found = i;
					foundj = j;
				}
				//the three edges story
				else
					dup = true;
			}
  
		}
	}

	//normal edge, setup neighbour pointers
	if (!dup) {
		triangles[found].neighbours[foundj] = index;
		return found;
	}
	//naughty egde let no-one have the neighbour
	//Con_Printf("%s: warning: open edge added\n",loadname);
	return -1;
}

int	numNormals[MAXALIASTRIS];
int	dupIndex[MAXALIASTRIS];

void TangentForTri(int *index, ftrivertx_t *vertices, fstvert_t *texcos, vec3_t Tangent, vec3_t Binormal) {
//see:
//http://members.rogers.com/deseric/tangentspace.htm
//	vec3_t stvecs [3];
	float *v0, *v1, *v2;
	float *st0, *st1, *st2;
	vec3_t vec1, vec2;
	vec3_t planes[3];
	int i;

	v0 = &vertices[index[0]].v[0];
	v1 = &vertices[index[1]].v[0];
	v2 = &vertices[index[2]].v[0];
	st0 = &texcos[index[0]].s;
	st1 = &texcos[index[1]].s;
	st2 = &texcos[index[2]].s;

	for (i=0; i<3; i++) {
		vec1[0] = v1[i]-v0[i];
		vec1[1] = st1[0]-st0[0];
		vec1[2] = st1[1]-st0[1];
		vec2[0] = v2[i]-v0[i];
		vec2[1] = st2[0]-st0[0];
		vec2[2] = st2[1]-st0[1];
		VectorNormalize(vec1);
		VectorNormalize(vec2);
		CrossProduct(vec1,vec2,planes[i]);
	}

	//Tangent = (-planes[B][x]/plane[A][x], -planes[B][y]/planes[A][y], - planes[B][z]/planes[A][z] )
	//Binormal = (-planes[C][x]/planes[A][x], -planes[C][y]/planes[A][y], -planes[C][z]/planes[A][z] )
	Tangent[0] = -planes[0][1]/planes[0][0];
	Tangent[1] = -planes[1][1]/planes[1][0];
	Tangent[2] = -planes[2][1]/planes[2][0];
	Binormal[0] = -planes[0][2]/planes[0][0];
	Binormal[1] = -planes[1][2]/planes[1][0];
	Binormal[2] = -planes[2][2]/planes[2][0];
	VectorNormalize(Tangent); //is this needed?
	VectorNormalize(Binormal);
}

int NormalToLatLong( const vec3_t normal) {

	unsigned short r;
	byte *bytes = (byte *)&r;
	// check for singularities
	if ( normal[0] == 0 && normal[1] == 0 ) {
		if ( normal[2] > 0 ) {
			r = 0;
			bytes[0] = 0;
			bytes[1] = 0;		// lat = 0, long = 0
		} else {
			bytes[0] = 128;
			bytes[1] = 0;		// lat = 0, long = 128
		}
	} else {
		int	a, b;

		a = RAD2DEG( atan2( normal[1], normal[0] ) ) * (255.0f / 360.0f );
		a &= 0xff;

		b = RAD2DEG( acos( normal[2] ) ) * ( 255.0f / 360.0f );
		b &= 0xff;

		bytes[0] = b;	// longitude
		bytes[1] = a;	// lattitude
	}

	return r;
}

void Orthogonalize(vec3_t v1, vec3_t v2, vec3_t res);
void DecodeNormal(int quant, vec3_t norm);
			
/*
================
GL_MakeAliasModelDisplayLists

PENTA: For shadow volume generation & bumpmapping we need extra data so we generate/save
it here. This is very memory consuming at the moment (I had to increase quake's default 
heap size for it) but since everything still fits well withing 32MB of RAM I don't have high
priority for fixing it.
================
*/

void GL_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr)
{
	int		i, j, k, l;
	ftrivertx_t	*verts, *v, *oldverts;
	mtriangle_t *tris;
	fstvert_t	*texcoords;
//	char	cache[MAX_QPATH], fullpath[MAX_OSPATH];
//	FILE	*f;
	plane_t	*norms;
	vec3_t	v1, v2, normal;
	vec3_t	triangle[3];
	vec3_t	*tangents, *binormals;
	float	s,t;
	int		*indecies;
	int		newcount;


	aliasmodel = m;
	paliashdr = hdr;	// (aliashdr_t *)Mod_Extradata (m);
	//
	// look for a cached version
	//

/*
	strcpy (cache, "glquake/");
	COM_StripExtension (m->name+strlen("progs/"), cache+strlen("glquake/"));
	strcat (cache, ".ms2");

	COM_FOpenFile (cache, &f);	
	if (f)
	{
		fread (&numcommands, 4, 1, f);
		fread (&numorder, 4, 1, f);
		fread (&commands, numcommands * sizeof(commands[0]), 1, f);
		fread (&vertexorder, numorder * sizeof(vertexorder[0]), 1, f);
		fclose (f);
	}
	else
	{
		//
		// build it from scratch
		//
		Con_Printf ("meshing %s...\n",m->name);

		BuildTris ();		// trifans or strips

		//
		// save out the cached version
		//
		sprintf (fullpath, "%s/%s", com_gamedir, cache);
		f = fopen (fullpath, "wb");
		if (f)
		{
			fwrite (&numcommands, 4, 1, f);
			fwrite (&numorder, 4, 1, f);
			fwrite (&commands, numcommands * sizeof(commands[0]), 1, f);
			fwrite (&vertexorder, numorder * sizeof(vertexorder[0]), 1, f);
			fclose (f);
		}
	}

*/
	// save the data out
	paliashdr->poseverts = paliashdr->numverts;//numorder;
/*
	//cmds = Hunk_Alloc (numcommands * 4);
	//paliashdr->commands = (byte *)cmds - (byte *)paliashdr;
	//memcpy (cmds, commands, numcommands * 4);
*/
	//backup verts in oldverts since we use verts as an loop pointer
	//& convert vertices back to floating point

	//Set neighbours to NULL
	for (i=0 ; i<paliashdr->numtris ; i++)
		for (j=0 ; j<3 ; j++) {
			triangles[i].neighbours[j] = -1;
		}
	

	//PENTA: Generate edges information (for shadow volumes)
	//Note: We do this with the original vertices not the reordered onces since reordening them
	//duplicates vertices and we only compare indices
	for (i=0 ; i<paliashdr->numtris ; i++)
		for (j=0 ; j<3 ; j++) {
			//none found yet 
			if (triangles[i].neighbours[j] == -1) {
				triangles[i].neighbours[j] = findneighbour(i, j, paliashdr->numtris);
			}
		}

	//PENTA: Calculate texcoords for triangles (this duplicates some vertices that have different
	//texcoords for the sames verts)
	for (i=0; i<paliashdr->poseverts; i++) {
		dupIndex[i] = 0;
	}

	newcount = paliashdr->poseverts;
	for (i=0; i<paliashdr->numtris ; i++) {
		for (j=0; j<3; j++) {
			if (!triangles[i].facesfront && stverts[triangles[i].vertindex[j]].onseam) {

				if (dupIndex[triangles[i].vertindex[j]] != 0) {
					continue;
				}
				dupIndex[triangles[i].vertindex[j]] = newcount;
				newcount++;
			}
		}
	}

	for (i=0; i<newcount; i++) {
		dupIndex[i] = 0;
	}

	oldverts = verts = Hunk_Alloc (paliashdr->numposes * newcount * sizeof(ftrivertx_t) );
	paliashdr->posedata = (int)((byte *)verts - (byte *)paliashdr);

	for (i=0; i<paliashdr->numtris ; i++) {
		for (j=0; j<3; j++) {
			s = stverts[triangles[i].vertindex[j]].s;
			t = stverts[triangles[i].vertindex[j]].t;

			if (!triangles[i].facesfront && stverts[triangles[i].vertindex[j]].onseam) {
				int newindex;

				if (dupIndex[triangles[i].vertindex[j]] != 0) {
					triangles[i].vertindex[j] = dupIndex[triangles[i].vertindex[j]];
					continue;
				}

				newindex = paliashdr->poseverts;

				if (paliashdr->poseverts >= MAXALIASVERTS) {
					Con_Printf("To many verts");
				}

				//Duplicate it in all poses
				for (k=0; k<paliashdr->numposes; k++) {
					verts[k*newcount+newindex].v[0] = poseverts[k][triangles[i].vertindex[j]].v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
					verts[k*newcount+newindex].v[1] = poseverts[k][triangles[i].vertindex[j]].v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
					verts[k*newcount+newindex].v[2] = poseverts[k][triangles[i].vertindex[j]].v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];
					verts[k*newcount+newindex].lightnormalindex = NormalToLatLong(r_avertexnormals[poseverts[k][triangles[i].vertindex[j]].lightnormalindex]); //XYZ
				}
				
				//Create a new stvert
				s += pheader->skinwidth / 2;//yet another crappy way to save some space
				stverts[newindex].s = (int)s;
				stverts[newindex].t = (int)t;

				//Adapt index pointer
				dupIndex[triangles[i].vertindex[j]] = newindex;
				triangles[i].vertindex[j] = newindex;

				//Next free
				paliashdr->poseverts++;
			} else {
				//Move it in all poses
				int newindex = triangles[i].vertindex[j];
				for (k=0; k<paliashdr->numposes; k++) {
					verts[k*newcount+newindex].v[0] = poseverts[k][newindex].v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
					verts[k*newcount+newindex].v[1] = poseverts[k][newindex].v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
					verts[k*newcount+newindex].v[2] = poseverts[k][newindex].v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];
					//verts[k*newcount+newindex].lightnormalindex = poseverts[k][newindex].lightnormalindex;
					verts[k*newcount+newindex].lightnormalindex = NormalToLatLong(r_avertexnormals[poseverts[k][triangles[i].vertindex[j]].lightnormalindex]); //XYZ
				}
			}
		}
	}
	if (paliashdr->poseverts != newcount) {
		Con_Printf("Not equal %i %i\n",paliashdr->poseverts,newcount);
	}

	//oldverts = verts = Hunk_Alloc (paliashdr->numposes * paliashdr->poseverts * sizeof(ftrivertx_t) );
	//paliashdr->posedata = (byte *)verts - (byte *)paliashdr;
	//for (i=0 ; i<paliashdr->numposes ; i++)
	//	for (j=0 ; j</*numorder*/paliashdr->poseverts ; j++) {
	//		vert = verts++;
	//		vert->v[0] = poseverts[i][/*vertexorder[j]*/j].v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
	//		vert->v[1] = poseverts[i][/*vertexorder[j]*/j].v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
	//		vert->v[2] = poseverts[i][/*vertexorder[j]*/j].v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];
	//		vert->lightnormalindex = poseverts[i][vertexorder[j]].lightnormalindex;
	//	}

	//verts = oldverts;

	//PENTA: Calculate texcoords for triangles (bump mapping)
	texcoords = Hunk_Alloc (paliashdr->poseverts * sizeof(fstvert_t));
	paliashdr->texcoords = (int)((byte *)texcoords - (byte *)paliashdr);
	for (i=0; i<paliashdr->poseverts ; i++) {
		s = stverts[i].s;
		t = stverts[i].t;
		//if (!triangles[i].facesfront && stverts[triangles[i].vertindex[j]].onseam)
		//	s += pheader->skinwidth / 2;//yet another crappy way to save some space
		s = (s-0.5)  / pheader->skinwidth;
		t = (t-0.5)  / pheader->skinheight;
		texcoords[i].s = s;
		texcoords[i].t = t;
	}

	//PENTA: Save triangles (for shadow volumes)
	tris = Hunk_Alloc (paliashdr->numtris * sizeof(mtriangle_t));
	paliashdr->triangles = (int)((byte *)tris - (byte *)paliashdr);
	Q_memcpy(tris, &triangles, paliashdr->numtris * sizeof(mtriangle_t));

	//PENTA: Calculate plane eq's for triangles (shadow volumes)
	norms = Hunk_Alloc (paliashdr->numtris * paliashdr->numposes * sizeof(plane_t));
	paliashdr->planes = (int)((byte *)norms - (byte *)paliashdr);
	for (i=0; i<paliashdr->numposes; i++) {
		for (j=0; j<paliashdr->numtris ; j++) {

			//make 3 vec3_t's of this triangle's vertices
			for (k=0; k<3; k++) {
				v = &verts[i*paliashdr->poseverts + tris[j].vertindex[k]];
				for (l=0; l<3; l++)
					triangle[k][l] = v->v[l];		
			}

			//calculate their normal
			VectorSubtract(triangle[0], triangle[1], v1);
			VectorSubtract(triangle[2], triangle[1], v2);
			CrossProduct(v2,v1, normal);		
			VectorScale(normal, 1/Length(normal), norms[i*paliashdr->numtris+j].normal);
			
			//distance of plane eq
			norms[i*paliashdr->numtris+j].dist = DotProduct(triangle[0],norms[i*paliashdr->numtris+j].normal);
		}
	}
	

	//PENTA: Create index lists
	indecies = Hunk_Alloc (paliashdr->numtris * sizeof(int) * 3);
	paliashdr->indecies = (int)((byte *)indecies - (byte *)paliashdr);
	for (i=0 ; i<paliashdr->numtris ; i++) {
		for (j=0 ; j<3 ; j++) {
			//Throw vertex index into our index array
			(*indecies) = triangles[i].vertindex[j];
			indecies++;
		}
	}
	indecies = (int *)((byte *)pheader+pheader->indecies);
	
	//PENTA: Calculate tangents for vertices (bump mapping)
	tangents = Hunk_Alloc (paliashdr->poseverts * paliashdr->numposes * sizeof(vec3_t));
	paliashdr->tangents = (int)((byte *)tangents - (byte *)paliashdr);

	binormals = Hunk_Alloc (pheader->poseverts * pheader->numposes * sizeof(vec3_t));
	pheader->binormals = (int)((byte *)binormals - (byte *)pheader);
	//for all frames
	for (i=0; i<paliashdr->numposes; i++) {

		//set temp to zero
		for (j=0; j<pheader->poseverts; j++) {
			tangents[i*pheader->poseverts+j][0] = 0;
			tangents[i*pheader->poseverts+j][1] = 0;
			tangents[i*pheader->poseverts+j][2] = 0;
			binormals[i*pheader->poseverts+j][0] = 0;
			binormals[i*pheader->poseverts+j][1] = 0;
			binormals[i*pheader->poseverts+j][2] = 0;
			numNormals[j] = 0;
		}

		
		//for all tris
		for (j=0; j<paliashdr->numtris; j++) {
			vec3_t tangent, binormal;
			TangentForTri(&indecies[j*3],&verts[i*pheader->poseverts],texcoords,tangent,binormal);			//for all vertices in the tri
			for (k=0; k<3; k++) {
				l = tris[j].vertindex[k];
				VectorAdd(tangents[i*paliashdr->poseverts+l],tangent,
							tangents[i*paliashdr->poseverts+l]); 
				VectorAdd(binormals[i*pheader->poseverts+l],binormal,
							binormals[i*pheader->poseverts+l]); 
				numNormals[l]++;
			}
		}
		
		//calculate average
		for (j=0; j<paliashdr->poseverts; j++) {
			if (!numNormals[j]) continue;
			tangents[i*paliashdr->poseverts+j][0] = tangents[i*paliashdr->poseverts+j][0]/numNormals[j];
			tangents[i*paliashdr->poseverts+j][1] = tangents[i*paliashdr->poseverts+j][1]/numNormals[j];
			tangents[i*paliashdr->poseverts+j][2] = tangents[i*paliashdr->poseverts+j][2]/numNormals[j];

			binormals[i*pheader->poseverts+j][0] = binormals[i*pheader->poseverts+j][0]/numNormals[j];
			binormals[i*pheader->poseverts+j][1] = binormals[i*pheader->poseverts+j][1]/numNormals[j];
			binormals[i*pheader->poseverts+j][2] = binormals[i*pheader->poseverts+j][2]/numNormals[j];

			VectorNormalize(tangents[i*paliashdr->poseverts+j]);
			VectorNormalize(binormals[i*paliashdr->poseverts+j]);

			DecodeNormal(verts[i*pheader->poseverts+j].lightnormalindex, normal);

			Orthogonalize(normal, tangents[i*pheader->poseverts+j], tangents[i*pheader->poseverts+j]);
			Orthogonalize(normal, binormals[i*pheader->poseverts+j], binormals[i*pheader->poseverts+j]);
		}
	}
}
