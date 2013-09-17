//----------------------------------------------------------------------------------------------------------------------------
//
// "QGLDisplaysPanel.h"
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2012 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
//----------------------------------------------------------------------------------------------------------------------------

#import "QSettingsPanel.h"
#import <Cocoa/Cocoa.h>

//----------------------------------------------------------------------------------------------------------------------------

@interface QGLDisplaysPanel : QSettingsPanel
{
@private
    IBOutlet NSPopUpButton*     mDisplayPopUp;
    IBOutlet NSPopUpButton*     mModePopUp;
    IBOutlet NSPopUpButton*     mColorsPopUp;
    IBOutlet NSPopUpButton*     mSamplesPopUp;
    IBOutlet NSButton*          mForceGenericCheckBox;
    IBOutlet NSButton*          mFullscreenCheckBox;
    IBOutlet NSButton*          mFadeAllCheckBox;
}

- (NSString*) nibName;
- (void) awakeFromNib;

- (NSString*) toolbarIdentifier;
- (NSToolbarItem*) toolbarItem;

- (void) buildDisplayList;
- (void) buildDisplayModeList;

- (void) selectDisplayFromDescription: (NSString*) description;
- (void) selectDisplayModeFromDescription: (NSString*) description;

- (IBAction) displayChanged: (id) sender;
- (IBAction) modeChanged: (id) sender;
- (IBAction) colorsChanged: (id) sender;
- (IBAction) samplesChanged: (id) sender;
- (IBAction) forceGenericChanged: (id) sender;
- (IBAction) fullscreenChanged: (id) sender;
- (IBAction) fadeChanged: (id) sender;

@end

//----------------------------------------------------------------------------------------------------------------------------
