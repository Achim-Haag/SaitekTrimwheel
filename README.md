# SaitekTrimwheel - Saitek Pro Flight Trimwheel initialization checking

## Background and Problem

The Saitek Pro Flight Trimwheel is a USB controller with a single axis and no buttons or switches
to be used in flight simulators as a trimwheel.

This device has an implementation bug regarding its configuration at boot time:
__If this wheel isn't plugged after Windows has started, it is present but the axis doesn't change__

## Solution

If the problem occurs, the wheel has to be manually turned around some revolutions, best in both directions.
This seem to initialize the wheel's internal logic, so Windows detects its axis.

But: sometimes the wheel's axis starts to work after boot without turning, so it's not necessary to turn the wheel.

The only possibility to check the status of the wheel and its axis was to check the axis in the Windows "Devices and Printers",
a complicated way not convenient on each Windows startup.

## Helper program against Microsoft GameInput API V.0

I derived this helper program from the source https://github.com/MysteriousJ/Joystick-Input-Examples/blob/main/src/gameinput.cpp
to have a scriptable method to check the axis of the Saitek Pro Flight Trimwheel in a startup script.

It works by calling the Microsoft GameInput API V.0, this environment should be available at least in Windows 10 22H2
without further steps.

To understand the logic and the GameInput API, I stepped through the program and commented every step
comparing it with the Microsoft API documentation and added comments of what I have understood.
Afterwards, I changed the logic to process mainly the Saitek Trimwheel.

## Parameters

	-h : help
	-v : verbose, print additional msgs, reduces loop wait from 500 ms to 2 secs
	-s : silent, suppress while-cycle message written on each cycle loop
	-c <number of cycles> : cycle time in seconds
	-a : process all controllers settings, not only Saitek Trimwheel

## Return codes

	Return codes:
	* Trimwheel is not zero : RC=0
	* Trimwheel is zero : RC=1
	* Called with "-h" : RC=4
	* Parameter error : RC=8
	* Other errors : RC>8

## Example code from my startup script

    - to be done

## Notes regarding Microsoft GameInput

Although GameInput isn't part of Windows GDK anymore, I found an overview of GameInput in
https://learn.microsoft.com/en-us/gaming/gdk/docs/features/common/input/overviews/input-overview
and a detailed API V.0 description in
https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/gameinput_members

I wasn't able to migrate it to GameInput API V.1 as the steps weren't as easy as said in
https://learn.microsoft.com/en-us/gaming/gdk/docs/features/common/input/overviews/input-nuget
for me, unexperienced in C++.
* Hard to find only gameinput.lib and gameinput.h as they were part of a big package in
  https://www.nuget.org/packages/Microsoft.GameInput, but finally I found it by extracting with 7zip
  the folder "native" from microsoft.gameinput.1.2.26100.4782.nupkg with the GameInput V.1 API:
	native\include\GameInput.h		Änderungsdatum 17.04.25
	native\lib\x64\GameInput.lib	Änderungsdatum 17.04.25
* GetDeviceInfo switched from assignment to parameter (after long time, I found out)
* Crash in setup the GameInput's dispatcher

So I stopped the conversion and continued to use the V.0 gameinput.lib and src/gameinput.h I found here:
https://github.com/MysteriousJ/Joystick-Input-Examples/tree/main


