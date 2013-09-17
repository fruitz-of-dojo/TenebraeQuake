//----------------------------------------------------------------------------------------------------------------------------
//
// "sys_osx.m" - MacOS X system functions.
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2012 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
// Quakeª is copyrighted by id software	[http://www.idsoftware.com].
//
// Version History:
// v1.2.0: Rewrote support for case sensitive file systems.
//         Moved all dialog related code out.
// v1.0.9: Revised the event handling model [Uses now a NSTimer and a subclassed NSApplication instance].
// v1.0.8: Extended the GLQuake and GLQuakeWorld startup dialog with "FSAA Samples" option.
//	       Added dialog to Quake and QuakeWorld to enter and store command line parameters.
//	       Added option to show the startup dialog only when pressing the Option key.
// v1.0.7: From now on you will only be asked to locate the "id1" folder, if the application is not
//         installed inside the same folder which holds the "id1" folder.
// v1.0.6: Fixed cursor vsisibility on system error.
//         Fixed loss of mouse control after CMD-TABing with the software renderer.
//         Fixed a glitch with the Quake/QuakeWorld options menu [see "menu.c"].
//         Reworked keypad code [is now hardware dependent] to avoid "sticky" keys.
//	       Moved fixing of linebreaks to "cmd.c".
// v1.0.5: Added support for numeric keypad.
//         Added support for paste [CMD-V and "Edit/Paste" menu].
//	       Added support for up to 5 mouse buttons.
//         Added "Connect To Server" service.
//	       Added support for CMD-M, CMD-H and CMD-Q.
//         Added web link to the help menu.
//	       Fixed keyboard repeat after application quit.
// v1.0.4: Changed all vsprintf () calls to vsnprintf (). vsnprintf () is cleaner.
// v1.0.3: Fixed the broken "drag MOD onto Quake icon" method [blame me].
//	       Enabled Num. Lock Key.
// v1.0.2: Reworked settings dialog.
//	       Reenabled keyboard repeat and mousescaling.
//         Some internal changes.
// v1.0.1: Added support for GLQuake.
//	       Obscure characters within the base pathname [like 'Ä'...] are now allowed.
//	       Better support for case sensitive file systems.
//	       FIX: "mkdir" commmand [for QuakeWorld].
//	       FIX: The "id1" folder had to be lower case. Can now be upper or lower case.
// v1.0:   Initial release.
//
//----------------------------------------------------------------------------------------------------------------------------

#import <AppKit/AppKit.h>
#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <mach/mach_time.h>

#import <dlfcn.h>
#import <unistd.h>
#import <signal.h>
#import <stdlib.h>
#import <limits.h>
#import <sys/time.h>
#import <sys/types.h>
#import <unistd.h>
#import <fcntl.h>
#import <stdarg.h>
#import <stdio.h>
#import <sys/ipc.h>
#import <sys/shm.h>
#import <sys/stat.h>
#import <string.h>
#import <ctype.h>
#import <sys/wait.h>
#import <sys/mman.h>
#import <sys/param.h>
#import <errno.h>

#import "quakedef.h"
#import "QApplication.h"
#import "QController.h"
#import "QShared.h"

#import "in_osx.h"
#import "sys_osx.h"
#import "vid_osx.h"

#import "FDFramework/FDFramework.h"

//----------------------------------------------------------------------------------------------------------------------------

#define SYS_QWSV_BASE_PATH			"."
#define	SYS_STRING_SIZE				(1024)

//----------------------------------------------------------------------------------------------------------------------------

typedef struct	{
    NSString *					filter;
    NSString *					path;
    NSDirectoryEnumerator *		enumerator;
} sys_findenum_t;

//----------------------------------------------------------------------------------------------------------------------------

qboolean			isDedicated;

//----------------------------------------------------------------------------------------------------------------------------

static const char*  Sys_FixFileNameCase (const char*);
void                Sys_Warn (char *theWarning, ...);
double              Sys_DoubleTime (void);
int                 main (int, const char **);

//----------------------------------------------------------------------------------------------------------------------------

const char*	Sys_FixFileNameCase (const char* pPath)
{
    BOOL                isDirectory = NO;
    NSFileManager*      fileManager = [NSFileManager defaultManager];
    NSString*           path        = [fileManager stringWithFileSystemRepresentation: pPath length: strlen (pPath)];
    BOOL                pathExists  = [fileManager fileExistsAtPath: path isDirectory: &isDirectory];
    
    if ((pathExists == NO) || (isDirectory == YES))
    {        
        NSString*   outName  = nil;
        NSArray*    outArray = nil;
        
        if ([path completePathIntoString: &outName caseSensitive: NO matchesIntoArray: &outArray filterTypes: nil] > 0)
        {
            pPath = [outName fileSystemRepresentation];
        }
    }
    
    return pPath;
}

//----------------------------------------------------------------------------------------------------------------------------

int	Sys_FileOpenRead (char* pPath, int* pHandle)
{
    struct stat fileStat = { 0 };

    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    
    *pHandle = open (Sys_FixFileNameCase (pPath), O_RDONLY, 0666);
    
    [pool release];
    
    if (*pHandle == -1)
    {
        return -1;
    }
        
    if (fstat (*pHandle, &fileStat) == -1)
    {
        Sys_Error ("Can\'t open file \"%s\", reason: \"%s\".", pPath, strerror (errno));
    }
        
    return (int) fileStat.st_size;
}

//----------------------------------------------------------------------------------------------------------------------------

int	Sys_FileOpenWrite (char* pPath)
{
    int handle = -1;

    umask (0);
    
    handle = open (pPath, O_RDWR | O_CREAT | O_TRUNC, 0666);
    
    if (handle == -1)
    {
        Sys_Error ("Can\'t open file \"%s\" for writing, reason: \"%s\".", pPath, strerror (errno));
    }
        
    return handle;
}

//----------------------------------------------------------------------------------------------------------------------------

void	Sys_FileClose (int handle)
{
    close (handle);
}

//----------------------------------------------------------------------------------------------------------------------------

void	Sys_FileSeek (int handle, int position)
{
    lseek (handle, position, SEEK_SET);
}

//----------------------------------------------------------------------------------------------------------------------------

int	Sys_FileRead (int handle, void* pDest, SInt numBytes)
{
    return (int) read (handle, pDest, numBytes);
}

//----------------------------------------------------------------------------------------------------------------------------

int	Sys_FileWrite (int handle, void* pData, SInt numBytes)
{
    return (int) write (handle, pData, numBytes);
}

//----------------------------------------------------------------------------------------------------------------------------

int	Sys_FileTime (char* pPath)
{
    struct stat fileStat = { 0 };
    int         result   = stat (pPath, &fileStat);

    if (result != -1)
    {
        result = (int) fileStat.st_mtime;
    }
    
    return result;
}

//----------------------------------------------------------------------------------------------------------------------------

void	Sys_mkdir (char* pPath)
{
    if (mkdir (pPath, 0777) == -1)
    {
        if (errno != EEXIST)
        {
            Sys_Error ("\"mkdir %s\" failed, reason: \"%s\".", pPath, strerror (errno));
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------

dirdata_t *	Sys_Findfirst (char* thePath, char* pFilter, dirdata_t* pDirData)
{
    dirdata_t *				pFirstDirData   = NULL;
    NSString *				path			= nil;
	NSString*               filterUpper     = nil;
	NSString*				file            = nil;
    NSDirectoryEnumerator*	dirEnum         = nil;
    
    path        = [[NSFileManager defaultManager] stringWithFileSystemRepresentation: thePath length: strlen(thePath)];
    filterUpper = [[[NSString stringWithCString: pFilter encoding: NSASCIIStringEncoding] pathExtension] uppercaseString];
    dirEnum     = [[NSFileManager defaultManager] enumeratorAtPath: path];
    
    while ((file = [dirEnum nextObject]) != nil)
    {
        // does the file match our extension filter?
        if ([[[file pathExtension] uppercaseString] isEqualToString: filterUpper])
        {
            sys_findenum_t *	pInternalData = (sys_findenum_t *) Z_Malloc (sizeof (sys_findenum_t));
            
            // prepare the internal data:
            pInternalData->path         = [path retain];
            pInternalData->filter		= [filterUpper retain];
            pInternalData->enumerator	= [dirEnum retain];
            pDirData->internal          = pInternalData;
            
            [[path stringByAppendingPathComponent: file] getFileSystemRepresentation: pDirData->entry
                                                                               maxLength: MAX_OSPATH];
            
            pFirstDirData = pDirData;
            
            break;
        }
    }
    
    return pFirstDirData;
}

//----------------------------------------------------------------------------------------------------------------------------

dirdata_t*	Sys_Findnext (dirdata_t* pDirData)
{
    dirdata_t *	pNextDirData = NULL;
    
    if ((pDirData != NULL) && (pDirData->internal != NULL))
    {
        sys_findenum_t* pInternalData  = pDirData->internal;
        NSString*		pFile          = nil;
        
        while ((pFile = [pInternalData->enumerator nextObject]) != NULL)
        {
            if ([[[pFile pathExtension] uppercaseString] isEqualToString: pInternalData->filter])
            {
                pFile = [pInternalData->path stringByAppendingPathComponent: pFile];
                
                [pFile getFileSystemRepresentation: pDirData->entry maxLength: MAX_OSPATH];

                pNextDirData = pDirData;
                break;
            }
        }
        
        if ((pNextDirData == NULL) && (pInternalData != NULL))
        {
            [pInternalData->enumerator release];
            [pInternalData->filter release];
            [pInternalData->path release];
            
            Z_Free (pInternalData);
        }
    }
    
    return pNextDirData;
}

//----------------------------------------------------------------------------------------------------------------------------

void	Sys_MakeCodeWriteable (unsigned long startAddress, unsigned long len)
{
    const int           pageSize    = getpagesize();
    const unsigned long address     = (startAddress & ~(pageSize - 1)) - pageSize;
    
    if (mprotect ((char*) address, len + startAddress - address + pageSize, 7) < 0)
    {
        Sys_Error ("Memory protection change failed!\n");
    }
}

//----------------------------------------------------------------------------------------------------------------------------

void *	Sys_GetProcAddress (const char* pName, qboolean isSafeMode)
{
    void*   pSymbol = dlsym (RTLD_DEFAULT, pName);
    
    if ((isSafeMode == YES) && (pSymbol == NULL))
    {
        Sys_Error ("Failed to import a required function!\n");
    }
    
    return pSymbol;
}

//----------------------------------------------------------------------------------------------------------------------------

void	Sys_Error (const char* pFormat, ...)
{
    va_list     argPtr;
    char        buffer[SYS_STRING_SIZE];

    fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
    
    va_start (argPtr, pFormat);
    vsnprintf (buffer, SYS_STRING_SIZE, pFormat, argPtr);
    va_end (argPtr);

    Host_Shutdown();
    [[NSApp delegate] setHostInitialized: NO];
    
    NSString* msg = [NSString stringWithCString: buffer encoding: NSASCIIStringEncoding];
    
    NSRunCriticalAlertPanel (@"An error has occured:", msg, nil, nil, nil);
    NSLog (@"An error has occured: %@\n", msg);

    exit (EXIT_FAILURE);
}

//----------------------------------------------------------------------------------------------------------------------------

void	Sys_Warn (char* pFormat, ...)
{
    va_list     argPtr;
    char        buffer[SYS_STRING_SIZE];
    
    va_start (argPtr, pFormat);
    vsnprintf (buffer, SYS_STRING_SIZE, pFormat, argPtr);
    va_end (argPtr);

    NSLog (@"Warning: %s\n", buffer);
} 

//----------------------------------------------------------------------------------------------------------------------------

void	Sys_Printf (char* pFormat, ...)
{
    FD_UNUSED (pFormat);
}

//----------------------------------------------------------------------------------------------------------------------------

void	Sys_Quit (void)
{
    extern cvar_t	gl_fsaa;
    int             numSamples = gl_fsaa.value;

    // save the FSAA setting again [for Radeon users]:
    if ((numSamples != 4) && (numSamples != 8))
    {
        numSamples = 0;
    }
    
    [[FDPreferences sharedPrefs] setObject: [NSNumber numberWithInt: numSamples] forKey: QUAKE_PREFS_KEY_GL_SAMPLES];
    [[FDPreferences sharedPrefs] synchronize];

    // shutdown host:
    Host_Shutdown ();
    [[NSApp delegate] setHostInitialized: NO];
    fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
    fflush (stdout);

    exit (EXIT_SUCCESS);
}

//----------------------------------------------------------------------------------------------------------------------------

double	Sys_FloatTime (void)
{
    static uint64_t startTime   = 0;
    static double   scale       = 0.0;
    const uint64_t  time        = mach_absolute_time();
    
    if (startTime == 0)
    {
        mach_timebase_info_data_t   info = { 0 };
        
        if (mach_timebase_info (&info) != 0)
        {
            Sys_Error ("Failed to read timebase!");
        }
        
        scale       = 1e-9 * ((double) info.numer) / ((double) info.denom);
        startTime   = time;
    }

    return (double) (time - startTime) * scale;
}

//----------------------------------------------------------------------------------------------------------------------------

void	Sys_Strtime (char* pBuffer)
{
    struct tm*      pTime       = NULL;
    static time_t   timeStruct  = { 0 };
    
    time (&timeStruct);
    pTime = localtime (&timeStruct);
    
    snprintf (pBuffer, 20, "%02d:%02d:%02d", pTime->tm_hour, pTime->tm_min, pTime->tm_sec);
}

//----------------------------------------------------------------------------------------------------------------------------

char*	Sys_ConsoleInput (void)
{
    return NULL;
}

//----------------------------------------------------------------------------------------------------------------------------

#if !id386

void	Sys_HighFPPrecision (void)
{
}

//----------------------------------------------------------------------------------------------------------------------------

void	Sys_LowFPPrecision (void)
{
}

#endif // !id386

//----------------------------------------------------------------------------------------------------------------------------

double	Sys_DoubleTime (void)
{
    return Sys_FloatTime ();
}

//----------------------------------------------------------------------------------------------------------------------------

void	Sys_DebugLog (char* pPath, char* pFormat, ...)
{
    const int fileDesc = open (pPath, O_WRONLY | O_CREAT | O_APPEND, 0666);
    
    if (fileDesc != -1)
    {
        va_list args; 
        char 	text[SYS_STRING_SIZE];

        va_start (args, pFormat);
        vsnprintf (text, SYS_STRING_SIZE, pFormat, args);
        va_end (args);

        write (fileDesc, text, strlen (text));
        close (fileDesc);
    }
}

//----------------------------------------------------------------------------------------------------------------------------

char*	Sys_GetClipboardData (void)
{
    NSPasteboard*   pasteboard  = [NSPasteboard generalPasteboard];
    NSArray*        types       = [pasteboard types];
    
    if ([types containsObject: NSStringPboardType])
    {
        NSString* clipboardString = [pasteboard stringForType: NSStringPboardType];
        
        if (clipboardString != NULL && [clipboardString length] > 0)
        {
            return strdup ([clipboardString cStringUsingEncoding: NSASCIIStringEncoding]);
        }
    }
    return NULL;
}

//----------------------------------------------------------------------------------------------------------------------------

void	Sys_SendKeyEvents (void)
{
    // will only be called if in modal loop
    NSAutoreleasePool*  pool    = [[NSAutoreleasePool alloc] init];
    NSEvent*            event   = [NSApp nextEventMatchingMask: NSAnyEventMask
                                                     untilDate: [NSDate distantPast]
                                                        inMode: NSDefaultRunLoopMode
                                                       dequeue: YES];
    
    [NSApp sendEvent: event];
    [pool release];

    IN_SendKeyEvents();
}

//----------------------------------------------------------------------------------------------------------------------------

int	main (int argc, const char** pArgv)
{
    NSAutoreleasePool *	pool        = [[NSAutoreleasePool alloc] init];
    NSUserDefaults *	defaults    = [NSUserDefaults standardUserDefaults];

    [defaults registerDefaults: [NSDictionary dictionaryWithObject: @"YES" forKey: @"AppleDockIconEnabled"]];
    [pool release];

    return NSApplicationMain (argc, pArgv);
}

//----------------------------------------------------------------------------------------------------------------------------
