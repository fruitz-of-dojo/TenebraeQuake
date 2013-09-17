//----------------------------------------------------------------------------------------------------------------------------
//
// "QGLDisplaysPanel.m"
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2012 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
//----------------------------------------------------------------------------------------------------------------------------

#import "QGLDisplaysPanel.h"
#import "QShared.h"

#import "FDFramework/FDFramework.h"

//----------------------------------------------------------------------------------------------------------------------------

@implementation QGLDisplaysPanel

- (NSString *) nibName
{
	return @"GLDisplaysPanel";
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) awakeFromNib
{
    if (![mColorsPopUp selectItemWithTag: [[FDPreferences sharedPrefs] integerForKey: QUAKE_PREFS_KEY_GL_COLORS]])
    {
        [mColorsPopUp selectItemAtIndex: 0];
    }
    
    if (![mSamplesPopUp selectItemWithTag: [[FDPreferences sharedPrefs] integerForKey: QUAKE_PREFS_KEY_GL_SAMPLES]])
    {
        [mSamplesPopUp selectItemAtIndex: 0];
    }

    [mForceGenericCheckBox setState: [[FDPreferences sharedPrefs] boolForKey: QUAKE_PREFS_KEY_GL_FORCE_GENERIC]];
    [mFullscreenCheckBox setState: [[FDPreferences sharedPrefs] boolForKey: QUAKE_PREFS_KEY_GL_FULLSCREEN]];
    [mFadeAllCheckBox setState: [[FDPreferences sharedPrefs] boolForKey: QUAKE_PREFS_KEY_GL_FADE_ALL]];
    [mColorsPopUp setEnabled: ([mFullscreenCheckBox state] == NSOnState)];
    
    [self buildDisplayList];
    [self selectDisplayFromDescription: [[FDPreferences sharedPrefs] stringForKey: QUAKE_PREFS_KEY_GL_DISPLAY]];
    
    [self buildDisplayModeList];
    [self selectDisplayModeFromDescription:[[FDPreferences sharedPrefs] stringForKey:QUAKE_PREFS_KEY_GL_DISPLAY_MODE]];
    
    if ([mDisplayPopUp numberOfItems] <= 1)
    {
        [mDisplayPopUp setEnabled: NO];
        [mFadeAllCheckBox setEnabled: NO];
    }
    else
    {
        [mDisplayPopUp setEnabled: YES];
        [mFadeAllCheckBox setEnabled: YES];
    }
    
    [self setTitle: @"Displays"];
}

//----------------------------------------------------------------------------------------------------------------------------

- (NSString*) toolbarIdentifier
{
    return @"Quake Displays ToolbarItem";
}

//----------------------------------------------------------------------------------------------------------------------------

- (NSToolbarItem*) toolbarItem
{
    NSToolbarItem* item = [super toolbarItem];
    
    [item setLabel: @"Displays"];
    [item setPaletteLabel: @"Displays"];
    [item setToolTip: @"Change display settings."];
    [item setImage: [NSImage imageNamed: @"Displays.icns"]];
    
    return item;
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) buildDisplayList
{
    NSString*   key = [[NSString alloc] init];

    [mDisplayPopUp removeAllItems];
    
    for (FDDisplay* display in [FDDisplay displays])
    {
        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle: [display description] action: nil keyEquivalent: key];
        
        [item setRepresentedObject: display];
        [[mDisplayPopUp menu] addItem: [item autorelease]];
    }
    
    [mDisplayPopUp selectItemAtIndex: 0];
    [key release];
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) buildDisplayModeList
{
    const NSInteger     bitsPerPixel    = [mColorsPopUp selectedTag];
    NSString*           key             = [[[NSString alloc] init] autorelease];
    FDDisplay*          display         = [[FDDisplay displays] objectAtIndex: [mDisplayPopUp indexOfSelectedItem]];
    FDDisplayMode*      displayModeOld  = [[mModePopUp itemAtIndex: [mModePopUp indexOfSelectedItem]] representedObject];
    
    [mModePopUp removeAllItems];
    
    for (FDDisplayMode* displayMode in [display displayModes])
    {
        if ([displayMode bitsPerPixel] == bitsPerPixel)
        {
            NSString*   description = [displayMode description];
            NSMenuItem* item        = [[NSMenuItem alloc] initWithTitle: description action: nil keyEquivalent: key];
            
            [item setRepresentedObject: displayMode];
            [[mModePopUp menu] addItem: [item autorelease]];
            
            if ([[displayModeOld description] isEqualToString: description])
            {
                [mModePopUp selectItem: item];
            }
        }
    }
    
    if ([mModePopUp numberOfItems] <= 1)
    {
        [mModePopUp addItemWithTitle: @"not available!"];
        [mModePopUp selectItemAtIndex: 0];
        [mModePopUp setEnabled: NO];
    }
    else
    {
        [mModePopUp setEnabled: YES];
    }

    if ([display hasFSAA] == YES)
    {
        [mSamplesPopUp setEnabled: YES];
    }
    else
    {
        [mSamplesPopUp setEnabled: NO];
        [mSamplesPopUp selectItemAtIndex: 0];
    }
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) selectDisplayFromDescription: (NSString*) description
{
    const NSInteger numItems = [mDisplayPopUp numberOfItems];
    
    for (NSInteger i = 0; i < numItems; ++i)
    {
        if ([[[[mDisplayPopUp itemAtIndex: i] representedObject] description] isEqualToString: description] == YES)
        {
            [mDisplayPopUp selectItemAtIndex: i];
            
            break;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) selectDisplayModeFromDescription: (NSString*) description
{
    const NSInteger numItems = [mModePopUp numberOfItems];
    
    for (NSInteger i = 0; i < numItems; ++i)
    {
        if ([[[[mModePopUp itemAtIndex: i] representedObject] description] isEqualToString: description] == YES)
        {
            [mModePopUp selectItemAtIndex: i];
            
            break;
        }
    }    
}

//----------------------------------------------------------------------------------------------------------------------------

- (IBAction) displayChanged: (id) sender
{
    FD_UNUSED (sender);
    
    [self buildDisplayModeList];
    
    [[FDPreferences sharedPrefs] setObject: mDisplayPopUp forKey: QUAKE_PREFS_KEY_GL_DISPLAY];
    [[FDPreferences sharedPrefs] setObject: mModePopUp forKey: QUAKE_PREFS_KEY_GL_DISPLAY_MODE];
}

//----------------------------------------------------------------------------------------------------------------------------

- (IBAction) fadeChanged: (id) sender
{
    FD_UNUSED (sender);
    
    [[FDPreferences sharedPrefs] setObject: mFadeAllCheckBox forKey: QUAKE_PREFS_KEY_GL_FADE_ALL];
}

//----------------------------------------------------------------------------------------------------------------------------

- (IBAction) modeChanged: (id) sender
{
    FD_UNUSED (sender);
    
    [[FDPreferences sharedPrefs] setObject: mModePopUp forKey: QUAKE_PREFS_KEY_GL_DISPLAY_MODE];
}

//----------------------------------------------------------------------------------------------------------------------------

- (IBAction) colorsChanged: (id) sender
{
    FD_UNUSED (sender);
    
    [self buildDisplayModeList];
    
    [[FDPreferences sharedPrefs] setObject: mColorsPopUp forKey: QUAKE_PREFS_KEY_GL_COLORS];
    [[FDPreferences sharedPrefs] setObject: mModePopUp forKey: QUAKE_PREFS_KEY_GL_DISPLAY_MODE];
}

//----------------------------------------------------------------------------------------------------------------------------

- (IBAction) samplesChanged: (id) sender
{
    FD_UNUSED (sender);
    
    [[FDPreferences sharedPrefs] setObject: mSamplesPopUp forKey: QUAKE_PREFS_KEY_GL_SAMPLES];
}

//----------------------------------------------------------------------------------------------------------------------------

- (IBAction) fullscreenChanged: (id) sender
{
    FD_UNUSED (sender);
    
    [[FDPreferences sharedPrefs] setObject: mFullscreenCheckBox forKey: QUAKE_PREFS_KEY_GL_FULLSCREEN];
    [mColorsPopUp setEnabled: ([mFullscreenCheckBox state] == NSOnState)];
}

//----------------------------------------------------------------------------------------------------------------------------

- (IBAction) forceGenericChanged: (id) sender
{
    FD_UNUSED (sender);
    
    [[FDPreferences sharedPrefs] setObject: mForceGenericCheckBox forKey: QUAKE_PREFS_KEY_GL_FORCE_GENERIC];
}
@end

//----------------------------------------------------------------------------------------------------------------------------
