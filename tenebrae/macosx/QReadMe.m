//----------------------------------------------------------------------------------------------------------------------------
//
// "QReadMe.m"
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2012 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
//----------------------------------------------------------------------------------------------------------------------------

#import "FDFramework/FDFramework.h"

#import <AppKit/AppKit.h>

//----------------------------------------------------------------------------------------------------------------------------

@interface ReadMe : NSObject <NSApplicationDelegate>
@end

//----------------------------------------------------------------------------------------------------------------------------

@implementation ReadMe

- (void) applicationDidFinishLaunching: (NSNotification*) notification
{
    FD_UNUSED (notification);
    
    // workaround for MacOS X 10.6, help refuses to open. Try to open it as a regular HTML page. 
  
    NSURL*  helpBundleUrl   = [[NSBundle mainBundle] URLForResource: @"TenebraeQuake" withExtension: @"help"];
    BOOL    success         = NO;
    
    if (helpBundleUrl)
    {
        NSBundle*   helpBundle = [NSBundle bundleWithURL: helpBundleUrl];
        
        if (helpBundle)
        {
            NSURL* helpIndex = [helpBundle URLForResource: @"TenebraeQuake" withExtension: @"html"];
            
            if (helpIndex)
            {
                success = [[NSWorkspace sharedWorkspace] openURL: helpIndex];
            }
        }
    }
    
    if (success == NO)
    {
        [NSApp showHelp: nil];
    }
    
    [NSApp terminate: nil];
}

@end

//----------------------------------------------------------------------------------------------------------------------------

int	main (int argc, const char** ppArgv)
{
    FD_UNUSED (argc, ppArgv);
    
    NSAutoreleasePool*  pool    = [[NSAutoreleasePool alloc] init];
    NSApplication*      app     = [NSApplication sharedApplication];
    ReadMe*             readme  = [[ReadMe alloc] init];

    [app setDelegate: readme];
    [app run];
    [readme release];
    [pool release];
    
    return 0;
}

//----------------------------------------------------------------------------------------------------------------------------
