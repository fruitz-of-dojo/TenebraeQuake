//----------------------------------------------------------------------------------------------------------------------------
//
// "FDBundleVersion.h"
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2012 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
//----------------------------------------------------------------------------------------------------------------------------

#import <Foundation/Foundation.h>

//----------------------------------------------------------------------------------------------------------------------------

@interface FDBundleVersion : NSObject
{
}

+ (UInt32) versionOfBundleWithIdentifier: (NSString*) identifier;
+ (UInt32) versionOfBundleAtPath: (NSString*) path;

+ (BOOL) versionOfBundleWithIdentifier: (NSString*) identifier isGreaterThan: (UInt32) version;
+ (BOOL) versionOfBundleWithIdentifier: (NSString*) identifier isGreaterOrEqualTo: (UInt32) version;
+ (BOOL) versionOfBundleWithIdentifier: (NSString*) identifier isLessThan: (UInt32) version;
+ (BOOL) versionOfBundleWithIdentifier: (NSString*) identifier isLessOrEqualTo: (UInt32) version;
+ (BOOL) versionOfBundleWithIdentifier: (NSString*) identifier isEqualTo: (UInt32) version;

+ (BOOL) versionOfBundleAtPath: (NSString*) path isGreaterThan: (UInt32) version;
+ (BOOL) versionOfBundleAtPath: (NSString*) path isGreaterOrEqualTo: (UInt32) version;
+ (BOOL) versionOfBundleAtPath: (NSString*) path isLessThan: (UInt32) version;
+ (BOOL) versionOfBundleAtPath: (NSString*) path isLessOrEqualTo: (UInt32) version;
+ (BOOL) versionOfBundleAtPath: (NSString*) path isEqualTo: (UInt32) version;

@end

//----------------------------------------------------------------------------------------------------------------------------
