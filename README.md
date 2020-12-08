# KonamiDDRPad-Linux
For using a Konami dancepad on Linux via USB.

This repository is written in order to correctly interpret input from a Konami DDR pad as a dancepad rather than a gamepad controller. Specifically, it allows input to be correctly interpreted when multiple buttons are pressed simultaneously.

## How to use
1. Clone this repository.
2. Confirm the IDs at the top of `main.c` (`KONAMI_VENDOR_ID` and `KONAMI_PRODUCT_ID`) match the device, e.g. using `lsusb -v`. 
3. Run `make` inside of KonamiDDRPad-Linux.
4. Run the generated executable as root: `sudo ./KonamiDDRPadLinux`
5. Run your game or program as usual!
