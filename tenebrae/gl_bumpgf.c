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

Same as gl_bumpmap.c but geforce3&4 optimized 
These routines require 4 texture units an some need nvidia shaders

Most lights reqire 2 passes this way
1 diffuse
2 specular

If a light has a cubemap filter it requires 3 passes
1 attenuation
2 diffuse
3 specular
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

//<AWE> "diffuse_program_object" has to be defined static. Otherwise nameclash with "gl_bumpradeon.c".
static GLuint diffuse_program_object;
static GLuint specularalias_program_object; //He he nice name to type a lot

/*
Pixel shader for diffuse bump mapping does diffuse bumpmapping with norm cube, self shadowing & dist attent in
1 pass (thanx to the 4 texture units on a gf4)
*/
void GL_EnableDiffuseShaderGF3(qboolean world, vec3_t lightOrig) {

	float invrad = 1/currentshadowlight->radius;

	//tex 0 = normal map
	//tex 1 = nomalization cube map (tangent space light vector)
	//tex 2 = color map
	//tex 3 = (attenuation or light filter, depends on light settings but the actual
	//			register combiner setup does not change only the bound texture)

	GL_SelectTexture(GL_TEXTURE1_ARB);
	glEnable(GL_TEXTURE_CUBE_MAP_ARB);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, normcube_texture_object);

	GL_SelectTexture(GL_TEXTURE2_ARB);
	glEnable(GL_TEXTURE_2D);

	GL_SelectTexture(GL_TEXTURE3_ARB);
	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glLoadIdentity();
	if (currentshadowlight->filtercube) {
		glEnable(GL_TEXTURE_CUBE_MAP_ARB);
		glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, currentshadowlight->filtercube);
		GL_SetupCubeMapMatrix(world);
	} else {
		glEnable(GL_TEXTURE_3D);
		glBindTexture(GL_TEXTURE_3D, atten3d_texture_object);

		glTranslatef(0.5,0.5,0.5);
		glScalef(0.5,0.5,0.5);
		glScalef(invrad, invrad, invrad);
		glTranslatef(-lightOrig[0], -lightOrig[1], -lightOrig[2]);
	}

	GL_SelectTexture(GL_TEXTURE0_ARB);

	//combiner0 RGB: calculate 
	//	(normal map = A) dot (norm cubemap = B) save in Spare0 RGB
	//	(color map = C) mul (light filter = D) save in Spare1 RGB
	qglCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_A_NV, GL_TEXTURE0_ARB, GL_EXPAND_NORMAL_NV, GL_RGB);
	qglCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_B_NV, GL_TEXTURE1_ARB, GL_EXPAND_NORMAL_NV, GL_RGB);
	qglCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_C_NV, GL_TEXTURE2_ARB, GL_UNSIGNED_IDENTITY_NV , GL_RGB);
	qglCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_D_NV, GL_TEXTURE3_ARB, GL_UNSIGNED_IDENTITY_NV , GL_RGB);
	qglCombinerOutputNV(GL_COMBINER0_NV, GL_RGB, GL_SPARE0_NV, GL_SPARE1_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_TRUE, GL_FALSE, GL_FALSE);


	//combiner0 Alpha: store 8*expand(tang space light vect z comp) into Spare0 Alpha (this is the selfshadow term)
	qglCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_A_NV, GL_TEXTURE1_ARB, GL_EXPAND_NORMAL_NV, GL_BLUE);
	qglCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_B_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
	qglCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_C_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_BLUE);
	qglCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
	qglCombinerOutputNV(GL_COMBINER0_NV, GL_ALPHA, GL_DISCARD_NV, GL_DISCARD_NV, GL_SPARE0_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);

	//Only if the light is not white we use a second combiner
	//this is when the light is at its full brightness (for flickering lights)
	//and doesn't have any color (other than white)
	
	if ((currentshadowlight->color[0] != 1) || (currentshadowlight->color[1] != 1) || (currentshadowlight->color[2] != 1)) {
		qglCombinerParameteriNV(GL_NUM_GENERAL_COMBINERS_NV, 2);

		qglCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_A_NV, GL_SPARE1_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		qglCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_B_NV, GL_PRIMARY_COLOR_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		qglCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_C_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV , GL_RGB);
		qglCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV , GL_RGB);
		qglCombinerOutputNV(GL_COMBINER1_NV, GL_RGB, GL_SPARE1_NV, GL_DISCARD_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
		//alpha out = nothing
		qglCombinerOutputNV(GL_COMBINER1_NV, GL_ALPHA, GL_DISCARD_NV, GL_DISCARD_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
	} else {
		qglCombinerParameteriNV(GL_NUM_GENERAL_COMBINERS_NV, 1);
	}
	
	//final combiner: final RGB = (Spare 0 Alpha) * ( (Spare 0 RGB) * (Spare 1 RGB) )
	qglFinalCombinerInputNV(GL_VARIABLE_A_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
	//qglFinalCombinerInputNV(GL_VARIABLE_B_NV, GL_TEXTURE2_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
	qglFinalCombinerInputNV(GL_VARIABLE_B_NV, GL_E_TIMES_F_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
	//qglFinalCombinerInputNV(GL_VARIABLE_B_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_RGB);
	qglFinalCombinerInputNV(GL_VARIABLE_C_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
	qglFinalCombinerInputNV(GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
	qglFinalCombinerInputNV(GL_VARIABLE_E_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
	qglFinalCombinerInputNV(GL_VARIABLE_F_NV, GL_SPARE1_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);

	//final cominer alpha doesn't really matter we use A dot B
	qglFinalCombinerInputNV(GL_VARIABLE_G_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_BLUE);

	glEnable(GL_REGISTER_COMBINERS_NV);

    // Enable the vertex program.
    //qglBindProgramARB( GL_VERTEX_PROGRAM_ARB, diffuse_program_object );
    //glEnable( GL_VERTEX_PROGRAM_ARB );
	qglBindProgramNV( GL_VERTEX_PROGRAM_NV, diffuse_program_object );
    glEnable( GL_VERTEX_PROGRAM_NV );

}

void GL_DisableDiffuseShaderGF3() {

	qglCombinerParameteriNV(GL_NUM_GENERAL_COMBINERS_NV, 1);

	//tex 0 = normal map
	//tex 1 = nomalization cube map (tangent space light vector)
	//tex 2 = color map
	//tex 3 = (attenuation or light filter, depends on light settings)

	GL_SelectTexture(GL_TEXTURE1_ARB);
	glDisable(GL_TEXTURE_CUBE_MAP_ARB);

	GL_SelectTexture(GL_TEXTURE2_ARB);
	glDisable(GL_TEXTURE_2D);

	GL_SelectTexture(GL_TEXTURE3_ARB);
	if (currentshadowlight->filtercube) {
		glDisable(GL_TEXTURE_CUBE_MAP_ARB);
	} else {
		glDisable(GL_TEXTURE_3D);
	}
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	GL_SelectTexture(GL_TEXTURE0_ARB);
	glDisable(GL_REGISTER_COMBINERS_NV);

//    glDisable( GL_VERTEX_PROGRAM_ARB );
    glDisable( GL_VERTEX_PROGRAM_NV );
}

void GL_EnableSpecularShaderGF3(qboolean world, vec3_t lightOrig, qboolean alias) {

	vec3_t scaler = {0.5f, 0.5f, 0.5f};
	float invrad = 1/currentshadowlight->radius;

	//tex 0 = normal map
	//tex 1 = nomalization cube map (tangent space half angle)
	//tex 2 = color map
	//tex 3 = (attenuation or light filter, depends on light settings but the actual
	//			register combiner setup does not change only the bound texture)

	GL_SelectTexture(GL_TEXTURE1_ARB);
	glEnable(GL_TEXTURE_CUBE_MAP_ARB);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, normcube_texture_object);

	GL_SelectTexture(GL_TEXTURE2_ARB);

        glEnable(GL_TEXTURE_2D);

	GL_SelectTexture(GL_TEXTURE3_ARB);
	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glLoadIdentity();

	if (currentshadowlight->filtercube) {
		glEnable(GL_TEXTURE_CUBE_MAP_ARB);
		glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, currentshadowlight->filtercube);
		GL_SetupCubeMapMatrix(world);
	} else {
		glEnable(GL_TEXTURE_3D);
		glBindTexture(GL_TEXTURE_3D, atten3d_texture_object);

		glTranslatef(0.5,0.5,0.5);
		glScalef(0.5,0.5,0.5);
		glScalef(invrad, invrad, invrad);
		glTranslatef(-lightOrig[0], -lightOrig[1], -lightOrig[2]);


	}

	GL_SelectTexture(GL_TEXTURE0_ARB);

	qglCombinerParameteriNV(GL_NUM_GENERAL_COMBINERS_NV, 4);
	qglCombinerParameterfvNV(GL_CONSTANT_COLOR0_NV, &scaler[0]);

	//combiner0 RGB: calculate 
	//	(normal map = A) dot (norm cubemap = B) save in Spare0 RGB
	//	(gloss map = C) mul (light filter = D) save in Spare1 RGB
	qglCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_A_NV, GL_TEXTURE0_ARB, GL_EXPAND_NORMAL_NV, GL_RGB);
	qglCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_B_NV, GL_TEXTURE1_ARB, GL_EXPAND_NORMAL_NV, GL_RGB);
	qglCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_C_NV, GL_TEXTURE0_ARB, GL_UNSIGNED_IDENTITY_NV , GL_ALPHA);
	qglCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_D_NV, GL_TEXTURE3_ARB, GL_UNSIGNED_IDENTITY_NV , GL_RGB);
	qglCombinerOutputNV(GL_COMBINER0_NV, GL_RGB, GL_SPARE0_NV, GL_SPARE1_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_TRUE, GL_FALSE, GL_FALSE);

	//combiner0 Alpha: store 8*expand(tang space light vect z comp) into Spare1 Alpha (this is the selfshadow term)
	if (alias) {
		qglCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_A_NV, GL_SECONDARY_COLOR_NV, GL_EXPAND_NORMAL_NV, GL_BLUE);
		qglCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_B_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
		qglCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_C_NV, GL_SECONDARY_COLOR_NV, GL_EXPAND_NORMAL_NV, GL_BLUE);
		qglCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
	} else {
		qglCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_A_NV, GL_TEXTURE0_ARB, GL_EXPAND_NORMAL_NV, GL_BLUE);
		qglCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_B_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
		qglCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_C_NV, GL_TEXTURE0_ARB, GL_EXPAND_NORMAL_NV, GL_BLUE);
		qglCombinerInputNV(GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
	}

	qglCombinerOutputNV(GL_COMBINER0_NV, GL_ALPHA, GL_DISCARD_NV, GL_DISCARD_NV, GL_SPARE1_NV, GL_SCALE_BY_FOUR_NV, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);

	//rgb = multipy light with color
	qglCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_A_NV, GL_SPARE1_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
	qglCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_B_NV, GL_PRIMARY_COLOR_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
	qglCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_C_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV , GL_RGB);
	qglCombinerInputNV(GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV , GL_RGB);
	qglCombinerOutputNV(GL_COMBINER1_NV, GL_RGB, GL_SPARE1_NV, GL_DISCARD_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);

	//combiner1 Alpha: calculate 2*((N'dotH)^2 - 0.5f) -> store in Spare0 Alpha ("raise" to an exponent)
	qglCombinerInputNV(GL_COMBINER1_NV, GL_ALPHA, GL_VARIABLE_A_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_BLUE);
	qglCombinerInputNV(GL_COMBINER1_NV, GL_ALPHA, GL_VARIABLE_B_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_BLUE);
	qglCombinerInputNV(GL_COMBINER1_NV, GL_ALPHA, GL_VARIABLE_C_NV, GL_CONSTANT_COLOR0_NV, GL_SIGNED_NEGATE_NV, GL_BLUE);
	qglCombinerInputNV(GL_COMBINER1_NV, GL_ALPHA, GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
	qglCombinerOutputNV(GL_COMBINER1_NV, GL_ALPHA, GL_DISCARD_NV, GL_DISCARD_NV, GL_SPARE0_NV, GL_SCALE_BY_TWO_NV, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);

	//combiner2 Alpha: Raise specular further
	qglCombinerInputNV(GL_COMBINER2_NV, GL_ALPHA, GL_VARIABLE_A_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
	qglCombinerInputNV(GL_COMBINER2_NV, GL_ALPHA, GL_VARIABLE_B_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
	qglCombinerOutputNV(GL_COMBINER2_NV, GL_ALPHA, GL_SPARE0_NV, GL_DISCARD_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
	//combiner2 rgb: Do nothing
//	qglCombinerInputNV(GL_COMBINER2_NV, GL_RGB, GL_VARIABLE_A_NV, GL_SPARE1_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
//	qglCombinerInputNV(GL_COMBINER2_NV, GL_RGB, GL_VARIABLE_B_NV, GL_TEXTURE2_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
//	qglCombinerOutputNV(GL_COMBINER2_NV, GL_RGB, GL_SPARE1_NV, GL_DISCARD_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
	qglCombinerOutputNV(GL_COMBINER2_NV, GL_RGB, GL_DISCARD_NV, GL_DISCARD_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);

	//combiner3 Alpha: Raise specular further
	qglCombinerInputNV(GL_COMBINER3_NV, GL_ALPHA, GL_VARIABLE_A_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
	qglCombinerInputNV(GL_COMBINER3_NV, GL_ALPHA, GL_VARIABLE_B_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
	qglCombinerOutputNV(GL_COMBINER3_NV, GL_ALPHA, GL_SPARE0_NV, GL_DISCARD_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
	//combiner3 rgb: Do nothing
//	qglCombinerOutputNV(GL_COMBINER2_NV, GL_RGB, GL_DISCARD_NV, GL_DISCARD_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
	qglCombinerInputNV(GL_COMBINER3_NV, GL_RGB, GL_VARIABLE_A_NV, GL_SPARE1_NV, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
	qglCombinerInputNV(GL_COMBINER3_NV, GL_RGB, GL_VARIABLE_B_NV, GL_SPARE1_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
	qglCombinerInputNV(GL_COMBINER3_NV, GL_RGB, GL_VARIABLE_C_NV, GL_TEXTURE0_ARB, GL_UNSIGNED_IDENTITY_NV , GL_ALPHA);
	qglCombinerInputNV(GL_COMBINER3_NV, GL_RGB, GL_VARIABLE_D_NV, GL_TEXTURE3_ARB, GL_UNSIGNED_IDENTITY_NV , GL_RGB);
	qglCombinerOutputNV(GL_COMBINER3_NV, GL_RGB, GL_SPARE1_NV, GL_DISCARD_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);


	//final combiner: final RGB = (Spare 0 Alpha) * ( (Spare 0 RGB) * (Spare 1 RGB) )
	qglFinalCombinerInputNV(GL_VARIABLE_A_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);
	qglFinalCombinerInputNV(GL_VARIABLE_B_NV, GL_E_TIMES_F_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
	qglFinalCombinerInputNV(GL_VARIABLE_C_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
	qglFinalCombinerInputNV(GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
	qglFinalCombinerInputNV(GL_VARIABLE_E_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
	qglFinalCombinerInputNV(GL_VARIABLE_F_NV, GL_SPARE1_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);

	//final cominer alpha doesn't really matter we use A dot B
	qglFinalCombinerInputNV(GL_VARIABLE_G_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_BLUE);

	glEnable(GL_REGISTER_COMBINERS_NV);


    // Enable the vertex program.
//    qglBindProgramARB( GL_VERTEX_PROGRAM_ARB, diffuse_program_object );
//    glEnable( GL_VERTEX_PROGRAM_ARB );
	if (alias)
		qglBindProgramNV( GL_VERTEX_PROGRAM_NV, specularalias_program_object );
	else
		qglBindProgramNV( GL_VERTEX_PROGRAM_NV, diffuse_program_object );
	glEnable( GL_VERTEX_PROGRAM_NV );

}

/*
GL_DisableSpecularShaderGF3() ??
Same as GL_DisableDiffuseShaderGF3()
*/

void GL_EnableAttentShaderGF3(vec3_t lightOrig) {

	float invrad = 1/currentshadowlight->radius;
	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glLoadIdentity();
	glTranslatef(0.5,0.5,0.5);
	glScalef(0.5,0.5,0.5);
	glScalef(invrad, invrad, invrad);
	glTranslatef(-lightOrig[0],
				 -lightOrig[1],
				 -lightOrig[2]);
	
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_3D);
	glBindTexture(GL_TEXTURE_3D, atten3d_texture_object);
}

void GL_DisableAttentShaderGF3() {

	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glDisable(GL_TEXTURE_3D);
	glEnable(GL_TEXTURE_2D);
}

void R_DrawWorldGF3Diffuse(lightcmd_t *lightCmds) {

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

		GL_SelectTexture(GL_TEXTURE0_ARB);
		GL_Bind(t->gl_texturenum+1);
		GL_SelectTexture(GL_TEXTURE2_ARB);
		GL_Bind(t->gl_texturenum);

		glBegin(command);
		//v = surf->polys->verts[0];
		v = (float *)(&globalVertexTable[surf->polys->firstvertex]);
		for (i=0; i<num; i++, v+= VERTEXSIZE) {
			//skip attent texture coord.
			lightPos+=2;

			qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, v[3], v[4]);
			qglMultiTexCoord3fvARB(GL_TEXTURE1_ARB,
					      &lightCmds[lightPos].asFloat);
			lightPos+=3;
			//qglMultiTexCoord2fARB(GL_TEXTURE2_ARB, v[3], v[4]);
			//qglMultiTexCoord3fvARB(GL_TEXTURE3_ARB,&v[0]);
			glVertex3fv(&v[0]);
		}
		glEnd();
	}

	GL_SelectTexture(GL_TEXTURE0_ARB);
}

void R_DrawWorldGF3Specular(lightcmd_t *lightCmds) {

	int command, num, i;
	int lightPos = 0;
	vec3_t tsH,H;
	float* lightP;
	msurface_t *surf;
	float		*v;
	vec3_t lightDir;
	texture_t	*t;//XYZ

	//support flickering lights
	//VectorCopy(currentshadowlight->origin,lightOr);

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


		GL_SelectTexture(GL_TEXTURE0_ARB);
		GL_Bind(t->gl_texturenum+1);
		GL_SelectTexture(GL_TEXTURE2_ARB);
		GL_Bind(t->gl_texturenum);

		glBegin(command);
		//v = surf->polys->verts[0];
		v = (float *)(&globalVertexTable[surf->polys->firstvertex]);
		for (i=0; i<num; i++, v+= VERTEXSIZE) {
			lightPos+=2;//skip texcoords
			lightP = &lightCmds[lightPos].asFloat;

                        VectorCopy(lightP, lightDir);
			VectorNormalize(lightDir);
			lightPos+=3;

			//calculate local H vector and put it into tangent space

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

			//
			VectorAdd(lightDir,tsH,tsH);

			qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, v[3], v[4]);
			qglMultiTexCoord3fvARB(GL_TEXTURE1_ARB,&tsH[0]);
			glVertex3fv(&v[0]);
		}
		glEnd();
	}

	GL_SelectTexture(GL_TEXTURE0_ARB);
}

void R_DrawBrushGF3Diffuse(entity_t *e) {

	model_t	*model = e->model;
	msurface_t *surf;
	glpoly_t	*poly;
	int		i, j, count;
	brushlightinstant_t *ins = e->brushlightinstant;
	float	*v;
	texture_t *t; //XYZ

	count = 0;

	surf = &model->surfaces[model->firstmodelsurface];
	for (i=0; i<model->nummodelsurfaces; i++, surf++)
	{
		if (!ins->polygonVis[i]) continue;

		poly = surf->polys;

		//XYZ
		t = R_TextureAnimation (surf->texinfo->texture);

		GL_SelectTexture(GL_TEXTURE0_ARB);
		GL_Bind(t->gl_texturenum+1);
		GL_SelectTexture(GL_TEXTURE2_ARB);
		GL_Bind(t->gl_texturenum);

		glBegin(GL_TRIANGLE_FAN);
		//v = poly->verts[0];
		v = (float *)(&globalVertexTable[poly->firstvertex]);
		for (j=0 ; j<poly->numverts ; j++, v+= VERTEXSIZE)
		{	
			qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, v[3], v[4]);
			qglMultiTexCoord3fvARB(GL_TEXTURE1_ARB,&ins->tslights[count+j][0]);
			glVertex3fv(v);
		}
		glEnd();
		count+=surf->numedges;
	}	
}

void R_DrawBrushGF3Specular(entity_t *e) {

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

		GL_SelectTexture(GL_TEXTURE0_ARB);
		GL_Bind(t->gl_texturenum+1);
		GL_SelectTexture(GL_TEXTURE2_ARB);
		GL_Bind(t->gl_texturenum);

		glBegin(GL_TRIANGLE_FAN);
		//v = poly->verts[0];
		v = (float *)(&globalVertexTable[poly->firstvertex]);
		for (j=0 ; j<poly->numverts ; j++, v+= VERTEXSIZE)
		{	
			qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, v[3], v[4]);
			qglMultiTexCoord3fvARB(GL_TEXTURE1_ARB,&ins->tshalfangles[count+j][0]);
			glVertex3fv(v);
		}
		glEnd();
		count+=surf->numedges;
	}	
}

void R_DrawAliasFrameGF3Diffuse (aliashdr_t *paliashdr, aliasframeinstant_t *instant)
{

	mtriangle_t *tris;
	fstvert_t	*texcoords;
	int anim;
	int			*indecies;
	aliaslightinstant_t *linstant = instant->lightinstant;

	tris = (mtriangle_t *)((byte *)paliashdr + paliashdr->triangles);
	texcoords = (fstvert_t *)((byte *)paliashdr + paliashdr->texcoords);

	//bind normal map
	anim = (int)(cl.time*10) & 3;

	GL_SelectTexture(GL_TEXTURE0_ARB);
	GL_Bind(paliashdr->gl_texturenum[currententity->skinnum][anim]+1);
	GL_SelectTexture(GL_TEXTURE2_ARB);
	GL_Bind(paliashdr->gl_texturenum[currententity->skinnum][anim]);
	
	indecies = (int *)((byte *)paliashdr + paliashdr->indecies);

	glVertexPointer(3, GL_FLOAT, 0, instant->vertices);
	glEnableClientState(GL_VERTEX_ARRAY);

	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	qglClientActiveTextureARB(GL_TEXTURE1_ARB);
	glTexCoordPointer(3, GL_FLOAT, 0, linstant->tslights);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	//glDrawElements(GL_TRIANGLES,paliashdr->numtris*3,GL_UNSIGNED_INT,indecies);
	glDrawElements(GL_TRIANGLES,linstant->numtris*3,GL_UNSIGNED_INT,&linstant->indecies[0]);

	if (sh_noshadowpopping.value) {
		glStencilFunc(GL_LEQUAL, 1, 0xffffffff);
		glDrawElements(GL_TRIANGLES,(paliashdr->numtris*3)-(linstant->numtris*3),GL_UNSIGNED_INT,&linstant->indecies[linstant->numtris*3]);
		glStencilFunc(GL_EQUAL, 0, 0xffffffff);
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	GL_SelectTexture(GL_TEXTURE0_ARB);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void R_DrawAliasFrameGF3Specular (aliashdr_t *paliashdr, aliasframeinstant_t *instant)
{

	mtriangle_t *tris;
	fstvert_t	*texcoords;
	vec3_t		lightOr;
	int anim;
	int *indecies;
	aliaslightinstant_t *linstant = instant->lightinstant;

	tris = (mtriangle_t *)((byte *)paliashdr + paliashdr->triangles);
	texcoords = (fstvert_t *)((byte *)paliashdr + paliashdr->texcoords);

	VectorCopy(currentshadowlight->origin,lightOr);

	//bind normal map
	anim = (int)(cl.time*10) & 3;

	GL_SelectTexture(GL_TEXTURE0_ARB);
    GL_Bind(paliashdr->gl_texturenum[currententity->skinnum][anim]+1);
	GL_SelectTexture(GL_TEXTURE2_ARB);
    GL_Bind(paliashdr->gl_texturenum[currententity->skinnum][anim]);

	indecies = (int *)((byte *)paliashdr + paliashdr->indecies);

	glVertexPointer(3, GL_FLOAT, 0, instant->vertices);
	glEnableClientState(GL_VERTEX_ARRAY);

	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	qglClientActiveTextureARB(GL_TEXTURE1_ARB);
	glTexCoordPointer(3, GL_FLOAT, 0, linstant->tshalfangles);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	//to to correct self shadowing on alias models send the light vectors an extra time...
	qglClientActiveTextureARB(GL_TEXTURE2_ARB);
	glTexCoordPointer(3, GL_FLOAT, 0, linstant->tslights);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glDrawElements(GL_TRIANGLES,linstant->numtris*3,GL_UNSIGNED_INT,&linstant->indecies[0]);

	if (sh_noshadowpopping.value) {
		glStencilFunc(GL_LEQUAL, 1, 0xffffffff);
		//Con_Printf("%i backfacing tris\n",(paliashdr->numtris*3)-(linstant->numtris*3));
		glDrawElements(GL_TRIANGLES,(paliashdr->numtris*3)-(linstant->numtris*3),GL_UNSIGNED_INT,&linstant->indecies[linstant->numtris*3]);
		glStencilFunc(GL_EQUAL, 0, 0xffffffff);
	}

	//qglClientActiveTextureARB(GL_TEXTURE2_ARB);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglClientActiveTextureARB(GL_TEXTURE1_ARB);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);


	GL_SelectTexture(GL_TEXTURE0_ARB);
	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void R_DrawWorldBumpedGF3() {

	if (!currentshadowlight->visible)
		return;

	glDepthMask (0);
	glShadeModel (GL_SMOOTH);

	if (currentshadowlight->filtercube) {
		//draw attent into dest alpha
		GL_DrawAlpha();
		GL_EnableAttentShaderGF3(currentshadowlight->origin);
		R_DrawWorldWV(currentshadowlight->lightCmds, false);
		GL_DisableAttentShaderGF3();
		GL_ModulateAlphaDrawColor();
	} else {
		GL_AddColor();
	}
	glColor3fv(&currentshadowlight->color[0]);

	GL_EnableSpecularShaderGF3(true,currentshadowlight->origin,false);
	R_DrawWorldGF3Specular(currentshadowlight->lightCmds);
	GL_DisableDiffuseShaderGF3();

	GL_EnableDiffuseShaderGF3(true,currentshadowlight->origin);
	R_DrawWorldGF3Diffuse(currentshadowlight->lightCmds);
	GL_DisableDiffuseShaderGF3();

	glColor3f (1,1,1);
	glDisable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask (1);
}

void R_DrawBrushBumpedGF3(entity_t *e) {

	if (currentshadowlight->filtercube) {
		//draw attent into dest alpha
		GL_DrawAlpha();
		GL_EnableAttentShaderGF3(((brushlightinstant_t *)e->brushlightinstant)->lightpos);
		R_DrawBrushWV(e, false);
		GL_DisableAttentShaderGF3();
		GL_ModulateAlphaDrawColor();
	} else {
		GL_AddColor();
	}
	glColor3fv(&currentshadowlight->color[0]);

	GL_EnableSpecularShaderGF3(false,((brushlightinstant_t *)e->brushlightinstant)->lightpos,false);
	R_DrawBrushGF3Specular(e);
	GL_DisableDiffuseShaderGF3();

        GL_EnableDiffuseShaderGF3(false,((brushlightinstant_t *)e->brushlightinstant)->lightpos);
	R_DrawBrushGF3Diffuse(e);
	GL_DisableDiffuseShaderGF3();
}

void R_DrawAliasBumpedGF3(aliashdr_t *paliashdr, aliasframeinstant_t *instant) {

	if (currentshadowlight->filtercube) {
		//draw attent into dest alpha
		GL_DrawAlpha();
		GL_EnableAttentShaderGF3(instant->lightinstant->lightpos);
		R_DrawAliasFrameWV(paliashdr,instant, false);
		GL_DisableAttentShaderGF3();
		GL_ModulateAlphaDrawColor();
	} else {
		GL_AddColor();
	}
	glColor3fv(&currentshadowlight->color[0]);

	GL_EnableSpecularShaderGF3(false,instant->lightinstant->lightpos,true);
	R_DrawAliasFrameGF3Specular(paliashdr,instant);
	GL_DisableDiffuseShaderGF3();

        GL_EnableDiffuseShaderGF3(false,instant->lightinstant->lightpos);
	R_DrawAliasFrameGF3Diffuse(paliashdr,instant);
	GL_DisableDiffuseShaderGF3();
}

/*
	Vertex programs 
*/


/*
  Thisone does not do anything too usefull, it just copyies some things around
  instead of sending the save coordinates for unit0/2 we send them once and copy
  them here, this saves some bandwith and is slightly faster.
*/ 
  char vpDiffuseGF3 [] = 
      "!!VP1.1 # Diffuse bumpmapping vetex program.\n"
	  "OPTION NV_position_invariant;"
      // Generates a necessary input for the diffuse bumpmapping registers
      // 
      // c[0]...c[3]      contains the modelview projection composite matrix
      // c[4]...c[7]      contains the texture matrix of unit 3
      // v[OPOS]          contains the per-vertex position
      // v[TEX1]          contains the per-vertex tangent space light vector
      // v[TEX0]          contains the per-vertex texture coordinate 0

      // o[HPOS]          output register for homogeneous position
      // o[TEX0]          output register for texture coordinate 0
      // o[TEX1]          output register for texture coordinate 1
      // o[TEX2]          output register for texture coordinate 2
      // o[TEX3]          output register for texture coordinate 3

      // Transform vertex to view-space

      // Transform vertex by texture matrix and copy to output
      "DP4   o[TEX3].x, v[OPOS], c[4];"
      "DP4   o[TEX3].y, v[OPOS], c[5];"
      "DP4   o[TEX3].z, v[OPOS], c[6];"
      "DP4   o[TEX3].w, v[OPOS], c[7];"

	  //copy tex coords of unit 0 to unit 2
	  "MOV   o[TEX0], v[TEX0];"
	  "MOV   o[TEX2], v[TEX0];"

	  "MOV   o[TEX1], v[TEX1];"

	  "MOV   o[COL0], v[COL0];"

      "END";

/*
	Only used for specular on alias models when noshadowpopping is enabled...
*/ 
  char vpSpecularAliasGF3 [] = 
      "!!VP1.1 # Diffuse bumpmapping vetex program.\n"
	  "OPTION NV_position_invariant;"
      // Generates a necessary input for the diffuse bumpmapping registers
      // 
      // c[0]...c[3]      contains the modelview projection composite matrix
      // c[4]...c[7]      contains the texture matrix of unit 3
      // v[OPOS]          contains the per-vertex position
      // v[TEX1]          contains the per-vertex tangent space light vector
      // v[TEX0]          contains the per-vertex texture coordinate 0
	  // v[TEX2]		  contains the per-vertex light vector

      // o[HPOS]          output register for homogeneous position
      // o[TEX0]          output register for texture coordinate 0
      // o[TEX1]          output register for texture coordinate 1
      // o[TEX2]          output register for texture coordinate 2
      // o[TEX3]          output register for texture coordinate 3

      // Transform vertex to view-space

      // Transform vertex by texture matrix and copy to output
      "DP4   o[TEX3].x, v[OPOS], c[4];"
      "DP4   o[TEX3].y, v[OPOS], c[5];"
      "DP4   o[TEX3].z, v[OPOS], c[6];"
      "DP4   o[TEX3].w, v[OPOS], c[7];"


	  "MOV   o[TEX0], v[TEX0];"

	  //range compress into secondary color
	  "MAD   o[COL1], v[TEX2], c[20], c[20];"

	  //copy tex coords of unit 0 to unit 2
	  "MOV   o[TEX2], v[TEX0];"

	  "MOV   o[TEX1], v[TEX1];"

	  "MOV   o[COL0], v[COL0];"

      "END";

void R_LoadVertexProgram() {

	GLint errPos, errCode;
	const GLubyte *errString;

	if ( gl_cardtype != GEFORCE3 ) return;

	// Create the vertex program.
	qglGenProgramsNV( 1, &diffuse_program_object);

	qglLoadProgramNV( GL_VERTEX_PROGRAM_NV, diffuse_program_object,
					(int)strlen(vpDiffuseGF3), (const GLubyte *) vpDiffuseGF3);

	qglGenProgramsNV( 1, &specularalias_program_object);

	qglLoadProgramNV( GL_VERTEX_PROGRAM_NV, specularalias_program_object,
					(int)strlen(vpSpecularAliasGF3), (const GLubyte *) vpSpecularAliasGF3);

	if ( (errCode = glGetError()) != GL_NO_ERROR ) {
		errString = gluErrorString( errCode );
//		glGetIntegerv( GL_PROGRAM_ERROR_POSITION_ARB, &errPos);
		glGetIntegerv( GL_PROGRAM_ERROR_POSITION_NV, &errPos);
		Con_Printf("LoadVertexProgram: %s\n", errString);
		Con_Printf("error is located at line: %d\n", errPos);
		exit( -1 );
	} else {
		Con_Printf("VertexProgram loaded\n");
	}

    
    // Track the concatenation of the modelview and projection matrix in registers 0-3.
    qglTrackMatrixNV( GL_VERTEX_PROGRAM_NV, 0, GL_MODELVIEW_PROJECTION_NV, GL_IDENTITY_NV );

    // Track the texture unit 3 maxtix in registers 4-7
    qglTrackMatrixNV( GL_VERTEX_PROGRAM_NV, 4, GL_TEXTURE3_ARB, GL_IDENTITY_NV );

	//store 0.5 0.5 0.5 0.5 in register 8
	qglProgramParameter4fNV( GL_VERTEX_PROGRAM_NV, 20, 0.5, 0.5, 0.5, 0.5);
}
