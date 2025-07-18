# Saitek Pro Flight Trimwheel initialization checking

## Background

The Saitek Pro Flight Trimwheel is a USB controller with a single axis and no buttons or switches,
this axis is used in flight simulators as a [trim wheel axis](https://pilotinstitute.com/aircraft-trim-explained/).

## Problem

This USB device has a small implementation bug concerning its configuration at boot time:
> [!IMPORTANT]
> __If the wheel is plugged in before Windows has been started, it is shown in "*Devices and Printers*", but the axis doesn't change its value, it stays on zero until the wheel is turned some revolutions__.

## Solutions

### 1 - Powershell : bus number change - unreliable

As the problem occurs in my system on nearly every reboot, after Windows is running, the wheel has to be __manually turned around some revolutions__ , preferably in both directions. This re-initializes the wheel's internal logic that lead in most cases to a changed "bus number". Afterwards, its axis values are reported back to Windows.

But as sometimes the wheel's axis starts to work without manual turning, it's not always necessary to turn it.

The only reliable working possibility to check the correct status of the wheel and its axis was to check it in the Windows "*Devices and Printers*" menu that lead to a bunch of clicks, complicated and inconvenient.

As a first workaround, I found the most-time increase of the device's bus number after re-initalization and put it into my Windows startup script:
```
set "busnomin=3"
set "pwsp=-NoProfile -NoLogo"
set "trimvid=06A3"
set "trimpid=0BD4"
set "mytrimwh="
For /F %%R In (
  'powershell.exe %pwsp% -Command "Get-PnpDevice -PresentOnly | Where-Object { ($_.InstanceId -match '^HID\\VID_%trimvid%&PID_%trimpid%') } | select -expandproperty InstanceId"'
) do Set "mytrimwh=%%R"
if not Defined mytrimwh goto nixtrimvid
powershell.exe %pwsp% -Command "$busnbr=Get-PnpDeviceProperty -InstanceId '%mytrimwh%' -Keyname 'DEVPKEY_Device_BusNumber' | select -ExpandProperty data; exit $busnbr"
set busno=%ERRORLEVEL%
set busnoinit=%busno%
if %busno% EQU %busnoinit% if %busno% LEQ %busnomin% goto asktrimwheel
```

### 2 - SaitekTrimwheel.exe : check axis value

As the checking by bus number worked not in all cases, I decided to check the change of the axis value directly, but I didn't find a simple, working tool.
So I started to find a simple method and ended up at Microsoft GameInput and found a working sample in C++.

# SaitekTrimwheel.cpp - Description

I derived the source code by copying https://github.com/MysteriousJ/Joystick-Input-Examples/blob/main/src/gameinput.cpp
to SaitekTrimwheel.cpp and started to rework.

The program calls the Microsoft GameInput API V.0, this environment is available at least in my Windows 10 22H2.

To understand the logic and the GameInput API, I analyzed the contents of my copy and commented every step
according to the Microsoft API documentation. I added comments of what I have understood.
Afterwards, I changed my source's logic to process mainly the Saitek Trimwheel.

## Parameters

I extract commandline parameters by getopt.c from https://github.com/alex85k/wingetopt/tree/master

	-h : help
	-a : process all controllers settings, not only Saitek Trimwheel
	-c <number of cycles> : cycle for ### seconds, default about 24 hrs (until exit key 'Q' pressed)
	-s : silent loop, don't write cycle messages
  -t : play tone when trimwheel should be turned and on exit
	-v : verbose, debugging msgs, level increased by multiple occurences; changes loop-wait too

## Return codes

	Return codes:
	* Trimwheel axis is not zero : RC=0
	* Trimwheel axis is not detected or zero : RC=1
	* Called with "-h" : RC=4
	* Parameter error : RC=8
	* Other errors : RC>8

## Calling example from my Windows .bat script

Hint for german keybord users: "diamond with question mark" is the ESC character (027 or 0x1B) and used to set ANSI colors
* Num-Lock active (light on)
* Windows Command Prompt
* Hold left "Alt" key, then type 0 2 7 on the numeric keypad to the right, do not use "Alt-Gr"
* Character "^[" should appear, copy
* Edit this line to "echo ^[ > esc.txt" and execute this command
* Use Notepad++ to further process esc.txt as it shows it correct as "ESC"
 
```
REM Part I - Other steps before checking the trimwheel
...

REM Part II - Check the Saitek Pro Flight Trimwheel by SaitekTrimwheel.exe
:trimwheel
echo Checking Saitek Trimwheel...
if not exist %~dp0\SaitekTrimwheel.exe (
 echo Missing program: %~dp0SaitekTrimwheel.exe
 goto endtwcheck
)

:newtwcheck
REM First request to turn the Trimwheel
echo [30m
powershell.exe -Command "$text=\"Now $env:username, please turn the Sai-tec Trimwheel after the beep...\" ; $voice = New-Object -ComObject Sapi.spvoice ; $voice.rate = 0 ; $rc=$voice.speak($text)"
echo [0m
:newtwchknext
echo [97m
echo Please turn the Saitek Trimwheel some revolutions !
echo [0m
REM Beep
powershell.exe -Command "[console]::beep(500,1000)" > nul
REM SaitekTrimwheel.exe : RC=0 : Trimwheel axis <> 0 (OK), RC=1 : Trimwheel axis = 0 (wheel not turned), RC=4 : -h = help, RC=16 : other error
%~dp0\SaitekTrimwheel.exe -s -c20
REM Trimwheel found, axis = 0 (RC=1 : wheel not turned)
if %errorlevel% == 1 (
  echo [97m
  echo Saitek Trimwheel existing but not turned
  echo [0m
  echo [30m
  powershell.exe -Command "$text=\"Come on, $env:username, turn the Sai-tec Trimwheel a little bit...\" ; $voice = New-Object -ComObject Sapi.spvoice ; $voice.rate = 0 ; $rc=$voice.speak($text)"
  echo [0m
  goto newtwchknext 
)
REM Trimwheel found, axis <> 0 (RC=0 : OK)
if %errorlevel% == 0 (
  echo [97m
  echo Saitek Trimwheel ready for use
  echo [0m
  echo [30m
  powershell.exe -Command "$text=\"Thank you, $env:username, the Sai-tec Trimwheel seems to be ready for use...\" ; $voice = New-Object -ComObject Sapi.spvoice ; $voice.rate = 0 ; $rc=$voice.speak($text)"
  echo [0m
  goto final
)
REM Other error (other RC's > 1)
REM e.g. Trimwheel not plugged in
echo [97m
echo Returncode %errorlevel% from SaitekTrimwheel.exe
echo [0m
echo [30m
powershell.exe -Command "$text=\"The Sai-tec Trimwheel check returned error %errorlevel%, really check once more ?\" ; $voice = New-Object -ComObject Sapi.spvoice ; $voice.rate = 0 ; $rc=$voice.speak($text)"
echo [0m

REM Ask user: repeat the check ?
echo [93m
CHOICE /T 5 /C NYX /D N /M "Check once more ? "
echo [0m
REM N: errorlevel 1 = Back to Weiter zu endtwcheck (next work to do)
REM Y: errorlevel 2 = Back to trimwheel check (check once more))
REM X: errorlevel 3 = End of script
REM Attention ! "If Errorlevel x" -> Errorlevel greater equal to x ! So check in descending order !
If Errorlevel 3 goto final
If Errorlevel 2 goto trimwheel
If Errorlevel 1 goto endtwcheck
:endtwcheck

REM Part III - remaining steps not related to Saitek Trimwheel
...
:final
```

## Notes

### Microsoft GameInput API V.0

Although GameInput isn't part of Windows GDK anymore, I found an overview of GameInput in
https://learn.microsoft.com/en-us/gaming/gdk/docs/features/common/input/overviews/input-overview
and a detailed API V.0 description in
https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/gameinput_members

### Microsoft GameInput API V.1

I wasn't able to migrate it to GameInput API V.1 as the steps weren't as easy as said in
https://learn.microsoft.com/en-us/gaming/gdk/docs/features/common/input/overviews/input-nuget
for me, unexperienced in C++.
* Hard to find only gameinput.lib and gameinput.h as they were part of a big package in
  https://www.nuget.org/packages/Microsoft.GameInput, but finally I found it by extracting with 7zip
  the folder "native" from _microsoft.gameinput.1.2.26100.4782.nupkg_ with the GameInput V.1 API:
  ```
  native\include\GameInput.h
  native\lib\x64\GameInput.lib
  ```
* GetDeviceInfo switched from assignment to parameter (as a non-C++ programmer, after long time, I found out)
* Crash when calling the GameInput's dispatcher

So I dropped the planned conversion to API V.1 and continued to use the V.0 gameinput.lib/gameinput.h found in https://github.com/MysteriousJ/Joystick-Input-Examples/tree/main

### CMake Release vs. Debug

As I started to dive a little deeper into CMake, two modules are generated and copied to the source folder by the CMake build process:
* GameInput.exe	-> release version, should run in Windows 10 (22H2 here), runs in my non-development gaming rig
* GameInput_debug.exe -> debug version, runs only in a Visual Studio (2022 here) environment as it needs the Visual Studio Debug Libraries !

### Microsoft GameInput API shortcommings

I would have printed the displayName of the controller, but:
* Microsoft GameInput API V.0 documentation shows a bunch of functions with a note "*not implemented yet*"
* Microsoft GameInput API V.1 documentation states "*Removed deprecated APIs, fields, and constants.*" (and I wasn't able to migrate to V.1,see above)

So I couldn't get access to the displayName structure as the returned pointer was zero.  
But I left my debugging statements in the program, they will be executed with verbosity level 3 ( -vvv ).

### Experience

The programming of this tool gave me insights into

	* Microsoft Visual C++
	* CMake
	* Microsoft GameInput API (and its deficiencies ;-)

# Credits

* getopt.c      Thanks to https://github.com/alex85k/wingetopt/tree/master
* gameinput.cpp Thanks to https://github.com/MysteriousJ/Joystick-Input-Examples/tree/main

_19.06.25,13:15/AH_

