//----------------------------------------------------------------------------------------------------------------------------
//
// "QArgumentsPanel.h"
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2012 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
//----------------------------------------------------------------------------------------------------------------------------

#import "QArgumentEdit.h"
#import "QSettingsPanel.h"

#import <Cocoa/Cocoa.h>

//----------------------------------------------------------------------------------------------------------------------------

@interface QArgumentsPanel : QSettingsPanel <NSTableViewDelegate>
{
@private
    IBOutlet NSTableView*       mTableView;
    IBOutlet NSButton*          mAddButton;
    IBOutlet NSButton*          mRemoveButton;
    IBOutlet NSArrayController* mArguments;
    QArgumentEdit*              mArgumentEdit;
}

- (NSString*) nibName;
- (void) awakeFromNib;

- (void) synchronize;

- (NSString*) toolbarIdentifier;
- (NSToolbarItem*) toolbarItem;

- (IBAction) insertArgument: (id) sender;

@end

//----------------------------------------------------------------------------------------------------------------------------
