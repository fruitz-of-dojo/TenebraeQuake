//----------------------------------------------------------------------------------------------------------------------------
//
// "QController.m" - the controller.
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2012 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
// Quakeª is copyrighted by id software		[http://www.idsoftware.com].
//
//----------------------------------------------------------------------------------------------------------------------------

#import <Cocoa/Cocoa.h>
#import <fcntl.h>
#import <unistd.h>

#import "quakedef.h"
#import "QController.h"
#import "QArguments.h"
#import "QShared.h"

#import "FDFramework/FDFramework.h"

#import "cd_osx.h"
#import "in_osx.h"
#import "sys_osx.h"
#import "vid_osx.h"

//----------------------------------------------------------------------------------------------------------------------------

extern void     M_Menu_Quit_f (void);

//----------------------------------------------------------------------------------------------------------------------------

@implementation QController

//----------------------------------------------------------------------------------------------------------------------------

+ (void) initialize
{
    FDPreferences*  prefs       = [FDPreferences sharedPrefs];
    NSString*       defaultPath = [[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent];
    
    defaultPath = [defaultPath stringByAppendingPathComponent: QUAKE_PREFS_VALUE_BASE_PATH];    
    
    [prefs registerDefaultObject: defaultPath forKey: QUAKE_PREFS_KEY_BASE_PATH];
    [prefs registerDefaultObject: QUAKE_PREFS_VALUE_AUDIO_PATH forKey: QUAKE_PREFS_KEY_AUDIO_PATH];
    [prefs registerDefaultObject: QUAKE_PREFS_VALUE_ARGUMENTS forKey: QUAKE_PREFS_KEY_ARGUMENTS];
    [prefs registerDefaultObject: QUAKE_PREFS_VALUE_GL_DISPLAY forKey: QUAKE_PREFS_KEY_GL_DISPLAY];
    [prefs registerDefaultObject: QUAKE_PREFS_VALUE_GL_DISPLAY_MODE forKey: QUAKE_PREFS_KEY_GL_DISPLAY_MODE];
    [prefs registerDefaultObject: QUAKE_PREFS_VALUE_GL_COLORS forKey: QUAKE_PREFS_KEY_GL_COLORS];
    [prefs registerDefaultObject: QUAKE_PREFS_VALUE_GL_SAMPLES forKey: QUAKE_PREFS_KEY_GL_SAMPLES];
    [prefs registerDefaultObject: QUAKE_PREFS_VALUE_GL_FORCE_GENERIC forKey: QUAKE_PREFS_KEY_GL_FORCE_GENERIC];
    [prefs registerDefaultObject: QUAKE_PREFS_VALUE_GL_FADE_ALL forKey: QUAKE_PREFS_KEY_GL_FADE_ALL];
    [prefs registerDefaultObject: QUAKE_PREFS_VALUE_GL_FULLSCREEN forKey: QUAKE_PREFS_KEY_GL_FULLSCREEN];
    [prefs registerDefaultObject: QUAKE_PREFS_VALUE_OPTION_KEY forKey: QUAKE_PREFS_KEY_OPTION_KEY];
}

//----------------------------------------------------------------------------------------------------------------------------

- (id) init
{
    self = [super init];
    
    if (self != nil)
    {
        QArguments* arguments = [QArguments sharedArguments];
        
        [arguments setArgumentsFromProccessInfo];
        [arguments setEditable: ([[arguments arguments] count] == 0)];
        
        mRequestedCommands = [[NSMutableArray alloc] init];
    }
    
    return self;
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) dealloc
{
    [mSettingsWindow release];
    [mRequestedCommands release];
    
    [super dealloc];
}

//----------------------------------------------------------------------------------------------------------------------------

- (BOOL) application: (NSApplication *) sender openFile: (NSString *) filePath
{
    FD_UNUSED (sender);
    
    BOOL isValidFile = NO;
    
    if ([[QArguments sharedArguments] isEditable] == YES)
    {
        NSMutableArray* arguments   = [NSMutableArray arrayWithObject: @""];
        BOOL            isDirectory = NO;
        
        if (![[NSFileManager defaultManager] fileExistsAtPath: filePath isDirectory: &isDirectory])
        {
            Sys_Error ("The dragged item is not a valid file!");
        }
        
        if (isDirectory == NO)
        {
            Sys_Error ("The dragged item is not a folder!");
        }
        
        if ([[filePath lastPathComponent] caseInsensitiveCompare: @"hipnotic"] == NSOrderedSame)
        {
            [arguments addObject: @"-hipnotic"];
        }
        else if ([[filePath lastPathComponent] caseInsensitiveCompare: @"rogue"] == NSOrderedSame)
        {
            [arguments addObject: @"-rogue"];            
        }
        else
        {
            [arguments addObject: @"-game"];
            [arguments addObject: [filePath lastPathComponent]];
        }
        
        [[QArguments sharedArguments] setArgumentsFromArray: arguments];
        [[QArguments sharedArguments] setEditable: NO];
        
        isValidFile = YES;
    }
    
    return isValidFile;
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) applicationDidFinishLaunching: (NSNotification*) notification
{
    FD_UNUSED (notification);
    
    FD_DURING
    {
        mOptionPressed  = [FDModifierCheck checkForOptionKey];
        
        [FDHIDManager checkForIncompatibleDevices];
        [self selectIdDirectory];
        
        [NSTimer scheduledTimerWithTimeInterval: 0.5f
                                         target: self
                                       selector: @selector (setupDialog:)
                                       userInfo: nil
                                        repeats: NO];        
    }
    FD_HANDLER;
}

//----------------------------------------------------------------------------------------------------------------------------

- (NSApplicationTerminateReply) applicationShouldTerminate: (NSApplication*) sender
{
    FD_UNUSED (sender);
    
    NSApplicationTerminateReply reply = NSTerminateNow;
    
    if ([self hostInitialized] == YES)
    {
        if ([NSApp isHidden] == YES || [NSApp isActive] == NO)
        {
            [NSApp activateIgnoringOtherApps: YES];
        }
        
        if (gVidDisplayFullscreen == NO && gVidWindow != NULL)
        {
            if ([gVidWindow isMiniaturized] == YES)
            {
                [gVidWindow deminiaturize: NULL];
            }
            
            [gVidWindow orderFront: NULL];
        }
        
#if !defined (QUAKE_WORLD)
        if (cls.state == ca_dedicated)
        {
            Sys_Quit ();
        }
        else
#endif // !QUAKE_WORLD
        {
            M_Menu_Quit_f ();
            
            reply = NSTerminateCancel;
        }
    }
    
    return reply;
}

//----------------------------------------------------------------------------------------------------------------------------

- (void)applicationWillTerminate: (NSNotification*) notification
{
    FD_UNUSED (notification);
    
    [mSettingsWindow release];
    mSettingsWindow = nil;
    
    [[FDPreferences sharedPrefs] synchronize];
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) applicationDidResignActive: (NSNotification*) notification
{
    FD_UNUSED (notification);
    
    if ([self hostInitialized] == YES)
    {
        for (int i = 0; i < K_PAUSE; ++i)
        {
            Key_Event (i, false);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) applicationWillHide: (NSNotification *) notification
{
    FD_UNUSED (notification);
    
    if ([self hostInitialized] == YES)
    {
        S_StopAllSounds (YES);
        VID_HideFullscreen (YES);
        
        if (mFrameTimer != nil)
        {
            [mFrameTimer invalidate];
            mFrameTimer = nil;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) applicationWillUnhide: (NSNotification *) notification
{
    FD_UNUSED (notification);
    
    if ([self hostInitialized] == YES)
    {
        VID_HideFullscreen (NO);
        
        [self installFrameTimer];
    }
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) setHostInitialized: (BOOL) theState
{
    mHostInitialized = theState;
}

//----------------------------------------------------------------------------------------------------------------------------

- (BOOL) hostInitialized
{
    return mHostInitialized;
}


//----------------------------------------------------------------------------------------------------------------------------

- (void) requestCommand: (NSString *) command
{
    if ([self hostInitialized] == YES)
    {
        Cbuf_AddText (va ("%s\n", [command cStringUsingEncoding: NSASCIIStringEncoding]));
    }
    else
    {
        [mRequestedCommands addObject: command];
    }
}

//----------------------------------------------------------------------------------------------------------------------------

- (BOOL) validateIdDirectory: (NSString*) basePath
{
    BOOL success = NO;
    
    if (basePath != nil)
    {
        NSString*   validationFile = [basePath stringByAppendingPathComponent: @"PAK0.PAK"];
        
        if ([[NSFileManager defaultManager] fileExistsAtPath: validationFile] == YES)
        {
            char*   pBaseDir    = (char*) [basePath fileSystemRepresentation];
            size_t  pathLength  = strlen (pBaseDir);
            
            if (pathLength >= 3)
            {
                if (strcasecmp (&(pBaseDir[pathLength - 3]), "ID1") ==0)
                {
                    pBaseDir[pathLength - 3] = '\0';
                    
                    // change working directory to the selected path:
                    success = !chdir (pBaseDir);
                    
                    if (success)
                    {
                        [[NSUserDefaults standardUserDefaults] setObject: basePath forKey: QUAKE_PREFS_KEY_BASE_PATH];
                        [[NSUserDefaults standardUserDefaults] synchronize];
                    }
                    else
                    {
                        NSRunCriticalAlertPanel (@"Can\'t change to the selected path!",
                                                 @"The selection was: \"%@\"", nil, nil, nil, basePath);
                    }
                }
            }
        }
    }
    
    return success;
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) selectIdDirectory
{
    BOOL success = [self validateIdDirectory: [[FDPreferences sharedPrefs] stringForKey: QUAKE_PREFS_KEY_BASE_PATH]];
    
    if (!success)
    {
        NSString* basePath = [[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent];
        
        success = [self validateIdDirectory: [basePath stringByAppendingPathComponent: QUAKE_PREFS_VALUE_BASE_PATH]];
    }
    
    if (!success)
    {
        NSRunInformationalAlertPanel (@"You will now be asked to locate the \"id1\" folder.",
                                      @"This folder is part of the standard installation of "
                                      @"Quake. You will only be asked for it again, if you "
                                      @"change the location of this folder.", nil, nil, nil);
    }
    
    while (!success)
    {
        NSAutoreleasePool*  pool        = [[NSAutoreleasePool alloc] init];
		NSOpenPanel*        openPanel   = [[[NSOpenPanel alloc] init] autorelease];
        
		[openPanel setAllowsMultipleSelection: NO];
		[openPanel setCanChooseFiles: NO];
		[openPanel setCanChooseDirectories: YES];
		[openPanel setTitle: @"Please locate the \"id1\" folder:"];
        
        if ([openPanel runModal] == NSCancelButton)
        {
            [NSApp terminate: nil];
            break;
        }
        
        success = [self validateIdDirectory: [[openPanel directoryURL] path]];
		
		[pool release];
        
        if (!success)
        {
            NSRunInformationalAlertPanel (@"You have not selected the \"id1\" folder.",
                                          @"The \"id1\" folder comes with the shareware or retail "
                                          @"version of Quake and has to contain at least the two "
                                          @"files \"PAK0.PAK\" and \"PAK1.PAK\".", nil, nil, nil);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------

- (NSString*) mediaFolder
{
    BOOL        isDirectory = NO;
    NSString*   path        = [[FDPreferences sharedPrefs] stringForKey: QUAKE_PREFS_KEY_AUDIO_PATH];
    
    if ([path length] > 0)
    {
        const BOOL pathExists = [[NSFileManager defaultManager] fileExistsAtPath: path isDirectory: &isDirectory];
        
        if ((pathExists == NO) || (isDirectory == NO))
        {
            path = nil;
        }
    }
    else
    {
        path = nil;
    }

    return path;
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) connectToServer: (NSPasteboard*) pasteboard userData: (NSString*) data error: (NSString**) error
{
    FD_UNUSED (data);
    
    NSArray*    pasteboardTypes = [pasteboard types];
    
    if ([pasteboardTypes containsObject: NSStringPboardType])
    {
        NSString*   requestedServer = [pasteboard stringForType: NSStringPboardType];
        
        if (requestedServer != nil)
        {
            Cbuf_AddText (va ("connect %s\n", [requestedServer cStringUsingEncoding: NSASCIIStringEncoding]));
            return;
        }
    }
	
    *error = @"Unable to connect to a server: could not find a string on the pasteboard!";
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) pasteString: (id) sender
{
    FD_UNUSED (sender);
    
    extern qboolean		keydown[];
    qboolean			oldCommand = keydown[K_COMMAND];
    
    keydown[K_COMMAND] = true;
    
    Key_Event ('v', true);
    Key_Event ('v', false);
    
    keydown[K_COMMAND] = oldCommand;
}

//----------------------------------------------------------------------------------------------------------------------------

- (IBAction) visitFOD: (id) sender
{
    FD_UNUSED (sender);
    
	[[NSWorkspace sharedWorkspace] openURL: [NSURL URLWithString: FRUITZ_OF_DOJO_URL]];
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) setupDialog: (NSTimer*) timer
{
    FD_UNUSED (timer);
    
    FD_DURING
    {
        const BOOL doCheckOption = [[FDPreferences sharedPrefs] boolForKey: QUAKE_PREFS_KEY_OPTION_KEY];
        
        if ([[QArguments sharedArguments] isEditable] == YES)
        {
            [[QArguments sharedArguments] setArguments: [[FDPreferences sharedPrefs] arrayForKey: QUAKE_PREFS_KEY_ARGUMENTS]];
        }
        
        if (doCheckOption == NO || (doCheckOption == YES && mOptionPressed == YES))
        {
            mSettingsWindow = [[QSettingsWindow alloc] init];
            
            [mSettingsWindow setNewGameAction: @selector (newGame:) target: self];
            [mSettingsWindow showWindow: self];
        }
        else
        {
            // start the game immediately:
            [self newGame: nil];
        }
    }
    FD_HANDLER;
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) newGame: (id) sender
{
    FD_UNUSED (sender);
    
    FD_DURING
    {
        NSString* basePath = [[FDPreferences sharedPrefs] stringForKey: QUAKE_PREFS_KEY_BASE_PATH];
        
        [[QArguments sharedArguments] setEditable: NO];
        
        if ([[QArguments sharedArguments] validateWithBasePath: basePath] == NO)
        {
            Sys_Error ("The mission pack has to be located within the same folder as the \"id1\" folder.");
        }
        
        mSettingsWindow = nil;
        
        [QMediaScan scanFolder: [self mediaFolder] observer: self selector: @selector (initGame:)];
    }
    FD_HANDLER;
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) initGame: (NSNotification*) notification
{
    FD_UNUSED (notification);
    
    quakeparms_t    parameters = { 0 };
    int             customMem  = 0;
    int             argc = 0;
    char**          argv = [[QArguments sharedArguments] cArguments: &argc];
    
    [[FDDebug sharedDebug] setLogHandler: &Con_Printf];
    [[FDDebug sharedDebug] setErrorHandler: &Sys_Error];
    
    [pasteMenuItem setTarget: self];
    [pasteMenuItem setAction: @selector (pasteString:)];

    // prepare host init:
    signal (SIGFPE, SIG_IGN);

    COM_InitArgv (argc, argv);
    
    parameters.argc = com_argc;
    parameters.argv = com_argv;

    parameters.memsize = 16 * 1024 * 1024 * 2;

    customMem = COM_CheckParm ("-mem");
    
    if (customMem)
    {
        parameters.memsize = (int) (Q_atof (com_argv[customMem + 1]) * 1024 * 1024);
    }
    
    parameters.membase = malloc (parameters.memsize);
    parameters.basedir = ".";
    
    fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);
    
    Host_Init (&parameters);
    
    [self setHostInitialized: YES];
    [NSApp setServicesProvider: self];

    const char divider[36] = {  0x80, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
                                0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
                                0x81, 0x81, 0x82, 0x00 };
    
    
    Con_Printf ("\n%s\n", divider);
    Con_Printf (" Quake for MacOS X -- Version %0.2f\n", MACOSX_VERSION);
    Con_Printf ("   Ported by: awe^fruitz of dojo\n");
    Con_Printf ("Visit: http://www.fruitz-of-dojo.de\n");
    Con_Printf ("\n   tiger style kung fu is strong\n");
    Con_Printf ("  but our style is more effective!\n");
    Con_Printf ("\n%s\n", divider);
    
    mPrevFrameTime = Sys_FloatTime ();

    // did we receive an AppleScript command?
    while ([mRequestedCommands count] > 0)
    {
        NSString*   command = [mRequestedCommands objectAtIndex: 0];

        Cbuf_AddText (va ("%s\n", [command cStringUsingEncoding: NSASCIIStringEncoding]));
        
        [mRequestedCommands removeObjectAtIndex: 0];
    }

    [self installFrameTimer];
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) installFrameTimer
{
    if (mFrameTimer == nil)
    {
        mFrameTimer = [NSTimer scheduledTimerWithTimeInterval: 0.0003f
                                                       target: self
                                                     selector: @selector (doFrame:)
                                                     userInfo: nil
                                                      repeats: YES];
        
        if (mFrameTimer != nil)
        {
            [[NSRunLoop currentRunLoop] addTimer: mFrameTimer forMode: NSEventTrackingRunLoopMode];
        }
        else
        {
            Sys_Error ("Failed to install the renderer loop!");
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) doFrame: (NSTimer*) timer
{
    FD_UNUSED (timer);
    
    if ([NSApp isHidden] == NO)
    {
#ifdef QUAKE_WORLD
        
        double  curTime     = Sys_DoubleTime ();
        double  deltaTime   = curTime - mPrevFrameTime;
        
        mPrevFrameTime	= curTime;
        
#else
        double  curTime = Sys_FloatTime();
        double  deltaTime = curTime - mPrevFrameTime;
        
        if (cls.state == ca_dedicated)
        {
            extern int      vcrFile;
            extern qboolean recording;
            
            if (deltaTime < sys_ticrate.value && (vcrFile == -1 || recording))
            {
                usleep (1);
                return;
            }
            
            deltaTime = sys_ticrate.value;
        }
        
        if (deltaTime > sys_ticrate.value * 2)
        {
            mPrevFrameTime = curTime;
        }
        else
        {
            mPrevFrameTime += deltaTime;
        }
        
#endif /* QUAKE_WORLD */

        Host_Frame (deltaTime);
    }
}

@end

//----------------------------------------------------------------------------------------------------------------------------
