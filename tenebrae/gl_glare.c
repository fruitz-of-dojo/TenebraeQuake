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

All right this is slow & ugly but I just had to to it

Carmack talked about something similar in his speach.
The Halo2 video has it 
And I want to see it in action!

*/

#include "quakedef.h"

#define GLARE_WIDTH 64

byte glarepixels[GLARE_WIDTH*GLARE_WIDTH][4];
byte glareblurpixels[GLARE_WIDTH*GLARE_WIDTH][4];
long glaresum[GLARE_WIDTH*GLARE_WIDTH][4]; //Sums for fast blurring

int	glare_object;

void R_InitGlare() {

	GL_Bind (texture_extension_number);
	glare_object = texture_extension_number;
	GL_TexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, GLARE_WIDTH, GLARE_WIDTH, 0, GL_RGBA,
						GL_UNSIGNED_BYTE, glarepixels);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	texture_extension_number++;
}

byte mulc(int i1) {
	//amplify colors a bit
	int r = i1 + (i1/4);
	
	if (r > 255) return 255;
	else return r;
}

#define	GLARE_SUBTR	128.0
#define	IGLARE_SUBTR 128
#define GLARE_MULT	4
#define GLARE_COMP 120

void ProcessGlare() {

	int i;

	for (i=0; i<GLARE_WIDTH*GLARE_WIDTH; i++) {

		if ( (glarepixels[i][0] > GLARE_COMP) ||
			  (glarepixels[i][1] > GLARE_COMP) ||
			  (glarepixels[i][2] > GLARE_COMP) )
		{
			
			glarepixels[i][0] = mulc(glarepixels[i][0]);
			glarepixels[i][1] = mulc(glarepixels[i][1]);
			glarepixels[i][2] = mulc(glarepixels[i][2]);
		} else {
			glarepixels[i][0] = 0;
			glarepixels[i][1] = 0;
			glarepixels[i][2] = 0;
		}
	}
}

/*
Fast Idea Blurring from
http://www.gamasutra.com/features/20010209/evans_01.htm
*/
typedef struct bytecolor_s {
	byte r,g,b,a;
} bytecolor_t;

typedef struct longcolor_s {
	unsigned long r,g,b,a;
} longcolor_t;

void DoPreComputation(bytecolor_t *src, int src_w, int src_h, longcolor_t *dst)
{
	longcolor_t tot;
	int y,x;

	for (y=0;y<src_h;y++)
	{
		for (x=0;x<src_w;x++)
		{
			tot.r = src[0].r;
			tot.g = src[0].g;
			tot.b = src[0].b;
			tot.a = src[0].a;

			if (x>0) {
				tot.r+=dst[-1].r;
				tot.g+=dst[-1].g;
				tot.b+=dst[-1].b;
				tot.a+=dst[-1].a;
			}
			if (y>0) {
				tot.r+=dst[-src_w].r;
				tot.g+=dst[-src_w].g;
				tot.b+=dst[-src_w].b;
				tot.a+=dst[-src_w].a;
			}
			if (x>0 && y>0) {
				tot.r-=dst[-src_w-1].r;
				tot.g-=dst[-src_w-1].g;
				tot.b-=dst[-src_w-1].b;
				tot.a-=dst[-src_w-1].a;
			}
			dst->r=tot.r;
			dst->g=tot.g;
			dst->b=tot.b;
			dst->a=tot.a;
			dst++;
			src++;
		}
	}
}

// this is a utility function used by DoBoxBlur below
longcolor_t *ReadP(longcolor_t *p, int w, int h, int x, int y)
{
	if (x<0) x=0; else if (x>=w) x=w-1;
	if (y<0) y=0; else if (y>=h) y=h-1;
	return &p[x+y*w];
}

// the main meat of the algorithm lies here
void DoBoxBlur(bytecolor_t *src, int src_w, int src_h, bytecolor_t *dst, longcolor_t *p, int boxw, int boxh)
{
	longcolor_t *to1, *to2, *to3, *to4;
	int y,x;
	float mul;

	if (boxw<0 || boxh<0)
	{
		memcpy(dst,src,src_w*src_h*4); // deal with degenerate kernel sizes
		return;
	}
	mul=1.f/((boxw*2+1)*(boxh*2+1));
	for (y=0;y<src_h;y++)
	{
		for (x=0;x<src_w;x++)
		{
			to1 = ReadP(p,src_w,src_h,x+boxw,y+boxh);
			to2 = ReadP(p,src_w,src_h,x-boxw,y-boxh);
			to3 = ReadP(p,src_w,src_h,x-boxw,y+boxh);
			to4 = ReadP(p,src_w,src_h,x+boxw,y-boxh);

			dst->r= (to1->r + to2->r - to3->r - to4->r)*mul;
			dst->g= (to1->g + to2->g - to3->g - to4->g)*mul;
			dst->b= (to1->b + to2->b - to3->b - to4->b)*mul;
			dst->a= (to1->a + to2->a - to3->a - to4->a)*mul;
			
			dst++;
			src++;
		}
	}
}

void R_Glare ()
{
	int			ox, oy, ow, oh, ofovx, ofovy;

	if (!sh_glares.value || gl_wireframe.value) 
		return;

	glare = true;

	//AngleVectors (r_refdef.viewangles, vpn, vright, vup);

	//Render to a small view rectangle
	ox = r_refdef.vrect.x;
	oy = r_refdef.vrect.y;
	ow = r_refdef.vrect.width;
	oh = r_refdef.vrect.height;
	ofovx = r_refdef.fov_x;
	ofovy = r_refdef.fov_y;


	r_refdef.vrect.x = 0;
	r_refdef.vrect.y = 0;
	r_refdef.vrect.width = GLARE_WIDTH;
	r_refdef.vrect.height = GLARE_WIDTH;
//	r_refdef.fov_x = 90.0;
//	r_refdef.fov_y = 90.0;//CalcFov (90.0, r_refdef.vrect.width, r_refdef.vrect.height);
	r_refdef.fov_x = scr_fov.value;
	r_refdef.fov_y = CalcFov (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);

	//Con_Printf("render glaer\n");
	R_RenderScene ();

	glReadPixels (0, 0, GLARE_WIDTH, GLARE_WIDTH, GL_RGBA, GL_UNSIGNED_BYTE, glarepixels);
	ProcessGlare();
	DoPreComputation((bytecolor_t *)glarepixels, GLARE_WIDTH, GLARE_WIDTH, (longcolor_t *)glaresum);
	DoBoxBlur((bytecolor_t *)glarepixels, GLARE_WIDTH, GLARE_WIDTH,
			(bytecolor_t *)glareblurpixels, (longcolor_t *)glaresum, 5, 5);


	GL_Bind(glare_object);
	GL_TexImage2D (GL_TEXTURE_2D, 0, 4, GLARE_WIDTH, GLARE_WIDTH, 0, GL_RGBA,
					GL_UNSIGNED_BYTE, glareblurpixels);

//	GL_Bind(glare_object);
//	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 128, 128);

	glClear (GL_DEPTH_BUFFER_BIT);

	r_refdef.vrect.x = ox;
	r_refdef.vrect.y = oy;
	r_refdef.vrect.width = ow;
	r_refdef.vrect.height = oh;
	r_refdef.fov_x = ofovx;
	r_refdef.fov_y = ofovy;

	//we have drawn on it
	Sbar_Changed ();
	
	glare = false;

}

void GlareQuad() {

	glBegin (GL_QUADS);
	glTexCoord2f(-1,1);
	glVertex3f (0.1, 1, 1);
	glTexCoord2f(0,1);
	glVertex3f (0.1, -1, 1);
	glTexCoord2f(0,0);
	glVertex3f (0.1, -1, -1);
	glTexCoord2f(-1,0);
	glVertex3f (0.1, 1, -1);
	glEnd ();

}

#define GLARE_OFS 1.0/16

void R_DrawGlare (void)
{
	if (!sh_glares.value) return;	
	
	GL_DisableMultitexture();

	glEnable (GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glDisable (GL_DEPTH_TEST);
	GL_Bind(glare_object);
glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glLoadIdentity ();

    glRotatef (-90,  1, 0, 0);	    // put Z going up
    glRotatef (90,  0, 0, 1);	    // put Z going up


	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
    glLoadIdentity ();
	glMatrixMode(GL_MODELVIEW);

	glColor4f (0.2,0.2,0.2,0.2);

	glPushMatrix();
	glTranslatef(0,0,GLARE_OFS);
	GlareQuad();
	glPopMatrix();

	glPushMatrix();
	glTranslatef(0,0,-GLARE_OFS);
	GlareQuad();
	glPopMatrix();

	glPushMatrix();
	glTranslatef(0,-GLARE_OFS,0);
	GlareQuad();
	glPopMatrix();

	glPushMatrix();
	glTranslatef(0,GLARE_OFS,0);
	GlareQuad();
	glPopMatrix();

	GlareQuad();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	glDisable (GL_BLEND);
	glEnable (GL_ALPHA_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}