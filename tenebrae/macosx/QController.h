//----------------------------------------------------------------------------------------------------------------------------
//
// "QController.h"
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2012 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
// Quakeª is copyrighted by id software		[http://www.idsoftware.com].
//
//----------------------------------------------------------------------------------------------------------------------------

#import "QSettingsWindow.h"
#import "QMediaScan.h"

#import <Cocoa/Cocoa.h>

//----------------------------------------------------------------------------------------------------------------------------

@interface QController : NSObject
{
    QSettingsWindow*                mSettingsWindow;
    QMediaScan*                     mMediaScan;
    
    IBOutlet NSMenuItem*            pasteMenuItem;
 
    NSTimer*                        mFrameTimer;
    NSMutableArray*                 mRequestedCommands;
    double							mPrevFrameTime;
    BOOL							mOptionPressed;
    BOOL                            mHostInitialized;
}

+ (void) initialize;
- (id) init;
- (void) dealloc;

- (BOOL) application: (NSApplication *) sender openFile: (NSString *) filePath;
- (NSApplicationTerminateReply) applicationShouldTerminate: (NSApplication*) sender;
- (void) applicationDidFinishLaunching: (NSNotification*) notification;
- (void)applicationWillTerminate: (NSNotification*) notification;
- (void) applicationDidResignActive: (NSNotification*) notification;
- (void) applicationWillHide: (NSNotification *) notification;
- (void) applicationWillUnhide: (NSNotification *) notification;

- (void) setHostInitialized: (BOOL) state;
- (BOOL) hostInitialized;

- (BOOL) validateIdDirectory: (NSString*) basePath;
- (void) selectIdDirectory;

- (void) requestCommand: (NSString *) theCommand;

- (NSString *) mediaFolder;

- (void) connectToServer: (NSPasteboard*) pasteboard userData: (NSString*) data error: (NSString**) error;

- (void) pasteString: (id) sender;
- (IBAction) visitFOD: (id) sender;

- (void) setupDialog: (NSTimer*) timer;
- (void) newGame: (id) sender;
- (void) initGame: (NSNotification*) notification;
- (void) installFrameTimer;
- (void) doFrame: (NSTimer*) timer;

@end

//----------------------------------------------------------------------------------------------------------------------------
