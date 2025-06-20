/*
	SaitekTrimwheel.cpp

	As this program is derived from https://github.com/MysteriousJ/Joystick-Input-Examples/blob/main/src/gameinput.cpp
	which is under MIT license, this program is published under MIT license too.

	Program to check the status of a Saitek ProFlight Trimwheel (connected by USB)
	as this device doesn't active its axis when plugged in before starting the computer,
	in this case (axis value = 0), the wheel has to be turned some revolutions.
	So this Program should detect the state of the trimwheel's axis and set a corresponding return code

	Problem: it seems, the Saitek Trimwheel's axis is always zero when this program starts
		and only gets non-zero if it's turned while we run.
	Solution: cycle always
		* Saitek Trimwheel detected and axis <> 0 (Trimwheel has been turned) or
		* until max ~24h or user defined seconds (whatever smaller) or
		* user quits with "Q" or
		* an error occurs

	Parameters:
	-h : help
	-v : verbose, print additional msgs, reduces loop wait from 500 ms to 2 secs
	-s : silent, suppress while-cycle message written on each cycle loop
	-c <number of cycles> : cycle time in seconds
	-a : process all controllers settings, not only Saitek Trimwheel

	Return codes:
	* Trimwheel is not zero : RC=0
	* Trimwheel is zero : RC=1
	* Called with "-h" : RC=4
	* Parameter error : RC=8
	* Other errors : RC>8

	Notes
	* Without wait flag (-w), all controllers are checked once
	  In case the trimwheel is found, its state determines the return code
	* With wait flag (-w), the last known state of the trimwheel determines the return code

	Modifications:
	13.05.25/AH derived from https://github.com/MysteriousJ/Joystick-Input-Examples/blob/main/src/gameinput.cpp
		copy of content of "gameinput.cpp"
	27.05.25/AH after commenting and modifying the source to understand its function
		and to get deeper in Microsoft Windows GameInput API, now changing for my needs.
	04.06.25/AH several programming, first final state
	08.06.25/AH further message improvements 
	19.06.25/AH message improvements; added "spinning wheel" while silent looping
	20.06.25/AH reworked hexadecimal printouts
	
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

// #############################################################################################################
// Global variables, mostly static
// #############################################################################################################

// Size of Structure"GameInputDeviceInfo" (GameInput.h) with device attribute structure 
static const int GmInpDevInfSize = sizeof(GameInputDeviceInfo);
// Allocate pointer to structure joydevinfo of type GameInputDeviceInfo to receive address of device data block from GetDeviceInfo()
// Has to be const as the device data block is owned by IGameInput object and must not be modified by application
static const GameInputDeviceInfo *joydevinfo;


// Saitek Proflight Trimwheel Vendor-ID (VID) and Product-ID (PID)
static const int saitektwvid = 0x6a3;
static const int saitektwpid = 0xbd4;

// Do we have Saitek Trimwheel attached ?
static bool saitektwfound = false;
// Saitek Trimwheel axis turned, so not equal to zero ?
static bool saitektwturned = false;
// Return state of Saitek Trimwheel to caller of main (should be set explicitly, else 99)
// If we find a Saitek Trimwheel, we return 0 (axis not zero) or 1 (axis is zero) to OS
// Any other return to OS sets a returncode 4 or higher
static int osretcode = 16; // Default if not set otherwise is return code 16 to OS

// Default readloops 86.400 (one day's seconds)
static const int readldflt = (24*60*60);

// To check function results by SUCCEEDED()
HRESULT retresult;

// Variables for processing GameInputDeviceInfo structure
static int memsize, vid, pid, rev, ifc, col = 0;

// Variables for processing axes, switches, buttons
static int nbraxes, nbrswch, nbrbutt = 0;

// Default: no verbosity
static int verbolvl = 0;
// while-cycle message suppression, default: messages supressed
static bool cyclemessages=true;
// Get axes, switches, buttons not only from Saitek Trimwheel but all controllers
static bool allcontrollers=false;

// Definition of exit key. temp stor for the user-pressed key
static const int exitkey = 'Q';
static int keypressed = 0;

// #############################################################################################################
// Spinning wheel on console from
// https://stackoverflow.com/questions/199336/print-spinning-cursor-in-a-terminal-running-application-using-c
// #############################################################################################################
void advance_cursor() {
  static int pos=0;
  char cursor[4]={'/','-','\\','|'};
  printf("%c\b", cursor[pos]);
  fflush(stdout);
  pos = (pos+1) % 4;
}


// #############################################################################################################
// Start of asynchronous subroutine
// #############################################################################################################
// see https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/functions/gameinputdevicecallback
// For status enumeration see https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/enums/gameinputdevicestatus
//
// This routine isn't really called async as it is registered as "GameInputBlockingEnumeration"
// From GameInput, it's called a first time at 'registerDeviceCallback' for all devices, then after an event happened
//
// In every call, we receive this information:
// - Callback-Token (given when this routine was registered by RegisterDeviceCallback)
// - context = &joysticks = address of pointer 'joysticks'
//			(important informations that we have specified in RegisterDeviceCallback as parameter 5)
// - IGameInputDevice* (pointer to a specific controller that has changed its state)
// - Current state (connection and input status) of this controller
// - Previous state (connection and input status) of this controller
//
// Our processing counts the number of devices given, then reallocates (or allocates if previously not allocated [pointer is null]) the device
//
void CALLBACK deviceChangeCallback(GameInputCallbackToken callbackToken, void* context, IGameInputDevice* singledevice, uint64_t timestamp, GameInputDeviceStatus currentStatus, GameInputDeviceStatus previousStatus) 
{ 
// Print VID/PID of controller that has changed its status
	const GameInputDeviceInfo *joydevchgd	 = NULL;
	joydevchgd = singledevice->GetDeviceInfo();
	int vidchgd = joydevchgd->vendorId;
	int pidchgd = joydevchgd->productId;
// Hex chars: "%#"" -> "0x" -> counts as 2 digits ! So  %#04X prints "0x" + 4 digits, e.g. 0x3456 ;
// What not worked: As I want leading zeroes not leading spaces, I have to add a zero behing %#06 :  %#060x
// And in big letters (A instead of a), I have to use big X instead of little x
// But disadvantage: the prefix 0x is changed to uppercase 0X too, so I choose a clearer definition and changed it to 0x%04X : 0xABCD
    printf("Callback Subroutine: device state change for VID: 0x%04X, PID: 0x%04X\n", vidchgd, pidchgd);
//
	if ( verbolvl > 0 ) {
		printf("\t#DBG1 %s@%d ### callbk sub: routine starting (async)\n", __func__, __LINE__);
	}
// Access main pgm's "joysticks" array (of controllers) by copying main-routine's 'joysticks' pointer to the function-local (!) pointer 'joyarray'
	Joystruct* joyarray = (Joystruct*)context;
// currentStatus :
//		GameInputDeviceNoStatus = 0x00000000
//		GameInputDeviceConnected = 0x00000001
//		then other status = 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, 0x100000
//		and GameInputDeviceAnyStatus = 0x00FFFFFF
// GameInputDeviceConnected : 0x00000001
// so the "if" (0 = false, any other value = true), after the boolean (!) "and" executes its tree when flag "device is connected" is set
// meaning: the "if" executes its tree as a (new) device connects
	if (currentStatus & GameInputDeviceConnected) {
// Compare all actual devices with the delivered device that has changed its status
		for (uint32_t devctrcompare = 0; devctrcompare < joyarray->deviceCount; ++devctrcompare) {
// Check if the new contoller device is already in our list of controllers, if so, do nothing and return to caller
			if ( verbolvl > 0 )	{
				printf("\t#DBG1 %s@%d ### callbk sub: checking device %i\n", __func__, __LINE__, devctrcompare);
			}
			if (joyarray->devices[devctrcompare] == singledevice) {
				if ( verbolvl > 0 ) {
					printf("\t#DBG1 %s@%d ### callbk sub: routine leaving, joystick unchanged %i\n", __func__, __LINE__, devctrcompare);
				}
				return;
			}
		} // end for-loop over controller devices
// We have found a new device, so re-allocate our joystick list with the additional joystick definition

// add 1 to number of controllers
		++joyarray->deviceCount;
		if ( verbolvl > 0 ) {
			printf("\t#DBG1 %s@%d ### callbk sub: Joystick %i added\n", __func__, __LINE__,joyarray->deviceCount);
		}
// now realloc (resize) our list of controllers (add/change memory for the new controller)
		joyarray->devices = (IGameInputDevice**)realloc(joyarray->devices, joyarray->deviceCount * sizeof(IGameInputDevice*));
// and add the given new/changed device definition to the reallocated or newly allocated element of controller array
// Array handling as of devicecount starts at 1 but array index starts at 0:
// controller 1 to element 0, controller 2 to element 1 aso., therefore deviceCount-1)
// Get GameInputDeviceInfo contents by IGameInputDevice.GetDeviceInfo() into structure joydevinfo
		joyarray->devices[joyarray->deviceCount-1] = singledevice;
	} else {
		if ( verbolvl > 0 ) {
			printf("\t#DBG1 %s@%d ### callbk sub: no change detected (currentStatus: %i)\n", __func__, __LINE__, currentStatus);
		}
	}
	if ( verbolvl > 0 ) {
		printf("\t#DBG1 %s@%d ### callbk sub: routine leaving, normal end\n", __func__, __LINE__);
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
    	printf("\t#DBG1 %s@%d # Starting main()\n", __func__, __LINE__);
  	}	

// #############################################################################################################
// Process commandline options
// #############################################################################################################
/*
  Process commandline parameters  (argc = count, argv = pointer array) with Windows-specific getopt.c
  getopt is a well-known function in the Unix environment and shells to parse argument lists,
  see https://en.wikipedia.org/wiki/Getopt 
  or https://www.gnu.org/software/libc/manual/html_node/Example-of-Getopt.html (the sample I used here)
  The "main" function has to be defined with arguments "(int argc, char** argv)" (char** is a pointer to a pointer list)
*/
 	if ( verbolvl > 0 ) {
    	printf("\t#DBG1 %s@%d # Process commandline parameters by getopt.c\n", __func__, __LINE__);
  	}	

// Sleeptime in msecs for while-loop. Real readloop count is calculated before for-readloopctr loop
	int waitmsec = 1000 ;		// Loop in 1 second cycles
	int waitmsvb = 2000 ;		// Loop in 2 second cycles (verbose flag)
	int readloops = readldflt;	// default loop length: 24 hours = 86.400 Seconds
	int userloops = 0;			// user specified loop length in cycles
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
/* Implemented: "-h" = help; "-v" = verbosity (lvl increased by multiple occurences); "-c ###" = cycle ### seconds */
/* The colon after an option requests a value behind an option character */
	while ((cmdline_arg = getopt (argc, argv, "hvsc:a")) != -1) 	{
// As we don't have here a valid verbolvl, I leave this debugging statement as comment:
// printf("### Entering next getopts loop (while), cmdline_arg = %d = %c\n", cmdline_arg, cmdline_arg);
    	switch (cmdline_arg) {
		case 'h':                     // Option -h -> Help
        	printf("Processing Saitek ProFlight Trimwheel (VID 0x%04X, PID 0x%04X) axis\n"
           		"derived from https://github.com/MysteriousJ/Joystick-Input-Examples by Achim Haag\n"
           		"Allowed commandline parameters:\n"
           		"-h : this help\n"
           		"-v : debugging msgs, level increased by multiple occurences; changes loop-wait from %ims to %ims\n"
           		"-s : silent loop, don't write cycle messages\n"
           		"-c <###>: cycle for ### seconds (otherwise default: %i) until exit key %c pressed\n"
				"-a : process all controllers (axis, switches, buttons), not only trimwheel\n"
           		"Retcode: 0 = axis not zero (OK); 1 = axis zero; 4 = help ; 8 = parameter error, >8  = other errors\n",
				saitektwvid, saitektwpid, waitmsec, waitmsvb, readldflt, exitkey
			);
			osretcode = 4;
        	return osretcode; // !!! Attention !!! Early return to OS
        	break;    // break switch-branch, never reached because of return to OS
		case 'v':	                   // Option -v -> Verbosity, each occurence increases verbosity by 1 up to max. 9)
			waitmsec = waitmsvb;
        	if (verbolvl < 9) {
        		verbolvl = ++verbolvl;   // variable optarg definition in getopt.h, returned from compiled getopt function
        		printf("Verbosity increased to %i, loop sleep set to %i msecs\n", verbolvl, waitmsec);
        	}
        	break;    // break switch-branch
      	case 's':                     // Option -s -> suppress cycle related messages
        	printf("Suppression of cycle messages\n");
        	cyclemessages=false;
        	break;    // break switch-branch
      	case 'c':                     // Option -c ### -> cycle loop for ### seconds
	        userloops=atoi(optarg);   // optarg defined by getopt.h, returned from getopt
        	if ((userloops > 0) && (userloops <= readloops)) {
				readloops=userloops;
        		printf("Cycles set to %i\n", readloops);
			} else {
        		printf("Cycles out of range (0...%i), kept %i\n", readloops, readloops);
			}
        	break;    // break switch-branch
      	case 'a':                     // Option -a -> process all controllers
        	printf("Processing information of all controllers\n");
        	allcontrollers=true;
        	break;    // break switch-branch
      	case '?':                     // Any other commandline parameter error
        	if (optopt == 'v' || optopt == 'f') {         // optopt: Parameter in error, here -v without following number
          		fprintf(stderr, "Option -%c requires an argument. Try -h !\n", optopt);
        	} else if (isprint (optopt)) {    // here we found a parameter not specified in the third getopt argument (string, see above)
          		fprintf(stderr, "Unknown option '-%c'. Try -h !\n", optopt);
        	} else {                    // Any other getopt error - exit program
          		fprintf(stderr, "Bad option character value %#040x, try -h !\n", optopt);
        	} // endif
			osretcode = 8;
			return osretcode; // !!! Attention !!! Early return to OS
        	break;    // break switch-branch, never reached because of return to OS
      	default:                      // Parameter allowed but not handled - this should not occur
        	printf("Parameter %c not handled, contact programmer !", cmdline_arg);
			osretcode = 8;
			return osretcode; // !!! Attention !!! Early return to OS
        	break;    // break switch-branch, never reached because of return to OS
    	} // end switch
  	} // end while

	if ( verbolvl > 0 ) {
    	printf("Unprocessed commmandline parameters (%d parameters):\n", optind);
    	for (int index = optind; index < argc; index++) printf ("Non-option argument [%s]\n", argv[index]);
  	}

// #############################################################################################################
// Setup Microsoft GameInput V.0 interface
// #############################################################################################################

// Define joysticks as structure and initialize to zero both subfields (deviceCount, pointer to array of pointers to the specific devinces)
	Joystruct joysticks = {0};
	if ( verbolvl > 1 ) {
		printf("\t#DBG2 %s@%d Structure 'joysticks' allocated, size is %zu (expect 16 as 64bit ptr is 8bit aligned !)\n", __func__, __LINE__, sizeof(joysticks));
  	}

// The following two statements define the access to the device input stream
// gminputptr points to the IGameInput object

// Create pointer 'gminputptr' for later use to access object of class IGameInput (per-process singleton instance to access the device input stream)
// see https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/interfaces/igameinput/igameinput
	IGameInput* gminputptr;
	if ( verbolvl > 1 ) {
		printf("\t#DBG2 %s@%d Pointer 'gminputptr' to IGameInput allocated, size is %zu\n", __func__, __LINE__, sizeof(gminputptr));
  	}

// Call function to setup the IGameInput Interface, returns address of instance of class IGameInput in pointer 'gminputptr'.
// From this point, the GameInput API can be accessed by ptr 'gminputptr'
// see https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/functions/gameinputcreate
	retresult = GameInputCreate(&gminputptr);		// GameInput API V.0 function
	if (! SUCCEEDED(retresult)) {
		printf("Error from GameInputCreate: 0x%x\n",retresult);
		osretcode = 12;
		return osretcode; // !!! Attention !!! Early return to OS
	}
	if ( verbolvl > 1 ) {
		printf("\t#DBG2 %s@%d Created instance 'IGameInput', struc size is %zu, 'gminputptr', ptr points to %p\n", __func__, __LINE__, sizeof(IGameInput), (void*)gminputptr);
  	}
// The following three statements define the Callback-Interface Subroutine, that is called asynchron (= out of order)
// each time the device definitions are changed (e.g. another controller added)

// Create pointer "dispatcher" to object instance of class IGameInputDispatcher,
// see https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/interfaces/igameinputdispatcher/igameinputdispatcher
// By referencing IGameInputDispatcher, GameInput changes from "automatic mode" to "manual mode"
// so we have to schedule the background work of the GameInput API later manually
	IGameInputDispatcher* dispatcher;
// Create a Dispatcher for manually scheduling GameInput background work (switching GameInput to "manual dispatch mode")
// Use method "CreateDispatcher" of (addressed by gminputptr) IGameInput
// see https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/interfaces/igameinput/methods/igameinput_createdispatcher
	retresult = gminputptr->CreateDispatcher(&dispatcher);
	if (! SUCCEEDED(retresult)) {
		printf("Error from CreateDispatcher: 0x%x\n",retresult);
		osretcode = 12;
		return osretcode; // !!! Attention !!! Early return to OS
	}

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
		printf("\t#DBG1 %s@%d Registering async callback procedure 'deviceChangeCallback'\n", __func__, __LINE__);
	}
	gminputptr->RegisterDeviceCallback(0, GameInputKindController, GameInputDeviceAnyStatus, GameInputBlockingEnumeration, &joysticks, deviceChangeCallback, &callbackId);
	if ( verbolvl > 0 ) {
		printf("\t#DBG1 %s@%d Registering async callback done, should have run the callbk routine\n", __func__, __LINE__);
	}

// Define array of one controllers buttons as up to 64 button states
	bool buttons[64];
// Define array of one controllers switches array as enumerator of up to 64 switch states
// (e.g. GameInputSwitchCenter = 0, GameInputSwitchUp = 1, ...)
// see https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/enums/gameinputswitchposition
	GameInputSwitchPosition switches[64];

// Define array of one controllers axes as up to 64 axes floating point values
	float axes[64];

	printf("Starting Cycle-Loop for up to %i cycles with sleep %i msecs\n", readloops,waitmsec);
	printf("Press exit-key '%c' to interrupt if you don't like to run it a whole day ;-)\n", exitkey);

// #############################################################################################################
// Main processing Loop
// #############################################################################################################

// Cycle loop every second (-v : every two seconds)
	for (int readloopctr = 1 ; readloopctr <= readloops ; readloopctr++)	{
		saitektwfound = false;
		if (cyclemessages) {
			printf("\n*** Cycle %i of %i, exit='%c' ***\n", readloopctr, readloops, exitkey);
		} else if ( verbolvl > 0 ) {
			printf("\n\t#DBG1 %s@%d *** while-Cycle %i ***\n", __func__, __LINE__, readloopctr);
		} else {				// no cycle messages and no verbosity (totally silent):
			advance_cursor();	// show a spinning wheel (afterwards cursor on last wheel character)
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
		if ( verbolvl > 1 ) {
			printf("\t#DBG2 %s@%d Calling GameInput dispatcher\n", __func__, __LINE__);
		}
		bool dispretc = dispatcher->Dispatch(0);
		if ( verbolvl > 0 ) {
			printf("\t#DBG1 %s@%d GameInput dispatcher work to do: %s\n", __func__, __LINE__, dispretc ? "yes" : "no");
		}

// #############################################################################################################
// Controller devices processing loop
// #############################################################################################################

// Now let's start the processing of the "GameInput stream" for a specific controller device,
// a continuous data stream that consists of every action (buttons, switches, axis) on all filtered devices
		if ( verbolvl > 0 ) {
			printf("\t#DBG1 %s@%d Starting for-Loop over %i Joystick devices\n", __func__, __LINE__, joysticks.deviceCount);
		}
		for (uint32_t devctr = 0; devctr < joysticks.deviceCount; ++devctr)	{
// Assume current device VID / PID unknown			
			vid=0;
			pid=0;
// Define "reading" as instance of class IGameInputReading and capture/process controllers raw input data from the controllers...
// Every input state change received from a device is captured in an IGameInputReading instance. 
			IGameInputReading* reading;
			if ( verbolvl > 1 ) {
				printf("\t#DBG2 %s@%d Pointer 'reading' to IGameInputReading allocated, size is %zu\n", __func__, __LINE__, sizeof(gminputptr));
  			}

// ...and call moethod "GetCurrentReading" of object instance "input" to initially access the controller input stream, see
// https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/enums/gameinputkind
// Filter is GameInputKindController: "Combination of Axis, Button, and Switch"
// (another possible filter for Saitek Trimwheel would be "GameInputKindControllerAxis - Controller input from sticks")
// Optional filter is 'joysticks.devices[devctr]', so information is returned only for the specific controller in our joysticks list
//
// The joysticks list is a pointer array addressed by pointer 'joysticks.devices' and is built by the first call of our callback routine
// As we specified 'GameInputBlockingEnumeration' for 'RegisterDeviceCallback',  an initial call for the callback routine
// is made for every controller device at 'RegisterDeviceCallback', so our callback routine can build our pointer array

//
// The device is selected by inner loop variable i (so controller devices are processed sequentially)
// The last parameter (object instance "reading") is "the input reading to be returned".
// Returns NULL on failure
//
// SUCCEEDED seems to be a macro of winerror.h - although winerror.h isn't included
// Location: C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\shared\winerror.h
// Probably from other (nested) #include
//
			if (SUCCEEDED(gminputptr->GetCurrentReading(GameInputKindController, joysticks.devices[devctr], &reading)))	{
				if ( verbolvl > 1 ) {
					printf("\t#DBG2 %s@%d Created instance 'IGameInputReading', struc size is %zu, 'reading' ptr points to %p\n", __func__, __LINE__, sizeof(IGameInputReading), (void*)reading);
				}
				if ( cyclemessages) {
					printf("--- Processing Controller %d ---\n", devctr);
				}
// Check device information for actual controller joysticks.devices[i], according to
// https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/interfaces/igameinputdevice/methods/igameinputdevice_getdeviceinfo
// https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/structs/gameinputdeviceinfo
// We have to find the Saitek ProFlight Cessna Trim Wheel (VID: 0x6A3, PID: 0xBD4)
				if ( verbolvl > 0 ) {
					printf("\t#DBG1 %s@%d Now GetDeviceInfo for ctrl %i\n", __func__, __LINE__, devctr);
				}
// Get GameInputDeviceInfo contents by IGameInputDevice.GetDeviceInfo() into structure joydevinfo
				joydevinfo = NULL;
				joydevinfo = joysticks.devices[devctr]->GetDeviceInfo();
// Valid address returned from GetDeviceInfo ?				
				if (joydevinfo != NULL) {
// Then check if size of returned data block is large enough
					memsize = joydevinfo->infoSize;
// Check returned structure at least as big as the first fields we want to process (should always happen)
					if (memsize >= GmInpDevInfSize ) {
						if ( verbolvl > 0 ) {
							printf("\t#DBG1 %s@%d structure length %i vs. SizeOf: %i)\n", __func__, __LINE__, memsize, GmInpDevInfSize);
						}
						vid = joydevinfo->vendorId;
						pid = joydevinfo->productId;
						rev = joydevinfo->revisionNumber;
						ifc = joydevinfo->interfaceNumber;
						col = joydevinfo->collectionNumber;

// #############################################################################################################
// Not working: get device name from GameInput DeviceInfo
// #############################################################################################################

// Not implemented by Microsoft in DirectInput API V.0 ; removed by Microsoft in DirectInput API V.1 !
// Maybe a search in these two registry locations would have solved it:
// 1. HKCU\System\CurrentControlSet\Control\MediaResources\Joystick\DINPUT.DLL\CurrentJoystickSettings : Joystick1OEMName
// 2. HKCU\System\CurrentControlSet\Control\MediaProperties\PrivateProperties\Joystick\OEM\VID_...&PID_...\OEMName
// but that's beyond the scope of this "check script", so I left my debug statements (verbosity level 3 : -vvv)
//
// Linkage to device name:
// joydevinfo : pointer to device data block structure GameInputDeviceInfo			
// dispnameptr : pointer to displayName data block, address from pointer GameInputDeviceInfo.displayName
// displayName : pointer to structure of type GameInputString with 
//					"uint32_t sizeInBytes" : string size, "uint32_t codePointCount" : number of unicode characters
//					and "char cont* data" : UTF-8 encoded Unicode string
// But ! It seems, displayName is always a Nullpointer (see also https://github.com/microsoft/GDK/issues/35)
// So only if verbosity level 3 (-vvv) is selected: print the GamInputDeviceInfo structure
						if ( verbolvl > 2 ) {
							int singlechar;
// Load a pointer with the starting address of the GameInputDeviceInfo structure (pointed by joydevinfo)
// The pointer points to "unsigned char", so we can easily print each byte
							unsigned char *joyptr = (unsigned char *) &(joydevinfo->infoSize);
							printf("\t#DBG3 %s@%d Dumping structure GameInputDeviceInfo\n", __func__, __LINE__);
							printf("\t#DBG3 %s@%d joydevinfo pts to %p, joyptr to %p\n", __func__, __LINE__, (void *) joydevinfo, (void *) joyptr);
							for (int ix = 1 ; ix < GmInpDevInfSize ; ++ix) {
								singlechar = joyptr[0];
								printf("\t#DBG3 %s@%d ix=%03i joyptr=%p byte: dec=%03i, hex=[%020x], char=[%c]\n", __func__, __LINE__, ix-1, joyptr, singlechar, joyptr[0], joyptr[0]);
								joyptr++;
							}
// Load a pointer with the address of the displayName structure
							const GameInputString *dispnameptr = joydevinfo->displayName;
							printf("\t#DBG3 %s@%d Dumping substructure GameInputDeviceInfo.displayName\n", __func__, __LINE__ );
							printf("\t#DBG3 %s@%d dispnameptr (loaded from %p) points to %p\n", __func__, __LINE__, (void *) &(joydevinfo->displayName), (void *) dispnameptr);
							if (dispnameptr != NULL) {
								for (int ix = 1 ; ix < 8 ; ++ix) {
									singlechar = (char) dispnameptr->data[0];
									printf("\t#DBG3 %s@%d ix=%i dispnmptr=%p char=[%020x]\n", __func__, __LINE__, ix, dispnameptr, singlechar);
									dispnameptr++;
								}
							} else {
								printf("\t#DBG3 %s@%d dispnameptr is zero, displayName structure not accessible\n", __func__, __LINE__);
							}
						}

// #############################################################################################################
// Saitek Trimwheel VID_06A&PID_0BD4 specific processing
// #############################################################################################################

// We have found the Saitek Trimwheel by VID/PID ?
						if ( (vid = saitektwvid) && (pid == saitektwpid) ) {
							saitektwfound = true ;
						}
						if ( verbolvl > 0 ) {
							printf("\t#DBG1 %s@%d InfoSize: %i, VID: 0x%04X, PID: 0x%04X, REV: 0x%04X, IFC: 0x%04X, COL: 0x%04X\n", __func__, __LINE__, 
									memsize, vid, pid, rev, ifc, col);
						}
					} else {
						if ( verbolvl > 0 ) {
							printf("\t#DBG1 %s@%d GetDeviceInfo() gives structure too short in length (%i vs. SizeOf: %i)\n", __func__, __LINE__, 
									memsize, GmInpDevInfSize);
						}
						printf("Cannot get information for ctrl %i)\n", devctr);
					}
				} else {
					printf("No pointer returned from GetDeviceInfo() to joydevptr \n");
				}

// see https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/interfaces/igameinputreading/methods/igameinputreading_getcontrolleraxisstate
// Capture the axes, switches and buttons of the specific oysticks.devices[i]" controller into our own arrays
				if ( verbolvl > 0 ) {
					printf("\t#DBG1 %s@%d Get axes, switches and buttons for ctrl %i\n", __func__, __LINE__, devctr);
				}
// Only if allcontrollers-flag set or (in any case) Saitek Trimwheel

				if ( allcontrollers || ((vid = saitektwvid) && (pid == saitektwpid)) ) {
					if (cyclemessages) {
						printf("Controller %i (VID: 0x%04X, PID: 0x%04X):\t", devctr, vid, pid);
					}
					reading->GetControllerAxisState(ARRAYSIZE(axes), axes);
					reading->GetControllerSwitchState(ARRAYSIZE(switches), switches);
					reading->GetControllerButtonState(ARRAYSIZE(buttons), buttons);
// Get number of axes, switches, buttons				
					nbraxes = reading->GetControllerAxisCount();
					nbrswch = reading->GetControllerSwitchCount();
					nbrbutt = reading->GetControllerButtonCount();
// If not suppressed: print what we have captured from the GameInput input stream for this specific controller
					if (cyclemessages) {
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
					}
// Release the instance "reading" of class IGameInputReading used for this cycle
					reading->Release();

// Now processing the Saitek Trimwheel if found: has only axes[0]
					if ( (vid = saitektwvid) && (pid == saitektwpid) ) {
						if ( verbolvl > 0 ) {
							printf("\t#DBG1 %s@%d Saitek Trimwheel found, VID: 0x%04X, PID: 0x%04X, axis value: %f\n", __func__, __LINE__, vid, pid, axes[0]);
						}
						if ( axes[0] != 0 ) {
							osretcode = 0;		// Trimwheel axis 0 not equal 0 : wheel is initialized
							saitektwturned = true;
							if ( verbolvl > 0 ) {
								printf("\t#DBG1 %s@%d Saitek Trimwheel seems initialized, osretcode=%i\n", __func__, __LINE__, osretcode);
							}
						} else {
							osretcode = 1;		// Trimwheel axis 0 equal 0 : uncertain about wheel initialized
							if ( verbolvl > 0 ) {
								printf("\t#DBG1 %s@%d Saitek Trimwheel axis is zero, osretcode=%i\n", __func__, __LINE__, osretcode);
							}
						}
					}
				}
			} else {
				if ( cyclemessages) {
					printf("GetCurrentReading without success for Game controller %d\n", devctr);
				}
			}

// Tried to get raw data, so maybe the Trimwheel hasn't to be rotated to get its actual axis value
// But it seems the API documentation is correct:
// https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/interfaces/igameinputrawdevicereport/igameinputrawdevicereport
// -> Note: This interface is not yet implemented.
/*
			retresult = gminputptr->GetCurrentReading(GameInputKindRawDeviceReport, joysticks.devices[devctr], &reading);
			printf("Raw data by 'GetCurrentReading(GameInputKindRawDeviceReport', HRESULT=%x\n", retresult);
*/			

		} // end for devctr loop

// Check if we have found Sait		
		if (! saitektwfound) {
			printf("Saitek Trimwheel not found (assume VID: 0x%04X, PID: 0x%04X)\n", saitektwvid, saitektwpid);
		}
		
// exit for-readloopctr loop if Saitek Trimwheel found to be turned
		if (saitektwturned) {
			if ( verbolvl > 0 ) {
				printf("\t#DBG1 %s@%d Leaving for-readloopctr loop for Trimwheel axis not equal to zero\n", __func__, __LINE__);
			}
			break; // exit for-readloopctr loop
		}
// exit for-readloopctr loop if exit key pressed
		bool exitkeyflag = false;
		while ( _kbhit() ) { // as long as there are keycodes in the input buffer
			keypressed = toupper(_getch());
			if ( verbolvl > 0 ) {
				printf("\t#DBG1 %s@%d Key pressed: %i = '%c'\n", __func__, __LINE__, keypressed, keypressed);
			}
			if (keypressed == exitkey) {
				exitkeyflag = true;
				printf("Exit-key '%c' detected, stopping loop\n",keypressed);
			}
		}
		if (exitkeyflag) {
			if ( verbolvl > 0 ) {
				printf("\t#DBG1 %s@%d leaving for-readloopctr loop for exit-key, osretcode=%i\n", __func__, __LINE__, keypressed, keypressed, osretcode);
			}
			break; // exit for-readloopctr loop 
		}

// Wait a short moment, just not to overload our system
		if ( verbolvl > 1 ) {
			printf("\t#DBG2 %s@%d Sleeping for %i msecs\n", __func__, __LINE__, waitmsec);
		}
		Sleep(waitmsec); // Wait 500 msecs
	} // end for readloopctr loop
// Return to OS
	printf("End program, RC=%i\n", osretcode) ;
	return osretcode;
} // end main