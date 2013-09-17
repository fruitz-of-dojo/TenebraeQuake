//----------------------------------------------------------------------------------------------------------------------------
//
// "FDBundleVersion.m"
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2012 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
//----------------------------------------------------------------------------------------------------------------------------

#import "FDBundleVersion.h"

//----------------------------------------------------------------------------------------------------------------------------

@implementation FDBundleVersion

//----------------------------------------------------------------------------------------------------------------------------

+ (UInt32) versionOfBundleWithIdentifier: (NSString*) identifier
{
    const char*     pASCIIIdentifier    = [identifier cStringUsingEncoding: NSASCIIStringEncoding];
    CFStringRef		pIdentifier         = CFStringCreateWithCString (NULL, pASCIIIdentifier, kCFStringEncodingASCII);
    CFBundleRef		pBundle             = NULL;
    UInt32			bundleVersion       = 0;

    if (pIdentifier != NULL)
    {
        pBundle = CFBundleGetBundleWithIdentifier (pIdentifier);
        
        CFRelease (pIdentifier);
    }
    
    if (pBundle != NULL)
    {
        bundleVersion = CFBundleGetVersionNumber (pBundle);
        
        CFRelease (pBundle);
    }

    return bundleVersion;
}

//----------------------------------------------------------------------------------------------------------------------------

+ (BOOL) versionOfBundleWithIdentifier: (NSString*) identifier isGreaterThan: (UInt32) version
{
    return [FDBundleVersion versionOfBundleWithIdentifier: identifier] > version;
}

//----------------------------------------------------------------------------------------------------------------------------

+ (BOOL) versionOfBundleWithIdentifier: (NSString*) identifier isGreaterOrEqualTo: (UInt32) version
{
    return [FDBundleVersion versionOfBundleWithIdentifier: identifier] >= version;
}

//----------------------------------------------------------------------------------------------------------------------------

+ (BOOL) versionOfBundleWithIdentifier: (NSString*) identifier isLessThan: (UInt32) version
{
    return [FDBundleVersion versionOfBundleWithIdentifier: identifier] < version;
}

//----------------------------------------------------------------------------------------------------------------------------

+ (BOOL) versionOfBundleWithIdentifier: (NSString*) identifier isLessOrEqualTo: (UInt32) version
{
    return [FDBundleVersion versionOfBundleWithIdentifier: identifier] <= version;
}

//----------------------------------------------------------------------------------------------------------------------------

+ (BOOL) versionOfBundleWithIdentifier: (NSString*) identifier isEqualTo: (UInt32) version
{
    return [FDBundleVersion versionOfBundleWithIdentifier: identifier] == version;
}

//----------------------------------------------------------------------------------------------------------------------------

+ (UInt32) versionOfBundleAtPath: (NSString*) path
{
    CFStringRef pPath         = CFStringCreateWithCString (NULL, [path fileSystemRepresentation], kCFStringEncodingMacRoman);
    UInt32		bundleVersion = 0;
    
    if (pPath != NULL)
    {
        CFURLRef    pBundleURL = CFURLCreateWithFileSystemPath (kCFAllocatorDefault, pPath, kCFURLPOSIXPathStyle, true);
    
        if (pBundleURL != NULL)
        {
            CFBundleRef	 pBundle = CFBundleCreate (kCFAllocatorDefault, pBundleURL);

            if (pBundle != NULL)
            {
                bundleVersion = CFBundleGetVersionNumber (pBundle);
                
                CFRelease (pBundle);
            }
            
            CFRelease (pBundleURL);
        }
        
        CFRelease (pPath);
    }
    
    return bundleVersion;
}

//----------------------------------------------------------------------------------------------------------------------------

+ (BOOL) versionOfBundleAtPath: (NSString*) path isGreaterThan: (UInt32) version
{
    return [FDBundleVersion versionOfBundleAtPath: path] > version;
}

//----------------------------------------------------------------------------------------------------------------------------

+ (BOOL) versionOfBundleAtPath: (NSString*) path isGreaterOrEqualTo: (UInt32) version
{
    return [FDBundleVersion versionOfBundleAtPath: path] >= version;
}

//----------------------------------------------------------------------------------------------------------------------------

+ (BOOL) versionOfBundleAtPath: (NSString*) path isLessThan: (UInt32) version
{
    return [FDBundleVersion versionOfBundleAtPath: path] < version;
}

//----------------------------------------------------------------------------------------------------------------------------

+ (BOOL) versionOfBundleAtPath: (NSString*) path isLessOrEqualTo: (UInt32) version
{
    return [FDBundleVersion versionOfBundleAtPath: path] <= version;
}

//----------------------------------------------------------------------------------------------------------------------------

+ (BOOL) versionOfBundleAtPath: (NSString*) path isEqualTo: (UInt32) version
{
    return [FDBundleVersion versionOfBundleAtPath: path] == version;
}

@end

//----------------------------------------------------------------------------------------------------------------------------
