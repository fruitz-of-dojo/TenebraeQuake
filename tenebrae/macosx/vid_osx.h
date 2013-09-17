//----------------------------------------------------------------------------------------------------------------------------
//
// "vid_osx.h" - MacOS X Video driver
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2012 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
// Quakeª is copyrighted by id software	[http://www.idsoftware.com].
//
//----------------------------------------------------------------------------------------------------------------------------

#import "FDFramework/FDFramework.h"

//----------------------------------------------------------------------------------------------------------------------------

#define VID_FADE_DURATION   (1.0f)
#define VID_FONT_WIDTH      (8)
#define	VID_FONT_HEIGHT     (8)

//----------------------------------------------------------------------------------------------------------------------------

extern  cvar_t				_windowed_mouse;

extern  qboolean			gVidDisplayFullscreen;
extern  FDWindow *			gVidWindow;

#if defined (GLQUAKE)

extern  FDDisplay*          gVidDisplay;

#endif /* GLQUAKE */
        
//----------------------------------------------------------------------------------------------------------------------------

extern void	M_Menu_Options_f (void);
extern void	M_Print (int, int, char *);
extern void	M_PrintWhite (int, int, char *);
extern void	M_DrawCharacter (int, int, int);
extern void	M_DrawTransPic (int, int, qpic_t *);
extern void	M_DrawPic (int, int, qpic_t *);

extern BOOL VID_HideFullscreen (BOOL);

//----------------------------------------------------------------------------------------------------------------------------
