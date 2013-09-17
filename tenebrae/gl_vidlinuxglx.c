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
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/vt.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>
#include <stdio.h>


#include <dlfcn.h>

#include "quakedef.h"

#include <GL/glx.h>

#include <X11/keysym.h>
#include <X11/cursorfont.h>

#include <X11/extensions/xf86dga.h>
#include <X11/extensions/xf86vmode.h>

#define WARP_WIDTH              320
#define WARP_HEIGHT             200

static Display *dpy = NULL;
static int scrnum;
static Window win;
static GLXContext ctx = NULL;
//for glx1.3
static GLXWindow glwin;

#define KEY_MASK (KeyPressMask | KeyReleaseMask)
#define MOUSE_MASK (ButtonPressMask | ButtonReleaseMask | \
		    PointerMotionMask | ButtonMotionMask )
#define X_MASK (KEY_MASK | MOUSE_MASK | VisibilityChangeMask | StructureNotifyMask )



cvar_t	vid_mode = {"vid_mode","0",false};
 
static qboolean        mouse_avail=true;
static qboolean        mouse_active;
static int   mx, my;
static int	old_mouse_x, old_mouse_y;

static cvar_t in_mouse = {"in_mouse", "1", false};
static cvar_t in_dgamouse = {"in_dgamouse", "1", false};
static cvar_t m_filter = {"m_filter", "0"};

qboolean dgamouse = false;
qboolean vidmode_ext = false;

static int win_x, win_y;

static int scr_width, scr_height;

static XF86VidModeModeInfo **vidmodes;
static int num_vidmodes;
static qboolean vidmode_active = false;

int vm_ver, vm_rev, glx_ver, glx_rev;


/*-----------------------------------------------------------------------*/



/*-----------------------------------------------------------------------*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}

void D_EndDirectRect (int x, int y, int width, int height)
{
}

static int XLateKey(XKeyEvent *ev)
{

	int key;
	char buf[64];
	KeySym keysym;

	key = 0;

	XLookupString(ev, buf, sizeof buf, &keysym, 0);

	switch(keysym)
	{
		case XK_KP_Page_Up:	 
		case XK_Page_Up:	 key = K_PGUP; break;

		case XK_KP_Page_Down: 
		case XK_Page_Down:	 key = K_PGDN; break;

		case XK_KP_Home: 
		case XK_Home:	 key = K_HOME; break;

		case XK_KP_End:  
		case XK_End:	 key = K_END; break;

		case XK_KP_Left: 
		case XK_Left:	 key = K_LEFTARROW; break;

		case XK_KP_Right: 
		case XK_Right:	key = K_RIGHTARROW;		break;

		case XK_KP_Down: 
		case XK_Down:	 key = K_DOWNARROW; break;

		case XK_KP_Up:   
		case XK_Up:		 key = K_UPARROW;	 break;

		case XK_Escape: key = K_ESCAPE;		break;

		case XK_KP_Enter: 
		case XK_Return: key = K_ENTER;		 break;

		case XK_Tab:		key = K_TAB;			 break;

		case XK_F1:		 key = K_F1;				break;

		case XK_F2:		 key = K_F2;				break;

		case XK_F3:		 key = K_F3;				break;

		case XK_F4:		 key = K_F4;				break;

		case XK_F5:		 key = K_F5;				break;

		case XK_F6:		 key = K_F6;				break;

		case XK_F7:		 key = K_F7;				break;

		case XK_F8:		 key = K_F8;				break;

		case XK_F9:		 key = K_F9;				break;

		case XK_F10:		key = K_F10;			 break;

		case XK_F11:		key = K_F11;			 break;

		case XK_F12:		key = K_F12;			 break;

		case XK_BackSpace: key = K_BACKSPACE; break;

		case XK_KP_Delete: 
		case XK_Delete: key = K_DEL; break;

		case XK_Pause:	key = K_PAUSE;		 break;

		case XK_Shift_L:
		case XK_Shift_R:	key = K_SHIFT;		break;

		case XK_Execute: 
		case XK_Control_L: 
		case XK_Control_R:	key = K_CTRL;		 break;

		case XK_Alt_L:	
		case XK_Meta_L: 
		case XK_Alt_R:	
		case XK_Meta_R: key = K_ALT;			break;

		case XK_KP_Begin: key = '5';	break;

		case XK_KP_Insert: 
		case XK_Insert:key = K_INS; break;

		case XK_KP_Multiply: key = '*'; break;
		case XK_KP_Add:  key = '+'; break;
		case XK_KP_Subtract: key = '-'; break;
		case XK_KP_Divide: key = '/'; break;

#if 0
		case 0x021: key = '1';break;/* [!] */
		case 0x040: key = '2';break;/* [@] */
		case 0x023: key = '3';break;/* [#] */
		case 0x024: key = '4';break;/* [$] */
		case 0x025: key = '5';break;/* [%] */
		case 0x05e: key = '6';break;/* [^] */
		case 0x026: key = '7';break;/* [&] */
		case 0x02a: key = '8';break;/* [*] */
		case 0x028: key = '9';;break;/* [(] */
		case 0x029: key = '0';break;/* [)] */
		case 0x05f: key = '-';break;/* [_] */
		case 0x02b: key = '=';break;/* [+] */
		case 0x07c: key = '\'';break;/* [|] */
		case 0x07d: key = '[';break;/* [}] */
		case 0x07b: key = ']';break;/* [{] */
		case 0x022: key = '\'';break;/* ["] */
		case 0x03a: key = ';';break;/* [:] */
		case 0x03f: key = '/';break;/* [?] */
		case 0x03e: key = '.';break;/* [>] */
		case 0x03c: key = ',';break;/* [<] */
#endif

		default:
			key = *(unsigned char*)buf;
			if (key >= 'A' && key <= 'Z')
				key = key - 'A' + 'a';
			break;
	} 

	return key;
}

static Cursor CreateNullCursor(Display *display, Window root)
{
    Pixmap cursormask; 
    XGCValues xgc;
    GC gc;
    XColor dummycolour;
    Cursor cursor;

    cursormask = XCreatePixmap(display, root, 1, 1, 1/*depth*/);
    xgc.function = GXclear;
    gc =  XCreateGC(display, cursormask, GCFunction, &xgc);
    XFillRectangle(display, cursormask, gc, 0, 0, 1, 1);
    dummycolour.pixel = 0;
    dummycolour.red = 0;
    dummycolour.flags = 04;
    cursor = XCreatePixmapCursor(display, cursormask, cursormask,
          &dummycolour,&dummycolour, 0,0);
    XFreePixmap(display,cursormask);
    XFreeGC(display,gc);
    return cursor;
}

static void install_grabs(void)
{

// inviso cursor
	XDefineCursor(dpy, win, CreateNullCursor(dpy, win));

	XGrabPointer(dpy, win,
				 True,
				 0,
				 GrabModeAsync, GrabModeAsync,
				 win,
				 None,
				 CurrentTime);

	if (in_dgamouse.value) {
		int MajorVersion, MinorVersion;

		if (!XF86DGAQueryVersion(dpy, &MajorVersion, &MinorVersion)) { 
			// unable to query, probalby not supported
			Con_Printf( "Failed to detect XF86DGA Mouse\n" );
			in_dgamouse.value = 0;
		} else {
			dgamouse = true;
			XF86DGADirectVideo(dpy, DefaultScreen(dpy), XF86DGADirectMouse);
			XWarpPointer(dpy, None, win, 0, 0, 0, 0, 0, 0);
		}
	} else {
		XWarpPointer(dpy, None, win,
					 0, 0, 0, 0,
					 vid.width / 2, vid.height / 2);
	}

	XGrabKeyboard(dpy, win,
				  False,
				  GrabModeAsync, GrabModeAsync,
				  CurrentTime);

	mouse_active = true;
	Con_Printf("grabs installed.\n");

//	XSync(dpy, True);
}

static void uninstall_grabs(void)
{
	if (!dpy || !win)
		return;

	if (dgamouse) {
		dgamouse = false;
		XF86DGADirectVideo(dpy, DefaultScreen(dpy), 0);
	}

	XUngrabPointer(dpy, CurrentTime);
	XUngrabKeyboard(dpy, CurrentTime);

// inviso cursor
	XUndefineCursor(dpy, win);

	mouse_active = false;
	Con_Printf("grabs uninstalled.\n");
}

static void HandleEvents(void)
{
	XEvent event;
	int b;
	qboolean dowarp = false;
	int mwx = vid.width/2;
	int mwy = vid.height/2;

	if (!dpy)
		return;

	while (XPending(dpy)) {
		XNextEvent(dpy, &event);

		switch (event.type) {
		case KeyPress:
		case KeyRelease:
			Key_Event(XLateKey(&event.xkey), event.type == KeyPress);
			break;

		case MotionNotify:
			if (mouse_active) {
				if (dgamouse) {
					mx += (event.xmotion.x + win_x) * 2;
					my += (event.xmotion.y + win_y) * 2;
				}
				else
				{
					mx += ((int)event.xmotion.x - mwx) * 2;
					my += ((int)event.xmotion.y - mwy) * 2;
					mwx = event.xmotion.x;
					mwy = event.xmotion.y;

					if (mx || my)
						dowarp = true;
				}
			}
			break;

			break;

		case ButtonPress:
			b=-1;
			if (event.xbutton.button == 1)
				b = 0;
			else if (event.xbutton.button == 2)
				b = 2;
			else if (event.xbutton.button == 3)
				b = 1;
			if (b>=0)
				Key_Event(K_MOUSE1 + b, true);
			break;

		case ButtonRelease:
			b=-1;
			if (event.xbutton.button == 1)
				b = 0;
			else if (event.xbutton.button == 2)
				b = 2;
			else if (event.xbutton.button == 3)
				b = 1;
			if (b>=0)
				Key_Event(K_MOUSE1 + b, false);
			break;

		case CreateNotify :
			win_x = event.xcreatewindow.x;
			win_y = event.xcreatewindow.y;
			break;

		case ConfigureNotify :
			win_x = event.xconfigure.x;
			win_y = event.xconfigure.y;
			break;
		}
	}

	if (dowarp) {
		/* move the mouse to the window center again */
		XWarpPointer(dpy, None, win, 0, 0, 0, 0, vid.width / 2, vid.height / 2);
	}

}

static void IN_DeactivateMouse( void )
{
	if (!mouse_avail || !dpy || !win)
		return;

	if (mouse_active) {
		uninstall_grabs();
		mouse_active = false;
	}
}

static void IN_ActivateMouse( void )
{
	//Con_Printf("mouse activeting...");
	if (!mouse_avail || !dpy || !win)
	{
		//Con_Printf("failed.\n");
		return;
	}

	if (!mouse_active) {
		mx = my = 0; // don't spazz
		install_grabs();
		mouse_active = true;
		Con_Printf("done.\n");
	}
	else
	{
		Con_Printf("mouse allready activated.\n");
	}
}


void VID_Shutdown(void)
{
	if (!ctx || !dpy)
		return;
	IN_DeactivateMouse();
	if (dpy) {
		if (ctx)
		{
//FABE: very important! otherwise my x-server crashes after i start quake 3 times!
			glXMakeCurrent(dpy, None, NULL);
			glXDestroyContext(dpy, ctx);
		}
//FABE: ungrab mouse if necessary
		if (mouse_active)
			IN_DeactivateMouse();
		if (vidmode_active)
		{
			XF86VidModeSwitchToMode(dpy, scrnum, vidmodes[0]);
			XFree(vidmodes);
		}
		if (glwin)
		{
			glXDestroyWindow(dpy, glwin);
		}
		if (win)
		{
			XDestroyWindow(dpy, win);
		}
		XCloseDisplay(dpy);
	}
	vidmode_active = false;
	dpy = NULL;
	glwin = 0;
	win = 0;
	ctx = NULL;
}

void signal_handler(int sig)
{
	printf("Received signal %d, exiting...\n", sig);
	Sys_Quit();
	exit(0);
}

void InitSig(void)
{
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGILL, signal_handler);
	signal(SIGTRAP, signal_handler);
	signal(SIGIOT, signal_handler);
	signal(SIGBUS, signal_handler);
	signal(SIGFPE, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGTERM, signal_handler);
}

/*
=================
GL_BeginRendering

=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{

	*x = *y = 0;
	*width = scr_width;
	*height = scr_height;

//    if (!wglMakeCurrent( maindc, baseRC ))
//		Sys_Error ("wglMakeCurrent failed");

//	glViewport (*x, *y, *width, *height);
}


void GL_EndRendering (void)
{
	glFlush();
	glXSwapBuffers(dpy, win);
}

static void Check_Gamma (unsigned char *pal)
{
	float	f, inf;
	unsigned char	palette[768];
	int		i;

	if ((i = COM_CheckParm("-gamma")) == 0) {
		vid_gamma = 0.6; // default to 0.7 on non-3dfx hardware
							 //PENTA: lowered to make things a little brighter
	} else
		vid_gamma = Q_atof(com_argv[i+1]);

	for (i=0 ; i<768 ; i++)
	{
		f = pow ( (pal[i]+1)/256.0 , vid_gamma );
		inf = f*255 + 0.5;
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		palette[i] = inf;
	}

	memcpy (pal, palette, sizeof(palette));
}


void VID_CreateWindow(XVisualInfo *visinfo)
{
	int i;
	int Error, Event;
	qboolean fullscreen = true;
	int width=640, height=480, actualWidth, actualHeight;
	XSetWindowAttributes attr;
	Window root;
	unsigned long mask;

	root = RootWindow(dpy, scrnum);

	if ((i = COM_CheckParm("-window")) != 0)
		fullscreen = false;

	if ((i = COM_CheckParm("-width")) != 0)
		width = atoi(com_argv[i+1]);

	if ((i = COM_CheckParm("-height")) != 0)
		height = atoi(com_argv[i+1]);

	if ((i = COM_CheckParm("-winsize")) != 0){             
		width = atoi(com_argv[i+1]);
		height = atoi(com_argv[i+2]);
        }

        if ((i = COM_CheckParm("-geometry")) != 0){
             char *h = com_argv[i+1];
             char *w = h;
             
             while (*h!='\0'){
                  if (*h=='x' || *h=='X'){
                       *h='\0';                       
                       h++;                                         
                       break;                  
                  }
                  h++;
             }             
             if (*w!='\0' && *h!='\0'){
                  Con_Printf("width %s and height %s\n",w,h);
                  
                  width = atoi (w);
                  height = atoi (h);
             }
        }        

	if ((i = COM_CheckParm("-conwidth")) != 0)
		vid.conwidth = Q_atoi(com_argv[i+1]);
	else
		vid.conwidth = 640;

	vid.conwidth &= 0xfff8; // make it a multiple of eight

	if (vid.conwidth < 320)
		vid.conwidth = 320;

	// pick a conheight that matches with correct aspect
	vid.conheight = vid.conwidth*3 / 4;

	if ((i = COM_CheckParm("-conheight")) != 0)
		vid.conheight = Q_atoi(com_argv[i+1]);
	if (vid.conheight < 200)
		vid.conheight = 200;


	// Get video mode list
	if (!XF86VidModeQueryExtension(dpy, &Event, &Error))
	{
		Con_Printf("XF86VidMode Extenstion not available.\n");
		if(fullscreen)
			Con_Printf("Falling back to window mode.\n");
		vidmode_ext = false;
	}
	else
	{
		XF86VidModeQueryVersion(dpy, &vm_ver, &vm_rev);
		Con_Printf("Using XFree86-VidModeExtension Version %d.%d\n", vm_ver, vm_rev);
		vidmode_ext = true;
	}


	if (vidmode_ext)
	{
		int best_fit, best_dist, dist, x, y;

		XF86VidModeGetAllModeLines(dpy, scrnum, &num_vidmodes, &vidmodes);

		// Are we going fullscreen?  If so, let's change video mode
		if (fullscreen)
		{
			best_dist = 9999999;
			best_fit = -1;

			for (i = 0; i < num_vidmodes; i++)
			{
				if (width > vidmodes[i]->hdisplay ||
					height > vidmodes[i]->vdisplay)
					continue;

				x = width - vidmodes[i]->hdisplay;
				y = height - vidmodes[i]->vdisplay;
				dist = (x * x) + (y * y);
				if (dist < best_dist) {
					best_dist = dist;
					best_fit = i;
				}
			}

			if (best_fit != -1)
			{
				actualWidth = vidmodes[best_fit]->hdisplay;
				actualHeight = vidmodes[best_fit]->vdisplay;

				// change to the mode
				XF86VidModeSwitchToMode(dpy, scrnum, vidmodes[best_fit]);
				vidmode_active = true;

				// Move the viewport to top left
				XF86VidModeSetViewPort(dpy, scrnum, 0, 0);
			}
			else
			{
				Con_Printf("Couldn't get matching video mode.\nFalling back to window mode.\n");
				fullscreen = 0;
			}
		}
	}

	/* window attributes */
	attr.background_pixel = 0;
	attr.border_pixel = 0;
	attr.colormap = XCreateColormap(dpy, root, visinfo->visual, AllocNone);
	attr.event_mask = X_MASK;
	if (vidmode_active)
	{
  mask = CWBackPixel | CWColormap | CWSaveUnder | CWBackingStore |
			CWEventMask | CWOverrideRedirect;
		attr.override_redirect = True;
		attr.backing_store = NotUseful;
		attr.save_under = False;
	}
	else
		mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

	win = XCreateWindow(dpy, root, 0, 0, width, height,
						0, visinfo->depth, InputOutput,
						visinfo->visual, mask, &attr);
	XMapWindow(dpy, win);

	if (vidmode_active) {
		XMoveWindow(dpy, win, 0, 0);
		XRaiseWindow(dpy, win);
		XWarpPointer(dpy, None, win, 0, 0, 0, 0, 0, 0);
//FABE: activate mouse grabbing
		IN_ActivateMouse();
		XFlush(dpy);
		// Move the viewport to top left
		//XF86VidModeSetViewPort(dpy, scrnum, 0, 0);
	}

	XFlush(dpy);

	scr_width = width;
	scr_height = height;

	if (vid.conheight > height)
		vid.conheight = height;
	if (vid.conwidth > width)
		vid.conwidth = width;
	vid.width = vid.conwidth;
	vid.height = vid.conheight;

	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);
	vid.numpages = 2;

}


void VID_LoadGlx12()
{
	XVisualInfo *visual;
	//XEvent event;
	int ConfigAttrib[] =
	{
		GLX_USE_GL,
		GLX_BUFFER_SIZE, 32, //FABE: 32 bit depth buffer
		GLX_LEVEL, 0,
		GLX_RGBA,
		GLX_DOUBLEBUFFER,
		/*GLX_STEREO,*/
		GLX_AUX_BUFFERS, 0,
		/*GLX_RED_SIZE, 8,
		GLX_GREEN_SIZE, 8,
		GLX_BLUE_SIZE, 8,*/
		GLX_ALPHA_SIZE, 8,
		GLX_DEPTH_SIZE, 24,
		GLX_STENCIL_SIZE, 8, //FABE: 8 bit stencil buffer
		/*GLX_ACCUM_RED_SIZE, 0,
		GLX_ACCUM_GREEN_SIZE, 0,
		GLX_ACCUM_BLUE_SIZE, 0,
		GLX_ACCUM_ALPHA_SIZE, 0,*/
		None
	};

	if((visual = glXChooseVisual(dpy, scrnum, ConfigAttrib)) == NULL)
	{
		fprintf(stderr, "Error Unable to get GLX visual.\n");
		exit(1);
	}

	VID_CreateWindow(visual);

	//XMaskEvent(dpy, ExposureMask, &event);

	if((ctx = glXCreateContext(dpy, visual, NULL, True))==NULL)
	{
		fprintf(stderr, "Error unable to get GLX visual.\n");
		exit(1);
	}

	if(glXIsDirect(dpy, ctx))
		Con_Printf("GLX rendering context is direct.\n");
	else
		Con_Printf("GLX rendering context is not direct.\n");

	if(!glXMakeCurrent(dpy, win, ctx))
	{
		fprintf(stderr, "ERROR - Unable to make GLX rendering context current.\n");
		exit(1);
	}

	XFree(visual);
}


void VID_LoadGlx13()
{
	int iElements;
	GLXFBConfig *glxfbConfigs;
	XVisualInfo *visual;
	//XEvent event;
	int FBConfigAttrib[] =
	{
	GLX_FBCONFIG_ID, GLX_DONT_CARE,			/*0x8013*/
	GLX_BUFFER_SIZE, 32,							/*2	 depth of the color buffer */
	GLX_LEVEL, 0,									/*3	 level in plane stacking */
	GLX_DOUBLEBUFFER, True,						/*5	 double buffering supported */
	GLX_STEREO, False,								/*6	 stereo buffering supported */
	GLX_AUX_BUFFERS, GLX_DONT_CARE,			/*7	 number of aux buffers */
	GLX_RED_SIZE, GLX_DONT_CARE,				/*8	 number of red component bits */
	GLX_GREEN_SIZE, GLX_DONT_CARE,				/*9	 number of green component bits */
	GLX_BLUE_SIZE, GLX_DONT_CARE,				/*10	 number of blue component bits */
	GLX_ALPHA_SIZE, GLX_DONT_CARE,				/*11	 number of alpha component bits */
	GLX_DEPTH_SIZE, 24,							/*12	 number of depth bits */
	GLX_STENCIL_SIZE, 8,			/*13	 number of stencil bits */
	GLX_ACCUM_RED_SIZE, GLX_DONT_CARE,			/*14	 number of red accum bits */
	GLX_ACCUM_GREEN_SIZE, GLX_DONT_CARE,		/*15	 number of green accum bits */
	GLX_ACCUM_BLUE_SIZE, GLX_DONT_CARE,		/*16	 number of blue accum bits */
	GLX_ACCUM_ALPHA_SIZE, GLX_DONT_CARE,		/*17	 number of alpha accum bits */
	GLX_RENDER_TYPE, GLX_RGBA_BIT,					/*0x8011*/
	GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,				/*0x8010*/
	GLX_X_RENDERABLE, True,						/*0x8012*/
	GLX_X_VISUAL_TYPE, GLX_DONT_CARE,			/*0x22*/
	GLX_CONFIG_CAVEAT, GLX_DONT_CARE,			/*0x20	 Like visual_info VISUAL_CAVEAT_EXT */
	GLX_TRANSPARENT_TYPE, GLX_NONE,				/*0x23*/
	GLX_TRANSPARENT_INDEX_VALUE, GLX_DONT_CARE,/*0x24*/
	GLX_TRANSPARENT_RED_VALUE, GLX_DONT_CARE,	/*0x25*/
	GLX_TRANSPARENT_GREEN_VALUE, GLX_DONT_CARE,/*0x26*/
	GLX_TRANSPARENT_BLUE_VALUE, GLX_DONT_CARE,	/*0x27*/
	GLX_TRANSPARENT_ALPHA_VALUE, GLX_DONT_CARE,/*0x28*/
	GLX_MAX_PBUFFER_WIDTH, GLX_DONT_CARE,		/*0x8016*/
	GLX_MAX_PBUFFER_HEIGHT, GLX_DONT_CARE,		/*0x8017*/
	GLX_MAX_PBUFFER_PIXELS, GLX_DONT_CARE,		/*0x8018*/
	GLX_VISUAL_ID, GLX_DONT_CARE,				/*0x800B*/
	None
	};

	if((glxfbConfigs = glXChooseFBConfig(dpy, scrnum, FBConfigAttrib, &iElements)) == NULL)
	{
		fprintf(stderr, "Error unable to get GLX FBConfig.\n");
		exit(1);
	}
	visual = glXGetVisualFromFBConfig(dpy, *glxfbConfigs);

	VID_CreateWindow(visual);

	//XMaskEvent(dpy, ExposureMask, &event);

	glwin = glXCreateWindow(dpy, *glxfbConfigs, win, NULL);

	if((ctx = glXCreateNewContext(dpy, *glxfbConfigs, GLX_RGBA_TYPE, NULL, True)) == NULL)
	{
		fprintf(stderr, "Error unable to get GLX rendering context.\n");
		exit(1);
	}

	if(glXIsDirect(dpy, ctx))
		Con_Printf("GLX rendering context is direct.\n");
	else
		Con_Printf("GLX rendering context is not direct.\n");

	if(!glXMakeContextCurrent(dpy, glwin, glwin, ctx))
	{
		fprintf(stderr, "Error unable to make GLX rendering context current.\n");
		exit(1);
	}

	XFree(glxfbConfigs);
	XFree(visual);
}


void VID_Init(unsigned char *palette)
{
	char	gldir[MAX_OSPATH];
	int Event, Error;

	Cvar_RegisterVariable (&vid_mode);
	Cvar_RegisterVariable (&in_mouse);
	Cvar_RegisterVariable (&in_dgamouse);
	Cvar_RegisterVariable (&m_filter);
	//Cvar_RegisterVariable (&gl_ztrick); PENTA: Removed

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "Error couldn't open the X display\n");
		exit(1);
	}

//glx initialization
	if(!glXQueryExtension(dpy, &Error, &Event))
	{
		fprintf(stderr, "Error GLX Extension not available.\n");
		exit(1);
	}
	glXQueryVersion(dpy, &glx_ver, &glx_rev);
	Con_Printf("Using GLX Extension Version %d.%d\n", glx_ver, glx_rev);
        
        /*
	if((glx_ver==1&&glx_rev>=3)||glx_ver>1)
		VID_LoadGlx13();
                else */
		VID_LoadGlx12();

	InitSig(); // trap evil signals

	GL_Init();

	sprintf (gldir, "%s/glquake", com_gamedir);
	Sys_mkdir (gldir);

	VID_SetPalette(palette);

	// Check for 3DFX Extensions and initialize them.
	//VID_Init8bitPalette();

	Con_SafePrintf ("Video mode %dx%d initialized.\n", scr_width, scr_height);

	vid.recalc_refdef = 1;				// force a surface cache flush
}

void Sys_SendKeyEvents(void)
{
	HandleEvents();
}

void Force_CenterView_f (void)
{
	cl.viewangles[PITCH] = 0;
}

void IN_Init(void)
{
}

void IN_Shutdown(void)
{
}

/*
===========
IN_Commands
===========
*/
void IN_Commands (void)
{
	if (!dpy || !win)
		return;

//FABE: we just activate the mouse once at system startup
	/*if (vidmode_active || key_dest == key_game)
		IN_ActivateMouse();
	else
		IN_DeactivateMouse ();*/
}

/*
===========
IN_Move
===========
*/
void IN_MouseMove (usercmd_t *cmd)
{

	if (!mouse_avail)
		return;

	if (m_filter.value)
	{
		mx = (mx + old_mouse_x) * 0.5;
		my = (my + old_mouse_y) * 0.5;
	}
	old_mouse_x = mx;
	old_mouse_y = my;

	mx *= sensitivity.value;
	my *= sensitivity.value;

// add mouse X/Y movement to cmd
	if ( (in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1) ))
		cmd->sidemove += m_side.value * mx;
	else
		cl.viewangles[YAW] -= m_yaw.value * mx;

	if (in_mlook.state & 1)
		V_StopPitchDrift ();
		
	if ( (in_mlook.state & 1) && !(in_strafe.state & 1))
	{
		cl.viewangles[PITCH] += m_pitch.value * my;
		if (cl.viewangles[PITCH] > 80)
			cl.viewangles[PITCH] = 80;
		if (cl.viewangles[PITCH] < -70)
			cl.viewangles[PITCH] = -70;
	}
	else
	{
		if ((in_strafe.state & 1) && noclip_anglehack)
			cmd->upmove -= m_forward.value * my;
		else
			cmd->forwardmove -= m_forward.value * my;
	}
	mx = my = 0;
}

void IN_Move (usercmd_t *cmd)
{
	IN_MouseMove(cmd);
}


