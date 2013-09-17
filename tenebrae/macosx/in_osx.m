//----------------------------------------------------------------------------------------------------------------------------
//
// "in_osx.m" - MacOS X mouse driver
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2012 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
// Quakeª is copyrighted by id software		[http://www.idsoftware.com].
//
// Version History:
// v1.2.0: Rewritten. Uses now IOHIDManager.
//         Added support for game pads.
//         Added support for actuators.
// v1.0.8: F12 eject is now disabled while Quake is running.
// v1.0.6: Mouse sensitivity works now as expected.
// v1.0.5: Reworked whole mouse handling code [required for windowed mouse].
// v1.0.0: Initial release.
//
//----------------------------------------------------------------------------------------------------------------------------

#import <AppKit/AppKit.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/hidsystem/IOHIDLib.h>
#import <IOKit/hid/IOHIDLib.h>

#import "FDFramework/FDFramework.h"

#import "quakedef.h"
#import "in_osx.h"
#import "sys_osx.h"
#import "vid_osx.h"

//----------------------------------------------------------------------------------------------------------------------------

typedef enum
{
    eAxisNone,
    eAxisForward,
    eAxisSide,
    eAxisTurn,
    eAxisLook
} in_axismap_t;

//----------------------------------------------------------------------------------------------------------------------------

typedef struct
{
    SInt32  X;
    SInt32  Y;
} in_mousepos_t;

//----------------------------------------------------------------------------------------------------------------------------

cvar_t                          in_actuators            = { "actuators", "0", true };
cvar_t                          in_actuatorduration     = { "actuatorduration", "0.2", true };
static cvar_t                   aux_look                = { "auxlook", "1", true };
static cvar_t                   m_filter                = { "m_filter", "1" };
static cvar_t	                in_joystick             = { "joystick", "1", true };
static cvar_t	                joy_advanced            = { "joyadvanced", "1", true };
static cvar_t	                joy_advaxisx            = { "joyadvaxisx", "2", true };
static cvar_t	                joy_advaxisy            = { "joyadvaxisy", "1", true };
static cvar_t	                joy_advaxisz            = { "joyadvaxisz", "0", true };
static cvar_t	                joy_advaxisr            = { "joyadvaxisr", "3", true };
static cvar_t	                joy_advaxisu            = { "joyadvaxisu", "4", true };
static cvar_t	                joy_advaxisv            = { "joyadvaxisv", "0", true };
static cvar_t	                joy_forwardthreshold    = { "joyforwardthreshold", "0.15", true };
static cvar_t	                joy_sidethreshold       = { "joysidethreshold", "0.15", true };
static cvar_t	                joy_pitchthreshold      = { "joypitchthreshold", "0.15", true };
static cvar_t	                joy_yawthreshold        = { "joyyawthreshold", "0.15", true };
static cvar_t	                joy_forwardsensitivity  = { "joyforwardsensitivity", "-1.0", true };
static cvar_t	                joy_sidesensitivity     = { "joysidesensitivity", "1.0", true };
static cvar_t	                joy_pitchsensitivity    = { "joypitchsensitivity", "1.0", true };
static cvar_t                   joy_yawsensitivity      = { "joyyawsensitivity", "-1.0", true };

static in_mousepos_t            sInMouseNewPosition     = { 0 };
static in_mousepos_t            sInMouseOldPosition     = { 0 };
static float                    sInJoyValues[6]         = { 0 };
static double                   sInActuatorEndTime      = -1.0f;

//----------------------------------------------------------------------------------------------------------------------------

static void                     IN_Toggle_AuxLook_f (void);
static void                     IN_Force_CenterView_f (void);
static void                     IN_MouseMove (usercmd_t *);
static void                     IN_JoyMove (usercmd_t *cmd);
static void                     IN_UpdateActuators ();

//----------------------------------------------------------------------------------------------------------------------------

void	IN_Toggle_AuxLook_f (void)
{
    if (aux_look.value)
    {
        Cvar_Set ("auxlook", "0");
    }
    else
    {
        Cvar_Set ("auxlook", "1");
    }
}

//----------------------------------------------------------------------------------------------------------------------------

void	IN_Force_CenterView_f (void)
{
    cl.viewangles[PITCH] = 0;
}

//----------------------------------------------------------------------------------------------------------------------------

void 	IN_Init (void)
{
    FDHIDManager* sharedInput = nil;
    
    Cvar_RegisterVariable (&aux_look);    
    Cvar_RegisterVariable (&m_filter);
    
    Cvar_RegisterVariable (&in_actuators);
    Cvar_RegisterVariable (&in_actuatorduration);
    
    Cvar_RegisterVariable (&in_joystick);
    Cvar_RegisterVariable (&joy_advanced);
	Cvar_RegisterVariable (&joy_advaxisx);
	Cvar_RegisterVariable (&joy_advaxisy);
	Cvar_RegisterVariable (&joy_advaxisz);
	Cvar_RegisterVariable (&joy_advaxisr);
	Cvar_RegisterVariable (&joy_advaxisu);
	Cvar_RegisterVariable (&joy_advaxisv);
    Cvar_RegisterVariable (&joy_forwardthreshold);
	Cvar_RegisterVariable (&joy_sidethreshold);
	Cvar_RegisterVariable (&joy_pitchthreshold);
	Cvar_RegisterVariable (&joy_yawthreshold);
	Cvar_RegisterVariable (&joy_forwardsensitivity);
	Cvar_RegisterVariable (&joy_sidesensitivity);
	Cvar_RegisterVariable (&joy_pitchsensitivity);
	Cvar_RegisterVariable (&joy_yawsensitivity);
    
    Cmd_AddCommand ("toggle_auxlook", IN_Toggle_AuxLook_f);
    Cmd_AddCommand ("force_centerview", IN_Force_CenterView_f);
    
    in_mlook.state = 1;
    
    sharedInput = [FDHIDManager sharedHIDManager];
    
    if (sharedInput != nil)
    {
        NSArray* devices = [NSArray arrayWithObjects: FDHIDDeviceGamePad, FDHIDDeviceKeyboard, FDHIDDeviceMouse, nil];
        
        [sharedInput setDeviceFilter: devices];
    }
    else
    {
        Sys_Error ("Failed to open HID manager!");
    }
}

//----------------------------------------------------------------------------------------------------------------------------

void 	IN_Shutdown (void)
{
    for (FDHIDDevice* device in [[FDHIDManager sharedHIDManager] devices])
    {
        if ([device hasActuator])
        {
            [[device actuator] stop];
        }
    }
    
    [[FDHIDManager sharedHIDManager] release];
}

//----------------------------------------------------------------------------------------------------------------------------

void 	IN_Commands (void)
{
}

//----------------------------------------------------------------------------------------------------------------------------

void	IN_MouseMove (usercmd_t *cmd)
{
    in_mousepos_t   mousePos;

    if (((gVidDisplayFullscreen == NO) && (_windowed_mouse.value == 0.0f)) || ([gVidWindow isMiniaturized] == YES))
    {
        return;
    }

    if (m_filter.value != 0.0f)
    {
        mousePos.X = (sInMouseNewPosition.X + sInMouseOldPosition.X) >> 1;
        mousePos.Y = (sInMouseNewPosition.Y + sInMouseOldPosition.Y) >> 1;
    }
    else
    {
        mousePos = sInMouseNewPosition;
    }

    sInMouseOldPosition     = sInMouseNewPosition;
    sInMouseNewPosition.X   = 0;
    sInMouseNewPosition.Y   = 0;
    
    mousePos.X      *= sensitivity.value;
    mousePos.Y      *= sensitivity.value;

    if ((in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1)))
    {
        cmd->sidemove += m_side.value * mousePos.X;
    }
    else
    {
        cl.viewangles[YAW] -= m_yaw.value * mousePos.X;
    }
                
    if (in_mlook.state & 1)
    {
        V_StopPitchDrift ();
    }
            
    if ((in_mlook.state & 1) && !(in_strafe.state & 1))
    {
        cl.viewangles[PITCH] += m_pitch.value * mousePos.Y;
        
        if (cl.viewangles[PITCH] > 80)
        {
            cl.viewangles[PITCH] = 80;
        }
        
        if (cl.viewangles[PITCH] < -70)
        {
            cl.viewangles[PITCH] = -70;
        }
    }
    else
    {
        if ((in_strafe.state & 1) && noclip_anglehack)
        {
            cmd->upmove -= m_forward.value * mousePos.Y;
        }
        else
        {
            cmd->forwardmove -= m_forward.value * mousePos.Y;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------

void	IN_JoyMove (usercmd_t *cmd)
{
    in_axismap_t    joyMap[6]   = { 0 };
    float           speed       = (in_speed.state & 1) ? cl_movespeedkey.value : 1.0f;
    float           aspeed      = speed * host_frametime;
    
    if (!in_joystick.value)
	{
		return; 
	}
    
    joyMap[0] = ((uint32_t) joy_advaxisx.value) & 0xF;
    joyMap[1] = ((uint32_t) joy_advaxisy.value) & 0xF;
    joyMap[2] = ((uint32_t) joy_advaxisz.value) & 0xF;
    joyMap[3] = ((uint32_t) joy_advaxisr.value) & 0xF;
    joyMap[4] = ((uint32_t) joy_advaxisu.value) & 0xF;
    joyMap[5] = ((uint32_t) joy_advaxisv.value) & 0xF;
    
    for ( uint32 i = 0; i < FD_SIZE_OF_ARRAY (joyMap); ++i)
    {
        float fAxisValue = sInJoyValues[i];
        
        switch (joyMap[i])
        {
            case eAxisForward:
                if ((joy_advanced.value == 0.0) && (in_mlook.state & 1))
                {
                    // user wants forward control to become look control
                    if (fabsf (fAxisValue) > joy_pitchthreshold.value)
                    {		
                        if (m_pitch.value < 0.0)
                        {
                            cl.viewangles[PITCH] -= (fAxisValue * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
                        }
                        else
                        {
                            cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
                        }
                        V_StopPitchDrift();
                    }
                    else
                    {
                        if (lookspring.value == 0.0)
                        {
                            V_StopPitchDrift();
                        }
                    }
                }
                else
                {
                    if (fabs(fAxisValue) > joy_forwardthreshold.value)
                    {
                        cmd->forwardmove += (fAxisValue * joy_forwardsensitivity.value) * speed * cl_forwardspeed.value;
                    }
                }
                break;
                
            case eAxisSide:
                if (fabsf(fAxisValue) > joy_sidethreshold.value)
                {
                    cmd->sidemove += (fAxisValue * joy_sidesensitivity.value) * speed * cl_sidespeed.value;
                }
                break;
                
            case eAxisTurn:
                if ((in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1)))
                {
                    if (fabs(fAxisValue) > joy_sidethreshold.value)
                    {
                        cmd->sidemove -= (fAxisValue * joy_sidesensitivity.value) * speed * cl_sidespeed.value;
                    }
                }
                else
                {
                    if (fabs(fAxisValue) > joy_yawthreshold.value)
                    {
                        cl.viewangles[YAW] += (fAxisValue * joy_yawsensitivity.value) * aspeed * cl_yawspeed.value;
                    }
                }
                break;
                
            case eAxisLook:
                if (in_mlook.state & 1)
                {
                    if (fabs(fAxisValue) > joy_pitchthreshold.value)
                    {
                        cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
                        V_StopPitchDrift();
                    }
                    else
                    {
                        if (lookspring.value == 0.0f)
                        {
                            V_StopPitchDrift();
                        }
                    }
                }
                break;
                
            default:
                break;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------

void IN_Move (usercmd_t *cmd)
{
    IN_MouseMove (cmd);
    IN_JoyMove (cmd);
}

//----------------------------------------------------------------------------------------------------------------------------

void    IN_Damage (float duration)
{
    if (in_actuators.value != 0.0f)
    {
        if (sInActuatorEndTime < 0.0)
        {
            for (FDHIDDevice* device in [[FDHIDManager sharedHIDManager] devices])
            {
                if ([device hasActuator])
                {
                    [[device actuator] setDuration: duration];
                    [[device actuator] start];
                }
            }
        }
        
        sInActuatorEndTime = Sys_FloatTime () + (double) duration;
    }
}

//----------------------------------------------------------------------------------------------------------------------------

void    IN_UpdateActuators ()
{
    if ((sInActuatorEndTime >= 0.0) && (sInActuatorEndTime < Sys_FloatTime()))
    {
        for (FDHIDDevice* device in [[FDHIDManager sharedHIDManager] devices])
        {
            if ([device hasActuator])
            {
                [[device actuator] stop];
            }
        }
        
        sInActuatorEndTime = -1.0f;
    }
}

//----------------------------------------------------------------------------------------------------------------------------

void IN_SendKeyEvents()
{
    const FDHIDEvent*   pEvent      = nil;
    const BOOL          allowJoy    = (in_joystick.value != 0.0f);
    const BOOL          allowMouse  = (_windowed_mouse.value != 0.0f) || (gVidDisplayFullscreen == YES);
    
    while ((pEvent = [[FDHIDManager sharedHIDManager] nextEvent]) != nil)
    {
        switch (pEvent->mType)
        {
            case eFDHIDEventTypeGamePadAxis:
                if (allowJoy == YES)
                {
                    if (pEvent->mButton < FD_SIZE_OF_ARRAY (sInJoyValues))
                    {
                        sInJoyValues[pEvent->mButton] = pEvent->mFloatVal;
                    }                    
                }
                break;
                
            case eFDHIDEventTypeGamePadButton:
                if (allowJoy == YES)
                {
                    if (pEvent->mButton <= (K_AUX32 - K_AUX1))
                    {
                        Key_Event (K_AUX1 + pEvent->mButton, pEvent->mBoolVal);
                    }
                }
                break;
                
            case eFDHIDEventTypeKeyboard:
                if (pEvent->mButton < 255)
                {
                    Key_Event (pEvent->mButton, pEvent->mBoolVal);
                }
                break;
                
            case eFDHIDEventTypeMouseAxis:
                if (allowMouse == YES)
                {
                    if (pEvent->mButton == eFDHIDMouseAxisX)
                    {
                        sInMouseNewPosition.X += pEvent->mIntVal;
                        
                    }
                    else if (pEvent->mButton == eFDHIDMouseAxisY)
                    {
                        sInMouseNewPosition.Y += pEvent->mIntVal;
                    }
                    else if (pEvent->mButton == eFDHIDMouseAxisWheel)
                    {
                        if (pEvent->mIntVal != 0)
                        {
                            const int wheelEvent = (pEvent->mIntVal > 0) ? K_MWHEELUP : K_MWHEELDOWN;
                            
                            Key_Event (wheelEvent, true);
                            Key_Event (wheelEvent, false);
                        }
                    }
                }
                break;
                
            case eFDHIDEventTypeMouseButton:
                if (allowMouse == YES)
                {
                    if (pEvent->mButton < 5)
                    {
                        Key_Event (K_MOUSE1 + pEvent->mButton, pEvent->mBoolVal);
                    }
                }
                break;
        }
    }
    
    IN_UpdateActuators ();
}

//----------------------------------------------------------------------------------------------------------------------------
