//----------------------------------------------------------------------------------------------------------------------------
//
// "QArgumentEdit.m"
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2012 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
//----------------------------------------------------------------------------------------------------------------------------

#import "QArgumentEdit.h"

#import "FDFramework/FDFramework.h"

//----------------------------------------------------------------------------------------------------------------------------

#define skQArgumentNone         ( nil )
#define skQArgumentBoolean      ( "" )
#define skQArgumentNumber       ( "number" )
#define skQArgumentIp           ( "xxx.yyy.zzz.www" )
#define skQArgumentPlayers      ( "max players (16)" )
#define skQArgumentAbsFolder    ( "directory" )
#define skQArgumentRelFolder    ( "relative directory" )
#define skQArgumentMemoryKb     ( "memory in kb" )
#define skQArgumentMemoryMb     ( "memory in mb" )
#define skQArgumentParticles    ( "number of particles" )

//----------------------------------------------------------------------------------------------------------------------------

typedef struct
{
    const char*     mpName;
    const char*     mpDescription;
    const char*     mpPlaceHolder;
} QArgumentDesc;

//----------------------------------------------------------------------------------------------------------------------------

static QArgumentDesc    sQArguments[] = 
{

    { "-hipnotic",      "Start Quake with Mission Pack I: \"Scourge of Armagon\"",                      skQArgumentBoolean  },
    { "-rogue",         "Start Quake with Mission Pack II: \"Dissolution of Eternity\".",               skQArgumentBoolean  },
    { "-game",          "Start Quake using the files from a mission pack or mod.",                      skQArgumentRelFolder},
    { "-basedir",       "Specify the location of the game data files.",                                 skQArgumentAbsFolder},
    { "-cachedir",      "Specify the location of the game cache files.",                                skQArgumentAbsFolder},
    { "-path",          "Specify the location of the game data files.",                                 skQArgumentAbsFolder},

    { nil,              nil,                                                                            skQArgumentNone     },
    
    { "-dedicated",     "Start Quake in dedicated server mode.",                                        skQArgumentPlayers  },

#if defined (QUAKE_WORLD)
    
    { "-ip",            "Connect to a server at the specified IP address.",                             skQArgumentIp       },
    
#endif // QUAKE_WORLD
    
    { "-listen",        "Specify the maximum number of players that can connect to the listen server.", skQArgumentPlayers  },
    { "-nolan",         "Disable support for all network protocols.",                                   skQArgumentBoolean  },
    { "-noudp",         "Disable support for the TCP/IP network protocol.",                             skQArgumentBoolean  },
    { "-port",          "The UDP port number used for the TCP/IP network protocol for the server.",     skQArgumentNumber   },
    { "-udpport",       "The UDP port number used for the TCP/IP network protocol.",                    skQArgumentNumber   },
    
#if defined (GLQUAKE)
    
    { nil,              nil,                                                                            skQArgumentNone     },
    
    { "-gamma",         "Specify the gamma value for the initial texture conversion.",                  skQArgumentNumber   },
    { "-width",         "The horizontal resolution to use for the OpenGL game window.",                 skQArgumentNumber   },
    { "-height",        "The vertical resolution to use for the OpenGL game window.",                   skQArgumentNumber   },
    { "-conwidth",      "Specify the horizontal resolution for the console screen.",                    skQArgumentNumber   },
    { "-conheight",     "Specify the vertical resolution for the console screen.",                      skQArgumentNumber   },
    { "-lm_1",          "Use luminance textures as internal texture format for lightmaps.",             skQArgumentBoolean  },
    { "-lm_a",          "Use alpha textures as internal texture format for lightmaps.",                 skQArgumentBoolean  },
    { "-lm_i",          "Use intensity textures as internal texture format for lightmaps.",             skQArgumentBoolean  },
    { "-lm_2",          "Use 16 bit RGBA textures as internal texture format for lightmaps.",           skQArgumentBoolean  },
    { "-lm_4",          "Use 32 bit RGBA textures as internal texture format for lightmaps.",           skQArgumentBoolean  },
    { "-nomtex",        "Disable multi-texture extensions.",                                            skQArgumentBoolean  },

    { nil,              nil,                                                                            skQArgumentNone     },
    
    { "-forcegeneric",  "Force generic (slow) render path.",                                            skQArgumentBoolean  },
    { "-forcegf2",      "Force optimized rendering path for NVidia Geforce 2.",                         skQArgumentBoolean  },
    { "-forceparhelia", "Force optimized rendering path for Matrox Parhelia.",                          skQArgumentBoolean  },
    { "-noarb",         "Disable support for optimized renering path with ARB shaders.",                skQArgumentBoolean  },
    { "-notexcomp",     "Disable support for texture compression.",                                     skQArgumentBoolean  },
    
#endif // GLQUAKE

    { nil,              nil,                                                                            skQArgumentNone     },
    
    { "-nosound",       "Disable support for sound hardware.",                                          skQArgumentBoolean  },
    { "-simsound",      "Disable sound playback but leave all sound functions enabled.",                skQArgumentBoolean  },

    { nil,              nil,                                                                            skQArgumentNone     },
    
    { "-mem",           "Specify the amount of memory to be used by Quake.",                            skQArgumentMemoryMb },
    { "-minmemory",     "Force Quake to use a minimum amount of memory.",                               skQArgumentBoolean  },
    { "-particles",     "Specify the maximum number of particles to be active.",                        skQArgumentParticles},
    { "-surfcachesize", "Allocate the specified amount of memory for storing textures.",                skQArgumentMemoryKb },
    { "-zone",          "Specify the amount of memory in bytes to allocate for dynamic information.",   skQArgumentMemoryKb },
    
    { nil,              nil,                                                                            skQArgumentNone     },
    
    { "-safe",          "Start the game in safe mode.",                                                 skQArgumentBoolean  },
    { "-condebug",      "Save console output to \"id1/console.log\".",                                  skQArgumentBoolean  },
    { "-playback",      "Playback debug information from the file \"quake.vcr\".",                      skQArgumentBoolean  },
    { "-record",        "Store debug information to the file \"quake.vcr\".",                           skQArgumentBoolean  },
    { "-proghack",      "Hack to use quake 1 progs with quake 2 maps.",                                 skQArgumentBoolean  }

};

//----------------------------------------------------------------------------------------------------------------------------

@implementation QArgumentEdit

- (NSString*) windowNibName
{
	return @"ArgumentEdit";
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) awakeFromNib
{
    [mCommandPopup removeAllItems];
    
    for (NSUInteger i = 0; i < FD_SIZE_OF_ARRAY (sQArguments); ++i)
    {
        NSMenuItem*     item        = nil;
        QArgumentDesc*  pArgument   = &(sQArguments[i]);
        
        if (pArgument->mpName != nil)
        {
            NSString*   title = [NSString stringWithCString: pArgument->mpName encoding: NSASCIIStringEncoding];
            
            item = [[[NSMenuItem alloc] initWithTitle: title action: nil keyEquivalent: [NSString string]] autorelease];
            
            [item setTag: i];
            [item setTarget: self];
            [item setAction: @selector (selectedArgument:)];
        }
        else
        {
            item = [NSMenuItem separatorItem];
        }
        
        [[mCommandPopup menu] addItem: item];
    }
    
    [mCommandPopup selectItemAtIndex: 0];
    
    [self selectedArgument: [mCommandPopup selectedItem]];
}

//----------------------------------------------------------------------------------------------------------------------------

- (NSInteger) edit: (NSMutableDictionary*) item modalForWindow: (NSWindow*) window
{
    NSInteger result;
    
	[NSApp beginSheet: [self window] modalForWindow: window modalDelegate: nil didEndSelector: nil contextInfo: nil];
	
    result = [NSApp runModalForWindow: [self window]];
    
    if (result == NSOKButton)
    {
        NSCharacterSet* whitespace  = [NSCharacterSet whitespaceCharacterSet];
        NSString*       command     = [[mCommandPopup selectedItem] title];
        NSString*       text        = [[mArgumentTextField stringValue] stringByTrimmingCharactersInSet: whitespace];
        
        if ([text length] > 0)
        {
            command = [NSString stringWithFormat: @"%@ %@", command, text];
        }
        
        [item setObject: command forKey: @"value"];
    }
    
	[NSApp endSheet: [self window]];
    
	[[self window] orderOut: self];

	return result;
}

//----------------------------------------------------------------------------------------------------------------------------

- (IBAction) done: (id) cancel
{
    [NSApp stopModalWithCode: NSOKButton];
}

//----------------------------------------------------------------------------------------------------------------------------

- (IBAction) cancel: (id) sender
{
    [NSApp stopModalWithCode: NSCancelButton];
}

//----------------------------------------------------------------------------------------------------------------------------

- (void) selectedArgument: (id) sender
{
    NSInteger       index       = [sender tag];
    NSString*       placeholder = nil;
    NSString*       description = nil;
    BOOL            canEdit     = YES;
    
    if ((index >= 0) && (index < FD_SIZE_OF_ARRAY (sQArguments)))
    {
        description = [NSString stringWithCString: sQArguments[index].mpDescription encoding: NSASCIIStringEncoding];
        placeholder = [NSString stringWithCString: sQArguments[index].mpPlaceHolder encoding: NSASCIIStringEncoding];
        canEdit     = (sQArguments[index].mpPlaceHolder[0] != '\0');
    }
    else
    {
        description = [NSString string];
        placeholder = [NSString string];
    }
    
    [[mArgumentTextField cell] setPlaceholderString: placeholder];
    [mArgumentTextField setStringValue: [NSString string]];
    [mArgumentTextField setEnabled: canEdit];
    
    [mDescriptionTextField setStringValue: description];
}

@end

//----------------------------------------------------------------------------------------------------------------------------
