//----------------------------------------------------------------------------------------------------------------------------
//
// "QMediaScan.m"
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2012 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
//----------------------------------------------------------------------------------------------------------------------------

#import "QMediaScan.h"
#import "QController.h"
#import "QSoundPanel.h"

#import "cd_osx.h"

#import "FDFramework/FDFramework.h"

//----------------------------------------------------------------------------------------------------------------------------

@interface QMediaScan ()

- (id) init;
- (id) initWithFolder: (NSString*) folder observer: (id) observer selector: (SEL) selector;

- (void) scanComplete: (NSNotification*) notification;
- (void) scanThread: (id) sender;

@end

//----------------------------------------------------------------------------------------------------------------------------

@implementation QMediaScan

+ (BOOL) scanFolder: (NSString*) folder observer: (id) observer selector: (SEL) selector
{
    return [[QMediaScan alloc] initWithFolder: (NSString*) folder observer: (id) observer selector: selector] != nil;
}

//----------------------------------------------------------------------------------------------------------------------------

- (id) init;
{
    self = [super init];
    
	if (self != nil)
	{
        [self release];
        self = nil;
    }
    
    return self;
}

//----------------------------------------------------------------------------------------------------------------------------

- (id) initWithFolder: (NSString*) folder observer: (id) observer selector: (SEL) selector
{
    self = [super init];
    
	if (self != nil)
	{
        mStopConditionLock  = [[NSConditionLock alloc] initWithCondition: 0];
        mFolder             = [folder retain];
        mObserver           = observer;
        mSelector           = selector;
        
        [self showWindow: nil];
        [mProgressIndicator startAnimation: nil];
        
        [[NSDistributedNotificationCenter defaultCenter] addObserver: self
                                                            selector: @selector (scanComplete:)
                                                                name: @"QMediaScanIsComplete"
                                                              object: NULL];
        
        [NSThread detachNewThreadSelector: @selector (scanThread:) toTarget: self withObject: self];
    }
    
    return self;
}

//----------------------------------------------------------------------------------------------------------------------------

- (NSString*) windowNibName
{
	return @"MediaScan";
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) awakeFromNib
{
    if (mFolder != nil)
    {
        [mTextField setStringValue: @"Scanning folder for audio files..."];
    }
    else
    {
        [mTextField setStringValue: @"Scanning AudioCDs..."];
    }
    
    [[self window] setTitle: [[NSRunningApplication currentApplication] localizedName]];
    [[self window] center];
}


//----------------------------------------------------------------------------------------------------------------------------

- (void) dealloc
{
    [[NSDistributedNotificationCenter defaultCenter] removeObserver: self];
    
    [mFolder release];
    [mStopConditionLock release];
    
    [super dealloc];
}

//----------------------------------------------------------------------------------------------------------------------------

- (IBAction) stop: (id) sender
{
    [mStopConditionLock lock];
    [mStopConditionLock unlockWithCondition: 1];
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) scanComplete: (NSNotification*) notification
{
    [mProgressIndicator stopAnimation: nil];
    [self close];
    
    if ([mObserver respondsToSelector: mSelector] == YES)
    {
        [mObserver performSelector: mSelector withObject: nil];
    }
    
    [self autorelease];
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) scanThread: (id) sender
{
    FD_UNUSED (sender);
    
    FD_DURING
    {
        CDAudio_ScanForMedia (mFolder, mStopConditionLock);
        
        [[NSDistributedNotificationCenter defaultCenter] postNotificationName: @"QMediaScanIsComplete" object: nil];
        
        [NSThread exit];
    }
    FD_HANDLER;
}

@end

//----------------------------------------------------------------------------------------------------------------------------
