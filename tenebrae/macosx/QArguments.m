//----------------------------------------------------------------------------------------------------------------------------
//
// "QArguments.m"
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2012 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
//----------------------------------------------------------------------------------------------------------------------------

#import "QArguments.h"

#import "FDFramework/FDPreferences.h"

//----------------------------------------------------------------------------------------------------------------------------

static dispatch_once_t  sQArgumentsPredicate    = 0;
static QArguments*      sQArgumentsShared       = nil;

//----------------------------------------------------------------------------------------------------------------------------

@interface QArguments ()

- (id) initShared;

- (NSArray*) tokenizeString: (NSString*) string;
- (NSMutableArray*) tokenizeArguments: (NSArray*) arguments fromIndex: (NSInteger) startIndex;
- (BOOL) validateGame: (NSString*) gamePath withBasePath: (NSString*) basePath;

@end

//----------------------------------------------------------------------------------------------------------------------------

@implementation QArguments

+ (QArguments*) sharedArguments
{
    dispatch_once (&sQArgumentsPredicate, ^{ sQArgumentsShared = [[QArguments alloc] initShared]; });
    
    return sQArgumentsShared;
}

//----------------------------------------------------------------------------------------------------------------------------

- (id) initShared
{
    self = [super init];
           
    if (self != nil)
    {
        [self setEditable: YES];
    }
    
    return self;
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) dealloc
{
    [mArguments release];
    
    [super dealloc];
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) setEditable: (BOOL) editable
{
    mIsEditable = editable;
}

//----------------------------------------------------------------------------------------------------------------------------

- (BOOL) isEditable;
{
    return mIsEditable;
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) setArguments: (NSArray*) arguments
{
    [mArguments release];
    mArguments = [arguments mutableCopy];
}

//----------------------------------------------------------------------------------------------------------------------------

- (NSMutableArray*) arguments
{
    return mArguments;
}

//----------------------------------------------------------------------------------------------------------------------------

- (NSArray*) tokenizeString: (NSString*) string
{
    NSCharacterSet* quote   = [NSCharacterSet characterSetWithCharactersInString: @"\""];
    NSArray*        split   = [string componentsSeparatedByCharactersInSet: [NSCharacterSet whitespaceCharacterSet]];
    NSMutableArray* tokens  = [[[NSMutableArray alloc] init] autorelease];
    
    for (NSUInteger i = 0; i < [split count]; ++i)
    {
        NSString* token = [split objectAtIndex: i];
        
        if ([token hasPrefix: @"\""] ==YES)
        {
            token = [NSString string];
            
            for (; i < [split count]; ++i)
            {
                if ([token length])
                {
                    token = [NSString stringWithFormat: @"%@ %@", token, [split objectAtIndex: i]];
                }
                else
                {
                    token = [split objectAtIndex: i];
                }
                
                if ([token hasSuffix: @"\""] ==YES)
                {
                    break;
                }
            }
            
            token = [token stringByTrimmingCharactersInSet: quote];
        }
        
        [tokens addObject: token];
    }
        
    return tokens;
}

//----------------------------------------------------------------------------------------------------------------------------

- (NSMutableArray*) tokenizeArguments: (NSArray*) arguments fromIndex: (NSInteger) startIndex
{
    NSMutableArray* dicts       = [[[NSMutableArray alloc] init] autorelease];
    NSNumber*       onState     = [NSNumber numberWithBool: YES];
    
    for (NSUInteger i = startIndex; i < [arguments count]; ++i)
    {
        NSString*   arg = [arguments objectAtIndex: i];
        
        if ([arg rangeOfCharacterFromSet: [NSCharacterSet characterSetWithCharactersInString: @"\""]].location != NSNotFound)
        {
            arg = [NSString stringWithFormat: @"\"%@\"", arg];
        }

        if (([arg length] > 0) && ([arg characterAtIndex: 0] == '-'))
        {
            [dicts addObject: [NSMutableDictionary dictionaryWithObjectsAndKeys: onState, @"state", arg, @"value", nil]];
        }
        else if ([dicts count] > 0)
        {
            NSUInteger              index   = [dicts count] - 1;
            NSMutableDictionary*    dict    = [dicts objectAtIndex: index];
            
            [dict setObject: [NSString stringWithFormat: @"%@ %@", [dict objectForKey: @"value"], arg] forKey: @"value"];
            [dicts replaceObjectAtIndex: index withObject: dict]; 
        }
    }
    
    [dicts filterUsingPredicate: [NSPredicate predicateWithFormat: @"not value like '-psn_*'"]];
    [dicts filterUsingPredicate: [NSPredicate predicateWithFormat: @"not value like '-NSDocumentRevisionsDebugMode*'"]];
    
    return dicts;
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) setArgumentsFromArray: (NSArray*) array
{
    [self setArguments: [self tokenizeArguments: array fromIndex: 1]];
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) setArgumentsFromString: (NSString*) string
{
    [self setArguments: [self tokenizeArguments: [self tokenizeString: string] fromIndex: 0]];
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) setArgumentsFromProccessInfo
{
    NSArray*        arguments = [[NSProcessInfo processInfo] arguments];
    
    [self setArgumentsFromArray: arguments];
}

//----------------------------------------------------------------------------------------------------------------------------

- (BOOL) validateGame: (NSString*) gamePath withBasePath: (NSString*) basePath
{
    BOOL        isDirectory = NO;
    NSString*   path        = [[basePath stringByDeletingLastPathComponent] stringByAppendingPathComponent: gamePath];
    BOOL        pathExists  = [[NSFileManager defaultManager] fileExistsAtPath: path isDirectory: &isDirectory];
    BOOL        isValid     = ((pathExists == YES) && (isDirectory == YES));
    
    if (!isValid)
    {        
        NSString*   outName  = nil;
        NSArray*    outArray = nil;
        
        isValid = [path completePathIntoString: &outName caseSensitive: NO matchesIntoArray:&outArray filterTypes: nil] > 0;
        
        if (isValid)
        {
            pathExists  = [[NSFileManager defaultManager] fileExistsAtPath: outName isDirectory: &isDirectory];
            isValid     = ((pathExists == YES) && (isDirectory == YES));
        }
    }
    
    return isValid;
}

//----------------------------------------------------------------------------------------------------------------------------

- (BOOL) validateWithBasePath: (NSString*) basePath
{
    BOOL isValid = YES;
    
    for (NSDictionary* dict in mArguments)
    {
        NSArray*    arguments       = [self tokenizeString: [dict objectForKey: @"value"]];
        NSUInteger  numArguments    = [arguments count];
        
        for (NSUInteger i = 0; i < numArguments; ++i)
        {
            NSString* argument = [arguments objectAtIndex: i];
            NSString* gamePath = nil;
            
            if ([argument isEqualToString: @"-game"] == YES)
            {
                isValid = ((++i) < numArguments);
                
                if (isValid)
                {
                    gamePath = [arguments objectAtIndex: i];
                }
            }
            else if ([argument isEqualToString: @"-hipnotic"] == YES)
            {
                gamePath = @"Hipnotic";
            }
            else if ([argument isEqualToString: @"-rogue"] == YES)
            {
                gamePath = @"rogue";
            }
        
            if (gamePath != nil)
            {
                isValid = [self validateGame: gamePath withBasePath: basePath];
            }
            
            if (!isValid)
            {
                break;
            }
        }
    }
    
    return isValid;
}

//----------------------------------------------------------------------------------------------------------------------------

- (char**) cArguments: (int*) pCount
{
    NSMutableArray* arguments   = [[NSMutableArray alloc] initWithObjects: [NSString string], nil];
    char**          ppArgs      = NULL;
    
    for (NSDictionary* dict in [self arguments])
    {
        if ([[dict objectForKey: @"state"] boolValue] == YES)
        {
            [arguments addObjectsFromArray: [self tokenizeString: [dict objectForKey: @"value"]]];
        }
    }
    
    *pCount = 0;
    ppArgs  = (char**) malloc (sizeof (char*) * [arguments count]);
    
    if (ppArgs != NULL)
    {
        NSUInteger i = 0;
        
        for (i = 0; i < [arguments count]; ++i)
        {
            char**      ppDst   = &(ppArgs[i]);
            const char* pSrc    = [[arguments objectAtIndex: i] cStringUsingEncoding: NSASCIIStringEncoding];
            
            *ppDst = (char*) malloc (strlen (pSrc) + 1);
            
            if (ppArgs[i] != NULL)
            {
                strcpy (*ppDst, pSrc);
            }
            else
            {
                break;
            }
        }
        
        *pCount = (int) i;
    }
    
    return ppArgs;
}

@end

//----------------------------------------------------------------------------------------------------------------------------
