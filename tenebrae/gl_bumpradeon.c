/*
Copyright (C) 2001-2002 Charles Hollemeersch
Radeon Version (C) 2002 Jarno Paananen

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

Same as gl_bumpmap.c but Radeon 8500+ optimized 
These routines require 6 texture units, vertex shader and pixel shader

This could be further optimized for Radeon 9700 (specular exponent for
example), but would need better documentation and extension.

All lights not require only 1 pass:
1 diffuse + specular with optional light filter

*/

#include "quakedef.h"

#include "glATI.h"

PFNGLGENFRAGMENTSHADERSATIPROC		qglGenFragmentShadersATI = NULL;
PFNGLBINDFRAGMENTSHADERATIPROC		qglBindFragmentShaderATI = NULL;
PFNGLDELETEFRAGMENTSHADERATIPROC 	qglDeleteFragmentShaderATI = NULL;
PFNGLBEGINFRAGMENTSHADERATIPROC 	qglBeginFragmentShaderATI = NULL;
PFNGLENDFRAGMENTSHADERATIPROC 		qglEndFragmentShaderATI = NULL;
PFNGLPASSTEXCOORDATIPROC		qglPassTexCoordATI = NULL;
PFNGLSAMPLEMAPATIPROC			qglSampleMapATI = NULL;
PFNGLCOLORFRAGMENTOP1ATIPROC		qglColorFragmentOp1ATI = NULL;
PFNGLCOLORFRAGMENTOP2ATIPROC		qglColorFragmentOp2ATI = NULL;
PFNGLCOLORFRAGMENTOP3ATIPROC		qglColorFragmentOp3ATI = NULL;
PFNGLALPHAFRAGMENTOP1ATIPROC		qglAlphaFragmentOp1ATI = NULL;
PFNGLALPHAFRAGMENTOP2ATIPROC		qglAlphaFragmentOp2ATI = NULL;
PFNGLALPHAFRAGMENTOP3ATIPROC		qglAlphaFragmentOp3ATI = NULL;
PFNGLSETFRAGMENTSHADERCONSTANTATIPROC	qglSetFragmentShaderConstantATI = NULL;
PFNGLBEGINVERTEXSHADEREXTPROC		qglBeginVertexShaderEXT = NULL;
PFNGLENDVERTEXSHADEREXTPROC		qglEndVertexShaderEXT = NULL;
PFNGLBINDVERTEXSHADEREXTPROC		qglBindVertexShaderEXT = NULL;
PFNGLGENVERTEXSHADERSEXTPROC		qglGenVertexShadersEXT = NULL;
PFNGLDELETEVERTEXSHADEREXTPROC		qglDeleteVertexShaderEXT = NULL;
PFNGLSHADEROP1EXTPROC			qglShaderOp1EXT = NULL;
PFNGLSHADEROP2EXTPROC			qglShaderOp2EXT = NULL;
PFNGLSHADEROP3EXTPROC			qglShaderOp3EXT = NULL;
PFNGLSWIZZLEEXTPROC			qglSwizzleEXT = NULL;
PFNGLWRITEMASKEXTPROC			qglWriteMaskEXT = NULL;
PFNGLINSERTCOMPONENTEXTPROC		qglInsertComponentEXT = NULL;
PFNGLEXTRACTCOMPONENTEXTPROC		qglExtractComponentEXT = NULL;
PFNGLGENSYMBOLSEXTPROC			qglGenSymbolsEXT = NULL;
PFNGLSETINVARIANTEXTPROC		qglSetInvariantEXT = NULL;
PFNGLSETLOCALCONSTANTEXTPROC		qglSetLocalConstantEXT = NULL;
PFNGLVARIANTBVEXTPROC			qglVariantbvEXT = NULL;
PFNGLVARIANTSVEXTPROC			qglVariantsvEXT = NULL;
PFNGLVARIANTIVEXTPROC			qglVariantivEXT = NULL;
PFNGLVARIANTFVEXTPROC			qglVariantfvEXT = NULL;
PFNGLVARIANTDVEXTPROC			qglVariantdvEXT = NULL;
PFNGLVARIANTUBVEXTPROC			qglVariantubvEXT = NULL;
PFNGLVARIANTUSVEXTPROC			qglVariantusvEXT = NULL;
PFNGLVARIANTUIVEXTPROC			qglVariantuivEXT = NULL;
PFNGLVARIANTPOINTEREXTPROC		qglVariantPointerEXT = NULL;
PFNGLENABLEVARIANTCLIENTSTATEEXTPROC	qglEnableVariantClientStateEXT = NULL;
PFNGLDISABLEVARIANTCLIENTSTATEEXTPROC	qglDisableVariantClientStateEXT = NULL;
PFNGLBINDLIGHTPARAMETEREXTPROC		qglBindLightParameterEXT = NULL;
PFNGLBINDMATERIALPARAMETEREXTPROC	qglBindMaterialParameterEXT = NULL;
PFNGLBINDTEXGENPARAMETEREXTPROC		qglBindTexGenParameterEXT = NULL;
PFNGLBINDTEXTUREUNITPARAMETEREXTPROC	qglBindTextureUnitParameterEXT = NULL;
PFNGLBINDPARAMETEREXTPROC		qglBindParameterEXT = NULL;
PFNGLISVARIANTENABLEDEXTPROC		qglIsVariantEnabledEXT = NULL;
PFNGLGETVARIANTBOOLEANVEXTPROC		qglGetVariantBooleanvEXT = NULL;
PFNGLGETVARIANTINTEGERVEXTPROC		qglGetVariantIntegervEXT = NULL;
PFNGLGETVARIANTFLOATVEXTPROC		qglGetVariantFloatvEXT = NULL;
PFNGLGETVARIANTPOINTERVEXTPROC		qglGetVariantPointervEXT = NULL;
PFNGLGETINVARIANTBOOLEANVEXTPROC	qglGetInvariantBooleanvEXT = NULL;
PFNGLGETINVARIANTINTEGERVEXTPROC	qglGetInvariantIntegervEXT = NULL;
PFNGLGETINVARIANTFLOATVEXTPROC		qglGetInvariantFloatvEXT = NULL;
PFNGLGETLOCALCONSTANTBOOLEANVEXTPROC	qglGetLocalConstantBooleanvEXT = NULL;
PFNGLGETLOCALCONSTANTINTEGERVEXTPROC	qglGetLocalConstantIntegervEXT = NULL;
PFNGLGETLOCALCONSTANTFLOATVEXTPROC	qglGetLocalConstantFloatvEXT = NULL;

PFNGLPNTRIANGLESIATIPROC qglPNTrianglesiATI = NULL;
PFNGLPNTRIANGLESFATIPROC qglPNTrianglesfATI = NULL;

static unsigned int fragment_shaders;
static unsigned int vertex_shaders;

//#define RADEONDEBUG

#ifdef RADEONDEBUG
void checkerror()
{
    GLuint error = glGetError();
    if ( error != GL_NO_ERROR )
    {
        _asm { int 3 };
    }
}
#else

#define checkerror() do { } while(0)

#endif

#if defined (__APPLE__) || defined (MACOSX)

extern void *	Sys_GetProcAddress (const char *theName, qboolean theSafeMode);

qboolean	GL_LookupSymbolsRadeon (void)
{
    qglGenFragmentShadersATI = Sys_GetProcAddress ("glGenFragmentShadersATI", false);
    qglBindFragmentShaderATI = Sys_GetProcAddress ("glBindFragmentShaderATI", false);
    qglDeleteFragmentShaderATI = Sys_GetProcAddress ("glDeleteFragmentShaderATI", false);
    qglBeginFragmentShaderATI = Sys_GetProcAddress ("glBeginFragmentShaderATI", false);
    qglEndFragmentShaderATI = Sys_GetProcAddress ("glEndFragmentShaderATI", false);
    qglPassTexCoordATI = Sys_GetProcAddress ("glPassTexCoordATI", false);
    qglSampleMapATI = Sys_GetProcAddress ("glSampleMapATI", false);
    qglColorFragmentOp1ATI = Sys_GetProcAddress ("glColorFragmentOp1ATI", false);
    qglColorFragmentOp2ATI = Sys_GetProcAddress ("glColorFragmentOp2ATI", false);
    qglColorFragmentOp3ATI = Sys_GetProcAddress ("glColorFragmentOp3ATI", false);
    qglAlphaFragmentOp1ATI = Sys_GetProcAddress ("glAlphaFragmentOp1ATI", false);
    qglAlphaFragmentOp2ATI = Sys_GetProcAddress ("glAlphaFragmentOp2ATI", false);
    qglAlphaFragmentOp3ATI = Sys_GetProcAddress ("glAlphaFragmentOp3ATI", false);
    qglSetFragmentShaderConstantATI = Sys_GetProcAddress ("glSetFragmentShaderConstantATI", false);
    qglBeginVertexShaderEXT = Sys_GetProcAddress ("glBeginVertexShaderEXT", false);            
    qglEndVertexShaderEXT = Sys_GetProcAddress ("glEndVertexShaderEXT", false);          
    qglBindVertexShaderEXT = Sys_GetProcAddress ("glBindVertexShaderEXT", false);               
    qglGenVertexShadersEXT = Sys_GetProcAddress ("glGenVertexShadersEXT", false);               
    qglDeleteVertexShaderEXT = Sys_GetProcAddress ("glDeleteVertexShaderEXT", false);         
    qglShaderOp1EXT = Sys_GetProcAddress ("glShaderOp1EXT", false);            
    qglShaderOp2EXT = Sys_GetProcAddress ("glShaderOp2EXT", false);            
    qglShaderOp3EXT = Sys_GetProcAddress ("glShaderOp3EXT", false);            
    qglSwizzleEXT = Sys_GetProcAddress ("glSwizzleEXT", false);          
    qglWriteMaskEXT = Sys_GetProcAddress ("glWriteMaskEXT", false);            
    qglInsertComponentEXT = Sys_GetProcAddress ("glInsertComponentEXT", false);          
    qglExtractComponentEXT = Sys_GetProcAddress ("glExtractComponentEXT", false);               
    qglGenSymbolsEXT = Sys_GetProcAddress ("glGenSymbolsEXT", false);         
    qglSetInvariantEXT = Sys_GetProcAddress ("glSetInvariantEXT", false);           
    qglSetLocalConstantEXT = Sys_GetProcAddress ("glSetLocalConstantEXT", false);               
    qglVariantbvEXT = Sys_GetProcAddress ("glVariantbvEXT", false);            
    qglVariantsvEXT = Sys_GetProcAddress ("glVariantsvEXT", false);            
    qglVariantivEXT = Sys_GetProcAddress ("glVariantivEXT", false);            
    qglVariantfvEXT = Sys_GetProcAddress ("glVariantfvEXT", false);            
    qglVariantdvEXT = Sys_GetProcAddress ("glVariantdvEXT", false);            
    qglVariantubvEXT = Sys_GetProcAddress ("glVariantubvEXT", false);         
    qglVariantusvEXT = Sys_GetProcAddress ("glVariantusvEXT", false);         
    qglVariantuivEXT = Sys_GetProcAddress ("glVariantuivEXT", false);         
    qglVariantPointerEXT = Sys_GetProcAddress ("glVariantPointerEXT", false);             
    qglEnableVariantClientStateEXT = Sys_GetProcAddress ("glEnableVariantClientStateEXT", false);
    qglDisableVariantClientStateEXT = Sys_GetProcAddress ("glDisableVariantClientStateEXT", false);
    qglBindLightParameterEXT = Sys_GetProcAddress ("glBindLightParameterEXT", false);
    qglBindMaterialParameterEXT = Sys_GetProcAddress ("glBindMaterialParameterEXT", false);
    qglBindTexGenParameterEXT = Sys_GetProcAddress ("glBindTexGenParameterEXT", false);
    qglBindTextureUnitParameterEXT = Sys_GetProcAddress ("glBindTextureUnitParameterEXT", false);
    qglBindParameterEXT = Sys_GetProcAddress ("glBindParameterEXT", false);
    qglIsVariantEnabledEXT = Sys_GetProcAddress ("glIsVariantEnabledEXT", false);
    qglGetVariantBooleanvEXT = Sys_GetProcAddress ("glGetVariantBooleanvEXT", false);
    qglGetVariantIntegervEXT = Sys_GetProcAddress ("glGetVariantIntegervEXT", false);
    qglGetVariantFloatvEXT = Sys_GetProcAddress ("glGetVariantFloatvEXT", false);
    qglGetVariantPointervEXT = Sys_GetProcAddress ("glGetVariantPointervEXT", false);
    qglGetInvariantBooleanvEXT = Sys_GetProcAddress ("glGetInvariantBooleanvEXT", false);
    qglGetInvariantIntegervEXT = Sys_GetProcAddress ("glGetInvariantIntegervEXT", false);
    qglGetInvariantFloatvEXT = Sys_GetProcAddress ("glGetInvariantFloatvEXT", false);
    qglGetLocalConstantBooleanvEXT = Sys_GetProcAddress ("glGetLocalConstantBooleanvEXT", false);
    qglGetLocalConstantIntegervEXT = Sys_GetProcAddress ("glGetLocalConstantIntegervEXT", false);
    qglGetLocalConstantFloatvEXT = Sys_GetProcAddress ("glGetLocalConstantFloatvEXT", false);
	qglPNTrianglesiATI = Sys_GetProcAddress ("glPNTrianglesiATI", false);
	qglPNTrianglesfATI = Sys_GetProcAddress ("glPNTrianglesfATI", false);

    if (qglGenFragmentShadersATI != NULL &&
        qglBindFragmentShaderATI != NULL &&
        qglDeleteFragmentShaderATI != NULL &&
        qglBeginFragmentShaderATI != NULL &&
        qglEndFragmentShaderATI != NULL &&
        qglPassTexCoordATI != NULL &&
        qglSampleMapATI != NULL &&
        qglColorFragmentOp1ATI != NULL &&
        qglColorFragmentOp2ATI != NULL &&
        qglColorFragmentOp3ATI != NULL &&
        qglAlphaFragmentOp1ATI != NULL &&
        qglAlphaFragmentOp2ATI != NULL &&
        qglAlphaFragmentOp3ATI != NULL &&
        qglSetFragmentShaderConstantATI != NULL &&
        qglBeginVertexShaderEXT != NULL &&          
        qglEndVertexShaderEXT != NULL &&         
        qglBindVertexShaderEXT != NULL &&               
        qglGenVertexShadersEXT != NULL &&               
        qglDeleteVertexShaderEXT != NULL &&         
        qglShaderOp1EXT != NULL &&           
        qglShaderOp2EXT != NULL &&            
        qglShaderOp3EXT != NULL &&           
        qglSwizzleEXT != NULL &&         
        qglWriteMaskEXT != NULL &&             
        qglInsertComponentEXT != NULL &&           
        qglExtractComponentEXT != NULL &&               
        qglGenSymbolsEXT != NULL &&        
        qglSetInvariantEXT != NULL &&            
        qglSetLocalConstantEXT != NULL &&                
        qglVariantbvEXT != NULL &&             
        qglVariantsvEXT != NULL &&            
        qglVariantivEXT != NULL &&             
        qglVariantfvEXT != NULL &&             
        qglVariantdvEXT != NULL &&            
        qglVariantubvEXT != NULL &&          
        qglVariantusvEXT != NULL &&         
        qglVariantuivEXT != NULL &&         
        qglVariantPointerEXT != NULL &&             
        qglEnableVariantClientStateEXT != NULL && 
        qglDisableVariantClientStateEXT != NULL && 
        qglBindLightParameterEXT != NULL && 
        qglBindMaterialParameterEXT != NULL && 
        qglBindTexGenParameterEXT != NULL && 
        qglBindTextureUnitParameterEXT != NULL && 
        qglBindParameterEXT != NULL && 
        qglIsVariantEnabledEXT != NULL && 
        qglGetVariantBooleanvEXT != NULL && 
        qglGetVariantIntegervEXT != NULL && 
        qglGetVariantFloatvEXT != NULL && 
        qglGetVariantPointervEXT != NULL && 
        qglGetInvariantBooleanvEXT != NULL && 
        qglGetInvariantIntegervEXT != NULL && 
        qglGetInvariantFloatvEXT != NULL && 
        qglGetLocalConstantBooleanvEXT != NULL && 
        qglGetLocalConstantIntegervEXT != NULL && 
        qglGetLocalConstantFloatvEXT != NULL &&
		qglPNTrianglesiATI != NULL &&
		qglPNTrianglesfATI != NULL)
    {
        return (true);
    }
    
    return (false);
}

#endif /* __APPLE__ || MACOSX */

void GL_CreateShadersRadeon()
{
    float scaler[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    GLuint mvp, modelview, zcomp;
    GLuint texturematrix, texturematrix2;
    GLuint vertex;
    GLuint texcoord0;
    GLuint texcoord1;
    GLuint texcoord2;
    GLuint color;
    GLuint disttemp, disttemp2;
    GLuint fogstart, fogend;

#if !defined(__APPLE__) && !defined (MACOSX)
    SAFE_GET_PROC( qglGenFragmentShadersATI, PFNGLGENFRAGMENTSHADERSATIPROC, "glGenFragmentShadersATI");
    SAFE_GET_PROC( qglBindFragmentShaderATI, PFNGLBINDFRAGMENTSHADERATIPROC, "glBindFragmentShaderATI");
    SAFE_GET_PROC( qglDeleteFragmentShaderATI, PFNGLDELETEFRAGMENTSHADERATIPROC, "glDeleteFragmentShaderATI");
    SAFE_GET_PROC( qglBeginFragmentShaderATI, PFNGLBEGINFRAGMENTSHADERATIPROC, "glBeginFragmentShaderATI");
    SAFE_GET_PROC( qglEndFragmentShaderATI, PFNGLENDFRAGMENTSHADERATIPROC, "glEndFragmentShaderATI");
    SAFE_GET_PROC( qglPassTexCoordATI, PFNGLPASSTEXCOORDATIPROC, "glPassTexCoordATI");
    SAFE_GET_PROC( qglSampleMapATI, PFNGLSAMPLEMAPATIPROC, "glSampleMapATI");
    SAFE_GET_PROC( qglColorFragmentOp1ATI, PFNGLCOLORFRAGMENTOP1ATIPROC, "glColorFragmentOp1ATI");
    SAFE_GET_PROC( qglColorFragmentOp2ATI, PFNGLCOLORFRAGMENTOP2ATIPROC, "glColorFragmentOp2ATI");
    SAFE_GET_PROC( qglColorFragmentOp3ATI, PFNGLCOLORFRAGMENTOP3ATIPROC, "glColorFragmentOp3ATI");
    SAFE_GET_PROC( qglAlphaFragmentOp1ATI, PFNGLALPHAFRAGMENTOP1ATIPROC, "glAlphaFragmentOp1ATI");
    SAFE_GET_PROC( qglAlphaFragmentOp2ATI, PFNGLALPHAFRAGMENTOP2ATIPROC, "glAlphaFragmentOp2ATI");
    SAFE_GET_PROC( qglAlphaFragmentOp3ATI, PFNGLALPHAFRAGMENTOP3ATIPROC, "glAlphaFragmentOp3ATI");
    SAFE_GET_PROC( qglSetFragmentShaderConstantATI, PFNGLSETFRAGMENTSHADERCONSTANTATIPROC, "glSetFragmentShaderConstantATI");
    SAFE_GET_PROC( qglBeginVertexShaderEXT, PFNGLBEGINVERTEXSHADEREXTPROC, "glBeginVertexShaderEXT");            
    SAFE_GET_PROC( qglEndVertexShaderEXT, PFNGLENDVERTEXSHADEREXTPROC, "glEndVertexShaderEXT");          
    SAFE_GET_PROC( qglBindVertexShaderEXT, PFNGLBINDVERTEXSHADEREXTPROC, "glBindVertexShaderEXT");               
    SAFE_GET_PROC( qglGenVertexShadersEXT, PFNGLGENVERTEXSHADERSEXTPROC, "glGenVertexShadersEXT");               
    SAFE_GET_PROC( qglDeleteVertexShaderEXT, PFNGLDELETEVERTEXSHADEREXTPROC, "glDeleteVertexShaderEXT");         
    SAFE_GET_PROC( qglShaderOp1EXT, PFNGLSHADEROP1EXTPROC, "glShaderOp1EXT");            
    SAFE_GET_PROC( qglShaderOp2EXT, PFNGLSHADEROP2EXTPROC, "glShaderOp2EXT");            
    SAFE_GET_PROC( qglShaderOp3EXT, PFNGLSHADEROP3EXTPROC, "glShaderOp3EXT");            
    SAFE_GET_PROC( qglSwizzleEXT, PFNGLSWIZZLEEXTPROC, "glSwizzleEXT");          
    SAFE_GET_PROC( qglWriteMaskEXT, PFNGLWRITEMASKEXTPROC, "glWriteMaskEXT");            
    SAFE_GET_PROC( qglInsertComponentEXT, PFNGLINSERTCOMPONENTEXTPROC, "glInsertComponentEXT");          
    SAFE_GET_PROC( qglExtractComponentEXT, PFNGLEXTRACTCOMPONENTEXTPROC, "glExtractComponentEXT");               
    SAFE_GET_PROC( qglGenSymbolsEXT, PFNGLGENSYMBOLSEXTPROC, "glGenSymbolsEXT");         
    SAFE_GET_PROC( qglSetInvariantEXT, PFNGLSETINVARIANTEXTPROC, "glSetInvariantEXT");           
    SAFE_GET_PROC( qglSetLocalConstantEXT, PFNGLSETLOCALCONSTANTEXTPROC, "glSetLocalConstantEXT");               
    SAFE_GET_PROC( qglVariantbvEXT, PFNGLVARIANTBVEXTPROC, "glVariantbvEXT");            
    SAFE_GET_PROC( qglVariantsvEXT, PFNGLVARIANTSVEXTPROC, "glVariantsvEXT");            
    SAFE_GET_PROC( qglVariantivEXT, PFNGLVARIANTIVEXTPROC, "glVariantivEXT");            
    SAFE_GET_PROC( qglVariantfvEXT, PFNGLVARIANTFVEXTPROC, "glVariantfvEXT");            
    SAFE_GET_PROC( qglVariantdvEXT, PFNGLVARIANTDVEXTPROC, "glVariantdvEXT");            
    SAFE_GET_PROC( qglVariantubvEXT, PFNGLVARIANTUBVEXTPROC, "glVariantubvEXT");         
    SAFE_GET_PROC( qglVariantusvEXT, PFNGLVARIANTUSVEXTPROC, "glVariantusvEXT");         
    SAFE_GET_PROC( qglVariantuivEXT, PFNGLVARIANTUIVEXTPROC, "glVariantuivEXT");         
    SAFE_GET_PROC( qglVariantPointerEXT, PFNGLVARIANTPOINTEREXTPROC, "glVariantPointerEXT");             
    SAFE_GET_PROC( qglEnableVariantClientStateEXT, PFNGLENABLEVARIANTCLIENTSTATEEXTPROC, "glEnableVariantClientStateEXT");
    SAFE_GET_PROC( qglDisableVariantClientStateEXT, PFNGLDISABLEVARIANTCLIENTSTATEEXTPROC, "glDisableVariantClientStateEXT");
    SAFE_GET_PROC( qglBindLightParameterEXT, PFNGLBINDLIGHTPARAMETEREXTPROC, "glBindLightParameterEXT");
    SAFE_GET_PROC( qglBindMaterialParameterEXT, PFNGLBINDMATERIALPARAMETEREXTPROC, "glBindMaterialParameterEXT");
    SAFE_GET_PROC( qglBindTexGenParameterEXT, PFNGLBINDTEXGENPARAMETEREXTPROC, "glBindTexGenParameterEXT");
    SAFE_GET_PROC( qglBindTextureUnitParameterEXT, PFNGLBINDTEXTUREUNITPARAMETEREXTPROC, "glBindTextureUnitParameterEXT");
    SAFE_GET_PROC( qglBindParameterEXT, PFNGLBINDPARAMETEREXTPROC, "glBindParameterEXT");
    SAFE_GET_PROC( qglIsVariantEnabledEXT, PFNGLISVARIANTENABLEDEXTPROC, "glIsVariantEnabledEXT");
    SAFE_GET_PROC( qglGetVariantBooleanvEXT, PFNGLGETVARIANTBOOLEANVEXTPROC, "glGetVariantBooleanvEXT");
    SAFE_GET_PROC( qglGetVariantIntegervEXT, PFNGLGETVARIANTINTEGERVEXTPROC, "glGetVariantIntegervEXT");
    SAFE_GET_PROC( qglGetVariantFloatvEXT, PFNGLGETVARIANTFLOATVEXTPROC, "glGetVariantFloatvEXT");
    SAFE_GET_PROC( qglGetVariantPointervEXT, PFNGLGETVARIANTPOINTERVEXTPROC, "glGetVariantPointervEXT");
    SAFE_GET_PROC( qglGetInvariantBooleanvEXT, PFNGLGETINVARIANTBOOLEANVEXTPROC, "glGetInvariantBooleanvEXT");
    SAFE_GET_PROC( qglGetInvariantIntegervEXT, PFNGLGETINVARIANTINTEGERVEXTPROC, "glGetInvariantIntegervEXT");
    SAFE_GET_PROC( qglGetInvariantFloatvEXT, PFNGLGETINVARIANTFLOATVEXTPROC, "glGetInvariantFloatvEXT");
    SAFE_GET_PROC( qglGetLocalConstantBooleanvEXT, PFNGLGETLOCALCONSTANTBOOLEANVEXTPROC, "glGetLocalConstantBooleanvEXT");
    SAFE_GET_PROC( qglGetLocalConstantIntegervEXT, PFNGLGETLOCALCONSTANTINTEGERVEXTPROC, "glGetLocalConstantIntegervEXT");
    SAFE_GET_PROC( qglGetLocalConstantFloatvEXT, PFNGLGETLOCALCONSTANTFLOATVEXTPROC, "glGetLocalConstantFloatvEXT");

    SAFE_GET_PROC( qglPNTrianglesiATI, PFNGLPNTRIANGLESIATIPROC, "glPNTrianglesiATI");
    SAFE_GET_PROC( qglPNTrianglesfATI, PFNGLPNTRIANGLESFATIPROC, "glPNTrianglesfATI");
#endif /* !__APPLE__ && !MACOSX */

    glEnable(GL_FRAGMENT_SHADER_ATI);

    fragment_shaders = qglGenFragmentShadersATI(2);

    // combined diffuse & specular shader w/ vertex color
    qglBindFragmentShaderATI(fragment_shaders);
    checkerror();
    qglBeginFragmentShaderATI();
    checkerror();

    qglSetFragmentShaderConstantATI(GL_CON_0_ATI, &scaler[0]);
    checkerror();

    // texld r0, t0
    qglSampleMapATI (GL_REG_0_ATI, GL_TEXTURE0_ARB, GL_SWIZZLE_STR_ATI);
    checkerror();
    // texld r1, t1
    qglSampleMapATI (GL_REG_1_ATI, GL_TEXTURE1_ARB, GL_SWIZZLE_STR_ATI);
    checkerror();
    // texld r2, t2
    qglSampleMapATI (GL_REG_2_ATI, GL_TEXTURE2_ARB, GL_SWIZZLE_STR_ATI);
    checkerror();
    // texld r3, t3
    qglSampleMapATI (GL_REG_3_ATI, GL_TEXTURE3_ARB, GL_SWIZZLE_STR_ATI);
    checkerror();
    // texld r4, t4
    qglSampleMapATI (GL_REG_4_ATI, GL_TEXTURE4_ARB, GL_SWIZZLE_STR_ATI);
    checkerror();

    // gloss * atten * light color * specular +
    // dot * color * atten * light color * self shadow =
    // (gloss * specular + dot * color * self shadow ) * atten * light color
    // Alpha ops rule :-)

    // dp3_sat r2.rgb, r0_bx2.rgb, r2_bx2.rgb   // specular
    qglColorFragmentOp2ATI(GL_DOT3_ATI,
                           GL_REG_2_ATI, GL_RED_BIT_ATI|GL_GREEN_BIT_ATI|GL_BLUE_BIT_ATI, GL_SATURATE_BIT_ATI,
                           GL_REG_0_ATI, GL_NONE, GL_2X_BIT_ATI|GL_BIAS_BIT_ATI,
                           GL_REG_2_ATI, GL_NONE, GL_2X_BIT_ATI|GL_BIAS_BIT_ATI);
    checkerror();

    // +mov_x8_sat r2.a, r1_bx2.b               // self shadow term
    qglAlphaFragmentOp1ATI(GL_MOV_ATI,
                           GL_REG_2_ATI, GL_8X_BIT_ATI|GL_SATURATE_BIT_ATI,
                           GL_REG_1_ATI, GL_BLUE, GL_2X_BIT_ATI|GL_BIAS_BIT_ATI);
    checkerror();

    // dp3_sat r1.rgb, r0_bx2.rgb, r1_bx2.rgb   // diffuse
    qglColorFragmentOp2ATI(GL_DOT3_ATI,
                           GL_REG_1_ATI, GL_RED_BIT_ATI|GL_GREEN_BIT_ATI|GL_BLUE_BIT_ATI, GL_SATURATE_BIT_ATI,
                           GL_REG_0_ATI, GL_NONE, GL_2X_BIT_ATI|GL_BIAS_BIT_ATI,
                           GL_REG_1_ATI, GL_NONE, GL_2X_BIT_ATI|GL_BIAS_BIT_ATI);
    checkerror();

    // +mad_x2_sat r1.a, r2.b, r2.b, -c0.b      // specular exponent
    qglAlphaFragmentOp3ATI(GL_MAD_ATI,
                           GL_REG_1_ATI, GL_2X_BIT_ATI|GL_SATURATE_BIT_ATI,
                           GL_REG_2_ATI, GL_BLUE, GL_NONE,
                           GL_REG_2_ATI, GL_BLUE, GL_NONE,
                           GL_CON_0_ATI, GL_BLUE, GL_NEGATE_BIT_ATI);
    checkerror();

    // mul r1.rgb, r1.rgb, r3.rgb               // diffuse color * diffuse bump
    qglColorFragmentOp2ATI(GL_MUL_ATI,
                           GL_REG_1_ATI, GL_RED_BIT_ATI|GL_GREEN_BIT_ATI|GL_BLUE_BIT_ATI, GL_NONE,
                           GL_REG_1_ATI, GL_NONE, GL_NONE,
                           GL_REG_3_ATI, GL_NONE, GL_NONE);
    checkerror();

    // +mul r1.a, r1.a, r1.a                    // raise exponent
    qglAlphaFragmentOp2ATI(GL_MUL_ATI,
                           GL_REG_1_ATI, GL_NONE,
                           GL_REG_1_ATI, GL_NONE, GL_NONE,
                           GL_REG_1_ATI, GL_NONE, GL_NONE);
    checkerror();

    // mul r4.rgb, r4.rgb, v0.rgb               // atten * light color
    qglColorFragmentOp2ATI(GL_MUL_ATI,
                           GL_REG_4_ATI, GL_RED_BIT_ATI|GL_GREEN_BIT_ATI|GL_BLUE_BIT_ATI, GL_NONE,
                           GL_REG_4_ATI, GL_NONE, GL_NONE,
                           GL_PRIMARY_COLOR_ARB, GL_NONE, GL_NONE);
    checkerror();

    // +mul r1.a, r1.a, r1.a                    // raise exponent
    qglAlphaFragmentOp2ATI(GL_MUL_ATI,
                           GL_REG_1_ATI, GL_NONE,
                           GL_REG_1_ATI, GL_NONE, GL_NONE,
                           GL_REG_1_ATI, GL_NONE, GL_NONE);
    checkerror();

    // mul r1.rgb, r1.rgb, r2.a                 // self shadow * diffuse
    qglColorFragmentOp2ATI(GL_MUL_ATI,
                           GL_REG_1_ATI, GL_RED_BIT_ATI|GL_GREEN_BIT_ATI|GL_BLUE_BIT_ATI, GL_NONE,
                           GL_REG_1_ATI, GL_NONE, GL_NONE,
                           GL_REG_2_ATI, GL_ALPHA, GL_NONE);
    checkerror();

    // +mul r0.a, r1.a, r0.a                    // specular * gloss map
    qglAlphaFragmentOp2ATI(GL_MUL_ATI,
                           GL_REG_0_ATI, GL_SATURATE_BIT_ATI,
                           GL_REG_1_ATI, GL_NONE, GL_NONE,
                           GL_REG_0_ATI, GL_NONE, GL_NONE);
    checkerror();

    // add r0.rgb, r1.rgb, r0.a                 // diffuse + specular
    qglColorFragmentOp2ATI(GL_ADD_ATI,
                           GL_REG_0_ATI, GL_RED_BIT_ATI|GL_GREEN_BIT_ATI|GL_BLUE_BIT_ATI, GL_NONE,
                           GL_REG_1_ATI, GL_NONE, GL_NONE,
                           GL_REG_0_ATI, GL_ALPHA, GL_NONE);
    checkerror();

    // mul_sat r0.rgb, r0.rgb, r4.rgb           // (diffuse + specular)*atten*color
    qglColorFragmentOp2ATI(GL_MUL_ATI,
                           GL_REG_0_ATI, GL_NONE, GL_SATURATE_BIT_ATI,
                           GL_REG_0_ATI, GL_NONE, GL_NONE,
                           GL_REG_4_ATI, GL_NONE, GL_NONE);
    checkerror();


    qglEndFragmentShaderATI();
    checkerror();

    // Second shader with cube filter
    qglBindFragmentShaderATI(fragment_shaders+1);
    checkerror();
    qglBeginFragmentShaderATI();
    checkerror();

    qglSetFragmentShaderConstantATI(GL_CON_0_ATI, &scaler[0]);
    checkerror();

    // texld r0, t0
    qglSampleMapATI (GL_REG_0_ATI, GL_TEXTURE0_ARB, GL_SWIZZLE_STR_ATI);
    checkerror();
    // texld r1, t1
    qglSampleMapATI (GL_REG_1_ATI, GL_TEXTURE1_ARB, GL_SWIZZLE_STR_ATI);
    checkerror();
    // texld r2, t2
    qglSampleMapATI (GL_REG_2_ATI, GL_TEXTURE2_ARB, GL_SWIZZLE_STR_ATI);
    checkerror();
    // texld r3, t3
    qglSampleMapATI (GL_REG_3_ATI, GL_TEXTURE3_ARB, GL_SWIZZLE_STR_ATI);
    checkerror();
    // texld r4, t4
    qglSampleMapATI (GL_REG_4_ATI, GL_TEXTURE4_ARB, GL_SWIZZLE_STR_ATI);
    checkerror();
    // texld r5, t5
    qglSampleMapATI (GL_REG_5_ATI, GL_TEXTURE5_ARB, GL_SWIZZLE_STR_ATI);
    checkerror();

    // gloss * atten * light color * specular +
    // dot * color * atten * light color * self shadow =
    // (gloss * specular + dot * color * self shadow ) * atten * light color * filter
    // Alpha ops rule :-)

    // dp3_sat r2.rgb, r0_bx2.rgb, r2_bx2.rgb      // specular
    qglColorFragmentOp2ATI(GL_DOT3_ATI,
                           GL_REG_2_ATI, GL_RED_BIT_ATI|GL_GREEN_BIT_ATI|GL_BLUE_BIT_ATI, GL_SATURATE_BIT_ATI,
                           GL_REG_0_ATI, GL_NONE, GL_2X_BIT_ATI|GL_BIAS_BIT_ATI,
                           GL_REG_2_ATI, GL_NONE, GL_2X_BIT_ATI|GL_BIAS_BIT_ATI);
    checkerror();

    // +mov_x8_sat r2.a, r1_bx2.b                  // self shadow term
    qglAlphaFragmentOp1ATI(GL_MOV_ATI,
                           GL_REG_2_ATI, GL_8X_BIT_ATI|GL_SATURATE_BIT_ATI,
                           GL_REG_1_ATI, GL_BLUE, GL_2X_BIT_ATI|GL_BIAS_BIT_ATI);
    checkerror();

    // dp3_sat r1.rgb, r0_bx2.rgb, r1_bx2.rgb  // diffuse
    qglColorFragmentOp2ATI(GL_DOT3_ATI,
                           GL_REG_1_ATI, GL_RED_BIT_ATI|GL_GREEN_BIT_ATI|GL_BLUE_BIT_ATI, GL_SATURATE_BIT_ATI,
                           GL_REG_0_ATI, GL_NONE, GL_2X_BIT_ATI|GL_BIAS_BIT_ATI,
                           GL_REG_1_ATI, GL_NONE, GL_2X_BIT_ATI|GL_BIAS_BIT_ATI);
    checkerror();

    // +mad_x2_sat r1.a, r2.b, r2.b, -c0.b     // specular exponent
    qglAlphaFragmentOp3ATI(GL_MAD_ATI,
                           GL_REG_1_ATI, GL_2X_BIT_ATI|GL_SATURATE_BIT_ATI,
                           GL_REG_2_ATI, GL_BLUE, GL_NONE,
                           GL_REG_2_ATI, GL_BLUE, GL_NONE,
                           GL_CON_0_ATI, GL_BLUE, GL_NEGATE_BIT_ATI);
    checkerror();

    // mul r1.rgb, r1.rgb, r3.rgb              // diffuse color * diffuse bump
    qglColorFragmentOp2ATI(GL_MUL_ATI,
                           GL_REG_1_ATI, GL_RED_BIT_ATI|GL_GREEN_BIT_ATI|GL_BLUE_BIT_ATI, GL_NONE,
                           GL_REG_1_ATI, GL_NONE, GL_NONE,
                           GL_REG_3_ATI, GL_NONE, GL_NONE);
    checkerror();

    // +mul r1.a, r1.a, r1.a                   // raise exponent
    qglAlphaFragmentOp2ATI(GL_MUL_ATI,
                           GL_REG_1_ATI, GL_NONE,
                           GL_REG_1_ATI, GL_NONE, GL_NONE,
                           GL_REG_1_ATI, GL_NONE, GL_NONE);
    checkerror();

    // mul r4.rgb, r4.rgb, v0.rgb              // atten * light color
    qglColorFragmentOp2ATI(GL_MUL_ATI,
                           GL_REG_4_ATI, GL_RED_BIT_ATI|GL_GREEN_BIT_ATI|GL_BLUE_BIT_ATI, GL_NONE,
                           GL_REG_4_ATI, GL_NONE, GL_NONE,
                           GL_PRIMARY_COLOR_ARB, GL_NONE, GL_NONE);
    checkerror();

    // +mul r1.a, r1.a, r1.a                   // raise exponent
    qglAlphaFragmentOp2ATI(GL_MUL_ATI,
                           GL_REG_1_ATI, GL_NONE,
                           GL_REG_1_ATI, GL_NONE, GL_NONE,
                           GL_REG_1_ATI, GL_NONE, GL_NONE);
    checkerror();

    // mul r1.rgb, r1.rgb, r2.a               // self shadow * diffuse
    qglColorFragmentOp2ATI(GL_MUL_ATI,
                           GL_REG_1_ATI, GL_RED_BIT_ATI|GL_GREEN_BIT_ATI|GL_BLUE_BIT_ATI, GL_NONE,
                           GL_REG_1_ATI, GL_NONE, GL_NONE,
                           GL_REG_2_ATI, GL_ALPHA, GL_NONE);
    checkerror();

    // +mul r0.a, r1.a, r0.a                 // specular * gloss map
    qglAlphaFragmentOp2ATI(GL_MUL_ATI,

                           GL_REG_0_ATI, GL_SATURATE_BIT_ATI,

                           GL_REG_1_ATI, GL_NONE, GL_NONE,

                           GL_REG_0_ATI, GL_NONE, GL_NONE);

    checkerror();


    // add r0.rgb, r0.rgb, r0.a              // diffuse + specular
    qglColorFragmentOp2ATI(GL_ADD_ATI,
                           GL_REG_0_ATI, GL_RED_BIT_ATI|GL_GREEN_BIT_ATI|GL_BLUE_BIT_ATI, GL_NONE,
                           GL_REG_1_ATI, GL_NONE, GL_NONE,
                           GL_REG_0_ATI, GL_ALPHA, GL_NONE);
    checkerror();

    // mul r0.rgb, r0.rgb, r4.rgb            // (diffuse + specular)*atten*color
    qglColorFragmentOp2ATI(GL_MUL_ATI,
                           GL_REG_0_ATI, GL_NONE, GL_NONE,
                           GL_REG_0_ATI, GL_NONE, GL_NONE,
                           GL_REG_4_ATI, GL_NONE, GL_NONE);
    checkerror();

    // mul_sat r0.rgb, r0.rgb, r5.rgb    // (diffuse + specular)*atten*color*filter
    qglColorFragmentOp2ATI(GL_MUL_ATI,
                           GL_REG_0_ATI, GL_NONE, GL_SATURATE_BIT_ATI,
                           GL_REG_0_ATI, GL_NONE, GL_NONE,
                           GL_REG_5_ATI, GL_NONE, GL_NONE);
    checkerror();


    qglEndFragmentShaderATI();
    checkerror();

    glDisable(GL_FRAGMENT_SHADER_ATI);


    glEnable(GL_VERTEX_SHADER_EXT);

    vertex_shaders = qglGenVertexShadersEXT(2);
    checkerror();
    qglBindVertexShaderEXT(vertex_shaders);
    checkerror();
    qglBeginVertexShaderEXT();
    checkerror();

    // Generates a necessary input for the diffuse bumpmapping registers
    mvp           = qglBindParameterEXT( GL_MVP_MATRIX_EXT );
    checkerror();
    modelview     = qglBindParameterEXT( GL_MODELVIEW_MATRIX );
    checkerror();
    vertex        = qglBindParameterEXT( GL_CURRENT_VERTEX_EXT );
    checkerror();
    color         = qglBindParameterEXT( GL_CURRENT_COLOR );
    checkerror();
    texturematrix = qglBindTextureUnitParameterEXT( GL_TEXTURE4_ARB, GL_TEXTURE_MATRIX );
    checkerror();
    texcoord0     = qglBindTextureUnitParameterEXT( GL_TEXTURE0_ARB, GL_CURRENT_TEXTURE_COORDS );
    checkerror();
    texcoord1     = qglBindTextureUnitParameterEXT( GL_TEXTURE1_ARB, GL_CURRENT_TEXTURE_COORDS );
    checkerror();
    texcoord2     = qglBindTextureUnitParameterEXT( GL_TEXTURE2_ARB, GL_CURRENT_TEXTURE_COORDS );
    checkerror();
    disttemp      = qglGenSymbolsEXT(GL_SCALAR_EXT, GL_LOCAL_EXT, GL_FULL_RANGE_EXT, 1);
    checkerror();
    disttemp2     = qglGenSymbolsEXT(GL_SCALAR_EXT, GL_LOCAL_EXT, GL_FULL_RANGE_EXT, 1);
    checkerror();
    zcomp         = qglGenSymbolsEXT(GL_VECTOR_EXT, GL_LOCAL_EXT, GL_FULL_RANGE_EXT, 1);
    checkerror();
    fogstart      = qglBindParameterEXT( GL_FOG_START );
    checkerror();
    fogend        = qglBindParameterEXT( GL_FOG_END );
    checkerror();

    // Transform vertex to view-space
    qglShaderOp2EXT( GL_OP_MULTIPLY_MATRIX_EXT, GL_OUTPUT_VERTEX_EXT, mvp, vertex );
    checkerror();
    
    // Transform vertex by texture matrix and copy to output
    qglShaderOp2EXT( GL_OP_MULTIPLY_MATRIX_EXT, GL_OUTPUT_TEXTURE_COORD4_EXT, texturematrix, vertex );
    checkerror();

    // copy tex coords of unit 0 to unit 3
    qglShaderOp1EXT( GL_OP_MOV_EXT, GL_OUTPUT_TEXTURE_COORD0_EXT, texcoord0);
    checkerror();
    qglShaderOp1EXT( GL_OP_MOV_EXT, GL_OUTPUT_TEXTURE_COORD1_EXT, texcoord1);
    checkerror();
    qglShaderOp1EXT( GL_OP_MOV_EXT, GL_OUTPUT_TEXTURE_COORD2_EXT, texcoord2);
    checkerror();
    qglShaderOp1EXT( GL_OP_MOV_EXT, GL_OUTPUT_TEXTURE_COORD3_EXT, texcoord0);
    checkerror();
    qglShaderOp1EXT( GL_OP_MOV_EXT, GL_OUTPUT_COLOR0_EXT, color);
    checkerror();

    // Transform vertex and take z for fog
    qglExtractComponentEXT( zcomp, modelview, 2);
    checkerror();
    qglShaderOp2EXT( GL_OP_DOT4_EXT, disttemp, zcomp, vertex );
    checkerror();

    // calculate fog values end - z and end - start
    qglShaderOp2EXT( GL_OP_SUB_EXT, disttemp, fogend, disttemp);
    checkerror();
    qglShaderOp2EXT( GL_OP_SUB_EXT, disttemp2, fogend, fogstart);
    checkerror();

    // divide end - z by end - start, that's it
    qglShaderOp1EXT( GL_OP_RECIP_EXT, disttemp2, disttemp2);
    checkerror();
    qglShaderOp2EXT( GL_OP_MUL_EXT, GL_OUTPUT_FOG_EXT, disttemp, disttemp2);
    checkerror();

    qglEndVertexShaderEXT();
    checkerror();

    // Two transformed textures
    qglBindVertexShaderEXT(vertex_shaders+1);
    checkerror();
    qglBeginVertexShaderEXT();
    checkerror();

    // Generates a necessary input for the diffuse bumpmapping registers
    mvp            = qglBindParameterEXT( GL_MVP_MATRIX_EXT );
    checkerror();
    modelview      = qglBindParameterEXT( GL_MODELVIEW_MATRIX );
    checkerror();
    vertex         = qglBindParameterEXT( GL_CURRENT_VERTEX_EXT );
    checkerror();
    color          = qglBindParameterEXT( GL_CURRENT_COLOR );
    checkerror();
    texturematrix  = qglBindTextureUnitParameterEXT( GL_TEXTURE4_ARB, GL_TEXTURE_MATRIX );
    checkerror();
    texturematrix2 = qglBindTextureUnitParameterEXT( GL_TEXTURE5_ARB, GL_TEXTURE_MATRIX );
    checkerror();
    texcoord0      = qglBindTextureUnitParameterEXT( GL_TEXTURE0_ARB, GL_CURRENT_TEXTURE_COORDS );
    checkerror();
    texcoord1      = qglBindTextureUnitParameterEXT( GL_TEXTURE1_ARB, GL_CURRENT_TEXTURE_COORDS );
    checkerror();
    texcoord2      = qglBindTextureUnitParameterEXT( GL_TEXTURE2_ARB, GL_CURRENT_TEXTURE_COORDS );
    checkerror();
    disttemp       = qglGenSymbolsEXT(GL_SCALAR_EXT, GL_LOCAL_EXT, GL_FULL_RANGE_EXT, 1);
    checkerror();
    disttemp2      = qglGenSymbolsEXT(GL_SCALAR_EXT, GL_LOCAL_EXT, GL_FULL_RANGE_EXT, 1);
    checkerror();
    zcomp          = qglGenSymbolsEXT(GL_VECTOR_EXT, GL_LOCAL_EXT, GL_FULL_RANGE_EXT, 1);
    checkerror();
    fogstart       = qglBindParameterEXT( GL_FOG_START );
    checkerror();
    fogend         = qglBindParameterEXT( GL_FOG_END );
    checkerror();

    // Transform vertex to view-space
    qglShaderOp2EXT( GL_OP_MULTIPLY_MATRIX_EXT, GL_OUTPUT_VERTEX_EXT, mvp, vertex );
    checkerror();
    
    // Transform vertex by texture matrix and copy to output
    qglShaderOp2EXT( GL_OP_MULTIPLY_MATRIX_EXT, GL_OUTPUT_TEXTURE_COORD4_EXT, texturematrix, vertex );
    checkerror();

    // Transform vertex by texture matrix and copy to output
    qglShaderOp2EXT( GL_OP_MULTIPLY_MATRIX_EXT, GL_OUTPUT_TEXTURE_COORD5_EXT, texturematrix2, vertex );
    checkerror();

    // copy tex coords of unit 0 to unit 3
    qglShaderOp1EXT( GL_OP_MOV_EXT, GL_OUTPUT_TEXTURE_COORD0_EXT, texcoord0);
    checkerror();
    qglShaderOp1EXT( GL_OP_MOV_EXT, GL_OUTPUT_TEXTURE_COORD1_EXT, texcoord1);
    checkerror();
    qglShaderOp1EXT( GL_OP_MOV_EXT, GL_OUTPUT_TEXTURE_COORD2_EXT, texcoord2);
    checkerror();
    qglShaderOp1EXT( GL_OP_MOV_EXT, GL_OUTPUT_TEXTURE_COORD3_EXT, texcoord0);
    checkerror();
    qglShaderOp1EXT( GL_OP_MOV_EXT, GL_OUTPUT_COLOR0_EXT, color);
    checkerror();

    // Transform vertex and take z for fog
    qglExtractComponentEXT( zcomp, modelview, 2);
    checkerror();
    qglShaderOp2EXT( GL_OP_DOT4_EXT, disttemp, zcomp, vertex );
    checkerror();

    // calculate fog values end - z and end - start
    qglShaderOp2EXT( GL_OP_SUB_EXT, disttemp, fogend, disttemp);
    checkerror();
    qglShaderOp2EXT( GL_OP_SUB_EXT, disttemp2, fogend, fogstart);
    checkerror();

    // divide end - z by end - start, that's it
    qglShaderOp1EXT( GL_OP_RECIP_EXT, disttemp2, disttemp2);
    checkerror();
    qglShaderOp2EXT( GL_OP_MUL_EXT, GL_OUTPUT_FOG_EXT, disttemp, disttemp2);
    checkerror();

    qglEndVertexShaderEXT();
    checkerror();

    glDisable(GL_VERTEX_SHADER_EXT);
}


void GL_DisableDiffuseShaderRadeon()
{
    //tex 0 = normal map
    //tex 1 = normalization cube map (tangent space light vector)
    //tex 2 = normalization cube map (tangent space half vector)
    //tex 3 = color map
    //tex 4 = (attenuation or light filter, depends on light settings)

    glDisable(GL_VERTEX_SHADER_EXT);
    glDisable(GL_FRAGMENT_SHADER_ATI);

    GL_SelectTexture(GL_TEXTURE1_ARB);
    glDisable(GL_TEXTURE_CUBE_MAP_ARB);

    GL_SelectTexture(GL_TEXTURE2_ARB);
    glDisable(GL_TEXTURE_CUBE_MAP_ARB);

    GL_SelectTexture(GL_TEXTURE3_ARB);
    glDisable(GL_TEXTURE_2D);

    GL_SelectTexture(GL_TEXTURE4_ARB);
    if (currentshadowlight->filtercube)
    {
        glDisable(GL_TEXTURE_CUBE_MAP_ARB);
        glPopMatrix();
        GL_SelectTexture(GL_TEXTURE5_ARB);
    }
    glDisable(GL_TEXTURE_3D);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    GL_SelectTexture(GL_TEXTURE0_ARB);
}

void GL_EnableDiffuseSpecularShaderRadeon(qboolean world, vec3_t lightOrig)
{
    float invrad = 1/currentshadowlight->radius;

    //tex 0 = normal map
    //tex 1 = normalization cube map (tangent space light vector)
    //tex 2 = normalization cube map (tangent space half vector)
    //tex 3 = color map
    //tex 4 = (attenuation or light filter, depends on light settings but the actual
    //  register combiner setup does not change only the bound texture)

    GL_SelectTexture(GL_TEXTURE1_ARB);
    glEnable(GL_TEXTURE_CUBE_MAP_ARB);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, normcube_texture_object);

    GL_SelectTexture(GL_TEXTURE2_ARB);
    glEnable(GL_TEXTURE_CUBE_MAP_ARB);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, normcube_texture_object);

    GL_SelectTexture(GL_TEXTURE3_ARB);
    glEnable(GL_TEXTURE_2D);

    glEnable(GL_FRAGMENT_SHADER_ATI);
    glEnable(GL_VERTEX_SHADER_EXT);
   
    GL_SelectTexture(GL_TEXTURE4_ARB);
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    if (currentshadowlight->filtercube)
    {
        glGetError();
        qglBindFragmentShaderATI( fragment_shaders + 1 );
        checkerror();
        qglBindVertexShaderEXT( vertex_shaders + 1);
        checkerror();

        glEnable(GL_TEXTURE_CUBE_MAP_ARB);
        glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, currentshadowlight->filtercube);
        GL_SetupCubeMapMatrix(world);

        GL_SelectTexture(GL_TEXTURE5_ARB);
        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
        glLoadIdentity();
    }
    else
    {
        glGetError();
        qglBindFragmentShaderATI( fragment_shaders );
        checkerror();
        qglBindVertexShaderEXT( vertex_shaders );
        checkerror();
    }

    glEnable(GL_TEXTURE_3D);
    glBindTexture(GL_TEXTURE_3D, atten3d_texture_object);

    glTranslatef(0.5,0.5,0.5);
    glScalef(0.5,0.5,0.5);
    glScalef(invrad, invrad, invrad);
    glTranslatef(-lightOrig[0], -lightOrig[1], -lightOrig[2]);

    GL_SelectTexture(GL_TEXTURE0_ARB);

}


void R_DrawWorldRadeonDiffuseSpecular(lightcmd_t *lightCmds) 
{
    int command, num, i;
    int lightPos = 0;
    vec3_t lightOr;
    msurface_t *surf;
    float               *v;
    float* lightP;
    vec3_t lightDir;
    vec3_t tsH,H;

    texture_t	*t;//XYZ

    //support flickering lights
    VectorCopy(currentshadowlight->origin,lightOr);

    while (1)
    {
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
        GL_SelectTexture(GL_TEXTURE3_ARB);
        GL_Bind(t->gl_texturenum);

        glBegin(command);
        //v = surf->polys->verts[0];
        v = (float *)(&globalVertexTable[surf->polys->firstvertex]);
        for (i=0; i<num; i++, v+= VERTEXSIZE)
        {
            //skip attent texture coord.
            lightPos += 2;

            lightP = &lightCmds[lightPos].asFloat;
            lightPos += 3;

            VectorCopy(lightP, lightDir);
            VectorNormalize(lightDir);

            //calculate local H vector and put it into tangent space

            //r_origin = camera position
            VectorSubtract(r_refdef.vieworg,v,H);
            VectorNormalize(H);

            //put H in tangent space first since lightDir (precalc) is already in tang space
            if (surf->flags & SURF_PLANEBACK)
            {
                tsH[2] = -DotProduct(H,surf->plane->normal);
            }
            else
            { 
                tsH[2] = DotProduct(H,surf->plane->normal);
            }

            tsH[1] = -DotProduct(H,surf->texinfo->vecs[1]);
            tsH[0] = DotProduct(H,surf->texinfo->vecs[0]);

            VectorAdd(lightDir,tsH,tsH);

            // diffuse
            qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, v[3], v[4]);
	    
            qglMultiTexCoord3fvARB(GL_TEXTURE1_ARB, lightP);

            // half vector for specular
            qglMultiTexCoord3fvARB(GL_TEXTURE2_ARB,&tsH[0]);

            glVertex3fv(&v[0]);
        }
        glEnd();
    }

    GL_SelectTexture(GL_TEXTURE0_ARB);
}



void R_DrawBrushRadeonDiffuseSpecular(entity_t *e)
{
    model_t     *model = e->model;
    msurface_t *surf;
    glpoly_t    *poly;
    int         i, j, count;
    brushlightinstant_t *ins = e->brushlightinstant;
    float       *v;
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
        GL_SelectTexture(GL_TEXTURE3_ARB);
        GL_Bind(t->gl_texturenum);

        glBegin(GL_TRIANGLE_FAN);
        //v = poly->verts[0];
        v = (float *)(&globalVertexTable[poly->firstvertex]);
        for (j=0 ; j<poly->numverts ; j++, v+= VERTEXSIZE)
        {       
            qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, v[3], v[4]);
            qglMultiTexCoord3fvARB(GL_TEXTURE1_ARB,&ins->tslights[count+j][0]);
            qglMultiTexCoord3fvARB(GL_TEXTURE2_ARB,&ins->tshalfangles[count+j][0]);
            glVertex3fv(v);
        }
        glEnd();
        count+=surf->numedges;
    }   
}


void R_DrawAliasFrameRadeonDiffuseSpecular (aliashdr_t *paliashdr, aliasframeinstant_t *instant)
{
    mtriangle_t *tris;
    fstvert_t   *texcoords;
    int anim;
    int                 *indecies;
    aliaslightinstant_t *linstant = instant->lightinstant;

    tris = (mtriangle_t *)((byte *)paliashdr + paliashdr->triangles);
    texcoords = (fstvert_t *)((byte *)paliashdr + paliashdr->texcoords);

    //bind normal map
    anim = (int)(cl.time*10) & 3;

    GL_SelectTexture(GL_TEXTURE0_ARB);
    GL_Bind(paliashdr->gl_texturenum[currententity->skinnum][anim]+1);
    GL_SelectTexture(GL_TEXTURE3_ARB);
    GL_Bind(paliashdr->gl_texturenum[currententity->skinnum][anim]);
        
    indecies = (int *)((byte *)paliashdr + paliashdr->indecies);

    glVertexPointer(3, GL_FLOAT, 0, instant->vertices);
    glEnableClientState(GL_VERTEX_ARRAY);
    glNormalPointer(GL_FLOAT, 0, instant->normals);
    glEnableClientState(GL_NORMAL_ARRAY);

    qglClientActiveTextureARB(GL_TEXTURE0_ARB);
    glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    qglClientActiveTextureARB(GL_TEXTURE1_ARB);
    glTexCoordPointer(3, GL_FLOAT, 0, linstant->tslights);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    qglClientActiveTextureARB(GL_TEXTURE2_ARB);
    glTexCoordPointer(3, GL_FLOAT, 0, linstant->tshalfangles);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    //glDrawElements(GL_TRIANGLES,paliashdr->numtris*3,GL_UNSIGNED_INT,indecies);
    glDrawElements(GL_TRIANGLES,linstant->numtris*3,GL_UNSIGNED_INT,&linstant->indecies[0]);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    qglClientActiveTextureARB(GL_TEXTURE0_ARB);
    GL_SelectTexture(GL_TEXTURE0_ARB);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void R_DrawWorldBumpedRadeon()
{
    if (!currentshadowlight->visible)
        return;

    glDepthMask (0);
    glShadeModel (GL_SMOOTH);
    GL_AddColor();
    glColor3fv(&currentshadowlight->color[0]);

    GL_EnableDiffuseSpecularShaderRadeon(true,currentshadowlight->origin);
    R_DrawWorldRadeonDiffuseSpecular(currentshadowlight->lightCmds);
    GL_DisableDiffuseShaderRadeon();

    glColor3f (1,1,1);
    glDisable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask (1);
}

void R_DrawBrushBumpedRadeon(entity_t *e)
{
    GL_AddColor();
    glColor3fv(&currentshadowlight->color[0]);


    GL_EnableDiffuseSpecularShaderRadeon(false,((brushlightinstant_t *)e->brushlightinstant)->lightpos);
    R_DrawBrushRadeonDiffuseSpecular(e);
    GL_DisableDiffuseShaderRadeon();
}

void R_DrawAliasBumpedRadeon(aliashdr_t *paliashdr, aliasframeinstant_t *instant)
{
	extern qboolean	gl_pntriangles;
	
    if ( gl_pntriangles == true && gl_truform.value )
    {
        glEnable(GL_PN_TRIANGLES_ATI);
		qglPNTrianglesiATI(GL_PN_TRIANGLES_POINT_MODE_ATI, GL_PN_TRIANGLES_POINT_MODE_CUBIC_ATI);
		qglPNTrianglesiATI(GL_PN_TRIANGLES_NORMAL_MODE_ATI, GL_PN_TRIANGLES_NORMAL_MODE_QUADRATIC_ATI);
		qglPNTrianglesiATI(GL_PN_TRIANGLES_TESSELATION_LEVEL_ATI, gl_truform_tesselation.value);
    }

    GL_AddColor();
    glColor3fv(&currentshadowlight->color[0]);

    GL_EnableDiffuseSpecularShaderRadeon(false,instant->lightinstant->lightpos);
    R_DrawAliasFrameRadeonDiffuseSpecular(paliashdr,instant);
    GL_DisableDiffuseShaderRadeon();

    if ( gl_truform.value )
    {
        glDisable(GL_PN_TRIANGLES_ATI);
    }
}
