//----------------------------------------------------------------------------------------------------------------------------
//
// "gl_utility.c" - some OpenGL convenience functions.
//
// Written by:	Axel 'awe' Wefers		[mailto:awe@fruitz-of-dojo.de].
//		©2002 Fruitz Of Dojo 		[http://www.fruitz-of-dojo.de].
//
// Quakeª is copyrighted by id software		[http://www.idsoftware.com].
//
//----------------------------------------------------------------------------------------------------------------------------
//
// Functions:
//
// GL_TexImage1D (...)
//	¥ same as "glTexImage1D()", but checks if texture will fit into texture RAM.
//	¥ define NO_TEXCHECK to avoid texture ram check.	
//
// GL_TexImage2D (...)
//	¥ same as "glTexImage2D()", but checks if texture will fit into texture RAM.	
//	¥ define NO_TEXCHECK to avoid texture ram check.
//
// GL_TexImage3D (...)
//	¥ same as "glTexImage3D()", but checks if texture will fit into texture RAM.
//	¥ will call "qglTexImage3DEXT ()". simply bind "glTexImage3D" or "glTexImage3DEXT" to "qglTexImage3DEXT."
//	¥ define NO_TEXCHECK to avoid texture ram check.
//
// GLboolean	GL_CheckVersion (GLubyte theMajor, GLubyte theMinor)
//	¥ returns GL_TRUE if OpenGL version is greater than or equal to theMajor.theMinor. 
//	¥ returns GL_FALSE if OpenGL version is less than theMajor.theMinor.
//
// GLboolean	GL_ExtensionSupported (const GLubyte *theExtension)
//	¥ returns GL_TRUE if "theExtension" is supported.
//	¥ returns GL_FALSE if "theExtension" is unsupported.
//
//----------------------------------------------------------------------------------------------------------------------------

#include "quakedef.h"

#include <OpenGL/gl.h>

//----------------------------------------------------------------------------------------------------------------------------

static void	GL_TexImageProxy (GLenum target, GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth,
                              GLint border, GLenum format, GLenum type);

//----------------------------------------------------------------------------------------------------------------------------

void	GL_TexImageProxy (GLenum target, GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth,
                          GLint border, GLenum format, GLenum type)
{
    GLint	realWidth   = -1;
    GLenum	error       = GL_NO_ERROR;
    
    glGetError ();

    switch (target)
    {
        case GL_TEXTURE_1D:
        case GL_PROXY_TEXTURE_1D:
            target = GL_PROXY_TEXTURE_1D;
            glTexImage1D (target, level, internalFormat, width, border, format, type, NULL); 
            break;
        
        case GL_TEXTURE_2D:
        case GL_PROXY_TEXTURE_2D:
            target = GL_PROXY_TEXTURE_2D;
            glTexImage2D (target, level, internalFormat, width, height, border, format, type, NULL); 
            break;
        
        case GL_TEXTURE_3D:
        case GL_PROXY_TEXTURE_3D:
            target = GL_PROXY_TEXTURE_3D;
            qglTexImage3DEXT (target, level, internalFormat, width, height, depth, border, format, type, NULL);
            break;
        
        case GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB:
            target = GL_PROXY_TEXTURE_CUBE_MAP_ARB;
            glTexImage2D (target, level, internalFormat, width, height, border, format, type, NULL);
            break;
            
        default:
            return;
    }
    
    error = glGetError ();

    glGetTexLevelParameteriv (target, level, GL_TEXTURE_WIDTH, &realWidth);

    if ((error != GL_NO_ERROR) || (width != realWidth))
    {
        Sys_Error ("Out of texture RAM. Please try a lower resolution and/or depth!");
    }
}

//----------------------------------------------------------------------------------------------------------------------------

void	GL_TexImage1D (GLenum target, GLint level, GLenum internalFormat, GLsizei width, GLint border, GLenum format,
                       GLenum type, const GLvoid* pPixels)
{
#if !defined (NO_TEXCHECK)
    GL_TexImageProxy (target, level, internalFormat, width, 0, 0, border, format, type);
#endif // !NO_TEXCHECK
    
    glTexImage1D (target, level, internalFormat, width, border, format, type, pPixels);
}

//----------------------------------------------------------------------------------------------------------------------------

void	GL_TexImage2D (GLenum target, GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLint border,
                       GLenum format, GLenum type, const GLvoid* pPixels)
{
#if !defined (NO_TEXCHECK)
    GL_TexImageProxy (target, level, internalFormat, width, height, 0, border, format, type);
#endif // !NO_TEXCHECK
    
    glTexImage2D (target, level, internalFormat, width, height, border, format, type, pPixels);
}

//----------------------------------------------------------------------------------------------------------------------------

void	GL_TexImage3D (GLenum target, GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth,
                       GLint border, GLenum format, GLenum type, const GLvoid* pPixels)
{
#if !defined (NO_TEXCHECK)
    GL_TexImageProxy (target, level, internalFormat, width, height, depth, border, format, type);
#endif // !NO_TEXCHECK
    
    qglTexImage3DEXT (target, level, internalFormat, width, height, depth, border, format, type, pPixels);
}

//----------------------------------------------------------------------------------------------------------------------------

GLboolean	GL_CheckVersion (GLubyte reqMajor, GLubyte reqMinor)
{
    GLubyte	major = 0;
    GLubyte minor = 0;
    
    if (gl_version == NULL)
    {
        return GL_FALSE;
    }
    
    major = atoi (gl_version);
    
    if (major < reqMajor)
    {
        return GL_FALSE;
    }
    else if (major == reqMajor)
    {
        const char*	pGLVersion = gl_version;
        
        while ((*pGLVersion != '\0') && (*pGLVersion++ != '.'));
        
        minor = atoi (pGLVersion);
        
        if (minor < reqMinor)
        {
            return GL_FALSE;
        }        
    }

    return GL_TRUE;
}

//----------------------------------------------------------------------------------------------------------------------------

GLboolean	GL_ExtensionSupported (const char* pExtension)
{
    const char*     pExtensions = gl_extensions;
    size_t          len         = 0;
    
    if (pExtension == NULL || gl_extensions == NULL || strchr (pExtension, ' ') != NULL	|| *pExtension == '\0')
    {
        return GL_FALSE;
    }
    
    len = strlen (pExtension);
    
    while (1)
    {
        const char*  pStart  = strstr (pExtensions, pExtension);
		const char*  pEnd    = pStart + len;
        
        if (pStart == NULL)
        {
            break;
        }
		
        if ((pStart == pExtensions || *(pStart - 1) == ' ') && (*pEnd == ' ' || *pEnd == '\0'))
        {
            return GL_TRUE;
        }
        
        pExtensions = pEnd;
    }
    
    return GL_FALSE;
}

//----------------------------------------------------------------------------------------------------------------------------
