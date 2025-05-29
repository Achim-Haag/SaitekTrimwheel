/*
	Program to check the status of a Saitek ProFlight Trimwheel (connected by USB)
	as this device doesn't active its axis when plugged in before starting the computer,
	in this case (axis value = 0), the wheel has to be turned some revolutions.
	So this Program should detect the state of the trimwheel's axis and set a corresponding return code

	Modifications:
	13.05.25/AH derived from https://github.com/MysteriousJ/Joystick-Input-Examples
		copy of content of "gameinput.cpp"
	27.05.25/AH after commenting and modifying the source to understand its function
		and to get deeper in Microsoft Windows GameInput API, now changing for my needs.
*/

/*
	Trick to see where SUCCEEDED is defined, leads to error msg
		C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\shared\winerror.h(29881,9):
		warning C4005: "SUCCEEDED": Makro-Neudefinition
		[C:\Users\Achmed\Git_Repos_AH1\SaitekTrimwheel\out\build\Win10_MSVC-17-2022-x64\SaitekTrimwheel.vcxproj]
	now I know, definition of SUCCEEDED results from definition in winerror.h
*/
// Only activated once to find the location of SUCCEEDED definition.
// ##define SUCCEEDED 3.1415927

// To convert numeric preprocessor variable to string, so it could be printed by #pragma
// The first: MYSTRINGINQUOTES returns the value of x in quotes, the second: MYSTRING resolves the MYSTRINGINQUOTES macro
// see also https://gcc.gnu.org/onlinedocs/cpp/Stringizing.html
#define MYSTRINGQUOTES(s) #s
#define MYSTRING(s) MYSTRINGQUOTES(s)

/*
  Global section
  We define global variables before the "main" function, so they're available to all functions in this module
*/

/*
	Include MS GameInput API V.0,
	see https://learn.microsoft.com/en-us/gaming/gdk/docs/features/common/input/overviews/input-overview
	and https://learn.microsoft.com/en-us/gaming/gdk/docs/features/common/input/overviews/input-nuget
	API: https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/gameinput_members
*/	
#include "GameInput.h"

// For other nice things ;-)
#include <stdio.h>
// for _kbhit(), _getch()
#include <conio.h>
// for toupper()
#include <ctype.h>

// Windows-specific getopt
#include "getopt.h"   // see https://github.com/alex85k/wingetopt/tree/master


// Number of controllers and pointer to pointer array (name changed from "Joysticks" to "Joystruct" for better reading)
struct Joystruct
{
	uint32_t deviceCount;
// Create pointer to pointer array, the array pointers point to object instances of class IGameInputDevice
	IGameInputDevice** devices;
};

// Structure"GameInputDeviceInfo" (GameInput.h) minimum size (first fields we process) for device attribute structure 
const int GmInDevInfoHdr = sizeof(GameInputDeviceInfo().infoSize) + 
	sizeof(GameInputDeviceInfo().vendorId) + sizeof(GameInputDeviceInfo().productId) + 
	sizeof(GameInputDeviceInfo().revisionNumber) + sizeof(GameInputDeviceInfo().interfaceNumber) +
	sizeof(GameInputDeviceInfo().collectionNumber);

// Constants for while-loop to restrict to max. one day dependent on cycle-sleep
// Let the program run, but not endless ! 500 msec sleep time -> one day has 86400 seconds
const int waitmsec = 500 ;
const int waitloops = 86400 * 1000 / waitmsec ;

// Variables for processing GameInputDeviceInfo structure
int memsize, vid, pid, rev, ifc, col = 0;

// Variables for processing axes, switches, buttons
int nbraxes, nbrswch, nbrbutt = 0;

// Default: no verbosity
static int verbolvl = 0;
// Definition of exit key
static const int exitkey = 'Q';
//
int keypressed = 0;

// #############################################################################################################
// Start of asynchronous subroutine
// #############################################################################################################
// see https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/functions/gameinputdevicecallback
// From GameInput, at first at registration, then after an event happened, we get this information:
// - Callback-Token (given when this routine was registered by RegisterDeviceCallback)
// - context = &joysticks (important informations that we have specified in RegisterDeviceCallback as parameter 5)
// - IGameInputDevice* (pointer to a specific controller that has changed its state)
// - Current state (connection and input status) of this controller
// - Previous state (connection and input status) of this controller
// For status enumeration see https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/enums/gameinputdevicestatus
// This routine isn't really called async as it is registered as "GameInputBlockingEnumeration", 
// therefore it runs
void CALLBACK deviceChangeCallback(GameInputCallbackToken callbackToken, void* context, IGameInputDevice* device, uint64_t timestamp, GameInputDeviceStatus currentStatus, GameInputDeviceStatus previousStatus) 
{ 
	if ( verbolvl > 0 ) {
		printf("\t#DBG %s@%d ### callback sub: routine starting (async)\n", __func__, __LINE__);
	}
// Access our current "joysticks" array (of controllers)
	Joystruct* joysticks = (Joystruct*)context;
// currentStatus :
//		GameInputDeviceNoStatus = 0x00000000
//		GameInputDeviceConnected = 0x00000001
//		then other status = 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, 0x100000
//		and GameInputDeviceAnyStatus = 0x00FFFFFF
// GameInputDeviceConnected : 0x00000001
// so the "if" (0 = false, any other value = true), after the boolean "and" executes its tree when flag "device is connected" is set
// meaning: the "if" executes its tree as a (new) device connects
	if (currentStatus & GameInputDeviceConnected)
	{
		for (uint32_t devctrnew = 0; devctrnew < joysticks->deviceCount; ++devctrnew)
		{
// Check if the new contoller device is already in our list of controllers, if so, do nothing and return to caller
			if ( verbolvl > 0 ) {
				printf("\t#DBG %s@%d ### callback sub: checking device %i\n", __func__, __LINE__, devctrnew);
			}
			if (joysticks->devices[devctrnew] == device) {
				if ( verbolvl > 0 ) {
					printf("\t#DBG %s@%d ### callback sub: routine leaving, joystick unchanged %i\n", __func__, __LINE__, devctrnew);
				}
				return;
			}
		}
// We have found a new device, so re-allocate our joystick list with the additional joystick definition

// add 1 to number of controllers
		++joysticks->deviceCount;
		if ( verbolvl > 0 ) {
			printf("\t#DBG %s@%d ### callback sub: Joystick %i added\n", __func__, __LINE__,joysticks->deviceCount);
		}
// now realloc (resize) our list of controllers (add memory for the new controller)
		joysticks->devices = (IGameInputDevice**)realloc(joysticks->devices, joysticks->deviceCount * sizeof(IGameInputDevice*));
// move new device definition to new element of controller array
// Array handling: controller 1 to element 0, controller 2 to element 1 aso., therefore deviceCount-1)
		joysticks->devices[joysticks->deviceCount-1] = device;
	}
	printf("## callback: routine leaving\n");
	if ( verbolvl > 0 ) {
		printf("\t#DBG %s@%d ### callback sub: routine leaving\n", __func__, __LINE__);
	}
} 

// #############################################################################################################
// Start of main program entry
// #############################################################################################################
// Modified main entry to accept parameters for (later to implement) getopt processing
int main(int argc, char** argv)
{
// My own header for comparing build vs. execution msg to see if build/compile in VSCode really happened
// Building with Microsoft Visual-C
#if _MSC_VER          // see https://learn.microsoft.com/en-us/cpp/overview/compiler-versions?view=msvc-170
#define COMP_TYP "MSVC"
#define COMP_VER _MSC_FULL_VER
// Building with gcc
#elif __GNUC__		// see https://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html
#define COMP_TYP "gcc"
#define COMP_VER (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
// Building with whatever
#else
#define COMP_TYP "???"
#define COMP_VER 0
#endif

// Just to get information about the compile time at compilation (#pragma message) and execution (printf)
#pragma message ("***** " COMP_TYP " V." MYSTRING(COMP_VER) " Compile " __FILE__ " at " __DATE__ " " __TIME__ "*****\n")   
	printf("***** Running %s,\nBinary build date: %s @ %s by %s %d *****\n\n", \
		  argv[0], __DATE__, __TIME__, COMP_TYP, COMP_VER);

// Now let us start the application
 	if ( verbolvl > 0 ) {
    	printf("\t#DBG %s@%d # Starting main()\n", __func__, __LINE__);
  	}	

// Process option parameters from commandline
// ################################################################################ ANF
/*
  Process commandline parameters with Windows-specific getopt.c
  getopt is a well-known function in the Unix environment and shells to parse argument lists,
  see https://en.wikipedia.org/wiki/Getopt 
  or https://www.gnu.org/software/libc/manual/html_node/Example-of-Getopt.html (the sample I used here)
  The "main" function has to be defined with arguments "(int argc, char** argv)" (char** is a pointer to a pointer list)
*/
 	if ( verbolvl > 0 ) {
    	printf("\t#DBG %s@%d # Process commandline parameters by getopt.c\n", __func__, __LINE__);
  	}	
	bool waitfortrimwheel = false;   // Parameter "-w : wait fo trimwheel axis to become nonzero
	bool cheatword = false;   // Parameter "-c" : show each word to guess in advance

/*
  Variables defined for and by getopt.c (https://www.gnu.org/software/libc/manual/html_node/Using-Getopt.html)
  Call : getopt(int argc, char* const *argv, const char* options)
  argc = number of parameters in argv,
  argv = pointer to pointerlist (i.e. to a pointer array), each pointer in pointerlist points to a commandline parameter
  options = string with allowed commandline parameters. Colon after parameter means: parameter must have a following string value
*/  
	int cmdline_arg = 0;      // Returns the next commandline parameter from getopt prefixed by "-", else a value of -1
	int opterr = 0;           // getopt.c behaviour regarding error handling; 0 = silent but return "?" in case of error", not 0 = print msg
// int optopt in getopt.h     commandline parameter not specified in third parameter of getopt call, i.e. parameter not allowed
// int optind in getopt.h     set by getopt.c to the index of the next elemnt in argv. At end: points to first unprocessed argv element
// char* optarg in getopt.h   set by getopt.c to the option value behind the processed commandline parameter (e.g. "1" for "-v 1")

/* Now parse the given-to-main commandline parameters */
/* Implemented: "-h" = help; "-v" = verbosity (lvl increased by multiple occurences); -w = wait up to one day or user abort*/
/* The colon after an option requests a value behind an option character */
	while ((cmdline_arg = getopt (argc, argv, "hvw")) != -1) {
// As we don't have here a valid verbolvl, I leave this debugging statement as comment:
// printf("### Entering next getopts loop (while), cmdline_arg = %d = %c\n", cmdline_arg, cmdline_arg);
    	switch (cmdline_arg) {
		case 'h':                     // Option -h -> Help
        	printf("Processing Saitek ProFlight Trimwheel axis\n"
           		"derived from https://github.com/MysteriousJ/Joystick-Input-Examples by Achim Haag\n"
           		"Allowed commandline parameters:\n"
           		"-h : this help\n"
           		"-v : debugging msgs, level increased by multiple occurences\n"
           		"-w : wait for Saitek Trimwheel axis value <> 0 (else get actual axis value and return\n"
           		"Retcode: 0 = axis not zero; 1 = axis zero; 8 = no trimwheel (VID &6A3, PID &BD4)\n"
			);
        	return 1; // !!! Attention !!! Early return to OS
        	break;    // Never reached because of return
		case 'v':                     // Option -v -> Verbosity, each occurence increases verbosity by 1 up to max. 9)
        	if (verbolvl < 9) {
        		verbolvl = ++verbolvl;   // variable optarg definition in getopt.h, returned from compiled getopt function
        		printf("Verbosity increased to %i\n", verbolvl);
        	}
        	break;  
      	case 'w':                     // Option --w -> wait for trimwheel axis to become <> 0
        	printf("Waiting for trimwheel axis to become nonzero\n");
        	waitfortrimwheel=true;
        	break;
      	case '?':                     // Any other commandline parameter error
        	if (optopt == 'v' || optopt == 'f') {         // optopt: Parameter in error, here -v without following number
          		fprintf(stderr, "Option -%c requires an argument. Try -h !\n", optopt);
        	} else if (isprint (optopt)) {    // here we found a parameter not specified in the third getopt argument (string, see above)
          				fprintf(stderr, "Unknown option `-%c'. Try -h !\n", optopt);
        			} else {                    // Any other getopt error - exit program
          				fprintf(stderr, "Unknown option character `\\x%x', try -h ! Bye\n", optopt);
        			} // endif
			return 1; // !!! Attention !!! Early return to OS
        	break;    // Never reached because of return
      	default:                      // Parameter allowed but not handled - this should not occur
        	printf("Parameter %c not handled, contact programmer !", cmdline_arg);
        	abort (); // !!! Attention !!! Early return to OS
        	break;    // Never reached because of abort
    	} // end switch
  	} // end while

	if ( verbolvl > 0 ) {
    	printf("Unprocessed commmandline parameters (%d parameters):\n", optind);
    	for (int index = optind; index < argc; index++) printf ("Non-option argument [%s]\n", argv[index]);
  	}

// ################################################################################### END

// Define joysticks as structure and initialize to zero both subfields (deviceCount, pointer to array of pointers to the specific devinces)
	Joystruct joysticks = {0};

// Create per-process singleton object instance pointer "input" to acces the device input stream
// see https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/interfaces/igameinput/igameinput
	IGameInput* input;
// Call function to get instances of the IGameInput interface, so: get a list of our controllers
// see https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/functions/gameinputcreate
	GameInputCreate(&input);
// Create pointer "dispatcher" to object instance of class IGameInputDispatcher
// see https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/interfaces/igameinputdispatcher/igameinputdispatcher
// By referencing IGameInputDispatcher, GameInput changes from "automatic mode" to "manual mode"
// so we have to schedule the background work of the GameInput API later manually
	IGameInputDispatcher* dispatcher;
// Create a Dispatcher for manually scheduling GameInput background work (switching GameInput to "manual dispatch mode")
// see https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/interfaces/igameinput/methods/igameinput_createdispatcher
	input->CreateDispatcher(&dispatcher);
// Create object instance "callbackId" of type GameInputCallbackToken (defined in GameInput.h as "typedef uint64_t GameInputCallbackToken;")
// Device callbacks provide an asynchronous way to get informed about device status changes (e.g. device connects/disconnects)
	GameInputCallbackToken callbackId;
// Now we register our callback function "deviceChangeCallback" to be called
// whenever a devices is connected or disconnected or a device property change
// see https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/interfaces/igameinput/methods/igameinput_registerdevicecallback
// "deviceChangeCall is called asynchronously and only in the stated events"
// Parameter:
// - 0 = no specific device selected (else its IGameInputDevice* has to be specified)
// - Limit to kind/type "Controllers"
// - No Limit on device states
// - enumerate sychronously ("blocking" RegisterDeviceCallback until all callbacks are processed)
// - relevant information for callback function - give the address of our "joysticks pointer to pointer array" to the async subroutine
// - name of asynch subroutine: deviceChangeCallback (subroutine defined above)
// - token identifying the registered callback function (if we have to cancel or unregister this callback function)

	if ( verbolvl > 0 ) {
		printf("\t#DBG %s@%d Registering async callback procedure\n", __func__, __LINE__);
	}
	input->RegisterDeviceCallback(0, GameInputKindController, GameInputDeviceAnyStatus, GameInputBlockingEnumeration, &joysticks, deviceChangeCallback, &callbackId);

// Define array of one controllers buttons as up to 64 button states
	bool buttons[64];
// Define array of one controllers switches array as enumerator of up to 64 switch states
// (e.g. GameInputSwitchCenter = 0, GameInputSwitchUp = 1, ...)
// see https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/enums/gameinputswitchposition
	GameInputSwitchPosition switches[64];

// Define array of one controllers axes as up to 64 axes floating point values
	float axes[64];

// If wait-flag specified: loop (nearly) endless
	if ( waitfortrimwheel ) {
		printf("Starting while-Loop for up to %i cycles with sleep %i msecs\n", waitloops,waitmsec);
		printf("Press exit-key '%c' to interrupt if you don't like to run it a whole day ;-)\n", exitkey);
	}
	// Now start query cycles up to a whole day	
	for (int whilecounter = 1 ; whilecounter < waitloops ; whilecounter++)
	{
		if ( verbolvl > 0 ) {
			printf("\t#DBG %s@%d \n*** while-Cycle %i ***\n", __func__, __LINE__, whilecounter);
		} else if ( waitfortrimwheel ) {
			printf("*** while-Cycle %i , exit='%c' ***\n", whilecounter, exitkey);
		}
// Object instance "dispatcher" is of class IGameInputDispatcher, declaration see above
// According to https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/interfaces/IGameInputDispatcher/methods/igameinputdispatcher_dispatch
// the dispatcher executes for at least one queue item, even if the quota is set to zero (as here)
// In my understanding, this statement gives the async CALLBACK routine "deviceChangeCallback" a chance to execute ("manual dispatch")
// https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/interfaces/IGameInputDispatcher/igameinputdispatcher states:
// "Allows you to take manual control of scheduling the background work run by the GameInput API.""
// and that's what we do here.
// Return value: true if work items are pending in the dispatcher's queue, false if no work items remain
// Returns at the time that the queue is flushed
		bool dispretc = dispatcher->Dispatch(0);
		if ( verbolvl > 0 ) {
			printf("\t#DBG %s@%d GameInput dispatcher work to do: %s\n", __func__, __LINE__, dispretc ? "yes" : "no");
		}
// Now let's start the processing of the "GameInput stream" for a specific controller device,
// a continuous data stream that consists of every action (buttons, switches, axis) on all filtered devices
		if ( verbolvl > 0 ) {
			printf("\t#DBG %s@%d Starting for-Loop over %i Joystick devices\n", __func__, __LINE__, joysticks.deviceCount);
		}
		for (uint32_t devctr = 0; devctr < joysticks.deviceCount; ++devctr)
		{
// Define "reading" as instance of class IGameInputReading and capture/process the raw input data...
// Every input state change received from a device is captured in an IGameInputReading instance. 
			IGameInputReading* reading;
// ...and call moethod "GetCurrentReading" of object instance "input" to initially access the controller input stream, see
// https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/enums/gameinputkind
// Filter is GameInputKindController: "Combination of Axis, Button, and Switch"
// (another possible filter for Saitek Trimwheel would be "GameInputKindControllerAxis - Controller input from sticks")
// The device is selected by inner loop variable i (so controller devices are processed sequentially)
// The last parameter (object instance "reading") is "the input reading to be returned.
// Returns NULL on failure
//
// SUCCEEDED seems to be a macro of winerror.h - although winerror.h isn't included
// Location: C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\shared\winerror.h
// Probably from other (nested) #include
//
			if (SUCCEEDED(input->GetCurrentReading(GameInputKindController, joysticks.devices[devctr], &reading)))
			{
				printf("--- Processing Joystick %d ---\n", devctr);

// Check device information for actual controller joysticks.devices[i], according to
// https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/interfaces/igameinputdevice/methods/igameinputdevice_getdeviceinfo
// https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/structs/gameinputdeviceinfo
// We have to find the Saitek ProFlight Cessna Trim Wheel (VID: 0x6A3, PID: 0xBD4)
//					
				if ( verbolvl > 0 ) {
					printf("\t#DBG %s@%d Now GetDeviceInfo for ctrl %i\n", __func__, __LINE__, devctr);
				}
				auto joydevinfo = joysticks.devices[devctr]->GetDeviceInfo();
				memsize = joydevinfo->infoSize;
// Check returned structure at least as big as the first fields we want to process
				if (memsize >= GmInDevInfoHdr ) {
					if ( verbolvl > 0 ) {
						printf("\t#DBG %s@%d structure length (%i vs. prefix min: %i)\n", __func__, __LINE__, memsize, GmInDevInfoHdr);
					}
					vid = joydevinfo->vendorId;
					pid = joydevinfo->productId;
					rev = joydevinfo->revisionNumber;
					ifc = joydevinfo->interfaceNumber;
					col = joydevinfo->collectionNumber;
					if ( verbolvl > 0 ) {
						printf("\t#DBG %s@%d InfoSize: %i, VID: %#04x, PID: %#04x, REV: %#04x, IFC: %#04x, COL: %#04x\n", __func__, __LINE__, 
								memsize, vid, pid, rev, ifc, col);
					}
				} else {
					if ( verbolvl > 0 ) {
						printf("\t#DBG %s@%d GetDeviceInfo() return structure length too short (%i vs. prefix min: %i)\n", __func__, __LINE__, 
								memsize, GmInDevInfoHdr);
					}
					printf("Cannot get information for ctrl %i)\n", devctr);
				}

// see https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/interfaces/igameinputreading/methods/igameinputreading_getcontrolleraxisstate
// Capture the axes, switches and buttons of the specific oysticks.devices[i]" controller into our own arrays
				if ( verbolvl > 0 ) {
					printf("\t#DBG %s@%d Reading axes, switches and buttons for ctrl %i\n", __func__, __LINE__, devctr);
				}
				printf("Controller %i : ", devctr);
				reading->GetControllerAxisState(ARRAYSIZE(axes), axes);
				reading->GetControllerSwitchState(ARRAYSIZE(switches), switches);
				reading->GetControllerButtonState(ARRAYSIZE(buttons), buttons);
// Get number of axes, switches, buttons				
				nbraxes = reading->GetControllerAxisCount();
				nbrswch = reading->GetControllerSwitchCount();
				nbrbutt = reading->GetControllerButtonCount();
// Now print what we have captured from the GameInput input stream for this specific controller

// First the axes
				if (nbraxes > 0) {
					printf("  Axes - ");
					for (uint32_t axctr = 0; axctr < nbraxes; ++axctr) {
						printf("%d:%f ", axctr, axes[axctr]);
					} // end for axctr loop
				} else {
					printf (" No Axes ");
				}
// Second the switches
				if (nbrswch > 0) {
					printf("Switches - ");
					for (uint32_t swctr = 0; swctr < nbrswch; ++swctr) {
						printf("%d:%d ", swctr, switches[swctr]);
					} // end for swctr loop
				} else {
					printf (" No Swi  ");
				}
// Third the buttons
				if (nbrbutt > 0) {
					printf("Buttons - ");
					for (uint32_t btctr = 0; btctr < nbrbutt; ++btctr) {
						if (buttons[btctr]) printf("%d ", btctr);
					} // end for btctr loop
				} else {
					printf (" No Buttn");
				}
// puts just to print newline
				puts("");
// Release the instance "reading" of class IGameInputReading used for this cycle
				reading->Release();
			} else {
				printf("Nix Joystick found\n");
			}
		} // end for devctr loop

// exit whilectr loop if "wait" parameter not specified
		if ( ! waitfortrimwheel ) {
			if ( verbolvl > 0 ) {
				printf("\t#DBG %s@%d Leaving whilectr loop for no-wait-parm\n", __func__, __LINE__);
			}
			break; // exit whilectr loop
		}
// exit whilectr loop if exit key pressed
		bool exitkeyflag = false;
		while ( _kbhit() ) { // as long as there are keycodes in the input buffer
			keypressed = toupper(_getch());
			if ( verbolvl > 0 ) {
				printf("\t#DBG %s@%d Key pressed: %i = '%c'\n", __func__, __LINE__, keypressed, keypressed);
			}
			if (keypressed == exitkey) {
				exitkeyflag = true;
				printf("Exit-key '%c' detected, stopping loop\n",keypressed);
			}
		}
		if (exitkeyflag) {
			if ( verbolvl > 0 ) {
				printf("\t#DBG %s@%d leaving whilectr loop for exit-key\n", __func__, __LINE__, keypressed, keypressed);
			}
			break; // exit whilectr loop 
		}

// Wait a short moment, just not to overload our system
		Sleep(waitmsec); // Wait 500 msecs
	} // end for whilecounter loop
printf("End program\n") ;

}
