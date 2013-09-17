//----------------------------------------------------------------------------------------------------------------------------
//
// "QApplication.h" - required for event and AppleScript handling.
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2012 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
// Quakeª is copyrighted by id software		[http://www.idsoftware.com].
//
//----------------------------------------------------------------------------------------------------------------------------

#import <Cocoa/Cocoa.h>

//----------------------------------------------------------------------------------------------------------------------------

@interface QApplication : NSApplication

- (void) handleRunCommand: (NSScriptCommand *) theCommand;
- (void) handleConsoleCommand: (NSScriptCommand *) theCommand;

@end

//----------------------------------------------------------------------------------------------------------------------------
