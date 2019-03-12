# J.A.P.

J.A.P. (Just Another Printer) is a tiny firmware for DIY LCD 3D-printer.
Designed to work on Arduino Uno + CNC shild hardware with "Creation Workshop"
or "nanoDLP" software.

***

 Support
* Z-axis smooth movement
* Z-axis manual control buttons
* Z motor manual on/off button
* Cooler control
* LED control

***

 List of Supported G-Codes
 - Motion: G1
 - Pause: G4
 - Positioning: G90, G91, G92

 - Motor on:  M17
 - Motor off: M18, M84
 - Cooler on:  M7, M245
 - Cooler off: M9, M246
 - UV LED on:  M3, M4, M106
 - UV LED off: M5, M107
 
 - Current position:  M114

 ***

 CNC shild v.3 pinout for LCD:
 
 ![pinout](https://github.com/3DLab-DLP/jap/blob/master/Img/JAP_LCD_pinout.jpg)

***
 
  Setup:

  * #define STEPS_PER_MM - resolution in steps/mm for Z-axis
  * #define MAX_SPEED    - maximum speed in mm/s
  * #define ACC_Z        - acceleration in mm/s2
  * #define Z_MIN_MM     - lower Z-axis bound in mm
  * #define Z_MAX_MM     - upper Z-axis bound in mm
  * #define TIMEOUT      - stepper idle hold timeout in minutes
  
  P.S. Jumper or normally closed endstop needed on Z+. Z+ is not a home! It's only limiter. 

