/*
Copyright (C) 2001-2002 Charles Hollemeersch
Copyright (C) 2002 Jarno Paananen

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

Same as gl_bumpmap.c but Matrox Parhelia optimized 
These routines require 4 texture units and EXT_vertex_shader and
MTX_fragment shader extensions.

Most lights reqire 2 passes this way
1 diffuse
2 specular

If a light has a cubemap filter it requires 3 passes
1 attenuation
2 diffuse
3 specular
*/

#include "quakedef.h"

#include "glATI.h" // Yes, for EXT_vertex_shader

static GLuint vertex_shader;
static GLuint fragment_shaders;


//#define PARHELIADEBUG

#if defined(PARHELIADEBUG) && defined(_WIN32)
#define checkerror() checkrealerror(__LINE__)
static void checkrealerror(int line)
{
    GLuint error = glGetError();
    if ( error != GL_NO_ERROR )
    {
        char    buf[128];
	wsprintf(buf, "ERROR (%08lx) on line %d\n", error, line );
	MessageBox( NULL, buf, "ERROR", MB_OK );
    }
}
#else

#define checkerror() do { } while(0)

#endif


// --- MTX_Fragment Shader Extension ---
#define GL_FRAGMENT_SHADER_MTX          0x8A80

// Get Parameters
#define GL_FS_BINDING_MTX               0x8A81
#define GL_MAX_TA_OPS_MTX               0x8A82
#define GL_OPT_TA_OPS_MTX               0x8A83
#define GL_TA_OPS_MTX                   0x8A84
#define GL_MAX_FP_OPS_MTX               0x8A85
#define GL_MAX_FP_TEXTURES_MTX          0x8A86
#define GL_MAX_FP_COLOURS_MTX           0x8A87
#define GL_OPT_FP_OPS_MTX               0x8A88
#define GL_OPT_FP_TEXTURES_MTX          0x8A89
#define GL_OPT_FP_COLOURS_MTX           0x8A8A
#define GL_FP_OPS_MTX                   0x8A8B
#define GL_FP_TEXTURES_MTX              0x8A8C
#define GL_FP_COLOURS_MTX               0x8A8D

// Texture Opcodes
#define GL_OP_TEX_NOP_MTX               0x8A8E
#define GL_OP_TEX_MTX                   0x8A8F
#define GL_OP_TEX_COORD_MTX             0x8A90
#define GL_OP_TEXKILL_GE_MTX            0x8A91
#define GL_OP_TEXKILL_LT_MTX            0x8A92

#define GL_OP_TEXBEM_MTX                0x8A93
#define GL_OP_TEXDEP_AR_MTX             0x8A94
#define GL_OP_TEXDEP_GB_MTX             0x8A95
#define GL_OP_TEXDEP_RGB_MTX            0x8A96
#define GL_OP_TEXDOT3_COORD_MTX         0x8A97
#define GL_OP_TEXDOT3_MTX               0x8A98

#define GL_OP_TEXM3X2_MTX               0x8A99
#define GL_OP_TEXM3X2_DEPTH_MTX         0x8A9A

#define GL_OP_TEXM3X3_MTX               0x8A9B
#define GL_OP_TEXM3X3_COORD_MTX         0x8A9C
#define GL_OP_TEXM3X3_REFLECT_MTX       0x8A9D

#define GL_TEXBEM_MATRIX_MTX            0x8A9E
#define GL_TEXTURE_FORMAT_MTX           0x8A9F

// Texture Formats
#define GL_TEX_UNSIGNED_MTX             0x8AC0
#define GL_TEX_SIGNED_MTX               0x8AC1
#define GL_TEX_BIASED_MTX               0x8AC2

// Hints
#define GL_HINT_NEAREST_MTX             0x0001
#define GL_HINT_BILINEAR_MTX            0x0002
#define GL_HINT_TRILINEAR_MTX           0x0004
#define GL_HINT_ANISOTROPIC_MTX         0x0008
#define GL_HINT_TEXTURE_1D_MTX          0x0010
#define GL_HINT_TEXTURE_2D_MTX          0x0020
#define GL_HINT_TEXTURE_3D_MTX          0x0040
#define GL_HINT_TEXTURE_CUBE_MTX        0x0080
#define GL_HINT_TEXTURE_BEML_MTX        0x0100
#define GL_HINT_TEXTURE_BEM_MTX         0x0200

// Colour Opcodes
#define GL_OP_MOV_MTX                   0x8AA0
#define GL_OP_ADD_MTX                   0x8AA1
#define GL_OP_SUB_MTX                   0x8AA2
#define GL_OP_MUL_MTX                   0x8AA3
#define GL_OP_DP3_MTX                   0x8AA4
#define GL_OP_DP4_MTX                   0x8AA5
#define GL_OP_MAD_MTX                   0x8AA6
#define GL_OP_LRP_MTX                   0x8AA7
#define GL_OP_CND_MTX                   0x8AA8
#define GL_OP_CMV_MTX                   0x8AA9
#define GL_OP_CMP_MTX                   0x8AAA

// Result
#define GL_COLOR0_MTX                   0x0B00 // GL_CURRENT_COLOR
#define GL_COLOR1_MTX                   0x8459 // GL_CURRENT_SECONDARY_COLOR
#define GL_TEXTURE0                     0x84C0 // GL_TEXTURE0_ARB
#define GL_TEXTURE1                     0x84C1 // GL_TEXTURE1_ARB
#define GL_TEXTURE2                     0x84C2 // GL_TEXTURE2_ARB
#define GL_TEXTURE3                     0x84C3 // GL_TEXTURE3_ARB
#define GL_TEMPORARY0_MTX               0x8AB0
#define GL_TEMPORARY1_MTX               0x8AB1
#define GL_IGNORED_MTX                  0x8AB2
#define GL_OUTPUT_COLOR_MTX             0x8AB3

// Target
#define GL_RESULT_MTX                   0x8AC3
#define GL_ARGUMENT0_MTX                0x8AC4
#define GL_ARGUMENT1_MTX                0x8AC5
#define GL_ARGUMENT2_MTX                0x8AC6

// Result Modifier
#define GL_SATURATE_MTX                 0x8AC7
#define GL_SCALE_MTX                    0x8AC8

// Arguments Modifier
#define GL_REPLICATE_MTX                0x8AC9
#define GL_CONVERT_MTX                  0x8ACA

#define GL_NONE_MTX                     0x8ACB

// GL_SCALE_MTX
#define GL_SCALE_X2_MTX                 0x8ACC
#define GL_SCALE_X4_MTX                 0x8ACD
#define GL_SCALE_X8_MTX                 0x8ACE
#define GL_SCALE_D2_MTX                 0x8ACF
#define GL_SCALE_D4_MTX                 0x8AD0

// GL_CONVERT_MTX
#define GL_ARG_INVERT_MTX               0x8AB4
#define GL_ARG_NEGATE_MTX               0x8AB5
#define GL_ARG_BIAS_MTX                 0x8AB6
#define GL_ARG_BIAS_NEGATE_MTX          0x8AB7
#define GL_ARG_SIGNED_MTX               0x8AB8
#define GL_ARG_SIGNED_NEGATE_MTX        0x8AB9

// GL_REPLICATE_MTX
#define GL_RED_MTX                      0x8ABA
#define GL_GREEN_MTX                    0x8ABB
#define GL_BLUE_MTX                     0x8ABC
#define GL_ALPHA_MTX                    0x8ABD


/* Fragment_Shader_ext */
typedef GLvoid    (APIENTRY * PFNGLBEGINFRAGSHADEREXTPROC)          ( GLvoid );
typedef GLvoid    (APIENTRY * PFNGLBINDFRAGSHADEREXTPROC)           ( GLuint id);
typedef GLvoid    (APIENTRY * PFNGLDELETEFRAGSHADERSEXTPROC)        ( GLsizei n, const GLuint *shaders );
typedef GLvoid    (APIENTRY * PFNGLENDFRAGSHADEREXTPROC)            ( GLvoid );
typedef GLvoid    (APIENTRY * PFNGLFRAGPROCALPHAOP1EXTPROC)         ( GLenum op, GLenum res, GLenum arg0 );
typedef GLvoid    (APIENTRY * PFNGLFRAGPROCALPHAOP2EXTPROC)         ( GLenum op, GLenum res, GLenum arg0, GLenum arg1 );
typedef GLvoid    (APIENTRY * PFNGLFRAGPROCALPHAOP3EXTPROC)         ( GLenum op, GLenum res, GLenum arg0, GLenum arg1, GLenum arg2 );
typedef GLvoid    (APIENTRY * PFNGLFRAGPROCOPPARAMMTXEXTPROC)       ( GLenum target, GLenum pname, GLenum param );
typedef GLvoid    (APIENTRY * PFNGLFRAGPROCRGBAOP1EXTPROC)          ( GLenum op, GLenum res, GLenum arg0 );
typedef GLvoid    (APIENTRY * PFNGLFRAGPROCRGBAOP2EXTPROC)          ( GLenum op, GLenum res, GLenum arg0, GLenum arg1 );
typedef GLvoid    (APIENTRY * PFNGLFRAGPROCRGBAOP3EXTPROC)          ( GLenum op, GLenum res, GLenum arg0, GLenum arg1, GLenum arg2 );
typedef GLuint    (APIENTRY * PFNGLGENFRAGPROCINVARIANTBVEXTPROC)   ( GLbyte* values);
typedef GLuint    (APIENTRY * PFNGLGENFRAGPROCINVARIANTDVEXTPROC)   ( GLdouble* values);
typedef GLuint    (APIENTRY * PFNGLGENFRAGPROCINVARIANTFVEXTPROC)   ( GLfloat* values);
typedef GLuint    (APIENTRY * PFNGLGENFRAGPROCINVARIANTIVEXTPROC)   ( GLint* values);
typedef GLuint    (APIENTRY * PFNGLGENFRAGPROCINVARIANTSVEXTPROC)   ( GLshort* values);
typedef GLuint    (APIENTRY * PFNGLGENFRAGSHADERSEXTPROC)           ( GLsizei n );
typedef GLboolean (APIENTRY * PFNGLISFRAGSHADEREXTPROC)             ( GLuint id );
typedef GLvoid    (APIENTRY * PFNGLSETFRAGPROCINVARIANTBVPROC)      ( GLuint id, const GLbyte* values );
typedef GLvoid    (APIENTRY * PFNGLSETFRAGPROCINVARIANTDVPROC)      ( GLuint id, const GLdouble* values );
typedef GLvoid    (APIENTRY * PFNGLSETFRAGPROCINVARIANTFVPROC)      ( GLuint id, const GLfloat* values );
typedef GLvoid    (APIENTRY * PFNGLSETFRAGPROCINVARIANTIVPROC)      ( GLuint id, const GLint* values );
typedef GLvoid    (APIENTRY * PFNGLSETFRAGPROCINVARIANTSVPROC)      ( GLuint id, const GLshort* values );
typedef GLvoid    (APIENTRY * PFNGLTEXADDRESSHINTSEXTPROC)          ( GLenum target, GLbitfield hints );
typedef GLvoid    (APIENTRY * PFNGLTEXADDRESSOP1EXTPROC)            ( GLenum op, GLenum arg0 );
typedef GLvoid    (APIENTRY * PFNGLTEXADDRESSOP2EXTPROC)            ( GLenum op, GLenum arg0, GLenum arg1);
typedef GLvoid    (APIENTRY * PFNGLTEXADDRESSOP3EXTPROC)            ( GLenum op, GLenum arg0, GLenum arg1, GLenum arg2 );
typedef GLvoid    (APIENTRY * PFNGLTEXADDRESSOP4EXTPROC)            ( GLenum op, GLenum arg0, GLenum arg1, GLenum arg2, GLenum arg3 );
typedef GLvoid    (APIENTRY * PFNGLTEXADDRESSPARAMFVEXTPROC)        ( GLenum target, GLenum pname, const GLfloat* param );
typedef GLvoid    (APIENTRY * PFNGLTEXADDRESSPARAMUIEXTPROC)        ( GLenum target, GLenum pname, GLuint param );
typedef GLvoid    (APIENTRY * PFNGLGETFRAGPROCINVARIANTBVEXTPROC)   ( GLuint id, GLbyte* values);
typedef GLvoid    (APIENTRY * PFNGLGETFRAGPROCINVARIANTDVEXTPROC)   ( GLuint id, GLdouble* values);
typedef GLvoid    (APIENTRY * PFNGLGETFRAGPROCINVARIANTFVEXTPROC)   ( GLuint id, GLfloat* values);
typedef GLvoid    (APIENTRY * PFNGLGETFRAGPROCINVARIANTIVEXTPROC)   ( GLuint id, GLint* values);
typedef GLvoid    (APIENTRY * PFNGLGETFRAGPROCINVARIANTSVEXTPROC)   ( GLuint id, GLshort* values);
typedef GLvoid    (APIENTRY * PFNGLGETTEXADDRESSPARAMFVEXTPROC)     ( GLenum target, GLenum pname, GLfloat* param);
typedef GLvoid    (APIENTRY * PFNGLGETTEXADDRESSPARAMUIVEXTPROC)    ( GLenum target, GLenum pname, GLuint* param);


// --- End of MTX_Fragment Shader Extension ---

// Fragment Shader Extension Entry Points (pointers to the extension functions)
// ----------------------------------------------------------------------------

PFNGLBEGINFRAGSHADEREXTPROC qglBeginFragShaderMTX = NULL;
PFNGLBINDFRAGSHADEREXTPROC  qglBindFragShaderMTX = NULL;
PFNGLDELETEFRAGSHADERSEXTPROC qglDeleteFragShadersMTX = NULL;
PFNGLENDFRAGSHADEREXTPROC qglEndFragShaderMTX = NULL;
PFNGLFRAGPROCALPHAOP1EXTPROC qglFragProcAlphaOp1MTX = NULL;
PFNGLFRAGPROCALPHAOP2EXTPROC qglFragProcAlphaOp2MTX = NULL;
PFNGLFRAGPROCALPHAOP3EXTPROC qglFragProcAlphaOp3MTX = NULL;
PFNGLFRAGPROCOPPARAMMTXEXTPROC qglFragProcOpParamMTX = NULL;
PFNGLFRAGPROCRGBAOP1EXTPROC qglFragProcRGBAOp1MTX = NULL;
PFNGLFRAGPROCRGBAOP2EXTPROC qglFragProcRGBAOp2MTX = NULL;
PFNGLFRAGPROCRGBAOP3EXTPROC qglFragProcRGBAOp3MTX = NULL;
PFNGLGENFRAGPROCINVARIANTBVEXTPROC qglGenFragProcInvariantBVMTX = NULL;
PFNGLGENFRAGPROCINVARIANTDVEXTPROC qglGenFragProcInvariantDVMTX = NULL;
PFNGLGENFRAGPROCINVARIANTFVEXTPROC qglGenFragProcInvariantFVMTX = NULL;
PFNGLGENFRAGPROCINVARIANTIVEXTPROC qglGenFragProcInvariantIVMTX = NULL;
PFNGLGENFRAGPROCINVARIANTSVEXTPROC qglGenFragProcInvariantSVMTX = NULL;
PFNGLGENFRAGSHADERSEXTPROC qglGenFragShadersMTX = NULL;
PFNGLISFRAGSHADEREXTPROC qglIsFragShaderMTX = NULL;
PFNGLSETFRAGPROCINVARIANTBVPROC qglSetFragProcInvariantBVMTX = NULL;
PFNGLSETFRAGPROCINVARIANTDVPROC qglSetFragProcInvariantDVMTX = NULL;
PFNGLSETFRAGPROCINVARIANTFVPROC qglSetFragProcInvariantFVMTX = NULL;
PFNGLSETFRAGPROCINVARIANTIVPROC qglSetFragProcInvariantIVMTX = NULL;
PFNGLSETFRAGPROCINVARIANTSVPROC qglSetFragProcInvariantSVMTX = NULL;
PFNGLTEXADDRESSHINTSEXTPROC qglTexAddressHintsMTX = NULL;
PFNGLTEXADDRESSOP1EXTPROC qglTexAddressOp1MTX = NULL;
PFNGLTEXADDRESSOP2EXTPROC qglTexAddressOp2MTX = NULL;
PFNGLTEXADDRESSOP3EXTPROC qglTexAddressOp3MTX = NULL;
PFNGLTEXADDRESSOP4EXTPROC qglTexAddressOp4MTX = NULL;
PFNGLTEXADDRESSPARAMFVEXTPROC qglTexAddressParamFVMTX = NULL;
PFNGLTEXADDRESSPARAMUIEXTPROC qglTexAddressParamUIMTX = NULL;
PFNGLGETFRAGPROCINVARIANTBVEXTPROC qglGetFragProcInvariantBVMTX = NULL;
PFNGLGETFRAGPROCINVARIANTDVEXTPROC qglGetFragProcInvariantDVMTX = NULL;
PFNGLGETFRAGPROCINVARIANTFVEXTPROC qglGetFragProcInvariantFVMTX = NULL;
PFNGLGETFRAGPROCINVARIANTIVEXTPROC qglGetFragProcInvariantIVMTX = NULL;
PFNGLGETFRAGPROCINVARIANTSVEXTPROC qglGetFragProcInvariantSVMTX = NULL;
PFNGLGETTEXADDRESSPARAMFVEXTPROC qglGetTexAddressParamFVMTX = NULL;
PFNGLGETTEXADDRESSPARAMUIVEXTPROC qglGetTexAddressParamUIVMTX = NULL;

// EXT_vertex_shader stuff is in gl_bumpradeon.c
extern PFNGLBEGINVERTEXSHADEREXTPROC           qglBeginVertexShaderEXT;
extern PFNGLENDVERTEXSHADEREXTPROC             qglEndVertexShaderEXT;
extern PFNGLBINDVERTEXSHADEREXTPROC            qglBindVertexShaderEXT;
extern PFNGLGENVERTEXSHADERSEXTPROC            qglGenVertexShadersEXT;
extern PFNGLDELETEVERTEXSHADEREXTPROC          qglDeleteVertexShaderEXT;
extern PFNGLSHADEROP1EXTPROC                   qglShaderOp1EXT;
extern PFNGLSHADEROP2EXTPROC                   qglShaderOp2EXT;
extern PFNGLSHADEROP3EXTPROC                   qglShaderOp3EXT;
extern PFNGLSWIZZLEEXTPROC                     qglSwizzleEXT;
extern PFNGLWRITEMASKEXTPROC                   qglWriteMaskEXT;
extern PFNGLINSERTCOMPONENTEXTPROC             qglInsertComponentEXT;
extern PFNGLEXTRACTCOMPONENTEXTPROC            qglExtractComponentEXT;
extern PFNGLGENSYMBOLSEXTPROC                  qglGenSymbolsEXT;
extern PFNGLSETINVARIANTEXTPROC                qglSetInvariantEXT;
extern PFNGLSETLOCALCONSTANTEXTPROC            qglSetLocalConstantEXT;
extern PFNGLVARIANTBVEXTPROC                   qglVariantbvEXT;
extern PFNGLVARIANTSVEXTPROC                   qglVariantsvEXT;
extern PFNGLVARIANTIVEXTPROC                   qglVariantivEXT;
extern PFNGLVARIANTFVEXTPROC                   qglVariantfvEXT;
extern PFNGLVARIANTDVEXTPROC                   qglVariantdvEXT;
extern PFNGLVARIANTUBVEXTPROC                  qglVariantubvEXT;
extern PFNGLVARIANTUSVEXTPROC                  qglVariantusvEXT;
extern PFNGLVARIANTUIVEXTPROC                  qglVariantuivEXT;
extern PFNGLVARIANTPOINTEREXTPROC              qglVariantPointerEXT;
extern PFNGLENABLEVARIANTCLIENTSTATEEXTPROC    qglEnableVariantClientStateEXT;
extern PFNGLDISABLEVARIANTCLIENTSTATEEXTPROC   qglDisableVariantClientStateEXT;
extern PFNGLBINDLIGHTPARAMETEREXTPROC          qglBindLightParameterEXT;
extern PFNGLBINDMATERIALPARAMETEREXTPROC       qglBindMaterialParameterEXT;
extern PFNGLBINDTEXGENPARAMETEREXTPROC         qglBindTexGenParameterEXT;
extern PFNGLBINDTEXTUREUNITPARAMETEREXTPROC    qglBindTextureUnitParameterEXT;
extern PFNGLBINDPARAMETEREXTPROC               qglBindParameterEXT;
extern PFNGLISVARIANTENABLEDEXTPROC            qglIsVariantEnabledEXT;
extern PFNGLGETVARIANTBOOLEANVEXTPROC          qglGetVariantBooleanvEXT;
extern PFNGLGETVARIANTINTEGERVEXTPROC          qglGetVariantIntegervEXT;
extern PFNGLGETVARIANTFLOATVEXTPROC            qglGetVariantFloatvEXT;
extern PFNGLGETVARIANTPOINTERVEXTPROC          qglGetVariantPointervEXT;
extern PFNGLGETINVARIANTBOOLEANVEXTPROC        qglGetInvariantBooleanvEXT;
extern PFNGLGETINVARIANTINTEGERVEXTPROC        qglGetInvariantIntegervEXT;
extern PFNGLGETINVARIANTFLOATVEXTPROC          qglGetInvariantFloatvEXT;
extern PFNGLGETLOCALCONSTANTBOOLEANVEXTPROC    qglGetLocalConstantBooleanvEXT;
extern PFNGLGETLOCALCONSTANTINTEGERVEXTPROC    qglGetLocalConstantIntegervEXT;
extern PFNGLGETLOCALCONSTANTFLOATVEXTPROC      qglGetLocalConstantFloatvEXT;

#if defined(__APPLE__) || defined (MACOSX)

extern void *	Sys_GetProcAddress (const char *theName, qboolean theSafeMode);

qboolean GL_LookupSymbolsParhelia()
{
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
	qglBeginFragShaderMTX = Sys_GetProcAddress ("glBeginFragShaderMTX", false);
	qglBindFragShaderMTX = Sys_GetProcAddress ("glBindFragShaderMTX", false);
	qglDeleteFragShadersMTX = Sys_GetProcAddress ("glDeleteFragShadersMTX", false);
	qglEndFragShaderMTX = Sys_GetProcAddress ("glEndFragShaderMTX", false);
	qglFragProcAlphaOp1MTX = Sys_GetProcAddress ("glFragProcAlphaOp1MTX", false);
	qglFragProcAlphaOp2MTX = Sys_GetProcAddress ("glFragProcAlphaOp2MTX", false);
	qglFragProcAlphaOp3MTX = Sys_GetProcAddress ("glFragProcAlphaOp3MTX", false);
	qglFragProcOpParamMTX = Sys_GetProcAddress ("glFragProcOpParamMTX", false);
	qglFragProcRGBAOp1MTX = Sys_GetProcAddress ("glFragProcRGBAOp1MTX", false);
	qglFragProcRGBAOp2MTX = Sys_GetProcAddress ("glFragProcRGBAOp2MTX", false);
	qglFragProcRGBAOp3MTX = Sys_GetProcAddress ("glFragProcRGBAOp3MTX", false);
	qglGenFragProcInvariantBVMTX = Sys_GetProcAddress ("glGenFragProcInvariantbvMTX", false);
	qglGenFragProcInvariantDVMTX = Sys_GetProcAddress ("glGenFragProcInvariantdvMTX", false);
	qglGenFragProcInvariantFVMTX = Sys_GetProcAddress ("glGenFragProcInvariantfvMTX", false);
	qglGenFragProcInvariantIVMTX = Sys_GetProcAddress ("glGenFragProcInvariantivMTX", false);
	qglGenFragProcInvariantSVMTX = Sys_GetProcAddress ("glGenFragProcInvariantsvMTX", false);
	qglGenFragShadersMTX = Sys_GetProcAddress ("glGenFragShadersMTX", false);
	qglIsFragShaderMTX = Sys_GetProcAddress ("glIsFragShaderMTX", false);
	qglSetFragProcInvariantBVMTX = Sys_GetProcAddress ("glSetFragProcInvariantbvMTX", false);
	qglSetFragProcInvariantDVMTX = Sys_GetProcAddress ("glSetFragProcInvariantdvMTX", false);
	qglSetFragProcInvariantFVMTX = Sys_GetProcAddress ("glSetFragProcInvariantfvMTX", false);
	qglSetFragProcInvariantIVMTX = Sys_GetProcAddress ("glSetFragProcInvariantivMTX", false);
	qglSetFragProcInvariantSVMTX = Sys_GetProcAddress ("glSetFragProcInvariantsvMTX", false);
	qglTexAddressHintsMTX = Sys_GetProcAddress ("glTexAddressHintsMTX", false);
	qglTexAddressOp1MTX = Sys_GetProcAddress ("glTexAddressOp1MTX", false);
	qglTexAddressOp2MTX = Sys_GetProcAddress ("glTexAddressOp2MTX", false);
	qglTexAddressOp3MTX = Sys_GetProcAddress ("glTexAddressOp3MTX", false);
	qglTexAddressOp4MTX = Sys_GetProcAddress ("glTexAddressOp4MTX", false);
	qglTexAddressParamFVMTX = Sys_GetProcAddress ("glTexAddressParamfvMTX", false);
	qglTexAddressParamUIMTX = Sys_GetProcAddress ("glTexAddressParamuiMTX", false);
	qglGetFragProcInvariantBVMTX = Sys_GetProcAddress ("glGetFragProcInvariantbvMTX", false);
	qglGetFragProcInvariantDVMTX = Sys_GetProcAddress ("glGetFragProcInvariantdvMTX", false);
	qglGetFragProcInvariantFVMTX = Sys_GetProcAddress ("glGetFragProcInvariantfvMTX", false);
	qglGetFragProcInvariantIVMTX = Sys_GetProcAddress ("glGetFragProcInvariantivMTX", false);
	qglGetFragProcInvariantSVMTX = Sys_GetProcAddress ("glGetFragProcInvariantsvMTX", false);
	qglGetTexAddressParamFVMTX = Sys_GetProcAddress ("glGetTexAddressParamfvMTX", false);
	qglGetTexAddressParamUIVMTX = Sys_GetProcAddress ("glGetTexAddressParamuivMTX", false);

	if (qglBeginVertexShaderEXT != NULL &&
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
		qglBeginFragShaderMTX != NULL &&
		qglBindFragShaderMTX != NULL &&
		qglDeleteFragShadersMTX != NULL &&
		qglEndFragShaderMTX != NULL &&
		qglFragProcAlphaOp1MTX != NULL &&
		qglFragProcAlphaOp2MTX != NULL &&
		qglFragProcAlphaOp3MTX != NULL &&
		qglFragProcOpParamMTX != NULL &&
		qglFragProcRGBAOp1MTX != NULL &&
		qglFragProcRGBAOp2MTX != NULL &&
		qglFragProcRGBAOp3MTX != NULL &&
		qglGenFragProcInvariantBVMTX != NULL &&
		qglGenFragProcInvariantDVMTX != NULL &&
		qglGenFragProcInvariantFVMTX != NULL &&
		qglGenFragProcInvariantIVMTX != NULL &&
		qglGenFragProcInvariantSVMTX != NULL &&
		qglGenFragShadersMTX != NULL &&
		qglIsFragShaderMTX != NULL &&
		qglSetFragProcInvariantBVMTX != NULL &&
		qglSetFragProcInvariantDVMTX != NULL &&
		qglSetFragProcInvariantFVMTX != NULL &&
		qglSetFragProcInvariantIVMTX != NULL &&
		qglSetFragProcInvariantSVMTX != NULL &&
		qglTexAddressHintsMTX != NULL &&
		qglTexAddressOp1MTX != NULL &&
		qglTexAddressOp2MTX != NULL &&
		qglTexAddressOp3MTX != NULL &&
		qglTexAddressOp4MTX != NULL &&
		qglTexAddressParamFVMTX != NULL &&
		qglTexAddressParamUIMTX != NULL &&
		qglGetFragProcInvariantBVMTX != NULL &&
		qglGetFragProcInvariantDVMTX != NULL &&
		qglGetFragProcInvariantFVMTX != NULL &&
		qglGetFragProcInvariantIVMTX != NULL &&
		qglGetFragProcInvariantSVMTX != NULL &&
		qglGetTexAddressParamFVMTX != NULL &&
		qglGetTexAddressParamUIVMTX != NULL)
	{
		return (true);
	}

	return (false);
}

#endif // __APPLE__ || MACOSX

void GL_CreateShadersParhelia()
{
    GLuint mvp, modelview, zcomp;
    GLuint texturematrix;
    GLuint vertex, normal;
    GLuint texcoord0;
    GLuint texcoord1;
    GLuint color;
    GLuint disttemp, disttemp2;
    GLuint fogstart, fogend;
    GLuint scaler;
	GLfloat scalervalues[4] = { 0.5, 0.5, 0.5, 0.5 };

#if !defined(__APPLE__) && !defined (MACOSX)
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

    SAFE_GET_PROC( qglBeginFragShaderMTX, PFNGLBEGINFRAGSHADEREXTPROC, "glBeginFragShaderMTX");
    SAFE_GET_PROC( qglBindFragShaderMTX, PFNGLBINDFRAGSHADEREXTPROC, "glBindFragShaderMTX");
    SAFE_GET_PROC( qglDeleteFragShadersMTX, PFNGLDELETEFRAGSHADERSEXTPROC, "glDeleteFragShadersMTX");
    SAFE_GET_PROC( qglEndFragShaderMTX, PFNGLENDFRAGSHADEREXTPROC, "glEndFragShaderMTX");
    SAFE_GET_PROC( qglFragProcAlphaOp1MTX, PFNGLFRAGPROCALPHAOP1EXTPROC, "glFragProcAlphaOp1MTX");
    SAFE_GET_PROC( qglFragProcAlphaOp2MTX, PFNGLFRAGPROCALPHAOP2EXTPROC, "glFragProcAlphaOp2MTX");
    SAFE_GET_PROC( qglFragProcAlphaOp3MTX, PFNGLFRAGPROCALPHAOP3EXTPROC, "glFragProcAlphaOp3MTX");
    SAFE_GET_PROC( qglFragProcOpParamMTX, PFNGLFRAGPROCOPPARAMMTXEXTPROC, "glFragProcOpParamMTX");
    SAFE_GET_PROC( qglFragProcRGBAOp1MTX, PFNGLFRAGPROCRGBAOP1EXTPROC, "glFragProcRGBAOp1MTX");
    SAFE_GET_PROC( qglFragProcRGBAOp2MTX, PFNGLFRAGPROCRGBAOP2EXTPROC, "glFragProcRGBAOp2MTX");
    SAFE_GET_PROC( qglFragProcRGBAOp3MTX, PFNGLFRAGPROCRGBAOP3EXTPROC, "glFragProcRGBAOp3MTX");
    SAFE_GET_PROC( qglGenFragProcInvariantBVMTX, PFNGLGENFRAGPROCINVARIANTBVEXTPROC, "glGenFragProcInvariantbvMTX");
    SAFE_GET_PROC( qglGenFragProcInvariantDVMTX, PFNGLGENFRAGPROCINVARIANTDVEXTPROC, "glGenFragProcInvariantdvMTX");
    SAFE_GET_PROC( qglGenFragProcInvariantFVMTX, PFNGLGENFRAGPROCINVARIANTFVEXTPROC, "glGenFragProcInvariantfvMTX");
    SAFE_GET_PROC( qglGenFragProcInvariantIVMTX, PFNGLGENFRAGPROCINVARIANTIVEXTPROC, "glGenFragProcInvariantivMTX");
    SAFE_GET_PROC( qglGenFragProcInvariantSVMTX, PFNGLGENFRAGPROCINVARIANTSVEXTPROC, "glGenFragProcInvariantsvMTX");
    SAFE_GET_PROC( qglGenFragShadersMTX, PFNGLGENFRAGSHADERSEXTPROC, "glGenFragShadersMTX");
    SAFE_GET_PROC( qglIsFragShaderMTX, PFNGLISFRAGSHADEREXTPROC, "glIsFragShaderMTX");
    SAFE_GET_PROC( qglSetFragProcInvariantBVMTX, PFNGLSETFRAGPROCINVARIANTBVPROC, "glSetFragProcInvariantbvMTX");
    SAFE_GET_PROC( qglSetFragProcInvariantDVMTX, PFNGLSETFRAGPROCINVARIANTDVPROC, "glSetFragProcInvariantdvMTX");
    SAFE_GET_PROC( qglSetFragProcInvariantFVMTX, PFNGLSETFRAGPROCINVARIANTFVPROC, "glSetFragProcInvariantfvMTX");
    SAFE_GET_PROC( qglSetFragProcInvariantIVMTX, PFNGLSETFRAGPROCINVARIANTIVPROC, "glSetFragProcInvariantivMTX");
    SAFE_GET_PROC( qglSetFragProcInvariantSVMTX, PFNGLSETFRAGPROCINVARIANTSVPROC, "glSetFragProcInvariantsvMTX");
    SAFE_GET_PROC( qglTexAddressHintsMTX, PFNGLTEXADDRESSHINTSEXTPROC, "glTexAddressHintsMTX");
    SAFE_GET_PROC( qglTexAddressOp1MTX, PFNGLTEXADDRESSOP1EXTPROC, "glTexAddressOp1MTX");
    SAFE_GET_PROC( qglTexAddressOp2MTX, PFNGLTEXADDRESSOP2EXTPROC, "glTexAddressOp2MTX");
    SAFE_GET_PROC( qglTexAddressOp3MTX, PFNGLTEXADDRESSOP3EXTPROC, "glTexAddressOp3MTX");
    SAFE_GET_PROC( qglTexAddressOp4MTX, PFNGLTEXADDRESSOP4EXTPROC, "glTexAddressOp4MTX");
    SAFE_GET_PROC( qglTexAddressParamFVMTX, PFNGLTEXADDRESSPARAMFVEXTPROC, "glTexAddressParamfvMTX");
    SAFE_GET_PROC( qglTexAddressParamUIMTX, PFNGLTEXADDRESSPARAMUIEXTPROC, "glTexAddressParamuiMTX");
    SAFE_GET_PROC( qglGetFragProcInvariantBVMTX, PFNGLGETFRAGPROCINVARIANTBVEXTPROC, "glGetFragProcInvariantbvMTX");
    SAFE_GET_PROC( qglGetFragProcInvariantDVMTX, PFNGLGETFRAGPROCINVARIANTDVEXTPROC, "glGetFragProcInvariantdvMTX");
    SAFE_GET_PROC( qglGetFragProcInvariantFVMTX, PFNGLGETFRAGPROCINVARIANTFVEXTPROC, "glGetFragProcInvariantfvMTX");
    SAFE_GET_PROC( qglGetFragProcInvariantIVMTX, PFNGLGETFRAGPROCINVARIANTIVEXTPROC, "glGetFragProcInvariantivMTX");
    SAFE_GET_PROC( qglGetFragProcInvariantSVMTX, PFNGLGETFRAGPROCINVARIANTSVEXTPROC, "glGetFragProcInvariantsvMTX");

    SAFE_GET_PROC( qglGetTexAddressParamFVMTX, PFNGLGETTEXADDRESSPARAMFVEXTPROC, "glGetTexAddressParamfvMTX");
    SAFE_GET_PROC( qglGetTexAddressParamUIVMTX, PFNGLGETTEXADDRESSPARAMUIVEXTPROC, "glGetTexAddressParamuivMTX");
#endif

    //DEBUG
    // Con_Printf("Errors before creating shaders...\n%s\n",gluErrorString(glGetError()));
    // Con_Printf("----Fragment Shaders----\n");

	
    // Generate two shaders, diffuse and specular, with two variants for cube and 3d atten texture
    glEnable(GL_FRAGMENT_SHADER_MTX);
    
    //DEBUG
    // Con_Printf("Enabled GL_MTX_FRAGMENT_SHADER...\n%s\n",gluErrorString(glGetError()));


    fragment_shaders = qglGenFragShadersMTX(4);
    
    /*
      if (fragment_shaders == 0)
      {
      Con_Printf("Error: unable to allocate shaders: %s\n", gluErrorString(glGetError()));
      } else
      {
      Con_Printf("Allocated 4 fragment shaders\n");
      };	
    */


    // Diffuse shader 1 (3D atten)
    qglBindFragShaderMTX(fragment_shaders);
	
    //DEBUG
    // Con_Printf("Shader %i Bind Status: %s\n",fragment_shaders,gluErrorString(glGetError()));


    
    qglBeginFragShaderMTX();
    
    qglFragProcRGBAOp1MTX(GL_OP_MOV_MTX, GL_TEMPORARY1_MTX, GL_COLOR0_MTX); //Matrox Work Around

    // tex t0
    qglTexAddressHintsMTX( GL_TEXTURE0, GL_HINT_TEXTURE_2D_MTX|GL_HINT_TRILINEAR_MTX);
    qglTexAddressParamUIMTX( GL_TEXTURE0, GL_TEXTURE_FORMAT_MTX, GL_TEX_UNSIGNED_MTX);
    qglTexAddressOp1MTX( GL_OP_TEX_MTX, GL_TEXTURE0 );

    // tex t1
    qglTexAddressHintsMTX( GL_TEXTURE1, GL_HINT_TEXTURE_CUBE_MTX|GL_HINT_BILINEAR_MTX);
    qglTexAddressParamUIMTX( GL_TEXTURE1, GL_TEXTURE_FORMAT_MTX, GL_TEX_UNSIGNED_MTX);
    qglTexAddressOp1MTX( GL_OP_TEX_MTX, GL_TEXTURE1 );

    // tex t2
    qglTexAddressHintsMTX( GL_TEXTURE2, GL_HINT_TEXTURE_2D_MTX|GL_HINT_TRILINEAR_MTX);
    qglTexAddressParamUIMTX( GL_TEXTURE2, GL_TEXTURE_FORMAT_MTX, GL_TEX_UNSIGNED_MTX);
    qglTexAddressOp1MTX( GL_OP_TEX_MTX, GL_TEXTURE2 );

    // tex t3 (3D)
    qglTexAddressHintsMTX( GL_TEXTURE3, GL_HINT_TEXTURE_3D_MTX|GL_HINT_BILINEAR_MTX);
    qglTexAddressParamUIMTX( GL_TEXTURE3, GL_TEXTURE_FORMAT_MTX, GL_TEX_UNSIGNED_MTX);
    qglTexAddressOp1MTX( GL_OP_TEX_MTX, GL_TEXTURE3 );

    //dp3_sat r0, t0_bx2, t1_bx2
    qglFragProcOpParamMTX(GL_RESULT_MTX, GL_SATURATE_MTX, GL_TRUE);
    qglFragProcOpParamMTX(GL_ARGUMENT0_MTX, GL_CONVERT_MTX, GL_ARG_SIGNED_MTX);
    qglFragProcOpParamMTX(GL_ARGUMENT1_MTX, GL_CONVERT_MTX, GL_ARG_SIGNED_MTX);
    qglFragProcRGBAOp2MTX(GL_OP_DP3_MTX, GL_TEMPORARY0_MTX, GL_TEXTURE0, GL_TEXTURE1);

    //mul r0.rgb, r0, t2
    qglFragProcRGBAOp2MTX(GL_OP_MUL_MTX, GL_TEMPORARY0_MTX, GL_TEMPORARY0_MTX, GL_TEXTURE2);
    
    //+add_x4 r1.a, t0_bx2.b, t0_bx2.b
    qglFragProcOpParamMTX(GL_RESULT_MTX, GL_SATURATE_MTX, GL_TRUE);
    qglFragProcOpParamMTX(GL_RESULT_MTX, GL_SCALE_MTX, GL_SCALE_X4_MTX);
    qglFragProcOpParamMTX(GL_ARGUMENT0_MTX, GL_CONVERT_MTX, GL_ARG_SIGNED_MTX);
    qglFragProcOpParamMTX(GL_ARGUMENT0_MTX, GL_REPLICATE_MTX, GL_BLUE_MTX);
    qglFragProcOpParamMTX(GL_ARGUMENT1_MTX, GL_CONVERT_MTX, GL_ARG_SIGNED_MTX);
    qglFragProcOpParamMTX(GL_ARGUMENT1_MTX, GL_REPLICATE_MTX, GL_BLUE_MTX);
    qglFragProcRGBAOp1MTX(GL_OP_MOV_MTX, GL_IGNORED_MTX, GL_TEXTURE0); // [JPS] insert this
    qglFragProcAlphaOp2MTX(GL_OP_ADD_MTX, GL_TEMPORARY1_MTX, GL_TEXTURE0, GL_TEXTURE0);
    
    //mul r0, r0, v0
    qglFragProcRGBAOp2MTX(GL_OP_MUL_MTX, GL_TEMPORARY0_MTX, GL_TEMPORARY0_MTX, GL_COLOR0_MTX);
    
    //mul r0, r0, t3
    qglFragProcRGBAOp2MTX(GL_OP_MUL_MTX, GL_TEMPORARY0_MTX, GL_TEMPORARY0_MTX, GL_TEXTURE3);
    
    //mul r0, r0, r1.a
    qglFragProcOpParamMTX(GL_RESULT_MTX, GL_SATURATE_MTX, GL_TRUE);
    qglFragProcOpParamMTX(GL_ARGUMENT1_MTX, GL_REPLICATE_MTX, GL_ALPHA_MTX);
    qglFragProcRGBAOp2MTX(GL_OP_MUL_MTX, GL_OUTPUT_COLOR_MTX, GL_TEMPORARY0_MTX, GL_TEMPORARY1_MTX);
    
    qglEndFragShaderMTX();
    
    // DEBUG - display status of shader creation
    // Con_Printf("Diffuse 1 Status: %s\n",gluErrorString(glGetError()));


    // Diffuse shader 2 (cube atten)
    qglBindFragShaderMTX(fragment_shaders+1);

    //DEBUG
    // Con_Printf("Shader %i Bind Status: %s\n",fragment_shaders+1,gluErrorString(glGetError()));

    qglBeginFragShaderMTX();
    
    qglFragProcRGBAOp1MTX(GL_OP_MOV_MTX, GL_TEMPORARY1_MTX, GL_COLOR0_MTX); //Matrox Work Around
    // tex t0
    qglTexAddressHintsMTX( GL_TEXTURE0, GL_HINT_TEXTURE_2D_MTX|GL_HINT_TRILINEAR_MTX);
    qglTexAddressParamUIMTX( GL_TEXTURE0, GL_TEXTURE_FORMAT_MTX, GL_TEX_UNSIGNED_MTX);
    qglTexAddressOp1MTX( GL_OP_TEX_MTX, GL_TEXTURE0 );

    // tex t1
    qglTexAddressHintsMTX( GL_TEXTURE1, GL_HINT_TEXTURE_CUBE_MTX|GL_HINT_BILINEAR_MTX);
    qglTexAddressParamUIMTX( GL_TEXTURE1, GL_TEXTURE_FORMAT_MTX, GL_TEX_UNSIGNED_MTX);
    qglTexAddressOp1MTX( GL_OP_TEX_MTX, GL_TEXTURE1 );

    // tex t2
    qglTexAddressHintsMTX( GL_TEXTURE2, GL_HINT_TEXTURE_2D_MTX|GL_HINT_TRILINEAR_MTX);
    qglTexAddressParamUIMTX( GL_TEXTURE2, GL_TEXTURE_FORMAT_MTX, GL_TEX_UNSIGNED_MTX);
    qglTexAddressOp1MTX( GL_OP_TEX_MTX, GL_TEXTURE2 );

    // tex t3 (cube)
    qglTexAddressHintsMTX( GL_TEXTURE3, GL_HINT_TEXTURE_CUBE_MTX|GL_HINT_BILINEAR_MTX);
    qglTexAddressParamUIMTX( GL_TEXTURE3, GL_TEXTURE_FORMAT_MTX, GL_TEX_UNSIGNED_MTX);
    qglTexAddressOp1MTX( GL_OP_TEX_MTX, GL_TEXTURE3 );

    //dp3_sat r0, t0_bx2, t1_bx2
    qglFragProcOpParamMTX(GL_RESULT_MTX, GL_SATURATE_MTX, GL_TRUE);
    qglFragProcOpParamMTX(GL_ARGUMENT0_MTX, GL_CONVERT_MTX, GL_ARG_SIGNED_MTX);
    qglFragProcOpParamMTX(GL_ARGUMENT1_MTX, GL_CONVERT_MTX, GL_ARG_SIGNED_MTX);
    qglFragProcRGBAOp2MTX(GL_OP_DP3_MTX, GL_TEMPORARY0_MTX, GL_TEXTURE0, GL_TEXTURE1);

    //mul r0.rgb, r0, t2
    qglFragProcRGBAOp2MTX(GL_OP_MUL_MTX, GL_TEMPORARY0_MTX, GL_TEMPORARY0_MTX, GL_TEXTURE2);

    //+add_x4 r1.a, t0_bx2.b, t0_bx2.b
    qglFragProcOpParamMTX(GL_RESULT_MTX, GL_SATURATE_MTX, GL_TRUE);
    qglFragProcOpParamMTX(GL_RESULT_MTX, GL_SCALE_MTX, GL_SCALE_X4_MTX);
    qglFragProcOpParamMTX(GL_ARGUMENT0_MTX, GL_CONVERT_MTX, GL_ARG_SIGNED_MTX);
    qglFragProcOpParamMTX(GL_ARGUMENT0_MTX, GL_REPLICATE_MTX, GL_BLUE_MTX);
    qglFragProcOpParamMTX(GL_ARGUMENT1_MTX, GL_CONVERT_MTX, GL_ARG_SIGNED_MTX);
    qglFragProcOpParamMTX(GL_ARGUMENT1_MTX, GL_REPLICATE_MTX, GL_BLUE_MTX);
    qglFragProcRGBAOp1MTX(GL_OP_MOV_MTX, GL_IGNORED_MTX, GL_TEXTURE0); // [JPS] insert this
    qglFragProcAlphaOp2MTX(GL_OP_ADD_MTX, GL_TEMPORARY1_MTX, GL_TEXTURE0, GL_TEXTURE0);

    //mul r0, r0, v0
    qglFragProcRGBAOp2MTX(GL_OP_MUL_MTX, GL_TEMPORARY0_MTX, GL_TEMPORARY0_MTX, GL_COLOR0_MTX);

    //mul r0, r0, t3
    qglFragProcRGBAOp2MTX(GL_OP_MUL_MTX, GL_TEMPORARY0_MTX, GL_TEMPORARY0_MTX, GL_TEXTURE3);

    //mul r0, r0, r1.a
    qglFragProcOpParamMTX(GL_RESULT_MTX, GL_SATURATE_MTX, GL_TRUE);
    qglFragProcOpParamMTX(GL_ARGUMENT1_MTX, GL_REPLICATE_MTX, GL_ALPHA_MTX);
    qglFragProcRGBAOp1MTX(GL_OP_MOV_MTX, GL_IGNORED_MTX, GL_TEMPORARY0_MTX); // [JPS] insert this
    qglFragProcRGBAOp2MTX(GL_OP_MUL_MTX, GL_OUTPUT_COLOR_MTX, GL_TEMPORARY0_MTX, GL_TEMPORARY1_MTX);

    qglEndFragShaderMTX();

    // DEBUG - display status of shader creation
    // Con_Printf("Diffuse 2 Status: %s\n",gluErrorString(glGetError()));

    // Specular shader 1 (3D atten)
    qglBindFragShaderMTX(fragment_shaders+2);
	
    //DEBUG
    // Con_Printf("Shader %i Bind Status: %s\n",fragment_shaders+2,gluErrorString(glGetError()));

    qglBeginFragShaderMTX();
    
    qglFragProcRGBAOp1MTX(GL_OP_MOV_MTX, GL_TEMPORARY1_MTX, GL_COLOR0_MTX); //Matrox Work Around

    // tex t0
    qglTexAddressHintsMTX( GL_TEXTURE0, GL_HINT_TEXTURE_2D_MTX|GL_HINT_TRILINEAR_MTX);
    qglTexAddressParamUIMTX( GL_TEXTURE0, GL_TEXTURE_FORMAT_MTX, GL_TEX_UNSIGNED_MTX);
    qglTexAddressOp1MTX( GL_OP_TEX_MTX, GL_TEXTURE0 );

    // tex t1
    qglTexAddressHintsMTX( GL_TEXTURE1, GL_HINT_TEXTURE_CUBE_MTX|GL_HINT_BILINEAR_MTX);
    qglTexAddressParamUIMTX( GL_TEXTURE1, GL_TEXTURE_FORMAT_MTX, GL_TEX_UNSIGNED_MTX);
    qglTexAddressOp1MTX( GL_OP_TEX_MTX, GL_TEXTURE1 );

    // tex t2
    qglTexAddressHintsMTX( GL_TEXTURE2, GL_HINT_TEXTURE_2D_MTX|GL_HINT_TRILINEAR_MTX);
    qglTexAddressParamUIMTX( GL_TEXTURE2, GL_TEXTURE_FORMAT_MTX, GL_TEX_UNSIGNED_MTX);
    qglTexAddressOp1MTX( GL_OP_TEX_MTX, GL_TEXTURE2 );

    // tex t3 (3D)
    qglTexAddressHintsMTX( GL_TEXTURE3, GL_HINT_TEXTURE_3D_MTX|GL_HINT_BILINEAR_MTX);
    qglTexAddressParamUIMTX( GL_TEXTURE3, GL_TEXTURE_FORMAT_MTX, GL_TEX_UNSIGNED_MTX);
    qglTexAddressOp1MTX( GL_OP_TEX_MTX, GL_TEXTURE3 );

    //dp3_sat r0, t0_bx2, t1_bx2
    qglFragProcOpParamMTX(GL_RESULT_MTX, GL_SATURATE_MTX, GL_TRUE);
    qglFragProcOpParamMTX(GL_ARGUMENT0_MTX, GL_CONVERT_MTX, GL_ARG_SIGNED_MTX);
    qglFragProcOpParamMTX(GL_ARGUMENT1_MTX, GL_CONVERT_MTX, GL_ARG_SIGNED_MTX);
    qglFragProcRGBAOp2MTX(GL_OP_DP3_MTX, GL_TEMPORARY0_MTX, GL_TEXTURE0, GL_TEXTURE1);

    //mul r1.rgb, t3, t0.a
    qglFragProcOpParamMTX(GL_ARGUMENT1_MTX, GL_REPLICATE_MTX, GL_ALPHA_MTX);
    qglFragProcRGBAOp2MTX(GL_OP_MUL_MTX, GL_TEMPORARY1_MTX, GL_TEXTURE3, GL_TEXTURE0);

    //+mad_x2_sat r1.a, r0.b, r0.b, c0
    qglFragProcOpParamMTX(GL_RESULT_MTX, GL_SATURATE_MTX, GL_TRUE);
    qglFragProcOpParamMTX(GL_RESULT_MTX, GL_SCALE_MTX, GL_SCALE_X2_MTX);
    qglFragProcOpParamMTX(GL_ARGUMENT0_MTX, GL_REPLICATE_MTX, GL_BLUE_MTX);
    qglFragProcOpParamMTX(GL_ARGUMENT1_MTX, GL_REPLICATE_MTX, GL_BLUE_MTX);
    qglFragProcOpParamMTX(GL_ARGUMENT2_MTX, GL_CONVERT_MTX, GL_ARG_NEGATE_MTX);
    scaler = qglGenFragProcInvariantFVMTX(scalervalues);
    qglFragProcRGBAOp1MTX(GL_OP_MOV_MTX, GL_IGNORED_MTX, GL_TEXTURE0); // [JPS] insert this
    qglFragProcAlphaOp3MTX(GL_OP_MAD_MTX, GL_TEMPORARY1_MTX, GL_TEMPORARY0_MTX, GL_TEMPORARY0_MTX, scaler);

    //mul r0, r1, v0
    qglFragProcRGBAOp2MTX(GL_OP_MUL_MTX, GL_TEMPORARY0_MTX, GL_TEMPORARY1_MTX, GL_COLOR0_MTX);

    //+mul r1.a, r1.a, r1.a
    qglFragProcRGBAOp1MTX(GL_OP_MOV_MTX, GL_IGNORED_MTX, GL_TEXTURE0); // [JPS] insert this
    qglFragProcAlphaOp2MTX(GL_OP_MUL_MTX, GL_TEMPORARY1_MTX, GL_TEMPORARY1_MTX, GL_TEMPORARY1_MTX);

    //mul r1.a, r1.a, r1.a
    qglFragProcRGBAOp1MTX(GL_OP_MOV_MTX, GL_IGNORED_MTX, GL_TEXTURE0); // [JPS] insert this
    qglFragProcAlphaOp2MTX(GL_OP_MUL_MTX, GL_TEMPORARY1_MTX, GL_TEMPORARY1_MTX, GL_TEMPORARY1_MTX);


    //mul r0, r0, r1.a
    qglFragProcOpParamMTX(GL_ARGUMENT1_MTX, GL_REPLICATE_MTX, GL_ALPHA_MTX);
    qglFragProcRGBAOp2MTX(GL_OP_MUL_MTX, GL_OUTPUT_COLOR_MTX, GL_TEMPORARY0_MTX, GL_TEMPORARY1_MTX);

    qglEndFragShaderMTX();

    // DEBUG - display status of shader creation
    // Con_Printf("Specular 1 Status: %s\n",gluErrorString(glGetError()));

    // Specular shader 2 (cube atten)
    qglBindFragShaderMTX(fragment_shaders+3);

    //DEBUG
    // Con_Printf("Shader %i Bind Status: %s\n",fragment_shaders+3,gluErrorString(glGetError()));

    qglBeginFragShaderMTX();
	
    qglFragProcRGBAOp1MTX(GL_OP_MOV_MTX, GL_TEMPORARY1_MTX, GL_COLOR0_MTX); //Matrox Work Around    
    // tex t0
    qglTexAddressHintsMTX( GL_TEXTURE0, GL_HINT_TEXTURE_2D_MTX|GL_HINT_TRILINEAR_MTX);
    qglTexAddressParamUIMTX( GL_TEXTURE0, GL_TEXTURE_FORMAT_MTX, GL_TEX_UNSIGNED_MTX);
    qglTexAddressOp1MTX( GL_OP_TEX_MTX, GL_TEXTURE0 );

    // tex t1
    qglTexAddressHintsMTX( GL_TEXTURE1, GL_HINT_TEXTURE_CUBE_MTX|GL_HINT_BILINEAR_MTX);
    qglTexAddressParamUIMTX( GL_TEXTURE1, GL_TEXTURE_FORMAT_MTX, GL_TEX_UNSIGNED_MTX);
    qglTexAddressOp1MTX( GL_OP_TEX_MTX, GL_TEXTURE1 );

    // tex t2
    qglTexAddressHintsMTX( GL_TEXTURE2, GL_HINT_TEXTURE_2D_MTX|GL_HINT_TRILINEAR_MTX);
    qglTexAddressParamUIMTX( GL_TEXTURE2, GL_TEXTURE_FORMAT_MTX, GL_TEX_UNSIGNED_MTX);
    qglTexAddressOp1MTX( GL_OP_TEX_MTX, GL_TEXTURE2 );

    // tex t3 (cube)
    qglTexAddressHintsMTX( GL_TEXTURE3, GL_HINT_TEXTURE_CUBE_MTX|GL_HINT_BILINEAR_MTX);
    qglTexAddressParamUIMTX( GL_TEXTURE3, GL_TEXTURE_FORMAT_MTX, GL_TEX_UNSIGNED_MTX);
    qglTexAddressOp1MTX( GL_OP_TEX_MTX, GL_TEXTURE3 );

    //dp3_sat r0, t0_bx2, t1_bx2
    qglFragProcOpParamMTX(GL_RESULT_MTX, GL_SATURATE_MTX, GL_TRUE);
    qglFragProcOpParamMTX(GL_ARGUMENT0_MTX, GL_CONVERT_MTX, GL_ARG_SIGNED_MTX);
    qglFragProcOpParamMTX(GL_ARGUMENT1_MTX, GL_CONVERT_MTX, GL_ARG_SIGNED_MTX);
    qglFragProcRGBAOp2MTX(GL_OP_DP3_MTX, GL_TEMPORARY0_MTX, GL_TEXTURE0, GL_TEXTURE1);


    //mul r1.rgb, t3, t0.a
    qglFragProcOpParamMTX(GL_ARGUMENT1_MTX, GL_REPLICATE_MTX, GL_ALPHA_MTX);
    qglFragProcRGBAOp2MTX(GL_OP_MUL_MTX, GL_TEMPORARY1_MTX, GL_TEXTURE3, GL_TEXTURE0);

    //+mad_x2_sat r1.a, r0.b, r0.b, c0
    qglFragProcOpParamMTX(GL_RESULT_MTX, GL_SATURATE_MTX, GL_TRUE);
    qglFragProcOpParamMTX(GL_RESULT_MTX, GL_SCALE_MTX, GL_SCALE_X2_MTX);
    qglFragProcOpParamMTX(GL_ARGUMENT0_MTX, GL_REPLICATE_MTX, GL_BLUE_MTX);
    qglFragProcOpParamMTX(GL_ARGUMENT1_MTX, GL_REPLICATE_MTX, GL_BLUE_MTX);
    qglFragProcOpParamMTX(GL_ARGUMENT2_MTX, GL_CONVERT_MTX, GL_ARG_NEGATE_MTX);
    scaler = qglGenFragProcInvariantFVMTX(scalervalues);
    qglFragProcRGBAOp1MTX(GL_OP_MOV_MTX, GL_IGNORED_MTX, GL_TEXTURE0); // [JPS] insert this
    qglFragProcAlphaOp3MTX(GL_OP_MAD_MTX, GL_TEMPORARY1_MTX, GL_TEMPORARY0_MTX, GL_TEMPORARY0_MTX, scaler);

    //mul r0, r1, v0
    qglFragProcRGBAOp2MTX(GL_OP_MUL_MTX, GL_TEMPORARY0_MTX, GL_TEMPORARY1_MTX, GL_COLOR0_MTX);

    //+mul r1.a, r1.a, r1.a
    qglFragProcRGBAOp1MTX(GL_OP_MOV_MTX, GL_IGNORED_MTX, GL_TEXTURE0); // [JPS] insert this
    qglFragProcAlphaOp2MTX(GL_OP_MUL_MTX, GL_TEMPORARY1_MTX, GL_TEMPORARY1_MTX, GL_TEMPORARY1_MTX);
    
    //mul r1.a, r1.a, r1.a
    qglFragProcRGBAOp1MTX(GL_OP_MOV_MTX, GL_IGNORED_MTX, GL_TEXTURE0); // [JPS] insert this
    qglFragProcAlphaOp2MTX(GL_OP_MUL_MTX, GL_TEMPORARY1_MTX, GL_TEMPORARY1_MTX, GL_TEMPORARY1_MTX);

    //mul r0, r0, r1.a
    qglFragProcOpParamMTX(GL_ARGUMENT1_MTX, GL_REPLICATE_MTX, GL_ALPHA_MTX);
    qglFragProcRGBAOp2MTX(GL_OP_MUL_MTX, GL_OUTPUT_COLOR_MTX, GL_TEMPORARY0_MTX, GL_TEMPORARY1_MTX);

    qglEndFragShaderMTX();
	
    // DEBUG - display status of shader creation
    // Con_Printf("Specular 2 Status: %s\n",gluErrorString(glGetError()));
	
    glDisable(GL_FRAGMENT_SHADER_MTX);
    // checkerror();
    // Con_Printf("----Vertex Shaders----\n");
    // Generate vertex shader
    glEnable(GL_VERTEX_SHADER_EXT);
		
		
    //DEBUG
    // Con_Printf("Enabled GL_VERTEX_SHADER_EXT...\n%s\n",gluErrorString(glGetError()));

    vertex_shader = qglGenVertexShadersEXT(1);
     	
    /*
    //DEBUG
    if (fragment_shaders == 0)
    {
    Con_Printf("Error: unable to allocate shaders: %s\n", gluErrorString(glGetError()));
    } else
    {
    Con_Printf("Allocated 1 vertex shader\n");
    };
    */

    qglBindVertexShaderEXT(vertex_shader);

    //DEBUG
    // Con_Printf("Shader %i Bind Status: %s\n",fragment_shaders,gluErrorString(glGetError()));

    mvp           = qglBindParameterEXT( GL_MVP_MATRIX_EXT );
    modelview     = qglBindParameterEXT( GL_MODELVIEW_MATRIX );
    vertex        = qglBindParameterEXT( GL_CURRENT_VERTEX_EXT );
    normal        = qglBindParameterEXT( GL_CURRENT_NORMAL );
    color         = qglBindParameterEXT( GL_CURRENT_COLOR );
    texturematrix = qglBindTextureUnitParameterEXT( GL_TEXTURE3_ARB, GL_TEXTURE_MATRIX );
    texcoord0     = qglBindTextureUnitParameterEXT( GL_TEXTURE0_ARB, GL_CURRENT_TEXTURE_COORDS );
    texcoord1     = qglBindTextureUnitParameterEXT( GL_TEXTURE1_ARB, GL_CURRENT_TEXTURE_COORDS );
    fogstart      = qglBindParameterEXT( GL_FOG_START );
    checkerror();
    fogend        = qglBindParameterEXT( GL_FOG_END );
    checkerror();


    qglBeginVertexShaderEXT();

    disttemp      = qglGenSymbolsEXT(GL_SCALAR_EXT, GL_LOCAL_EXT, GL_FULL_RANGE_EXT, 1);
    disttemp2     = qglGenSymbolsEXT(GL_SCALAR_EXT, GL_LOCAL_EXT, GL_FULL_RANGE_EXT, 1);
    zcomp         = qglGenSymbolsEXT(GL_VECTOR_EXT, GL_LOCAL_EXT, GL_FULL_RANGE_EXT, 1);


    // Generates a necessary input for the diffuse bumpmapping registers

    // Transform vertex to view-space
    qglShaderOp2EXT( GL_OP_MULTIPLY_MATRIX_EXT, GL_OUTPUT_VERTEX_EXT, mvp, vertex );
    
    // Transform vertex by texture matrix and copy to output
    qglShaderOp2EXT( GL_OP_MULTIPLY_MATRIX_EXT, GL_OUTPUT_TEXTURE_COORD3_EXT, texturematrix, vertex );

    // copy tex coords of unit 0 to unit 2
    qglShaderOp1EXT( GL_OP_MOV_EXT, GL_OUTPUT_TEXTURE_COORD0_EXT, texcoord0);
    qglShaderOp1EXT( GL_OP_MOV_EXT, GL_OUTPUT_TEXTURE_COORD1_EXT, texcoord1);
    qglShaderOp1EXT( GL_OP_MOV_EXT, GL_OUTPUT_TEXTURE_COORD2_EXT, texcoord0);
    qglShaderOp1EXT( GL_OP_MOV_EXT, GL_OUTPUT_COLOR0_EXT, color);

    // Transform vertex and take z for fog
    qglExtractComponentEXT( zcomp, modelview, 2);
    qglShaderOp2EXT( GL_OP_DOT4_EXT, disttemp, zcomp, vertex );

    // calculate fog values end - z and end - start
    qglShaderOp2EXT( GL_OP_SUB_EXT, disttemp, fogend, disttemp);
    qglShaderOp2EXT( GL_OP_SUB_EXT, disttemp2, fogend, fogstart);

    // divide end - z by end - start, that's it
    qglShaderOp1EXT( GL_OP_RECIP_EXT, disttemp2, disttemp2);
    qglShaderOp2EXT( GL_OP_MUL_EXT, GL_OUTPUT_FOG_EXT, disttemp, disttemp2);

    qglEndVertexShaderEXT();

    // DEBUG - display status of shader creation
    // Con_Printf("Vertex Status: %s\n",gluErrorString(glGetError()));
        
		
    glDisable(GL_VERTEX_SHADER_EXT);
    // checkerror();

}

/*
  Pixel shader for diffuse bump mapping does diffuse bumpmapping with norm
  cube, self shadowing & dist attent in 1 pass
*/
void GL_EnableDiffuseShaderParhelia(qboolean world, vec3_t lightOrig)
{
    float invrad = 1/currentshadowlight->radius;

    //tex 0 = normal map
    //tex 1 = normalization cube map (tangent space light vector)
    //tex 2 = color map
    //tex 3 = (attenuation or light filter, depends on light settings but the
    //        actual register combiner setup does not change only the bound 
    //        texture)

    glEnable(GL_VERTEX_SHADER_EXT);
    qglBindVertexShaderEXT( vertex_shader );
    checkerror();

    glEnable(GL_FRAGMENT_SHADER_MTX );

    GL_SelectTexture(GL_TEXTURE1_ARB);
    glEnable(GL_TEXTURE_CUBE_MAP_ARB);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, normcube_texture_object);

    GL_SelectTexture(GL_TEXTURE2_ARB);
    glEnable(GL_TEXTURE_2D);

    GL_SelectTexture(GL_TEXTURE3_ARB);
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    if (currentshadowlight->filtercube)
    {
	glEnable(GL_TEXTURE_CUBE_MAP_ARB);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, currentshadowlight->filtercube);
	GL_SetupCubeMapMatrix(world);

        qglBindFragShaderMTX( fragment_shaders + 1 );
        checkerror();
      
    }
    else
    {
	glEnable(GL_TEXTURE_3D);
	glBindTexture(GL_TEXTURE_3D, atten3d_texture_object);

	glTranslatef(0.5,0.5,0.5);
	glScalef(0.5,0.5,0.5);
	glScalef(invrad, invrad, invrad);
	glTranslatef(-lightOrig[0], -lightOrig[1], -lightOrig[2]);

        qglBindFragShaderMTX( fragment_shaders );
        checkerror();
    }

    GL_SelectTexture(GL_TEXTURE0_ARB);

}

void GL_DisableDiffuseShaderParhelia()
{
    //tex 0 = normal map
    //tex 1 = normalization cube map (tangent space light vector)
    //tex 2 = color map
    //tex 3 = (attenuation or light filter, depends on light settings)

    glDisable(GL_VERTEX_SHADER_EXT);
    glDisable(GL_FRAGMENT_SHADER_MTX );

    GL_SelectTexture(GL_TEXTURE1_ARB);
    glDisable(GL_TEXTURE_CUBE_MAP_ARB);

    GL_SelectTexture(GL_TEXTURE2_ARB);
    glDisable(GL_TEXTURE_2D);

    GL_SelectTexture(GL_TEXTURE3_ARB);
    if (currentshadowlight->filtercube)
    {
	glDisable(GL_TEXTURE_CUBE_MAP_ARB);
    }
    else
    {
	glDisable(GL_TEXTURE_3D);
    }
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    GL_SelectTexture(GL_TEXTURE0_ARB);
}

void GL_EnableSpecularShaderParhelia(qboolean world, vec3_t lightOrig,
				     qboolean alias)
{
    float invrad = 1/currentshadowlight->radius;
    glEnable(GL_VERTEX_SHADER_EXT);
    qglBindVertexShaderEXT( vertex_shader );
    checkerror();

    glEnable(GL_FRAGMENT_SHADER_MTX );

    //tex 0 = normal map
    //tex 1 = normalization cube map (tangent space half angle)
    //tex 2 = color map
    //tex 3 = (attenuation or light filter, depends on light settings)

    GL_SelectTexture(GL_TEXTURE1_ARB);
    glEnable(GL_TEXTURE_CUBE_MAP_ARB);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, normcube_texture_object);

    GL_SelectTexture(GL_TEXTURE2_ARB);
    glEnable(GL_TEXTURE_2D);

    GL_SelectTexture(GL_TEXTURE3_ARB);
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();

    if (currentshadowlight->filtercube)
    {
	glEnable(GL_TEXTURE_CUBE_MAP_ARB);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, currentshadowlight->filtercube);
	GL_SetupCubeMapMatrix(world);

        qglBindFragShaderMTX( fragment_shaders + 3 );
        checkerror();
    }
    else
    {
	glEnable(GL_TEXTURE_3D);
	glBindTexture(GL_TEXTURE_3D, atten3d_texture_object);

	glTranslatef(0.5,0.5,0.5);
	glScalef(0.5,0.5,0.5);
	glScalef(invrad, invrad, invrad);
	glTranslatef(-lightOrig[0], -lightOrig[1], -lightOrig[2]);

        qglBindFragShaderMTX( fragment_shaders + 2 );
        checkerror();
    }

    GL_SelectTexture(GL_TEXTURE0_ARB);

}

/*
  GL_DisableSpecularShaderParhelia() ??
  Same as GL_DisableDiffuseShaderParhelia()
*/

void GL_EnableAttentShaderParhelia(vec3_t lightOrig)
{
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

void GL_DisableAttentShaderParhelia()
{
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glDisable(GL_TEXTURE_3D);
    glEnable(GL_TEXTURE_2D);
}

void R_DrawWorldParheliaDiffuse(lightcmd_t *lightCmds)
{
    int command, num, i;
    int lightPos = 0;
    vec3_t lightOr;
    msurface_t *surf;
    float	*v;
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
        
	if (surf->visframe != r_framecount)
	{
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
	for (i=0; i<num; i++, v+= VERTEXSIZE)
	{
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

void R_DrawWorldParheliaSpecular(lightcmd_t *lightCmds)
{
    int command, num, i;
    int lightPos = 0;
    vec3_t tsH,H;
    float* lightP;
    msurface_t *surf;
    float	*v;
    vec3_t lightDir;
    texture_t	*t;//XYZ

    //support flickering lights
    //VectorCopy(currentshadowlight->origin,lightOr);

    while (1)
    {
	command = lightCmds[lightPos++].asInt;
	if (command == 0) break; //end of list

    // <AWE> HACK: Fix for 64-bit pointers in lightcmd buffer
	//surf = lightCmds[lightPos++].asVoid;
    surf = (msurface_t*) (hunk_base + lightCmds[lightPos++].asOffset);
        
	if (surf->visframe != r_framecount)
	{
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
	for (i=0; i<num; i++, v+= VERTEXSIZE)
	{
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
	    if (surf->flags & SURF_PLANEBACK)
	    {
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

void R_DrawBrushParheliaDiffuse(entity_t *e)
{
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

void R_DrawBrushParheliaSpecular(entity_t *e)
{
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

void R_DrawAliasFrameParheliaDiffuse (aliashdr_t *paliashdr,
				      aliasframeinstant_t *instant)
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

    if (sh_noshadowpopping.value && 0)
    {
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

void R_DrawAliasFrameParheliaSpecular (aliashdr_t *paliashdr,
				       aliasframeinstant_t *instant)
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

//    if (sh_noshadowpopping.value)
//	qglBindProgramNV( GL_VERTEX_PROGRAM_NV, specularaliasnopopping_program_object);

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
    if (sh_noshadowpopping.value && 0)
    {
	qglClientActiveTextureARB(GL_TEXTURE2_ARB);
	glTexCoordPointer(3, GL_FLOAT, 0, linstant->tslights);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    }

    glDrawElements(GL_TRIANGLES,linstant->numtris*3,GL_UNSIGNED_INT,&linstant->indecies[0]);

    if (sh_noshadowpopping.value && 0)
    {
	glStencilFunc(GL_LEQUAL, 1, 0xffffffff);
	//Con_Printf("%i backfacing tris\n",(paliashdr->numtris*3)-(linstant->numtris*3));
	glDrawElements(GL_TRIANGLES,(paliashdr->numtris*3)-(linstant->numtris*3),GL_UNSIGNED_INT,&linstant->indecies[linstant->numtris*3]);
	glStencilFunc(GL_EQUAL, 0, 0xffffffff);
    }

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);


    GL_SelectTexture(GL_TEXTURE0_ARB);
    qglClientActiveTextureARB(GL_TEXTURE0_ARB);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void R_DrawWorldBumpedParhelia()
{
    if (!currentshadowlight->visible)
	return;

    glDepthMask (0);
    glShadeModel (GL_SMOOTH);

    if (currentshadowlight->filtercube)
    {
	//draw attent into dest alpha
	GL_DrawAlpha();
	GL_EnableAttentShaderParhelia(currentshadowlight->origin);
	R_DrawWorldWV(currentshadowlight->lightCmds, false);
	GL_DisableAttentShaderParhelia();
	GL_ModulateAlphaDrawColor();
    }
    else
    {
	GL_AddColor();
    }
    glColor3fv(&currentshadowlight->color[0]);

    GL_EnableDiffuseShaderParhelia(true,currentshadowlight->origin);
    R_DrawWorldParheliaDiffuse(currentshadowlight->lightCmds);
    GL_DisableDiffuseShaderParhelia();

    GL_EnableSpecularShaderParhelia(true,currentshadowlight->origin,false);
    R_DrawWorldParheliaSpecular(currentshadowlight->lightCmds);
    GL_DisableDiffuseShaderParhelia();

    glColor3f (1,1,1);
    glDisable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask (1);
}

void R_DrawBrushBumpedParhelia(entity_t *e)
{
    if (currentshadowlight->filtercube)
    {
	//draw attent into dest alpha
	GL_DrawAlpha();
	GL_EnableAttentShaderParhelia(((brushlightinstant_t *)e->brushlightinstant)->lightpos);
	R_DrawBrushWV(e, false);
	GL_DisableAttentShaderParhelia();
	GL_ModulateAlphaDrawColor();
    }
    else
    {
	GL_AddColor();
    }
    glColor3fv(&currentshadowlight->color[0]);

    GL_EnableDiffuseShaderParhelia(false,((brushlightinstant_t *)e->brushlightinstant)->lightpos);
    R_DrawBrushParheliaDiffuse(e);
    GL_DisableDiffuseShaderParhelia();

    GL_EnableSpecularShaderParhelia(false,((brushlightinstant_t *)e->brushlightinstant)->lightpos,false);
    R_DrawBrushParheliaSpecular(e);
    GL_DisableDiffuseShaderParhelia();
}

void R_DrawAliasBumpedParhelia(aliashdr_t *paliashdr,
			       aliasframeinstant_t *instant)
{
    if (currentshadowlight->filtercube)
    {
	//draw attent into dest alpha
	GL_DrawAlpha();
	GL_EnableAttentShaderParhelia(instant->lightinstant->lightpos);
	R_DrawAliasFrameWV(paliashdr,instant, false);
	GL_DisableAttentShaderParhelia();
	GL_ModulateAlphaDrawColor();
    }
    else
    {
	GL_AddColor();
    }
    glColor3fv(&currentshadowlight->color[0]);

    GL_EnableDiffuseShaderParhelia(false,instant->lightinstant->lightpos);
    R_DrawAliasFrameParheliaDiffuse(paliashdr,instant);
    GL_DisableDiffuseShaderParhelia();

    GL_EnableSpecularShaderParhelia(false,instant->lightinstant->lightpos,true);
    R_DrawAliasFrameParheliaSpecular(paliashdr,instant);
    GL_DisableDiffuseShaderParhelia();
}

