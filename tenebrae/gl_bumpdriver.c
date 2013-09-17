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

Abstracts the different paths that exist for different 3d-cards.

  Idea: Write a new R_DrawLightEntitys* and do whatever you want in it.
	adapt R_DrawLightEntitys to call your new code if the requirements are met.
*/

/*
DrawLightEntities, draws lit bumpmapped entities, calls apropriate function for the user's 3d card.

NOTE:  This should not draw sprites, sprites are drawn separately.

*/

#include "quakedef.h"

/* Some material definitions. */
float gl_Light_Ambience2[4] = {0.03,0.03,0.03,0.03};
float gl_Light_Diffuse2[4] = {0.03,0.03,0.03,0.03};
float gl_Light_Specular2[4] = {0,0,0,0};
float gl_Material_Color2[4] = {0.9, 0.9, 0.9, 0.9};

void R_DrawLightEntitiesGF3 (shadowlight_t *l);
void R_DrawLightEntitiesGF (shadowlight_t *l);
void R_DrawLightEntitiesGEN (shadowlight_t *l);
void R_DrawLightEntitiesRadeon (shadowlight_t *l); //PA:
void R_DrawLightEntitiesParhelia (shadowlight_t *l); //PA:
void R_DrawLightEntitiesARB (shadowlight_t *l); //PA:

void R_DrawWorldBumped (/* shadowlight_t *l */)  // <AWE> Function should not have parameters.
{
    switch(gl_cardtype )
    {
    case GEFORCE3:
	R_DrawWorldBumpedGF3(/* l */);	// <AWE> Function has no parameters.
	break;

    case GEFORCE:
	R_DrawWorldBumpedGF(/* l */);	// <AWE> Function has no parameters.
	break;

    case RADEON:
	R_DrawWorldBumpedRadeon(/* l */);
	break;
#ifndef __glx__
    case PARHELIA:
	R_DrawWorldBumpedParhelia(/* l */);
	break;
#endif
    case ARB:
	R_DrawWorldBumpedARB(/* l */);
	break;
    default:
	R_DrawWorldBumpedGEN(/* l */);
	break;
    }
}

void R_DrawLightEntities (shadowlight_t *l)
{
    switch(gl_cardtype )
    {
    case GEFORCE3:
	R_DrawLightEntitiesGF3( l );
	break;

    case GEFORCE:
	R_DrawLightEntitiesGF( l );
	break;

    case RADEON:
	R_DrawLightEntitiesRadeon( l );
	break;
#ifndef __glx__
    case PARHELIA:
	R_DrawLightEntitiesParhelia( l );
	break;
#endif
    case ARB:
	R_DrawLightEntitiesARB( l );
	break;
    default:
	R_DrawLightEntitiesGEN( l );
	break;
    }
}


void R_DrawLightEntitiesGF (shadowlight_t *l)
{
    int		i;
    float	pos[4];
    if (!r_drawentities.value)
	return;

    if (!currentshadowlight->visible)
	return;

    glDepthMask (0);
    glShadeModel (GL_SMOOTH);

    //Meshes: Atten & selfshadow via vertex ligting

    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE);
	
    pos[0] = l->origin[0];
    pos[1] = l->origin[1];
    pos[2] = l->origin[2];
    pos[3] = 1;

    glLightfv(GL_LIGHT0, GL_POSITION,&pos[0]);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, &l->color[0]);
    glLightfv(GL_LIGHT0, GL_AMBIENT, &gl_Light_Ambience2[0]);
    glLightfv(GL_LIGHT0, GL_SPECULAR, &gl_Light_Specular2[0]);
    glEnable(GL_COLOR_MATERIAL);


    for (i=0 ; i<cl_numlightvisedicts ; i++)
    {
	currententity = cl_lightvisedicts[i];

	if (currententity->model->type == mod_alias)
	{
	    //these models are full bright 
	    if (currententity->model->flags & EF_FULLBRIGHT) continue;
	    if (!currententity->aliasframeinstant) continue;
	    if ( ((aliasframeinstant_t *)currententity->aliasframeinstant)->shadowonly) continue;

	    R_DrawAliasObjectLight(currententity, R_DrawAliasBumped);
	}
    }

    if (R_ShouldDrawViewModel())
    {
	R_DrawAliasObjectLight(&cl.viewent, R_DrawAliasBumped);
    }


    glDisable(GL_COLOR_MATERIAL);
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);

    /*
      Brushes: we use the same thecnique as the world
    */
	
    //glEnable(GL_TEXTURE_2D);
    //GL_Bind(glow_texture_object);
    //glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    //glShadeModel (GL_SMOOTH);
    //glEnable(GL_ALPHA_TEST);
    //glAlphaFunc(GL_GREATER,0.2);
    for (i=0 ; i<cl_numlightvisedicts ; i++)
    {
	currententity = cl_lightvisedicts[i];

	if (currententity->model->type == mod_brush)
	{
	    if (!currententity->brushlightinstant) continue;
	    if ( ((brushlightinstant_t *)currententity->brushlightinstant)->shadowonly) continue;
	    R_DrawBrushObjectLight(currententity, R_DrawBrushBumped);
	}
	
    }
	
    //reset gl state

    //glAlphaFunc(GL_GREATER,0.666);//Satan!
    //glDisable(GL_ALPHA_TEST);
    glColor3f (1,1,1);
    glDepthMask (1);	
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glDisable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void R_DrawLightEntitiesGEN (shadowlight_t *l)
{
    //Currently this is merged with the geforce2 path
    R_DrawLightEntitiesGF(l);
}

void R_DrawLightEntitiesGF3 (shadowlight_t *l)
{
    int		i;

    if (!r_drawentities.value)
	return;

    if (!currentshadowlight->visible)
	return;

    glDepthMask (0);
    glShadeModel (GL_SMOOTH);

    //Alias models

    for (i=0 ; i<cl_numlightvisedicts ; i++)
    {
	currententity = cl_lightvisedicts[i];

	if (currententity->model->type == mod_alias)
	{
	    //these models are full bright 
	    if (currententity->model->flags & EF_FULLBRIGHT) continue;
	    if (!currententity->aliasframeinstant) continue;
	    if ( ((aliasframeinstant_t *)currententity->aliasframeinstant)->shadowonly) continue;

	    R_DrawAliasObjectLight(currententity, R_DrawAliasBumpedGF3);
	}
    }

    if (R_ShouldDrawViewModel()) {
	R_DrawAliasObjectLight(&cl.viewent, R_DrawAliasBumpedGF3);
    }

    //Brush models
    for (i=0 ; i<cl_numlightvisedicts ; i++)
    {
	currententity = cl_lightvisedicts[i];

	if (currententity->model->type == mod_brush)
	{
	    if (!currententity->brushlightinstant) continue;
	    if ( ((brushlightinstant_t *)currententity->brushlightinstant)->shadowonly) continue;
	    R_DrawBrushObjectLight(currententity, R_DrawBrushBumpedGF3);
	}

    }

    //Cleanup state
    glColor3f (1,1,1);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glDisable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask (1);
}

//PA:
void R_DrawLightEntitiesRadeon (shadowlight_t *l)
{
    int		i;

    if (!r_drawentities.value)
	return;

    if (!currentshadowlight->visible)
	return;

    glDepthMask (0);
    glShadeModel (GL_SMOOTH);

    //Alias models

    for (i=0 ; i<cl_numlightvisedicts ; i++)
    {
	currententity = cl_lightvisedicts[i];

	if (currententity->model->type == mod_alias)
	{
	    //these models are full bright 
	    if (currententity->model->flags & EF_FULLBRIGHT) continue;
	    if (!currententity->aliasframeinstant) continue;
	    if ( ((aliasframeinstant_t *)currententity->aliasframeinstant)->shadowonly) continue;

	    R_DrawAliasObjectLight(currententity, R_DrawAliasBumpedRadeon);
	}
    }

    if (R_ShouldDrawViewModel()) {
	R_DrawAliasObjectLight(&cl.viewent, R_DrawAliasBumpedRadeon);
    }

    //Brush models
    for (i=0 ; i<cl_numlightvisedicts ; i++)
    {
	currententity = cl_lightvisedicts[i];

	if (currententity->model->type == mod_brush)
	{
	    if (!currententity->brushlightinstant) continue;
	    if ( ((brushlightinstant_t *)currententity->brushlightinstant)->shadowonly) continue;
	    R_DrawBrushObjectLight(currententity, R_DrawBrushBumpedRadeon);
	}

    }

    //Cleanup state
    glColor3f (1,1,1);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glDisable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask (1);
}

#ifndef __glx__

//PA:
void R_DrawLightEntitiesParhelia (shadowlight_t *l)
{
    int		i;

    if (!r_drawentities.value)
	return;

    if (!currentshadowlight->visible)
	return;

    glDepthMask (0);
    glShadeModel (GL_SMOOTH);

    //Alias models

    for (i=0 ; i<cl_numlightvisedicts ; i++)
    {
	currententity = cl_lightvisedicts[i];

	if (currententity->model->type == mod_alias)
	{
	    //these models are full bright 
	    if (currententity->model->flags & EF_FULLBRIGHT) continue;
	    if (!currententity->aliasframeinstant) continue;
	    if ( ((aliasframeinstant_t *)currententity->aliasframeinstant)->shadowonly) continue;

	    R_DrawAliasObjectLight(currententity, R_DrawAliasBumpedParhelia);
	}
    }

    if (R_ShouldDrawViewModel())
    {
	R_DrawAliasObjectLight(&cl.viewent, R_DrawAliasBumpedParhelia);
    }

    //Brush models
    for (i=0 ; i<cl_numlightvisedicts ; i++)
    {
	currententity = cl_lightvisedicts[i];

	if (currententity->model->type == mod_brush)
	{
	    if (!currententity->brushlightinstant) continue;
	    if ( ((brushlightinstant_t *)currententity->brushlightinstant)->shadowonly) continue;
	    R_DrawBrushObjectLight(currententity, R_DrawBrushBumpedParhelia);
	}

    }

    //Cleanup state
    glColor3f (1,1,1);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glDisable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask (1);
}

#endif

void R_DrawLightEntitiesARB (shadowlight_t *l)
{
    int		i;

    if (!r_drawentities.value)
	return;

    if (!currentshadowlight->visible)
	return;

    glDepthMask (0);
    glShadeModel (GL_SMOOTH);

    //Alias models

    for (i=0 ; i<cl_numlightvisedicts ; i++)
    {
	currententity = cl_lightvisedicts[i];

	if (currententity->model->type == mod_alias)
	{
	    //these models are full bright 
	    if (currententity->model->flags & EF_FULLBRIGHT) continue;
	    if (!currententity->aliasframeinstant) continue;
	    if ( ((aliasframeinstant_t *)currententity->aliasframeinstant)->shadowonly) continue;

	    R_DrawAliasObjectLight(currententity, R_DrawAliasBumpedARB);
	}
    }

    if (R_ShouldDrawViewModel())
    {
	R_DrawAliasObjectLight(&cl.viewent, R_DrawAliasBumpedARB);
    }

    //Brush models
    for (i=0 ; i<cl_numlightvisedicts ; i++)
    {
	currententity = cl_lightvisedicts[i];

	if (currententity->model->type == mod_brush)
	{
	    if (!currententity->brushlightinstant) continue;
	    if ( ((brushlightinstant_t *)currententity->brushlightinstant)->shadowonly) continue;
	    R_DrawBrushObjectLight(currententity, R_DrawBrushBumpedARB);
	}

    }

    //Cleanup state
    glColor3f (1,1,1);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glDisable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask (1);
}
