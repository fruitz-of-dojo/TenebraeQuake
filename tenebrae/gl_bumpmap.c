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

All code conserning the drawing of bump mapped polygons.
Per vertex setup, shaders and the passes.
This is the general/gf2 approach for the gf3 code see gumpgf.c
*/

#include "quakedef.h"

#if (defined (__APPLE__) || defined (MACOSX)) && !defined (GL_NV_register_combiners)

#define GL_REGISTER_COMBINERS_NV          0x8522
#define GL_VARIABLE_A_NV                  0x8523
#define GL_VARIABLE_B_NV                  0x8524
#define GL_VARIABLE_C_NV                  0x8525
#define GL_VARIABLE_D_NV                  0x8526
#define GL_VARIABLE_E_NV                  0x8527
#define GL_VARIABLE_F_NV                  0x8528
#define GL_VARIABLE_G_NV                  0x8529
#define GL_CONSTANT_COLOR0_NV             0x852A
#define GL_CONSTANT_COLOR1_NV             0x852B
#define GL_PRIMARY_COLOR_NV               0x852C
#define GL_SECONDARY_COLOR_NV             0x852D
#define GL_SPARE0_NV                      0x852E
#define GL_SPARE1_NV                      0x852F
#define GL_DISCARD_NV                     0x8530
#define GL_E_TIMES_F_NV                   0x8531
#define GL_SPARE0_PLUS_SECONDARY_COLOR_NV 0x8532
#define GL_UNSIGNED_IDENTITY_NV           0x8536
#define GL_UNSIGNED_INVERT_NV             0x8537
#define GL_EXPAND_NORMAL_NV               0x8538
#define GL_EXPAND_NEGATE_NV               0x8539
#define GL_HALF_BIAS_NORMAL_NV            0x853A
#define GL_HALF_BIAS_NEGATE_NV            0x853B
#define GL_SIGNED_IDENTITY_NV             0x853C
#define GL_SIGNED_NEGATE_NV               0x853D
#define GL_SCALE_BY_TWO_NV                0x853E
#define GL_SCALE_BY_FOUR_NV               0x853F
#define GL_SCALE_BY_ONE_HALF_NV           0x8540
#define GL_BIAS_BY_NEGATIVE_ONE_HALF_NV   0x8541
#define GL_COMBINER_INPUT_NV              0x8542
#define GL_COMBINER_MAPPING_NV            0x8543
#define GL_COMBINER_COMPONENT_USAGE_NV    0x8544
#define GL_COMBINER_AB_DOT_PRODUCT_NV     0x8545
#define GL_COMBINER_CD_DOT_PRODUCT_NV     0x8546
#define GL_COMBINER_MUX_SUM_NV            0x8547
#define GL_COMBINER_SCALE_NV              0x8548
#define GL_COMBINER_BIAS_NV               0x8549
#define GL_COMBINER_AB_OUTPUT_NV          0x854A
#define GL_COMBINER_CD_OUTPUT_NV          0x854B
#define GL_COMBINER_SUM_OUTPUT_NV         0x854C
#define GL_MAX_GENERAL_COMBINERS_NV       0x854D
#define GL_NUM_GENERAL_COMBINERS_NV       0x854E
#define GL_COLOR_SUM_CLAMP_NV             0x854F
#define GL_COMBINER0_NV                   0x8550
#define GL_COMBINER1_NV                   0x8551
#define GL_COMBINER2_NV                   0x8552
#define GL_COMBINER3_NV                   0x8553
#define GL_COMBINER4_NV                   0x8554
#define GL_COMBINER5_NV                   0x8555
#define GL_COMBINER6_NV                   0x8556
#define GL_COMBINER7_NV                   0x8557
/* reuse GL_TEXTURE0_ARB */
/* reuse GL_TEXTURE1_ARB */
/* reuse GL_ZERO */
/* reuse GL_NONE */
/* reuse GL_FOG */

#endif // (__APPLE__ || MACOSX) && !GL_NV_register_combiners

//view origin in object space
vec3_t object_vieworg;

/**************************************************************

	PART1: Gl state management

***************************************************************/


void GL_EnableDiffuseShader () {
	//ordinary combiners = compatible with everything
	
	//texture coords for unit 0: Tangent space light vector
	//texture coords for unit 1: Normal map texture coords
	GL_SelectTexture(GL_TEXTURE0_ARB);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_CUBE_MAP_ARB);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, normcube_texture_object);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE);
    glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE);
	
	GL_EnableMultitexture();
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE);
    glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_DOT3_RGBA_ARB);
}

void GL_DisableDiffuseShader () {
	GL_DisableMultitexture();
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_CUBE_MAP_ARB);
}

void GL_EnableSpecularShader () {

	vec3_t scaler = {0.75f, 0.75f, 0.75f};

	if ( gl_cardtype == GEFORCE || gl_cardtype == GEFORCE3 ) {
		GL_SelectTexture(GL_TEXTURE0_ARB);
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_TEXTURE_CUBE_MAP_ARB);
		//GL_Bind(normcube_texture_object);
		glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, normcube_texture_object);
		GL_EnableMultitexture();

		//setup and enable the register combiners
		qglCombinerParameteriNV(GL_NUM_GENERAL_COMBINERS_NV, 2);
		qglCombinerParameterfvNV(GL_CONSTANT_COLOR0_NV, &scaler[0]);

		//combiner0 RGB: calculate N'dotH -> store in Spare0 RGB
		qglCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_A_NV, GL_TEXTURE1_ARB, GL_EXPAND_NORMAL_NV, GL_RGB);
		qglCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_B_NV, GL_TEXTURE0_ARB, GL_EXPAND_NORMAL_NV, GL_RGB);
		qglCombinerOutputNV(GL_COMBINER0_NV, GL_RGB, GL_SPARE0_NV, GL_DISCARD_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_TRUE, GL_FALSE, GL_FALSE);

		//combiner0 Alpha: ignore all
		qglCombinerOutputNV(GL_COMBINER0_NV, GL_ALPHA, GL_DISCARD_NV, GL_DISCARD_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);

		//combiner1 RGB: ignore all
		qglCombinerOutputNV(GL_COMBINER1_NV, GL_RGB, GL_DISCARD_NV, GL_DISCARD_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);

		//combiner1 Alpha: calculate 4*((N'dotH)^2 - 0.75f) -> store in Spare0 Alpha
		qglCombinerInputNV(GL_COMBINER1_NV, GL_ALPHA, GL_VARIABLE_A_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_BLUE);
		qglCombinerInputNV(GL_COMBINER1_NV, GL_ALPHA, GL_VARIABLE_B_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_BLUE);
		qglCombinerInputNV(GL_COMBINER1_NV, GL_ALPHA, GL_VARIABLE_C_NV, GL_CONSTANT_COLOR0_NV, GL_SIGNED_NEGATE_NV, GL_BLUE);
		qglCombinerInputNV(GL_COMBINER1_NV, GL_ALPHA, GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
		qglCombinerOutputNV(GL_COMBINER1_NV, GL_ALPHA, GL_DISCARD_NV, GL_DISCARD_NV, GL_SPARE0_NV, GL_SCALE_BY_FOUR_NV, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);

		//final combiner RGB: who cares
		//					Alpha: specular value of 4*((N'dotH)^2 - 0.75f)
		qglFinalCombinerInputNV(GL_VARIABLE_G_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);

		glEnable(GL_REGISTER_COMBINERS_NV);
	} else {
		GL_EnableDiffuseShader ();
	}
}

void GL_DisableSpecularShader () {

	//not supported?
	if ( gl_cardtype == GEFORCE || gl_cardtype == GEFORCE3 ) {
		GL_DisableMultitexture();
		glDisable(GL_REGISTER_COMBINERS_NV);
		glEnable(GL_TEXTURE_2D);
		glDisable(GL_TEXTURE_CUBE_MAP_ARB);
	} else {
		GL_DisableDiffuseShader ();
	}
}

void GL_EnableColorShader (qboolean specular) {
	//glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);	

	//primary color = light color
	//tu0 = light filter cube map 
	//tu1 = material color map
	GL_SelectTexture(GL_TEXTURE0_ARB);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_CUBE_MAP_ARB);

	GL_SelectTexture(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, currentshadowlight->filtercube);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE);
    glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
	
	if (!specular) {
		if (sh_colormaps.value) {
			GL_EnableMultitexture();
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
			glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
			glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE);
			glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
		} else {
			GL_DisableMultitexture();
		}
	} else {
		GL_EnableMultitexture();
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
		glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
		glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE);
		glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_ALPHA);
		glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
	}


	
}

void GL_DisableColorShader (qboolean specular) {
	if (!specular) {
		if (sh_colormaps.value) GL_DisableMultitexture();
	} else {
		glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
		GL_DisableMultitexture();
	}

	glEnable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_CUBE_MAP_ARB);
}

void GL_EnableAttShader() {
	
	GL_Bind(glow_texture_object);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

void GL_DisableAttShader() {
	//empty
	glDisable(GL_REGISTER_COMBINERS_NV);
}

/*
Draw incoming fragment to destination alpha.

Note: A fragment in the next comment means a pixel that will be drawn on the screen
(gl has some specific definition of it)
*/
void GL_DrawAlpha() {
	glColorMask(false, false, false, true);
	glDisable(GL_BLEND);
}

/*
Draw incoming fragment by modulating it with destination alpha
*/
void GL_ModulateAlphaDrawAlpha() {
	glColorMask(false, false, false, true);
	glBlendFunc (GL_DST_ALPHA, GL_ZERO);
	glEnable (GL_BLEND);
}

/*
Draw incoming fragment by adding it to destination alpha
*/
void GL_AddAlphaDrawAlpha() {
	glColorMask(false, false, false, true);
	glBlendFunc (GL_ONE, GL_ONE);
	glEnable (GL_BLEND);
}

/*
Draw incoming fragment by modulation it with destination alpha
*/
void GL_ModulateAlphaDrawColor() {
	glColorMask(true, true, true, true);
	glBlendFunc (GL_DST_ALPHA, GL_ONE);
	glEnable (GL_BLEND);
}

/*
Draw incoming alpha squared to destination alpha
*/
void GL_DrawSquareAlpha() {
	glColorMask(false, false, false, true);
	glBlendFunc (GL_SRC_ALPHA, GL_ZERO);
	glEnable (GL_BLEND);
}

/*
Square the destination alpha
*/
void GL_SquareAlpha() {
	glColorMask(false, false, false, true);
	glBlendFunc (GL_ZERO, GL_DST_ALPHA);
	glEnable (GL_BLEND);
}

/*
Add incoming fragment to destination color
*/
void GL_AddColor() {
	glColorMask(true, true, true, true);
	glBlendFunc (GL_ONE, GL_ONE);
	glEnable (GL_BLEND);
}

/*
Ovewrite destination color with incoming fragment
*/
void GL_DrawColor() {
	glColorMask(true, true, true, true);
	glDisable (GL_BLEND);
}

/*
=================
R_WorldToObjectMatrix

Returns a world to object coordinate transformation matrix.
This is crap and I know it ;)
The problem is that quake does strange stuff with its angles
(look at R_RotateForEntity) so inverting the matrix will
certainly give the desired result.
Why I use the gl matrix? Well quake doesn't have build in matix
routines an I don't have any lying around in c (Lot's in Pascal
of course i'm a Delphi man)

=================
*/

void R_WorldToObjectMatrix(entity_t *e, matrix_4x4 result)
{
	matrix_4x4 world;

	glPushMatrix();
	glLoadIdentity();
	R_RotateForEntity (e);
	glGetFloatv (GL_MODELVIEW_MATRIX, &world[0][0]);
	glPopMatrix();
	MatrixAffineInverse(world,result);
}

/*
=============
GL_SetupCubeMapMatrix

Loads the current matrix with a tranformation used for light filters
=============
*/

void GL_SetupCubeMapMatrix(qboolean world) {

	glLoadIdentity();

	glRotatef (-currentshadowlight->angles[0],  1.0f, 0.0f, 0.0f);
    glRotatef (-currentshadowlight->angles[1],  0.0f, 1.0f, 0.0f);
    glRotatef (-currentshadowlight->angles[2],  0.0f, 0.0f, 1.0f);

	glRotatef (currentshadowlight->rspeed*cl.time, 0.0f, 0.0f, 1.0f);

	glTranslatef(-currentshadowlight->origin[0],
				-currentshadowlight->origin[1],
				-currentshadowlight->origin[2]);

	if (!world)
		R_RotateForEntity(currententity);

}
/**************************************************************

	PART2: Geommety sending code

	This parts containts code for sending different geommety
	types (brushes, alias, ...) with different per vertex
	options (light, H, att)
	Every method is the core of 1 pass during bump map rendering.
	This is all direct mode stuff, it should be put in vertex
	arrays.

***************************************************************/

/*
=============
R_DrawWorldLLV

-Draws the world sending the tangent space light vector as unit 0's texture coordinates.
-Sends as uni1t's texture coordinates the original texture coordinates.
-Rebinds unit1's texture if the surface material changes (binds the current surface's bump map)
=============
*/
void R_DrawWorldLLV(lightcmd_t *lightCmds) {

	int command, num, i;
	int lightPos = 0;
	vec3_t lightOr;
	msurface_t *surf;
	float		*v;
	texture_t	*t;//XYZ

	//support flickering lights
	VectorCopy(currentshadowlight->origin,lightOr);

	while (1) {
		
		command = lightCmds[lightPos++].asInt;
		if (command == 0) break; //end of list

        // <AWE> HACK: Fix for 64-bit pointers in lightcmd buffer
		//surf = lightCmds[lightPos++].asVoid;
        surf = (msurface_t*) (hunk_base + lightCmds[lightPos++].asOffset);
        
		if (surf->visframe != r_framecount) {
			lightPos+=(4+surf->polys->numverts*(2+3));
			continue;
		}

		num = surf->polys->numverts;
		lightPos+=4;//skip color


		//XYZ
		t = R_TextureAnimation (surf->texinfo->texture);
		GL_Bind (t->gl_texturenum+1);

		glBegin(command);
		//v = surf->polys->verts[0];
		v = (float *)(&globalVertexTable[surf->polys->firstvertex]);
		for (i=0; i<num; i++, v+= VERTEXSIZE) {
			//skip attent texture coord.
			lightPos+=2;

			//calculate local light vector and put it into tangent space
			/*
			VectorSubtract(lightOr,cl.worldmodel->vertexes[ind].position,lightDir);

			if (surf->flags & SURF_PLANEBACK)	{
				tsLightDir[2] = -DotProduct(lightDir,surf->plane->normal);
			} else {
				tsLightDir[2] = DotProduct(lightDir,surf->plane->normal);
			}

			tsLightDir[1] = -DotProduct(lightDir,surf->texinfo->vecs[1]);
			tsLightDir[0] = DotProduct(lightDir,surf->texinfo->vecs[0]);
	
			glTexCoord3fv(&tsLightDir[0]);
			*/
            
			glTexCoord3fv(&lightCmds[lightPos].asFloat);
			lightPos+=3;

			qglMultiTexCoord2fARB(GL_TEXTURE1_ARB, v[3], v[4]);
			glVertex3fv(&v[0]);
		}
		glEnd();
	}

}

/*
=============
R_DrawWorldHAV

-Draws the world sending the tangent space half angle vector as unit 0's texture coordinates.
-Sends as uni1t's texture coordinates the original texture coordinates.
-Rebinds unit1's texture if the surface material changes (binds the current surface's bump map)
=============
*/
void R_DrawWorldHAV(lightcmd_t *lightCmds) {

	int command, num, i;
	int lightPos = 0;
	vec3_t lightOr,tsH,H;
	msurface_t *surf;
	float		*v,*lightP;
	vec3_t		lightDir;
	texture_t	*t;//XYZ

	//support flickering lights
	VectorCopy(currentshadowlight->origin,lightOr);

	while (1) {
		
		command = lightCmds[lightPos++].asInt;
		if (command == 0) break; //end of list

        // <AWE> HACK: Fix for 64-bit pointers in lightcmd buffer
		//surf = lightCmds[lightPos++].asVoid;
        surf = (msurface_t*) (hunk_base + lightCmds[lightPos++].asOffset);
        
		if (surf->visframe != r_framecount) {
			lightPos+=(4+surf->polys->numverts*(2+3));
			continue;
		}

		num = surf->polys->numverts;
		lightPos+=4;//skip color

		//XYZ
		t = R_TextureAnimation (surf->texinfo->texture);
		GL_Bind (t->gl_texturenum+1);

		glBegin(command);
		//v = surf->polys->verts[0];
		v = (float *)(&globalVertexTable[surf->polys->firstvertex]);
		for (i=0; i<num; i++, v+= VERTEXSIZE) {
			lightPos+=2;//skip texcoords
			lightP = &lightCmds[lightPos].asFloat;
			VectorCopy(lightP,lightDir);
			VectorNormalize(lightDir);
			lightPos+=3;//skip local light vector

			//r_origin = camera position
			VectorSubtract(r_refdef.vieworg,v,H);
			VectorNormalize(H);

			//put H in tangent space firste since lightDir (precalc) is already in tang space
			if (surf->flags & SURF_PLANEBACK)	{
				tsH[2] = -DotProduct(H,surf->plane->normal);
			} else {
				tsH[2] = DotProduct(H,surf->plane->normal);
			}

			tsH[1] = -DotProduct(H,surf->texinfo->vecs[1]);
			tsH[0] = DotProduct(H,surf->texinfo->vecs[0]);

			VectorAdd(lightDir,tsH,tsH);

			glTexCoord3fv(&tsH[0]);
			qglMultiTexCoord2fARB(GL_TEXTURE1_ARB, v[3], v[4]);
			glVertex3fv(&v[0]);
		}
		glEnd();
	}
}

/*
=============
R_DrawWorldATT

Draw the world sending as texture unit 0's texture coordinates the precalcuated attenuation
coordinates.
As primary color the light color modulated by the current brightness (for flickering lights
this depends on t)
=============
*/
void R_DrawWorldATT(lightcmd_t *lightCmds) {
	int command, num, i;
	int lightPos = 0;

	//edit: modulation of light maps
	msurface_t *surf;
	float		*v;

	//support flickering lights
	float b = d_lightstylevalue[currentshadowlight->style]/255.0;

	while (1) {
		
		command = lightCmds[lightPos++].asInt;
		if (command == 0) break; //end of list

		//edit: modulation of light maps
        // <AWE> HACK: Fix for 64-bit pointers in lightcmd buffer
		//surf = lightCmds[lightPos++].asVoid;
        surf = (msurface_t*) (hunk_base + lightCmds[lightPos++].asOffset);
        
		if (surf->visframe != r_framecount) {
			lightPos+=(4+surf->polys->numverts*(2+3));
			continue;
		}

		num = surf->polys->numverts;
		//v = surf->polys->verts[0];
		v = (float *)(&globalVertexTable[surf->polys->firstvertex]);
		
		glColor4f( (&lightCmds[lightPos].asFloat)[0] *b,
				   (&lightCmds[lightPos].asFloat)[1] *b,
				   (&lightCmds[lightPos].asFloat)[2] *b,
				   (&lightCmds[lightPos].asFloat)[3] *b );
		
		lightPos+=4;

		glBegin(command);
		for (i=0; i<num; i++, v+= VERTEXSIZE) {
			
			glTexCoord2fv(&lightCmds[lightPos].asFloat);			
			lightPos+=2;
			lightPos+=3;
			glVertex3fv(&v[0]);
		}
		glEnd();
	}
}

/*
=============
R_DrawWorldWV

-Draw the world sending as texture unit 0's coordinates the world space coordinates of the current vertex
-Sends as uni1t's texture coordinates the original texture coordinates.
-Rebinds unit1's texture if the surface material changes (binds the current surface's color map)
=============
*/
void R_DrawWorldWV(lightcmd_t *lightCmds, qboolean specular) {

	int command, num, i;
	int lightPos = 0;

	//edit: modulation of light maps
	msurface_t *surf;
	float		*v;
	texture_t	*t;//XYZ

	while (1) {
		
		command = lightCmds[lightPos++].asInt;
		if (command == 0) break; //end of list

        // <AWE> HACK: Fix for 64-bit pointers in lightcmd buffer
		//surf = lightCmds[lightPos++].asVoid;
        surf = (msurface_t*) (hunk_base + lightCmds[lightPos++].asOffset);
        
		if (surf->visframe != r_framecount) {
			lightPos+=(4+surf->polys->numverts*(2+3));
			continue;
		}

		num = surf->polys->numverts;
		//v = surf->polys->verts[0];
		v = (float *)(&globalVertexTable[surf->polys->firstvertex]);

		//XYZ
		t = R_TextureAnimation (surf->texinfo->texture);
		if ( specular )
		    GL_Bind (t->gl_texturenum+1);
		else
		    GL_Bind (t->gl_texturenum);

		lightPos+=4;//no attent color

		glBegin(command);
		for (i=0; i<num; i++, v+= VERTEXSIZE) {
			lightPos+=2;//no attent coords
			lightPos+=3;
			glTexCoord3fv(&v[0]);
			qglMultiTexCoord2fARB(GL_TEXTURE1_ARB, v[3], v[4]);
			glVertex3fv(&v[0]);
		}
		glEnd();
	}

}


/*
=============
R_DrawBrushLLV

-Draws the brush sending the tangent space light vector as unit 0's texture coordinates.
-Sends as uni1t's texture coordinates the original texture coordinates.
-Rebinds unit1's texture if the surface material changes (binds the current surface's bump map)
=============
*/
void R_DrawBrushLLV(entity_t *e) {

	model_t	*model = e->model;
	msurface_t *surf;
	glpoly_t	*poly;
	int		i, j, count;
	brushlightinstant_t *ins = e->brushlightinstant;
	float	*v;
	texture_t	*t;

	count = 0;

	surf = &model->surfaces[model->firstmodelsurface];
	for (i=0; i<model->nummodelsurfaces; i++, surf++)
	{
		if (!ins->polygonVis[i]) continue;

		poly = surf->polys;

		//XYZ
		t = R_TextureAnimation (surf->texinfo->texture);
		GL_Bind (t->gl_texturenum+1);

		glBegin(GL_TRIANGLE_FAN);
		//v = poly->verts[0];
		v = (float *)(&globalVertexTable[poly->firstvertex]);
		for (j=0 ; j<poly->numverts ; j++, v+= VERTEXSIZE)
		{	
			glTexCoord3fv(&ins->tslights[count+j][0]);
			qglMultiTexCoord2fARB(GL_TEXTURE1_ARB, v[3], v[4]);
			glVertex3fv(v);
		}
		glEnd();
		count+=surf->numedges;
	}	
}

/*
=============
R_DrawBrushHAV

-Draws the brush sending the tangent space half angle vector as unit 0's texture coordinates.
-Sends as uni1t's texture coordinates the original texture coordinates.
-Rebinds unit1's texture if the surface material changes (binds the current surface's bump map)
=============
*/
void R_DrawBrushHAV(entity_t *e) {

	model_t	*model = e->model;
	msurface_t *surf;
	glpoly_t	*poly;
	int		i, j, count;
	brushlightinstant_t *ins = e->brushlightinstant;
	float	*v;
	texture_t	*t;//XYZ

	count = 0;

	surf = &model->surfaces[model->firstmodelsurface];
	for (i=0; i<model->nummodelsurfaces; i++, surf++)
	{
		if (!ins->polygonVis[i]) continue;

		poly = surf->polys;

		//XYZ
		t = R_TextureAnimation (surf->texinfo->texture);
		GL_Bind (t->gl_texturenum+1);

		glBegin(GL_TRIANGLE_FAN);
		//v = poly->verts[0];
		v = (float *)(&globalVertexTable[poly->firstvertex]);
		for (j=0 ; j<poly->numverts ; j++, v+= VERTEXSIZE)
		{	
			glTexCoord3fv(&ins->tshalfangles[count+j][0]);
			qglMultiTexCoord2fARB(GL_TEXTURE1_ARB, v[3], v[4]);
			glVertex3fv(v);
		}
		glEnd();
		count+=surf->numedges;
	}	
}
/*
=============
R_DrawBrushATT

Draw the brush sending as texture unit 0's texture coordinates the calcuated attentuation
coordinates.
As primary color the light color modulated by the current brightness (for flickering lights
this depends on t)
=============
*/
void R_DrawBrushATT(entity_t *e) {

	model_t	*model = e->model;
	msurface_t *surf;
	glpoly_t	*poly;
	int		i, j, count, countc;
	brushlightinstant_t *ins = e->brushlightinstant;
	float	*v, bright;

	count = 0;
	countc = 0;
	surf = &model->surfaces[model->firstmodelsurface];
	for (i=0; i<model->nummodelsurfaces; i++, surf++)
	{
		if (!ins->polygonVis[i]) continue;

		poly = surf->polys;

		//GL_Bind(surf->texinfo->texture->gl_texturenum);

		bright = ins->colorscales[countc];
		countc++;

		glColor4f(bright,bright,bright,bright);

		glBegin(GL_TRIANGLE_FAN);
		//v = poly->verts[0];
		v = (float *)(&globalVertexTable[poly->firstvertex]);
		for (j=0 ; j<poly->numverts ; j++, v+= VERTEXSIZE)
		{	
			glTexCoord2fv(&ins->atencoords[count+j][0]);
			//qglMultiTexCoord2fARB(GL_TEXTURE1_ARB, v[3], v[4]);
			glVertex3fv(v);
		}
		glEnd();
		count+=surf->numedges;
	}	
}

/*
=============
R_DrawBrushWV

=============
*/

void R_DrawBrushWV(entity_t *e, qboolean specular) {

	model_t	*model = e->model;
	msurface_t *surf;
	glpoly_t	*poly;
	int		i, j, count;
	brushlightinstant_t *ins = e->brushlightinstant;
	float	*v;
	texture_t	*t;

	count = 0;

	surf = &model->surfaces[model->firstmodelsurface];
	for (i=0; i<model->nummodelsurfaces; i++, surf++)
	{
		if (!ins->polygonVis[i]) continue;

		poly = surf->polys;

		//XYZ
		t = R_TextureAnimation (surf->texinfo->texture);
		if ( specular )
		    GL_Bind (t->gl_texturenum+1);
		else
		    GL_Bind (t->gl_texturenum);

		glBegin(GL_TRIANGLE_FAN);
		//v = poly->verts[0];
		v = (float *)(&globalVertexTable[poly->firstvertex]);
		for (j=0 ; j<poly->numverts ; j++, v+= VERTEXSIZE)
		{	
			glTexCoord3fv(v);
			qglMultiTexCoord2fARB(GL_TEXTURE1_ARB, v[3], v[4]);
			glVertex3fv(v);
		}
		glEnd();
		count+=surf->numedges;
	}	
}
/*
=============
R_DrawBrushObjectLight

Idea: Creepy object oriented programming by using function pointers.
Function: Puts the light into object space, adapts the world->eye matrix
and calls BrushGeoSender if all that has been done.
Cleans up afterwards so nothing has changed.
=============
*/
void R_DrawBrushObjectLight(entity_t *e,void (*BrushGeoSender) (entity_t *e)) {
	
	model_t		*clmodel;
	
	vec3_t oldlightorigin;
	//backup light origin since we will have to translate
	//light into model space
	VectorCopy (currentshadowlight->origin, oldlightorigin);
	
	currententity = e;
	currenttexture = -1;
	
	clmodel = e->model;
	
    glPushMatrix ();
	e->angles[0] = -e->angles[0];	// stupid quake bug
	R_RotateForEntity (e);
	e->angles[0] = -e->angles[0];	// stupid quake bug
	
	BrushGeoSender(e);
	
	VectorCopy(oldlightorigin,currentshadowlight->origin);
	glPopMatrix ();
}

void R_DrawAliasFrameLLV (aliashdr_t *paliashdr, aliasframeinstant_t *instant)
{
	fstvert_t	*texcoords;
	int *indecies, anim;
	aliaslightinstant_t *linstant = instant->lightinstant;

	texcoords = (fstvert_t *)((byte *)paliashdr + paliashdr->texcoords);
	indecies = (int *)((byte *)paliashdr + paliashdr->indecies);

	//bind normal map
	anim = (int)(cl.time*10) & 3;
    GL_Bind(paliashdr->gl_texturenum[currententity->skinnum][anim]+1);

	glVertexPointer(3, GL_FLOAT, 0, instant->vertices);
	glEnableClientState(GL_VERTEX_ARRAY);

	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	glTexCoordPointer(3, GL_FLOAT, 0, linstant->tslights);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	qglClientActiveTextureARB(GL_TEXTURE1_ARB);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glDrawElements(GL_TRIANGLES,linstant->numtris*3,GL_UNSIGNED_INT,&linstant->indecies[0]);

	if (sh_noshadowpopping.value) {
		glStencilFunc(GL_LEQUAL, 1, 0xffffffff);
		glDrawElements(GL_TRIANGLES,(paliashdr->numtris*3)-(linstant->numtris*3),GL_UNSIGNED_INT,&linstant->indecies[linstant->numtris*3]);
		glStencilFunc(GL_EQUAL, 0, 0xffffffff);
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void R_DrawAliasFrameHAV (aliashdr_t *paliashdr, aliasframeinstant_t *instant)
{
	fstvert_t	*texcoords;
	int			*indecies, anim;
	aliaslightinstant_t *linstant = instant->lightinstant;

	texcoords = (fstvert_t *)((byte *)paliashdr + paliashdr->texcoords);
	indecies = (int *)((byte *)paliashdr + paliashdr->indecies);

	//bind normal map
	anim = (int)(cl.time*10) & 3;
    GL_Bind(paliashdr->gl_texturenum[currententity->skinnum][anim]+1);

	glVertexPointer(3, GL_FLOAT, 0, instant->vertices);
	glEnableClientState(GL_VERTEX_ARRAY);

	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	glTexCoordPointer(3, GL_FLOAT, 0, linstant->tshalfangles);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	qglClientActiveTextureARB(GL_TEXTURE1_ARB);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glDrawElements(GL_TRIANGLES,linstant->numtris*3,GL_UNSIGNED_INT,&linstant->indecies[0]);

	if (sh_noshadowpopping.value) {
		glStencilFunc(GL_LEQUAL, 1, 0xffffffff);
		glDrawElements(GL_TRIANGLES,(paliashdr->numtris*3)-(linstant->numtris*3),GL_UNSIGNED_INT,&linstant->indecies[linstant->numtris*3]);
		glStencilFunc(GL_EQUAL, 0, 0xffffffff);
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

}


/*
=============
R_DrawAliasFrameWV

Draw an alias model with the ojbect space vertex coordinates as texture coords
=============
*/
void R_DrawAliasFrameWV (aliashdr_t *paliashdr, aliasframeinstant_t *instant, qboolean specular) {

	int *indecies, anim;
	fstvert_t *texcoords;
	aliaslightinstant_t *linstant = instant->lightinstant;

	texcoords = (fstvert_t *)((byte *)paliashdr + paliashdr->texcoords);
	indecies = (int *)((byte *)paliashdr + paliashdr->indecies);

	anim = (int)(cl.time*10) & 3;
	if ( specular )
	    GL_Bind(paliashdr->gl_texturenum[currententity->skinnum][anim]+1);
	else
	    GL_Bind(paliashdr->gl_texturenum[currententity->skinnum][anim]);

	glVertexPointer(3, GL_FLOAT, 0, instant->vertices);
	glEnableClientState(GL_VERTEX_ARRAY);

	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	glTexCoordPointer(3, GL_FLOAT, 0, instant->vertices);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	
	if ( gl_cardtype == GENERIC || gl_cardtype == GEFORCE ) {//PA:

		qglClientActiveTextureARB(GL_TEXTURE1_ARB);
		glTexCoordPointer(2, GL_FLOAT, 0.0f, texcoords);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

		glNormalPointer(GL_FLOAT, 0.0f, instant->normals);
		glEnableClientState(GL_NORMAL_ARRAY);

		glEnableClientState(GL_COLOR_ARRAY);
		glColorPointer(3, GL_FLOAT, 0.0f, linstant->colors);
	}

	glDrawElements(GL_TRIANGLES,linstant->numtris*3,GL_UNSIGNED_INT,&linstant->indecies[0]);

	if (sh_noshadowpopping.value) {
		glStencilFunc(GL_LEQUAL, 1, 0xffffffff);
		glDrawElements(GL_TRIANGLES,(paliashdr->numtris*3)-(linstant->numtris*3),GL_UNSIGNED_INT,&linstant->indecies[linstant->numtris*3]);
		glStencilFunc(GL_EQUAL, 0, 0xffffffff);
	}

	if ( gl_cardtype == GENERIC || gl_cardtype == GEFORCE ) {//PA:
		glDisableClientState(GL_NORMAL_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
	}

	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void PrintScreenPos(vec3_t v) {

	double	Dproject_matrix[16];
	double	Dworld_matrix[16];
	GLint Iviewport[4];		// <AWE> changed from int to GLint.
	double px, py, pz;

	glGetDoublev (GL_MODELVIEW_MATRIX, Dworld_matrix);
	glGetDoublev (GL_PROJECTION_MATRIX, Dproject_matrix);
	glGetIntegerv (GL_VIEWPORT, Iviewport);
	gluProject(v[0], v[1], v[2],
				   Dworld_matrix, Dproject_matrix, Iviewport, &px, &py, &pz);
	Con_Printf("Pos: %f %f %f\n", px, py, pz);
}

/*
=================
R_DrawAliasObjectLight

Same as R_DrawBrushObjectLight but with alias models

=================
*/
void R_DrawAliasObjectLight(entity_t *e,void (*AliasGeoSender) (aliashdr_t *paliashdr, aliasframeinstant_t* instant))
{
	int				pose, numposes;
	//matrix_4x4		transf;
	//float			org[4],res[4];
	aliashdr_t	*paliashdr;
        alias3data_t    *data;
	vec3_t		oldlightpos;
        aliasframeinstant_t *aliasframeinstant;
        int i,maxnumsurf;

	currententity = e;
	
//	VectorCopy (currententity->origin, r_entorigin);
//	VectorSubtract (r_origin, r_entorigin, modelorg);

	glPushMatrix ();
	R_RotateForEntity (e);

	//
	// locate the proper data
	//
	data = (alias3data_t *)Mod_Extradata (e->model);
	maxnumsurf = data->numSurfaces;
	aliasframeinstant = e->aliasframeinstant;

	for (i=0;i<maxnumsurf;++i){

		paliashdr = (aliashdr_t *)((char*)data + data->ofsSurfaces[i]);
		
		if (!aliasframeinstant) {
			glPopMatrix();
			Con_Printf("R_DrawAliasObjectLight: missing instant for ent %s\n", e->model->name);
			return;
		}
		
		if (aliasframeinstant->shadowonly) continue;
		
		if ((e->frame >= paliashdr->numframes) || (e->frame < 0))
		{
			glPopMatrix();
			return;
		}
		
		VectorCopy(currentshadowlight->origin,oldlightpos);
		VectorCopy(currentshadowlight->origin,currentshadowlight->oldlightorigin);
		
		pose = paliashdr->frames[e->frame].firstpose;
		numposes = paliashdr->frames[e->frame].numposes;
		
		//Draw it!
		AliasGeoSender(paliashdr,aliasframeinstant);
		
		aliasframeinstant = aliasframeinstant->_next;
             
	} /* for paliashdr */

	glPopMatrix();
}


/*
=================
R_DrawSpriteModelWV

Draw a sprite texture with as tex0 coordinates the world space position of it's vertexes.
tex1 is the sprites texture coordinates.

=================
*/
void R_DrawSpriteModelWV (entity_t *e)
{
	vec3_t	point;
	mspriteframe_t	*frame;
	float		*up, *right;
	vec3_t		v_forward, v_right, v_up;
	msprite_t		*psprite;

	frame = R_GetSpriteFrame (e);
	psprite = currententity->model->cache.data;

	if (psprite->type == SPR_ORIENTED)
	{	// bullet marks on walls PENTA: bulltet marks in quake 1? never seen one ;)
		AngleVectors (currententity->angles, v_forward, v_right, v_up);
		up = v_up;
		right = v_right;
	}
	else
	{	// normal sprite
		up = vup;
		right = vright;
	}

    GL_Bind(frame->gl_texturenum);

	glBegin (GL_QUADS);

	qglMultiTexCoord2fARB(GL_TEXTURE1_ARB, 0.0f, 1.0f);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->left, right, point);
	glTexCoord3fv(point);
	glVertex3fv (point);

	qglMultiTexCoord2fARB(GL_TEXTURE1_ARB, 0.0f, 0.0f);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->left, right, point);
	glTexCoord3fv(point);
	glVertex3fv (point);

	qglMultiTexCoord2fARB(GL_TEXTURE1_ARB, 1.0f, 0.0f);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->right, right, point);
	glTexCoord3fv(point);
	glVertex3fv (point);

	qglMultiTexCoord2fARB(GL_TEXTURE1_ARB, 1.0f, 1.0f);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->right, right, point);
	glTexCoord3fv(point);
	glVertex3fv (point);
	
	glEnd ();

}

/**************************************************************

	PART3: Abstraction

***************************************************************/

/*
=============
GL_DrawWorldBumped

Draws the world bumped how this is done you don't have to know!
=============
*/
void R_DrawWorldBumpedGF() {

	if (!currentshadowlight->visible)
		return;

	glDepthMask (0);
	glShadeModel (GL_SMOOTH);

/*
	if (gl_geforce3) {
		
		if (currentshadowlight->filtercube) {
			//draw attent into dest alpha
			GL_DrawAlpha();
			GL_EnableAttentShaderGF3(NULL);
			R_DrawWorldWV(currentshadowlight->lightCmds);
			GL_DisableAttentShaderGF3();
			GL_ModulateAlphaDrawColor();
		} else {
			GL_AddColor();
		}
		glColor3fv(&currentshadowlight->color[0]);

		GL_EnableDiffuseShaderGF3(true,currentshadowlight->origin);
		R_DrawWorldGF3Diffuse(currentshadowlight->lightCmds);
		GL_DisableDiffuseShaderGF3();

		GL_EnableSpecularShaderGF3(true,currentshadowlight->origin);
		R_DrawWorldGF3Specular(currentshadowlight->lightCmds);
		GL_DisableDiffuseShaderGF3();

		glColor3f (1,1,1);
		glDisable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask (1);
		return;
	}
*/
	//	Diffuse bump mapping
	
	GL_DrawAlpha();
	GL_EnableDiffuseShader ();
	if (!currentshadowlight->isStatic) {
		R_DrawWorldLLV(&lightCmdsBuff[0]);
	} else {
		R_DrawWorldLLV(currentshadowlight->lightCmds);
	}
	GL_DisableDiffuseShader ();

	//	Attenuation
	
	GL_ModulateAlphaDrawAlpha();
	GL_EnableAttShader();
	R_DrawWorldATT(currentshadowlight->lightCmds);
	GL_DisableAttShader();
	
	//	Color map/light filter

	glMatrixMode(GL_TEXTURE);
	glPushMatrix();

	GL_SetupCubeMapMatrix(true);
	
	GL_ModulateAlphaDrawColor();
	glColor3fv(&currentshadowlight->baseColor[0]);
	GL_EnableColorShader (false);
	R_DrawWorldWV(currentshadowlight->lightCmds, false);
	GL_DisableColorShader (false);

	glPopMatrix();
	
	// Specular

	if ( gl_cardtype == GEFORCE || gl_cardtype == GEFORCE3 )
		GL_DrawAlpha();
	else
		GL_DrawSquareAlpha();

	GL_EnableSpecularShader ();
	
	R_DrawWorldHAV(currentshadowlight->lightCmds);

	GL_DisableSpecularShader ();

	if ( gl_cardtype == GENERIC ) { 
		int i;
		//raise specular to a high exponent my multiplying it a few times
		//this is evidently slow and we should use a better system for it
		GL_SquareAlpha();
		for (i=0; i<3; i++) {
				//why draw world att?: 1) I dont want to write new code for this
				//					   2) It uses the least bandwith/ setup
			R_DrawWorldATT (currentshadowlight->lightCmds);
		}
	}


	
	//	Attenttuation
	
	GL_ModulateAlphaDrawAlpha();
	GL_EnableAttShader();
	R_DrawWorldATT(currentshadowlight->lightCmds);
	GL_DisableAttShader();
	
	//	Color map/light filter
	glPushMatrix();

	GL_SetupCubeMapMatrix(true);

	GL_ModulateAlphaDrawColor();
	glColor3fv(&currentshadowlight->baseColor[0]);
	GL_EnableColorShader (true);
	R_DrawWorldWV(currentshadowlight->lightCmds, true);

	GL_DisableColorShader (true);


//	End separate specular

	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	glColor3f (1,1,1);
	glDisable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask (1);
}

void R_DrawWorldBumpedGEN() {
	R_DrawWorldBumpedGF();
}
/*
=============
GL_DrawBrushBumped

Draw a brush entity with bump maps
=============
*/
void R_DrawBrushBumped(entity_t *e) {

	//	Diffuse bump mapping
	
        GL_DrawAlpha();
	GL_EnableDiffuseShader ();
	R_DrawBrushLLV(e);
	GL_DisableDiffuseShader ();
	
	//	Attenttuation
	
	GL_ModulateAlphaDrawAlpha();
	GL_EnableAttShader();
	R_DrawBrushATT(e);
	GL_DisableAttShader();
	
	//	Color map/light filter

	glMatrixMode(GL_TEXTURE);
	glPushMatrix();

	GL_SetupCubeMapMatrix(false);

	GL_ModulateAlphaDrawColor();
	glColor3fv(&currentshadowlight->baseColor[0]);
	GL_EnableColorShader (false);
	R_DrawBrushWV(e, false);
	GL_DisableColorShader (false);

	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);


	//	Specular bump mapping

	if ( gl_cardtype == GEFORCE || gl_cardtype == GEFORCE3 )
		GL_DrawAlpha();
	else
		GL_DrawSquareAlpha();

	GL_EnableSpecularShader ();
	R_DrawBrushHAV(e);
	GL_DisableSpecularShader ();

	if ( gl_cardtype == GENERIC ) {
		int i;
		GL_SquareAlpha();
		//Why R_DrawBrushWV?: Same as for world but not WV is better
		for (i=0; i<3; i++) R_DrawBrushWV(e, false);
	}
	

	
	//	Attenttuation
	
	GL_ModulateAlphaDrawAlpha();
	GL_EnableAttShader();
	R_DrawBrushATT(e);
	GL_DisableAttShader();
	
	//	Color map/light filter

	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	GL_SetupCubeMapMatrix(false);

	GL_ModulateAlphaDrawColor();
	glColor3fv(&currentshadowlight->baseColor[0]);
	GL_EnableColorShader (true);
	R_DrawBrushWV(e, true);
	GL_DisableColorShader (true);

	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

}

/*
=============
R_DrawAliasBumped

Draw a alias entity with bump maps
=============
*/
void R_DrawAliasBumped(aliashdr_t *paliashdr, aliasframeinstant_t *instant) {

     
	//Diffuse bump mapping
	GL_DrawAlpha();
	GL_EnableDiffuseShader ();
	R_DrawAliasFrameLLV(paliashdr,instant);
	GL_DisableDiffuseShader ();

	//	Color map/light filter

glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	GL_SetupCubeMapMatrix(false);
	
	GL_ModulateAlphaDrawColor();

	glEnable(GL_LIGHTING);

	GL_EnableColorShader (false);
	R_DrawAliasFrameWV(paliashdr,instant, false);
	GL_DisableColorShader (false);

	glDisable(GL_LIGHTING);

	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	//Specular bump mapping
	if ( gl_cardtype == GEFORCE )
		GL_DrawAlpha();
	else
		GL_DrawSquareAlpha();

	GL_EnableSpecularShader ();
	R_DrawAliasFrameHAV(paliashdr,instant);
	GL_DisableSpecularShader ();

	if ( gl_cardtype == GENERIC ) {
		int i;
		GL_SquareAlpha();
		for (i=0; i<3; i++) R_DrawAliasFrameWV(paliashdr,instant, false);
	}


	//	Color map/light filter

	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	GL_SetupCubeMapMatrix(false);
	
	GL_ModulateAlphaDrawColor();

	glEnable(GL_LIGHTING);

	GL_EnableColorShader (true);
	R_DrawAliasFrameWV(paliashdr,instant, true);
	GL_DisableColorShader (true);

	glDisable(GL_LIGHTING);

	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

}
