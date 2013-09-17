//----------------------------------------------------------------------------------------------------------------------------
//
// "QArgumentEdit.h"
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2012 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
//----------------------------------------------------------------------------------------------------------------------------

#import <Cocoa/Cocoa.h>

//----------------------------------------------------------------------------------------------------------------------------

@interface QArgumentEdit : NSWindowController
{
    IBOutlet NSPopUpButton* mCommandPopup;
    IBOutlet NSTextField*   mArgumentTextField;
    IBOutlet NSTextField*   mDescriptionTextField;
}

- (NSInteger) edit: (NSMutableDictionary*) item modalForWindow: (NSWindow*) window;

- (IBAction) done: (id) cancel;
- (IBAction) cancel: (id) sender;
- (void) selectedArgument: (id) sender;

@end

//----------------------------------------------------------------------------------------------------------------------------
