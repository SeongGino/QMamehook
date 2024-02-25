[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/Z8Z5NNXWL)
# QMamehook
###### "We have MAMEHOOKER at home." 

A bare-bones implementation of a MAME network output client, made primarily for compatible lightgun systems, using MAMEHOOKER-compatible *`gamename.ini`* files.

### Why WOULD you use this over MAMEHOOKER?
 - Cross-platform: i.e, works natively on Linux for native emulators, and (in theory) should work similarly for Windows.
 - Modern: Built on C++ & QT5/6, and made to interface with the MAME network output *standard*, meaning implicit support e.g. for RetroArch cores that use TCP localhost:8000 for feeding force feedback events.
 - Small & Simple: runs in the background with a single command, no admin privileges necessary.
 - Designed for light guns: Made for and exclusively compatible with the serial port interface used by PC light gun systems (which currently is [IR-GUN4ALL](https://github.com/SeongGino/ir-light-gun-plus) and [GUN4IR](https://forum.arcadecontrols.com/index.php/topic,161189.0.html)).
 - Compatible with MAMEHOOKER configs: Uses the same files verbatim, no conversions needed!

### Why would you NOT use this over MAMEHOOKER?
 - It's barebones: Strictly only supports light gun peripherals over serial (COM devices).
 - It's primarily for Linux: which has no options for this specific niche (Linux light gun users wanting native force feedback) - either [LEDSpicer](https://github.com/meduzapat/LEDSpicer) for Linux, or MAMEHOOKER itself for Windows might be better if you need more devices support.
 - Only supports MAME network output standard: for [DemulShooter] users, that means MAMEHOOKER is still absolutely required for e.g. TeknoParrot/JConfig games or native Windows games et al (at least, until they too adopt the MAME network standard).

### But y tho?
Because I wasn't happy with the other (or lacking thereof) solutions available, none of which supported simple serial devices for my lightguns, and I got very impatient and whipped this up in a day while working on GUN4ALL-GUI.

## Running:
> [!NOTE]
> Serial devices **must** be plugged in at runtime in order to work! QMamehook will emit a warning message if no compatible devices are detected. Currently, only devices bearing the GUN4ALL or GUN4IR vendor IDs will be detected.
> Also keep in mind that QMamehook will only correctly work with COM port writes (`cmw`) to ports correlating to the intended player/slot number (usually 1-4) - this does not need to match the COM port number in Windows, as the index is based on the count of *verified COM devices detected* (meaning always starting from "1"), not their ports.
### For Linux:
 - AUR package coming soon!
 - Make sure your user is part of the `dialout` group (`# usermod -a -G dialout insertusernamehere`)
Just run the `QMamehook` executable in a terminal; send an interrupt signal (or `pkill QMamehook`) to stop it.
Game config files are searched in `~/.config/QMamehook/ini`, and the program output will indicate whether a correct file matching the `mame_start` message is found or not.

## Building:
#### Requires `qt-base` & `qt-serialport` (QT5/QT6)
 - Clone the repo:
  ```
  git clone https://github.com/SeongGino/QMamehook
  ```
 - Setup build directory:
  ```
  cd QMamehook
  mkdir build && mkdir build
  cmake .. -DCMAKE_BUILD_TYPE=Release
  ```
 - Make:
  ```
  make
  ```
And run:
  ```
  ./QMamehook
  ```

## TODO:
 - Fix code quality; getting a QT CLI app to use signals is kind of a PITA and the quit/finished signals aren't working rn for some reason.
 - Maybe other devices support soon?
 - Implement a way of generating config files and detecting all of a given game's outputs if none are found - currently, the best way to know all the output channels a game has is to use MAMEHOOKER still.

## Thanks
 - ArcadeForums users, whose collective support on the GUN4ALL project are what inspired this side hustle.
 - MAME, for the output signal standard that which this is made to communicate with.
 - Sleep deprivation
 - Stubbornness
 - And Autism
