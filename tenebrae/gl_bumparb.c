/*
Copyright (C) 2001-2002 Charles Hollemeersch
ARB_fragment_progam version (C) 2002 Jarno Paananen

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

Same as gl_bumpmap.c but Radeon 9700 / NV30 optimized 
These routines require 6 texture units, vertex shader and pixel shader

All lights require 1 pass:
1 diffuse + specular with optional light filter

*/

#include "quakedef.h"

#ifndef GL_ATI_pn_triangles
// PN_triangles_ATI
#define GL_PN_TRIANGLES_ATI                       0x87F0
#define GL_MAX_PN_TRIANGLES_TESSELATION_LEVEL_ATI 0x87F1
#define GL_PN_TRIANGLES_POINT_MODE_ATI            0x87F2
#define GL_PN_TRIANGLES_NORMAL_MODE_ATI           0x87F3
#define GL_PN_TRIANGLES_TESSELATION_LEVEL_ATI     0x87F4
#define GL_PN_TRIANGLES_POINT_MODE_LINEAR_ATI     0x87F5
#define GL_PN_TRIANGLES_POINT_MODE_CUBIC_ATI      0x87F6
#define GL_PN_TRIANGLES_NORMAL_MODE_LINEAR_ATI    0x87F7
#define GL_PN_TRIANGLES_NORMAL_MODE_QUADRATIC_ATI 0x87F8
#endif

#if !defined(GL_ATI_pn_triangles) || (defined(__APPLE__) || defined(MACOSX))
typedef void (APIENTRY *PFNGLPNTRIANGLESIATIPROC)(GLenum pname, GLint param);
typedef void (APIENTRY *PFNGLPNTRIANGLESFATIPROC)(GLenum pname, GLfloat param);
#endif

// actually in gl_bumpradeon (duh...)
extern PFNGLPNTRIANGLESIATIPROC qglPNTrianglesiATI;
extern PFNGLPNTRIANGLESFATIPROC qglPNTrianglesfATI;


// ARB_vertex_program

typedef void (APIENTRY * glVertexAttrib1sARBPROC) (GLuint index, GLshort x);
typedef void (APIENTRY * glVertexAttrib1fARBPROC) (GLuint index, GLfloat x);
typedef void (APIENTRY * glVertexAttrib1dARBPROC) (GLuint index, GLdouble x);
typedef void (APIENTRY * glVertexAttrib2sARBPROC) (GLuint index, GLshort x, GLshort y);
typedef void (APIENTRY * glVertexAttrib2fARBPROC) (GLuint index, GLfloat x, GLfloat y);
typedef void (APIENTRY * glVertexAttrib2dARBPROC) (GLuint index, GLdouble x, GLdouble y);
typedef void (APIENTRY * glVertexAttrib3sARBPROC) (GLuint index, GLshort x, GLshort y, GLshort z);
typedef void (APIENTRY * glVertexAttrib3fARBPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRY * glVertexAttrib3dARBPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRY * glVertexAttrib4sARBPROC) (GLuint index, GLshort x, GLshort y, GLshort z, GLshort w);
typedef void (APIENTRY * glVertexAttrib4fARBPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (APIENTRY * glVertexAttrib4dARBPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRY * glVertexAttrib4NubARBPROC) (GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
typedef void (APIENTRY * glVertexAttrib1svARBPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRY * glVertexAttrib1fvARBPROC) (GLuint index, const GLfloat *v);
typedef void (APIENTRY * glVertexAttrib1dvARBPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRY * glVertexAttrib2svARBPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRY * glVertexAttrib2fvARBPROC) (GLuint index, const GLfloat *v);
typedef void (APIENTRY * glVertexAttrib2dvARBPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRY * glVertexAttrib3svARBPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRY * glVertexAttrib3fvARBPROC) (GLuint index, const GLfloat *v);
typedef void (APIENTRY * glVertexAttrib3dvARBPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRY * glVertexAttrib4bvARBPROC) (GLuint index, const GLbyte *v);
typedef void (APIENTRY * glVertexAttrib4svARBPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRY * glVertexAttrib4ivARBPROC) (GLuint index, const GLint *v);
typedef void (APIENTRY * glVertexAttrib4ubvARBPROC) (GLuint index, const GLubyte *v);
typedef void (APIENTRY * glVertexAttrib4usvARBPROC) (GLuint index, const GLushort *v);
typedef void (APIENTRY * glVertexAttrib4uivARBPROC) (GLuint index, const GLuint *v);
typedef void (APIENTRY * glVertexAttrib4fvARBPROC) (GLuint index, const GLfloat *v);
typedef void (APIENTRY * glVertexAttrib4dvARBPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRY * glVertexAttrib4NbvARBPROC) (GLuint index, const GLbyte *v);
typedef void (APIENTRY * glVertexAttrib4NsvARBPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRY * glVertexAttrib4NivARBPROC) (GLuint index, const GLint *v);
typedef void (APIENTRY * glVertexAttrib4NubvARBPROC) (GLuint index, const GLubyte *v);
typedef void (APIENTRY * glVertexAttrib4NusvARBPROC) (GLuint index, const GLushort *v);
typedef void (APIENTRY * glVertexAttrib4NuivARBPROC) (GLuint index, const GLuint *v);
typedef void (APIENTRY * glVertexAttribPointerARBPROC) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRY * glEnableVertexAttribArrayARBPROC) (GLuint index);
typedef void (APIENTRY * glDisableVertexAttribArrayARBPROC) (GLuint index);
typedef void (APIENTRY * glProgramStringARBPROC) (GLenum target, GLenum format, GLsizei len, const GLvoid *string); 
typedef void (APIENTRY * glBindProgramARBPROC) (GLenum target, GLuint program);
typedef void (APIENTRY * glDeleteProgramsARBPROC) (GLsizei n, const GLuint *programs);
typedef void (APIENTRY * glGenProgramsARBPROC) (GLsizei n, GLuint *programs);
typedef void (APIENTRY * glProgramEnvParameter4dARBPROC) (GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRY * glProgramEnvParameter4dvARBPROC) (GLenum target, GLuint index, const GLdouble *params);
typedef void (APIENTRY * glProgramEnvParameter4fARBPROC) (GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (APIENTRY * glProgramEnvParameter4fvARBPROC) (GLenum target, GLuint index, const GLfloat *params);
typedef void (APIENTRY * glProgramLocalParameter4dARBPROC) (GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRY * glProgramLocalParameter4dvARBPROC) (GLenum target, GLuint index, const GLdouble *params);
typedef void (APIENTRY * glProgramLocalParameter4fARBPROC) (GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (APIENTRY * glProgramLocalParameter4fvARBPROC) (GLenum target, GLuint index, const GLfloat *params);
typedef void (APIENTRY * glGetProgramEnvParameterdvARBPROC) (GLenum target, GLuint index, GLdouble *params);
typedef void (APIENTRY * glGetProgramEnvParameterfvARBPROC) (GLenum target, GLuint index, GLfloat *params);
typedef void (APIENTRY * glGetProgramLocalParameterdvARBPROC) (GLenum target, GLuint index, GLdouble *params);
typedef void (APIENTRY * glGetProgramLocalParameterfvARBPROC) (GLenum target, GLuint index, GLfloat *params);
typedef void (APIENTRY * glGetProgramivARBPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRY * glGetProgramStringARBPROC) (GLenum target, GLenum pname, GLvoid *string);
typedef void (APIENTRY * glGetVertexAttribdvARBPROC) (GLuint index, GLenum pname, GLdouble *params);
typedef void (APIENTRY * glGetVertexAttribfvARBPROC) (GLuint index, GLenum pname, GLfloat *params);
typedef void (APIENTRY * glGetVertexAttribivARBPROC) (GLuint index, GLenum pname, GLint *params);
typedef void (APIENTRY * glGetVertexAttribPointervARBPROC) (GLuint index, GLenum pname, GLvoid **pointer);
typedef GLboolean (APIENTRY * glIsProgramARBPROC) (GLuint program);

static glVertexAttrib1sARBPROC qglVertexAttrib1sARB = NULL;
static glVertexAttrib1fARBPROC qglVertexAttrib1fARB = NULL;
static glVertexAttrib1dARBPROC qglVertexAttrib1dARB = NULL;
static glVertexAttrib2sARBPROC qglVertexAttrib2sARB = NULL;
static glVertexAttrib2fARBPROC qglVertexAttrib2fARB = NULL;
static glVertexAttrib2dARBPROC qglVertexAttrib2dARB = NULL;
static glVertexAttrib3sARBPROC qglVertexAttrib3sARB = NULL;
static glVertexAttrib3fARBPROC qglVertexAttrib3fARB = NULL;
static glVertexAttrib3dARBPROC qglVertexAttrib3dARB = NULL;
static glVertexAttrib4sARBPROC qglVertexAttrib4sARB = NULL;
static glVertexAttrib4fARBPROC qglVertexAttrib4fARB = NULL;
static glVertexAttrib4dARBPROC qglVertexAttrib4dARB = NULL;
static glVertexAttrib4NubARBPROC qglVertexAttrib4NubARB = NULL;
static glVertexAttrib1svARBPROC qglVertexAttrib1svARB = NULL;
static glVertexAttrib1fvARBPROC qglVertexAttrib1fvARB = NULL;
static glVertexAttrib1dvARBPROC qglVertexAttrib1dvARB = NULL;
static glVertexAttrib2svARBPROC qglVertexAttrib2svARB = NULL;
static glVertexAttrib2fvARBPROC qglVertexAttrib2fvARB = NULL;
static glVertexAttrib2dvARBPROC qglVertexAttrib2dvARB = NULL;
static glVertexAttrib3svARBPROC qglVertexAttrib3svARB = NULL;
static glVertexAttrib3fvARBPROC qglVertexAttrib3fvARB = NULL;
static glVertexAttrib3dvARBPROC qglVertexAttrib3dvARB = NULL;
static glVertexAttrib4bvARBPROC qglVertexAttrib4bvARB = NULL;
static glVertexAttrib4svARBPROC qglVertexAttrib4svARB = NULL;
static glVertexAttrib4ivARBPROC qglVertexAttrib4ivARB = NULL;
static glVertexAttrib4ubvARBPROC qglVertexAttrib4ubvARB = NULL;
static glVertexAttrib4usvARBPROC qglVertexAttrib4usvARB = NULL;
static glVertexAttrib4uivARBPROC qglVertexAttrib4uivARB = NULL;
static glVertexAttrib4fvARBPROC qglVertexAttrib4fvARB = NULL;
static glVertexAttrib4dvARBPROC qglVertexAttrib4dvARB = NULL;
static glVertexAttrib4NbvARBPROC qglVertexAttrib4NbvARB = NULL;
static glVertexAttrib4NsvARBPROC qglVertexAttrib4NsvARB = NULL;
static glVertexAttrib4NivARBPROC qglVertexAttrib4NivARB = NULL;
static glVertexAttrib4NubvARBPROC qglVertexAttrib4NubvARB = NULL;
static glVertexAttrib4NusvARBPROC qglVertexAttrib4NusvARB = NULL;
static glVertexAttrib4NuivARBPROC qglVertexAttrib4NuivARB = NULL;
static glVertexAttribPointerARBPROC qglVertexAttribPointerARB = NULL;
static glEnableVertexAttribArrayARBPROC qglEnableVertexAttribArrayARB = NULL;
static glDisableVertexAttribArrayARBPROC qglDisableVertexAttribArrayARB = NULL;
static glProgramStringARBPROC qglProgramStringARB = NULL;
static glBindProgramARBPROC qglBindProgramARB = NULL;
static glDeleteProgramsARBPROC qglDeleteProgramsARB = NULL;
static glGenProgramsARBPROC qglGenProgramsARB = NULL;
static glProgramEnvParameter4dARBPROC qglProgramEnvParameter4dARB = NULL;
static glProgramEnvParameter4dvARBPROC qglProgramEnvParameter4dvARB = NULL;
static glProgramEnvParameter4fARBPROC qglProgramEnvParameter4fARB = NULL;
static glProgramEnvParameter4fvARBPROC qglProgramEnvParameter4fvARB = NULL;
static glProgramLocalParameter4dARBPROC qglProgramLocalParameter4dARB = NULL;
static glProgramLocalParameter4dvARBPROC qglProgramLocalParameter4dvARB = NULL;
static glProgramLocalParameter4fARBPROC qglProgramLocalParameter4fARB = NULL;
static glProgramLocalParameter4fvARBPROC qglProgramLocalParameter4fvARB = NULL;
static glGetProgramEnvParameterdvARBPROC qglGetProgramEnvParameterdvARB = NULL;
static glGetProgramEnvParameterfvARBPROC qglGetProgramEnvParameterfvARB = NULL;
static glGetProgramLocalParameterdvARBPROC qglGetProgramLocalParameterdvARB = NULL;
static glGetProgramLocalParameterfvARBPROC qglGetProgramLocalParameterfvARB = NULL;
static glGetProgramivARBPROC qglGetProgramivARB = NULL;
static glGetProgramStringARBPROC qglGetProgramStringARB = NULL;
static glGetVertexAttribdvARBPROC qglGetVertexAttribdvARB = NULL;
static glGetVertexAttribfvARBPROC qglGetVertexAttribfvARB = NULL;
static glGetVertexAttribivARBPROC qglGetVertexAttribivARB = NULL;
static glGetVertexAttribPointervARBPROC qglGetVertexAttribPointervARB = NULL;
static glIsProgramARBPROC qglIsProgramARB = NULL;

#define GL_VERTEX_PROGRAM_ARB                                   0x8620
#define GL_VERTEX_PROGRAM_POINT_SIZE_ARB                        0x8642
#define GL_VERTEX_PROGRAM_TWO_SIDE_ARB                          0x8643
#define GL_COLOR_SUM_ARB                                        0x8458
#define GL_PROGRAM_FORMAT_ASCII_ARB                             0x8875
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED_ARB                      0x8622
#define GL_VERTEX_ATTRIB_ARRAY_SIZE_ARB                         0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE_ARB                       0x8624
#define GL_VERTEX_ATTRIB_ARRAY_TYPE_ARB                         0x8625
#define GL_VERTEX_ATTRIB_ARRAY_NORMALIZED_ARB                   0x886A
#define GL_CURRENT_VERTEX_ATTRIB_ARB                            0x8626
#define GL_VERTEX_ATTRIB_ARRAY_POINTER_ARB                      0x8645
#define GL_PROGRAM_LENGTH_ARB                                   0x8627
#define GL_PROGRAM_FORMAT_ARB                                   0x8876
#define GL_PROGRAM_BINDING_ARB                                  0x8677
#define GL_PROGRAM_INSTRUCTIONS_ARB                             0x88A0
#define GL_MAX_PROGRAM_INSTRUCTIONS_ARB                         0x88A1
#define GL_PROGRAM_NATIVE_INSTRUCTIONS_ARB                      0x88A2
#define GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB                  0x88A3
#define GL_PROGRAM_TEMPORARIES_ARB                              0x88A4
#define GL_MAX_PROGRAM_TEMPORARIES_ARB                          0x88A5
#define GL_PROGRAM_NATIVE_TEMPORARIES_ARB                       0x88A6
#define GL_MAX_PROGRAM_NATIVE_TEMPORARIES_ARB                   0x88A7
#define GL_PROGRAM_PARAMETERS_ARB                               0x88A8
#define GL_MAX_PROGRAM_PARAMETERS_ARB                           0x88A9
#define GL_PROGRAM_NATIVE_PARAMETERS_ARB                        0x88AA
#define GL_MAX_PROGRAM_NATIVE_PARAMETERS_ARB                    0x88AB
#define GL_PROGRAM_ATTRIBS_ARB                                  0x88AC
#define GL_MAX_PROGRAM_ATTRIBS_ARB                              0x88AD
#define GL_PROGRAM_NATIVE_ATTRIBS_ARB                           0x88AE
#define GL_MAX_PROGRAM_NATIVE_ATTRIBS_ARB                       0x88AF
#define GL_PROGRAM_ADDRESS_REGISTERS_ARB                        0x88B0
#define GL_MAX_PROGRAM_ADDRESS_REGISTERS_ARB                    0x88B1
#define GL_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB                 0x88B2
#define GL_MAX_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB             0x88B3
#define GL_MAX_PROGRAM_LOCAL_PARAMETERS_ARB                     0x88B4
#define GL_MAX_PROGRAM_ENV_PARAMETERS_ARB                       0x88B5
#define GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB                      0x88B6
#define GL_PROGRAM_STRING_ARB                                   0x8628
#define GL_PROGRAM_ERROR_POSITION_ARB                           0x864B
#define GL_CURRENT_MATRIX_ARB                                   0x8641
#define GL_TRANSPOSE_CURRENT_MATRIX_ARB                         0x88B7
#define GL_CURRENT_MATRIX_STACK_DEPTH_ARB                       0x8640
#define GL_MAX_VERTEX_ATTRIBS_ARB                               0x8869
#define GL_MAX_PROGRAM_MATRICES_ARB                             0x862F
#define GL_MAX_PROGRAM_MATRIX_STACK_DEPTH_ARB                   0x862E
#define GL_PROGRAM_ERROR_STRING_ARB                             0x8874
#define GL_MATRIX0_ARB                                          0x88C0
#define GL_MATRIX1_ARB                                          0x88C1
#define GL_MATRIX2_ARB                                          0x88C2
#define GL_MATRIX3_ARB                                          0x88C3
#define GL_MATRIX4_ARB                                          0x88C4
#define GL_MATRIX5_ARB                                          0x88C5
#define GL_MATRIX6_ARB                                          0x88C6
#define GL_MATRIX7_ARB                                          0x88C7
#define GL_MATRIX8_ARB                                          0x88C8
#define GL_MATRIX9_ARB                                          0x88C9
#define GL_MATRIX10_ARB                                         0x88CA
#define GL_MATRIX11_ARB                                         0x88CB
#define GL_MATRIX12_ARB                                         0x88CC
#define GL_MATRIX13_ARB                                         0x88CD
#define GL_MATRIX14_ARB                                         0x88CE
#define GL_MATRIX15_ARB                                         0x88CF
#define GL_MATRIX16_ARB                                         0x88D0
#define GL_MATRIX17_ARB                                         0x88D1
#define GL_MATRIX18_ARB                                         0x88D2
#define GL_MATRIX19_ARB                                         0x88D3
#define GL_MATRIX20_ARB                                         0x88D4
#define GL_MATRIX21_ARB                                         0x88D5
#define GL_MATRIX22_ARB                                         0x88D6
#define GL_MATRIX23_ARB                                         0x88D7
#define GL_MATRIX24_ARB                                         0x88D8
#define GL_MATRIX25_ARB                                         0x88D9
#define GL_MATRIX26_ARB                                         0x88DA
#define GL_MATRIX27_ARB                                         0x88DB
#define GL_MATRIX28_ARB                                         0x88DC
#define GL_MATRIX29_ARB                                         0x88DD
#define GL_MATRIX30_ARB                                         0x88DE
#define GL_MATRIX31_ARB                                         0x88DF

// ARB_fragment_program
#define GL_FRAGMENT_PROGRAM_ARB                                 0x8804

static char vertexprogram[] =
"!!ARBvp1.0\n"
"OPTION ARB_position_invariant;\n"
"ATTRIB iPos         = vertex.position;\n"
"ATTRIB iNormal      = vertex.normal;\n"
"ATTRIB iColor       = vertex.color;\n"
"ATTRIB iTex0        = vertex.texcoord[0];\n"
"ATTRIB iTex1        = vertex.texcoord[1];\n"
"ATTRIB iTex2        = vertex.texcoord[2];\n"
"PARAM  texMatrix[4] = { state.matrix.texture[2] };\n"
"TEMP   disttemp;\n"
"OUTPUT oColor       = result.color;\n"
"OUTPUT oTex0        = result.texcoord[0];\n"
"OUTPUT oTex1        = result.texcoord[1];\n"
"OUTPUT oTex2        = result.texcoord[2];\n"
"OUTPUT oTex3        = result.texcoord[3];\n"
"DP4   oTex3.x, texMatrix[0], iPos;\n"
"DP4   oTex3.y, texMatrix[1], iPos;\n"
"DP4   oTex3.z, texMatrix[2], iPos;\n"
"DP4   oTex3.w, texMatrix[3], iPos;\n"
"MOV   oTex0, iTex0;\n"
"MOV   oTex1, iTex1;\n"
"MOV   oTex2, iTex2;\n"
"MOV   oColor, iColor;\n"
"END";

static char vertexprogram2[] =
"!!ARBvp1.0\n"
"OPTION ARB_position_invariant;\n"
"ATTRIB iPos         = vertex.position;\n"
"ATTRIB iNormal      = vertex.normal;\n"
"ATTRIB iColor       = vertex.color;\n"
"ATTRIB iTex0        = vertex.texcoord[0];\n"
"ATTRIB iTex1        = vertex.texcoord[1];\n"
"ATTRIB iTex2        = vertex.texcoord[2];\n"
"PARAM  texMatrix[4] = { state.matrix.texture[2] };\n"
"PARAM  texMatrix2[4]= { state.matrix.texture[3] };\n"
"OUTPUT oColor       = result.color;\n"
"OUTPUT oTex0        = result.texcoord[0];\n"
"OUTPUT oTex1        = result.texcoord[1];\n"
"OUTPUT oTex2        = result.texcoord[2];\n"
"OUTPUT oTex3        = result.texcoord[3];\n"
"OUTPUT oTex4        = result.texcoord[4];\n"
"OUTPUT oFog         = result.fogcoord;\n"
"DP4   oTex3.x, texMatrix[0], iPos;\n"
"DP4   oTex3.y, texMatrix[1], iPos;\n"
"DP4   oTex3.z, texMatrix[2], iPos;\n"
"DP4   oTex3.w, texMatrix[3], iPos;\n"
"DP4   oTex4.x, texMatrix2[0], iPos;\n"
"DP4   oTex4.y, texMatrix2[1], iPos;\n"
"DP4   oTex4.z, texMatrix2[2], iPos;\n"
"DP4   oTex4.w, texMatrix2[3], iPos;\n"
"MOV   oTex0, iTex0;\n"
"MOV   oTex1, iTex1;\n"
"MOV   oTex2, iTex2;\n"
"MOV   oColor, iColor;\n"
"END";

static char fragmentprogram[] =
"!!ARBfp1.0\n"
"OPTION ARB_precision_hint_fastest;\n"
"ATTRIB tex0 = fragment.texcoord[0];\n"
"ATTRIB tex1 = fragment.texcoord[1];\n"
"ATTRIB tex2 = fragment.texcoord[2];\n"
"ATTRIB tex3 = fragment.texcoord[3];\n"
"ATTRIB col = fragment.color.primary;\n"
"PARAM scaler = { 16, 8, 2, -1 };\n"
"OUTPUT outColor = result.color;\n"
"TEMP normalmap, lightvec, halfvec, colormap, atten;\n"
"TEMP diffdot, specdot, selfshadow;\n"
"TEX normalmap, tex0, texture[0], 2D;\n"
"MAD normalmap.rgb, normalmap, scaler.b, scaler.a;\n"
"DP3 lightvec.x, tex1, tex1;\n"
"RSQ lightvec.x, lightvec.x;\n"
"MUL lightvec, tex1, lightvec.x;\n"
"TEX colormap, tex0, texture[1], 2D;\n"
"DP3 halfvec.x, tex2, tex2;\n"
"RSQ halfvec.x, halfvec.x;\n"
"MUL halfvec, tex2, halfvec.x;\n"
"TEX atten, tex3, texture[2], 3D;\n"
"DP3_SAT diffdot, normalmap, lightvec;\n"
"MUL_SAT selfshadow.r, lightvec.z, scaler.g;\n"
"DP3_SAT specdot.a, normalmap, halfvec;\n"
"MUL diffdot, diffdot, colormap;\n"
"POW specdot.a, specdot.a, scaler.r;\n"
"MUL specdot.a, specdot.a, normalmap.a;\n"
"MAD diffdot, diffdot, selfshadow.r, specdot.a;\n"
"MUL atten, col, atten;\n"
"MUL_SAT outColor, diffdot, atten;\n"
"END";

static char fragmentprogram2[] =
"!!ARBfp1.0\n"
"OPTION ARB_precision_hint_fastest;\n"
"ATTRIB tex0 = fragment.texcoord[0];\n"
"ATTRIB tex1 = fragment.texcoord[1];\n"
"ATTRIB tex2 = fragment.texcoord[2];\n"
"ATTRIB tex3 = fragment.texcoord[3];\n"
"ATTRIB tex4 = fragment.texcoord[4];\n"
"ATTRIB col = fragment.color.primary;\n"
"PARAM scaler = { 16, 8, 2, -1 };\n"
"OUTPUT outColor = result.color;\n"
"TEMP normalmap, lightvec, halfvec, colormap, atten;\n"
"TEMP diffdot, specdot, selfshadow, filter;\n"
"TEX normalmap, tex0, texture[0], 2D;\n"
"MAD normalmap.rgb, normalmap, scaler.b, scaler.a;\n"
"DP3 lightvec.x, tex1, tex1;\n"
"RSQ lightvec.x, lightvec.x;\n"
"MUL lightvec, tex1, lightvec.x;\n"
"TEX colormap, tex0, texture[1], 2D;\n"
"DP3 halfvec.x, tex2, tex2;\n"
"RSQ halfvec.x, halfvec.x;\n"
"MUL halfvec, tex2, halfvec.x;\n"
"TEX atten, tex3, texture[2], 3D;\n"
"DP3_SAT diffdot, normalmap, lightvec;\n"
"MUL_SAT selfshadow.r, lightvec.z, scaler.g;\n"
"DP3_SAT specdot.a, normalmap, halfvec;\n"
"MUL diffdot, diffdot, colormap;\n"
"POW specdot.a, specdot.a, scaler.r;\n"
"TEX filter, tex4, texture[3], CUBE;\n"
"MUL specdot.a, specdot.a, normalmap.a;\n"
"MUL atten, atten, filter;\n"
"MAD diffdot, diffdot, selfshadow.r, specdot.a;\n"
"MUL atten, col, atten;\n"
"MUL_SAT outColor, diffdot, atten;\n"
"END";

static GLuint fragment_programs[2];
static GLuint vertex_programs[2];


//#define ARBDEBUG

#if defined(ARBDEBUG) && defined(_WIN32)
static void checkerror()
{
    GLuint error = glGetError();
    if ( error != GL_NO_ERROR )
    {
        int line;
        const char* err;
        
        glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &line);
        err = glGetString(GL_PROGRAM_ERROR_STRING_ARB);
        _asm { int 3 };
    }
}
#else

#define checkerror() do { } while(0)

#endif

#if defined(__APPLE__) || defined (MACOSX)

extern void *	Sys_GetProcAddress (const char *theName, qboolean theSafeMode);

qboolean GL_LookupSymbolsARB()
{
	qglVertexAttrib1sARB = Sys_GetProcAddress ("glVertexAttrib1sARB", false);
	qglVertexAttrib1fARB = Sys_GetProcAddress ("glVertexAttrib1fARB", false);
	qglVertexAttrib1dARB = Sys_GetProcAddress ("glVertexAttrib1dARB", false);
	qglVertexAttrib2sARB = Sys_GetProcAddress ("glVertexAttrib2sARB", false);
	qglVertexAttrib2fARB = Sys_GetProcAddress ("glVertexAttrib2fARB", false);
	qglVertexAttrib2dARB = Sys_GetProcAddress ("glVertexAttrib2dARB", false);
	qglVertexAttrib3sARB = Sys_GetProcAddress ("glVertexAttrib3sARB", false);
	qglVertexAttrib3fARB = Sys_GetProcAddress ("glVertexAttrib3fARB", false);
	qglVertexAttrib3dARB = Sys_GetProcAddress ("glVertexAttrib3dARB", false);
	qglVertexAttrib4sARB = Sys_GetProcAddress ("glVertexAttrib4sARB", false);
	qglVertexAttrib4fARB = Sys_GetProcAddress ("glVertexAttrib4fARB", false);
	qglVertexAttrib4dARB = Sys_GetProcAddress ("glVertexAttrib4dARB", false);
	qglVertexAttrib4NubARB = Sys_GetProcAddress ("glVertexAttrib4NubARB", false);
	qglVertexAttrib1svARB = Sys_GetProcAddress ("glVertexAttrib1svARB", false);
	qglVertexAttrib1fvARB = Sys_GetProcAddress ("glVertexAttrib1fvARB", false);
	qglVertexAttrib1dvARB = Sys_GetProcAddress ("glVertexAttrib1dvARB", false);
	qglVertexAttrib2svARB = Sys_GetProcAddress ("glVertexAttrib2svARB", false);
	qglVertexAttrib2fvARB = Sys_GetProcAddress ("glVertexAttrib2fvARB", false);
	qglVertexAttrib2dvARB = Sys_GetProcAddress ("glVertexAttrib2dvARB", false);
	qglVertexAttrib3svARB = Sys_GetProcAddress ("glVertexAttrib3svARB", false);
	qglVertexAttrib3fvARB = Sys_GetProcAddress ("glVertexAttrib3fvARB", false);
	qglVertexAttrib3dvARB = Sys_GetProcAddress ("glVertexAttrib3dvARB", false);
	qglVertexAttrib4bvARB = Sys_GetProcAddress ("glVertexAttrib4bvARB", false);
	qglVertexAttrib4svARB = Sys_GetProcAddress ("glVertexAttrib4svARB", false);
	qglVertexAttrib4ivARB = Sys_GetProcAddress ("glVertexAttrib4ivARB", false);
	qglVertexAttrib4ubvARB = Sys_GetProcAddress ("glVertexAttrib4ubvARB", false);
	qglVertexAttrib4usvARB = Sys_GetProcAddress ("glVertexAttrib4usvARB", false);
	qglVertexAttrib4uivARB = Sys_GetProcAddress ("glVertexAttrib4uivARB", false);
	qglVertexAttrib4fvARB = Sys_GetProcAddress ("glVertexAttrib4fvARB", false);
	qglVertexAttrib4dvARB = Sys_GetProcAddress ("glVertexAttrib4dvARB", false);
	qglVertexAttrib4NbvARB = Sys_GetProcAddress ("glVertexAttrib4NbvARB", false);
	qglVertexAttrib4NsvARB = Sys_GetProcAddress ("glVertexAttrib4NsvARB", false);
	qglVertexAttrib4NivARB = Sys_GetProcAddress ("glVertexAttrib4NivARB", false);
	qglVertexAttrib4NubvARB = Sys_GetProcAddress ("glVertexAttrib4NubvARB", false);
	qglVertexAttrib4NusvARB = Sys_GetProcAddress ("glVertexAttrib4NusvARB", false);
	qglVertexAttrib4NuivARB = Sys_GetProcAddress ("glVertexAttrib4NuivARB", false);
	qglVertexAttribPointerARB = Sys_GetProcAddress ("glVertexAttribPointerARB", false);
	qglEnableVertexAttribArrayARB = Sys_GetProcAddress ("glEnableVertexAttribArrayARB", false);
	qglDisableVertexAttribArrayARB = Sys_GetProcAddress ("glDisableVertexAttribArrayARB", false);
	qglProgramStringARB = Sys_GetProcAddress ("glProgramStringARB", false);
	qglBindProgramARB = Sys_GetProcAddress ("glBindProgramARB", false);
	qglDeleteProgramsARB = Sys_GetProcAddress ("glDeleteProgramsARB", false);
	qglGenProgramsARB = Sys_GetProcAddress ("glGenProgramsARB", false);
	qglProgramEnvParameter4dARB = Sys_GetProcAddress ("glProgramEnvParameter4dARB", false);
	qglProgramEnvParameter4dvARB = Sys_GetProcAddress ("glProgramEnvParameter4dvARB", false);
	qglProgramEnvParameter4fARB = Sys_GetProcAddress ("glProgramEnvParameter4fARB", false);
	qglProgramEnvParameter4fvARB = Sys_GetProcAddress ("glProgramEnvParameter4fvARB", false);
	qglProgramLocalParameter4dARB = Sys_GetProcAddress ("glProgramLocalParameter4dARB", false);
	qglProgramLocalParameter4dvARB = Sys_GetProcAddress ("glProgramLocalParameter4dvARB", false);
	qglProgramLocalParameter4fARB = Sys_GetProcAddress ("glProgramLocalParameter4fARB", false);
	qglProgramLocalParameter4fvARB = Sys_GetProcAddress ("glProgramLocalParameter4fvARB", false);
	qglGetProgramEnvParameterdvARB = Sys_GetProcAddress ("glGetProgramEnvParameterdvARB", false);
	qglGetProgramEnvParameterfvARB = Sys_GetProcAddress ("glGetProgramEnvParameterfvARB", false);
	qglGetProgramLocalParameterdvARB = Sys_GetProcAddress ("glGetProgramLocalParameterdvARB", false);
	qglGetProgramLocalParameterfvARB = Sys_GetProcAddress ("glGetProgramLocalParameterfvARB", false);
	qglGetProgramivARB = Sys_GetProcAddress ("glGetProgramivARB", false);
	qglGetProgramStringARB = Sys_GetProcAddress ("glGetProgramStringARB", false);
	qglGetVertexAttribdvARB = Sys_GetProcAddress ("glGetVertexAttribdvARB", false);
	qglGetVertexAttribfvARB = Sys_GetProcAddress ("glGetVertexAttribfvARB", false);
	qglGetVertexAttribivARB = Sys_GetProcAddress ("glGetVertexAttribivARB", false);
	qglGetVertexAttribPointervARB = Sys_GetProcAddress ("glGetVertexAttribPointervARB", false);
	qglIsProgramARB = Sys_GetProcAddress ("glIsProgramARB", false);

	if (qglVertexAttrib1sARB != NULL &&
		qglVertexAttrib1fARB != NULL &&
		qglVertexAttrib1dARB != NULL &&
		qglVertexAttrib2sARB != NULL &&
		qglVertexAttrib2fARB != NULL &&
		qglVertexAttrib2dARB != NULL &&
		qglVertexAttrib3sARB != NULL &&
		qglVertexAttrib3fARB != NULL &&
		qglVertexAttrib3dARB != NULL &&
		qglVertexAttrib4sARB != NULL &&
		qglVertexAttrib4fARB != NULL &&
		qglVertexAttrib4dARB != NULL &&
		qglVertexAttrib4NubARB != NULL &&
		qglVertexAttrib1svARB != NULL &&
		qglVertexAttrib1fvARB != NULL &&
		qglVertexAttrib1dvARB != NULL &&
		qglVertexAttrib2svARB != NULL &&
		qglVertexAttrib2fvARB != NULL &&
		qglVertexAttrib2dvARB != NULL &&
		qglVertexAttrib3svARB != NULL &&
		qglVertexAttrib3fvARB != NULL &&
		qglVertexAttrib3dvARB != NULL &&
		qglVertexAttrib4bvARB != NULL &&
		qglVertexAttrib4svARB != NULL &&
		qglVertexAttrib4ivARB != NULL &&
		qglVertexAttrib4ubvARB != NULL &&
		qglVertexAttrib4usvARB != NULL &&
		qglVertexAttrib4uivARB != NULL &&
		qglVertexAttrib4fvARB != NULL &&
		qglVertexAttrib4dvARB != NULL &&
		qglVertexAttrib4NbvARB != NULL &&
		qglVertexAttrib4NsvARB != NULL &&
		qglVertexAttrib4NivARB != NULL &&
		qglVertexAttrib4NubvARB != NULL &&
		qglVertexAttrib4NusvARB != NULL &&
		qglVertexAttrib4NuivARB != NULL &&
		qglVertexAttribPointerARB != NULL &&
		qglEnableVertexAttribArrayARB != NULL &&
		qglDisableVertexAttribArrayARB != NULL &&
		qglProgramStringARB != NULL &&
		qglBindProgramARB != NULL &&
		qglDeleteProgramsARB != NULL &&
		qglGenProgramsARB != NULL &&
		qglProgramEnvParameter4dARB != NULL &&
		qglProgramEnvParameter4dvARB != NULL &&
		qglProgramEnvParameter4fARB != NULL &&
		qglProgramEnvParameter4fvARB != NULL &&
		qglProgramLocalParameter4dARB != NULL &&
		qglProgramLocalParameter4dvARB != NULL &&
		qglProgramLocalParameter4fARB != NULL &&
		qglProgramLocalParameter4fvARB != NULL &&
		qglGetProgramEnvParameterdvARB != NULL &&
		qglGetProgramEnvParameterfvARB != NULL &&
		qglGetProgramLocalParameterdvARB != NULL &&
		qglGetProgramLocalParameterfvARB != NULL &&
		qglGetProgramivARB != NULL &&
		qglGetProgramStringARB != NULL &&
		qglGetVertexAttribdvARB != NULL &&
		qglGetVertexAttribfvARB != NULL &&
		qglGetVertexAttribivARB != NULL &&
		qglGetVertexAttribPointervARB != NULL &&
		qglIsProgramARB != NULL)
	{
		return (true);
	}
	
	return (false);
}

#endif // __APPLE__ || MACOSX

void GL_CreateShadersARB()
{
#if !defined(__APPLE__) && !defined (MACOSX)
    SAFE_GET_PROC(qglVertexAttrib1sARB,glVertexAttrib1sARBPROC,"glVertexAttrib1sARB");
    SAFE_GET_PROC(qglVertexAttrib1fARB,glVertexAttrib1fARBPROC,"glVertexAttrib1fARB");
    SAFE_GET_PROC(qglVertexAttrib1dARB,glVertexAttrib1dARBPROC,"glVertexAttrib1dARB");
    SAFE_GET_PROC(qglVertexAttrib2sARB,glVertexAttrib2sARBPROC,"glVertexAttrib2sARB");
    SAFE_GET_PROC(qglVertexAttrib2fARB,glVertexAttrib2fARBPROC,"glVertexAttrib2fARB");
    SAFE_GET_PROC(qglVertexAttrib2dARB,glVertexAttrib2dARBPROC,"glVertexAttrib2dARB");
    SAFE_GET_PROC(qglVertexAttrib3sARB,glVertexAttrib3sARBPROC,"glVertexAttrib3sARB");
    SAFE_GET_PROC(qglVertexAttrib3fARB,glVertexAttrib3fARBPROC,"glVertexAttrib3fARB");
    SAFE_GET_PROC(qglVertexAttrib3dARB,glVertexAttrib3dARBPROC,"glVertexAttrib3dARB");
    SAFE_GET_PROC(qglVertexAttrib4sARB,glVertexAttrib4sARBPROC,"glVertexAttrib4sARB");
    SAFE_GET_PROC(qglVertexAttrib4fARB,glVertexAttrib4fARBPROC,"glVertexAttrib4fARB");
    SAFE_GET_PROC(qglVertexAttrib4dARB,glVertexAttrib4dARBPROC,"glVertexAttrib4dARB");
    SAFE_GET_PROC(qglVertexAttrib4NubARB,glVertexAttrib4NubARBPROC,"glVertexAttrib4NubARB");
    SAFE_GET_PROC(qglVertexAttrib1svARB,glVertexAttrib1svARBPROC,"glVertexAttrib1svARB");
    SAFE_GET_PROC(qglVertexAttrib1fvARB,glVertexAttrib1fvARBPROC,"glVertexAttrib1fvARB");
    SAFE_GET_PROC(qglVertexAttrib1dvARB,glVertexAttrib1dvARBPROC,"glVertexAttrib1dvARB");
    SAFE_GET_PROC(qglVertexAttrib2svARB,glVertexAttrib2svARBPROC,"glVertexAttrib2svARB");
    SAFE_GET_PROC(qglVertexAttrib2fvARB,glVertexAttrib2fvARBPROC,"glVertexAttrib2fvARB");
    SAFE_GET_PROC(qglVertexAttrib2dvARB,glVertexAttrib2dvARBPROC,"glVertexAttrib2dvARB");
    SAFE_GET_PROC(qglVertexAttrib3svARB,glVertexAttrib3svARBPROC,"glVertexAttrib3svARB");
    SAFE_GET_PROC(qglVertexAttrib3fvARB,glVertexAttrib3fvARBPROC,"glVertexAttrib3fvARB");
    SAFE_GET_PROC(qglVertexAttrib3dvARB,glVertexAttrib3dvARBPROC,"glVertexAttrib3dvARB");
    SAFE_GET_PROC(qglVertexAttrib4bvARB,glVertexAttrib4bvARBPROC,"glVertexAttrib4bvARB");
    SAFE_GET_PROC(qglVertexAttrib4svARB,glVertexAttrib4svARBPROC,"glVertexAttrib4svARB");
    SAFE_GET_PROC(qglVertexAttrib4ivARB,glVertexAttrib4ivARBPROC,"glVertexAttrib4ivARB");
    SAFE_GET_PROC(qglVertexAttrib4ubvARB,glVertexAttrib4ubvARBPROC,"glVertexAttrib4ubvARB");
    SAFE_GET_PROC(qglVertexAttrib4usvARB,glVertexAttrib4usvARBPROC,"glVertexAttrib4usvARB");
    SAFE_GET_PROC(qglVertexAttrib4uivARB,glVertexAttrib4uivARBPROC,"glVertexAttrib4uivARB");
    SAFE_GET_PROC(qglVertexAttrib4fvARB,glVertexAttrib4fvARBPROC,"glVertexAttrib4fvARB");
    SAFE_GET_PROC(qglVertexAttrib4dvARB,glVertexAttrib4dvARBPROC,"glVertexAttrib4dvARB");
    SAFE_GET_PROC(qglVertexAttrib4NbvARB,glVertexAttrib4NbvARBPROC,"glVertexAttrib4NbvARB");
    SAFE_GET_PROC(qglVertexAttrib4NsvARB,glVertexAttrib4NsvARBPROC,"glVertexAttrib4NsvARB");
    SAFE_GET_PROC(qglVertexAttrib4NivARB,glVertexAttrib4NivARBPROC,"glVertexAttrib4NivARB");
    SAFE_GET_PROC(qglVertexAttrib4NubvARB,glVertexAttrib4NubvARBPROC,"glVertexAttrib4NubvARB");
    SAFE_GET_PROC(qglVertexAttrib4NusvARB,glVertexAttrib4NusvARBPROC,"glVertexAttrib4NusvARB");
    SAFE_GET_PROC(qglVertexAttrib4NuivARB,glVertexAttrib4NuivARBPROC,"glVertexAttrib4NuivARB");
    SAFE_GET_PROC(qglVertexAttribPointerARB,glVertexAttribPointerARBPROC,"glVertexAttribPointerARB");
    SAFE_GET_PROC(qglEnableVertexAttribArrayARB,glEnableVertexAttribArrayARBPROC,"glEnableVertexAttribArrayARB");
    SAFE_GET_PROC(qglDisableVertexAttribArrayARB,glDisableVertexAttribArrayARBPROC,"glDisableVertexAttribArrayARB");
    SAFE_GET_PROC(qglProgramStringARB,glProgramStringARBPROC,"glProgramStringARB");
    SAFE_GET_PROC(qglBindProgramARB,glBindProgramARBPROC,"glBindProgramARB");
    SAFE_GET_PROC(qglDeleteProgramsARB,glDeleteProgramsARBPROC,"glDeleteProgramsARB");
    SAFE_GET_PROC(qglGenProgramsARB,glGenProgramsARBPROC,"glGenProgramsARB");
    SAFE_GET_PROC(qglProgramEnvParameter4dARB,glProgramEnvParameter4dARBPROC,"glProgramEnvParameter4dARB");
    SAFE_GET_PROC(qglProgramEnvParameter4dvARB,glProgramEnvParameter4dvARBPROC,"glProgramEnvParameter4dvARB");
    SAFE_GET_PROC(qglProgramEnvParameter4fARB,glProgramEnvParameter4fARBPROC,"glProgramEnvParameter4fARB");
    SAFE_GET_PROC(qglProgramEnvParameter4fvARB,glProgramEnvParameter4fvARBPROC,"glProgramEnvParameter4fvARB");
    SAFE_GET_PROC(qglProgramLocalParameter4dARB,glProgramLocalParameter4dARBPROC,"glProgramLocalParameter4dARB");
    SAFE_GET_PROC(qglProgramLocalParameter4dvARB,glProgramLocalParameter4dvARBPROC,"glProgramLocalParameter4dvARB");
    SAFE_GET_PROC(qglProgramLocalParameter4fARB,glProgramLocalParameter4fARBPROC,"glProgramLocalParameter4fARB");
    SAFE_GET_PROC(qglProgramLocalParameter4fvARB,glProgramLocalParameter4fvARBPROC,"glProgramLocalParameter4fvARB");
    SAFE_GET_PROC(qglGetProgramEnvParameterdvARB,glGetProgramEnvParameterdvARBPROC,"glGetProgramEnvParameterdvARB");
    SAFE_GET_PROC(qglGetProgramEnvParameterfvARB,glGetProgramEnvParameterfvARBPROC,"glGetProgramEnvParameterfvARB");
    SAFE_GET_PROC(qglGetProgramLocalParameterdvARB,glGetProgramLocalParameterdvARBPROC,"glGetProgramLocalParameterdvARB");
    SAFE_GET_PROC(qglGetProgramLocalParameterfvARB,glGetProgramLocalParameterfvARBPROC,"glGetProgramLocalParameterfvARB");
    SAFE_GET_PROC(qglGetProgramivARB,glGetProgramivARBPROC,"glGetProgramivARB");
    SAFE_GET_PROC(qglGetProgramStringARB,glGetProgramStringARBPROC,"glGetProgramStringARB");
    SAFE_GET_PROC(qglGetVertexAttribdvARB,glGetVertexAttribdvARBPROC,"glGetVertexAttribdvARB");
    SAFE_GET_PROC(qglGetVertexAttribfvARB,glGetVertexAttribfvARBPROC,"glGetVertexAttribfvARB");
    SAFE_GET_PROC(qglGetVertexAttribivARB,glGetVertexAttribivARBPROC,"glGetVertexAttribivARB");
    SAFE_GET_PROC(qglGetVertexAttribPointervARB,glGetVertexAttribPointervARBPROC,"glGetVertexAttribPointervARB");
    SAFE_GET_PROC(qglIsProgramARB,glIsProgramARBPROC,"glIsProgramARB");

    SAFE_GET_PROC( qglPNTrianglesiATI, PFNGLPNTRIANGLESIATIPROC, "glPNTrianglesiATI");
    SAFE_GET_PROC( qglPNTrianglesfATI, PFNGLPNTRIANGLESFATIPROC, "glPNTrianglesfATI");
#endif /* !__APPLE__ && !MACOSX */

    glEnable(GL_VERTEX_PROGRAM_ARB);
    checkerror();
    
    qglGenProgramsARB(2, &vertex_programs[0]);
    checkerror();
    qglBindProgramARB(GL_VERTEX_PROGRAM_ARB, vertex_programs[0]);
    checkerror();
    qglProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                        sizeof(vertexprogram)-1, vertexprogram);
    checkerror();
    
    qglBindProgramARB(GL_VERTEX_PROGRAM_ARB, vertex_programs[1]);
    checkerror();
    qglProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                        sizeof(vertexprogram2)-1, vertexprogram2);
    checkerror();
    
    glDisable(GL_VERTEX_PROGRAM_ARB);
    checkerror();
    
    glEnable(GL_FRAGMENT_PROGRAM_ARB);
    checkerror();
    
    qglGenProgramsARB(2, &fragment_programs[0]);
    checkerror();
    qglBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fragment_programs[0]);
    checkerror();
    qglProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
		                sizeof(fragmentprogram)-1, fragmentprogram);
    checkerror();
    
    qglBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fragment_programs[1]);
    checkerror();
    qglProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
		                sizeof(fragmentprogram2)-1, fragmentprogram2);
    checkerror();
    
    glDisable(GL_FRAGMENT_PROGRAM_ARB);
    checkerror();
}

void GL_DisableDiffuseShaderARB()
{
    //tex 0 = normal map
    //tex 1 = color map
    //tex 2 = attenuation 3d texture
    //tex 3 = (light filter, depends on light settings)
    
    glDisable(GL_FRAGMENT_PROGRAM_ARB);
    glDisable(GL_VERTEX_PROGRAM_ARB);
    
    GL_SelectTexture(GL_TEXTURE1_ARB);
    
    GL_SelectTexture(GL_TEXTURE2_ARB);
    glPopMatrix();
    
    if (currentshadowlight->filtercube)
    {
        GL_SelectTexture(GL_TEXTURE3_ARB);
        glPopMatrix();
    }
    glMatrixMode(GL_MODELVIEW);
    
    GL_SelectTexture(GL_TEXTURE0_ARB);
}

void GL_EnableDiffuseSpecularShaderARB(qboolean world, vec3_t lightOrig)
{
    // No need to enable/disable textures as the fragment program
    // extension uses the information contained in the program anyway
    float invrad = 1/currentshadowlight->radius;
    
    //tex 0 = normal map
    //tex 1 = color map
    //tex 2 = attenuation 3d texture
    //tex 3 = (light filter, depends on light settings)
    
    GL_SelectTexture(GL_TEXTURE2_ARB);
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    
    glBindTexture(GL_TEXTURE_3D, atten3d_texture_object);
    
    glTranslatef(0.5,0.5,0.5);
    glScalef(0.5,0.5,0.5);
    glScalef(invrad, invrad, invrad);
    glTranslatef(-lightOrig[0], -lightOrig[1], -lightOrig[2]);
    
    glGetError();
    glEnable(GL_VERTEX_PROGRAM_ARB);
    checkerror();
    glEnable(GL_FRAGMENT_PROGRAM_ARB);
    checkerror();
    
    if (currentshadowlight->filtercube)
    {
        GL_SelectTexture(GL_TEXTURE3_ARB);
        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
        glLoadIdentity();
        
        glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, currentshadowlight->filtercube);
        GL_SetupCubeMapMatrix(world);
        
        qglBindProgramARB( GL_VERTEX_PROGRAM_ARB, vertex_programs[1] );
        checkerror();
        qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, fragment_programs[1] );
        checkerror();
    }
    else
    {
        qglBindProgramARB( GL_VERTEX_PROGRAM_ARB, vertex_programs[0] );
        checkerror();
        qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, fragment_programs[0] );
        checkerror();
    }
    
    GL_SelectTexture(GL_TEXTURE0_ARB);
}

void R_DrawWorldARBDiffuseSpecular(lightcmd_t *lightCmds) 
{
    int command, num, i;
    int lightPos = 0;
    vec3_t lightOr;
    msurface_t *surf;
    float               *v;
    float* lightP;
    vec3_t lightDir;
    vec3_t tsH,H;
    
    texture_t   *t;//XYZ
    
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
        GL_SelectTexture(GL_TEXTURE1_ARB);
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

void R_DrawBrushARBDiffuseSpecular(entity_t *e)
{
    model_t     *model = e->model;
    msurface_t *surf;
    glpoly_t    *poly;
    int         i, j, count;
    brushlightinstant_t *ins = e->brushlightinstant;
    float       *v;
    texture_t   *t;//XYZ        
    
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
        GL_SelectTexture(GL_TEXTURE1_ARB);
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


void R_DrawAliasFrameARBDiffuseSpecular (aliashdr_t *paliashdr, aliasframeinstant_t *instant)
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
    GL_SelectTexture(GL_TEXTURE1_ARB);
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

void R_DrawWorldBumpedARB()
{
    if (!currentshadowlight->visible)
        return;
    
    glDepthMask (0);
    glShadeModel (GL_SMOOTH);
    
    GL_AddColor();
    glColor3fv(&currentshadowlight->color[0]);
    
    GL_EnableDiffuseSpecularShaderARB(true,currentshadowlight->origin);
    R_DrawWorldARBDiffuseSpecular(currentshadowlight->lightCmds);
    GL_DisableDiffuseShaderARB();
    
    glColor3f (1,1,1);
    glDisable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask (1);
}

void R_DrawBrushBumpedARB(entity_t *e)
{
    GL_AddColor();
    glColor3fv(&currentshadowlight->color[0]);
    
    GL_EnableDiffuseSpecularShaderARB(false,((brushlightinstant_t *)e->brushlightinstant)->lightpos);
    R_DrawBrushARBDiffuseSpecular(e);
    GL_DisableDiffuseShaderARB();
}

void R_DrawAliasBumpedARB(aliashdr_t *paliashdr, aliasframeinstant_t *instant)
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
    
    GL_EnableDiffuseSpecularShaderARB(false,instant->lightinstant->lightpos);
    R_DrawAliasFrameARBDiffuseSpecular(paliashdr,instant);
    GL_DisableDiffuseShaderARB();
    
    if ( gl_truform.value )
    {
        glDisable(GL_PN_TRIANGLES_ATI);
    }
}
