# AVR-OSTR

This project is an implementation of a turtle robot based on AVR microcontroller.

![assembled robot](assembled_robot.jpg | width=200)

## Mechanical design

The hardware design is based on [https://www.instructables.com/OSTR/](https://www.instructables.com/OSTR/).

Mechanical parts are designed using [OpenSCAD](https://openscad.org). Running
`make` in the `mechanical` directory builds STL files suitable for 3d-printing.
The design has the following parts:

* `Chassis.scad` is the main chassis structure.
* `Caster.scad` holds a support bearing ball.
* `PenCollar.scad` holds the pen.
* `Wheel.scad` is the wheel. You need to print 2 of them

The following extra parts are needed for assembly:

* Bearing ball with diameter approx 15.875mm.
* 2x o-ring 42mm x 3mm used as tires. It is important that it fits tightly to the wheels.
* 2x battery holder for 2 AA batteries (e.g. 12BHC321A-GR).
* M3 nuts and screws (for battery holders and stepper motors).
* DIN7981 screws to fix the wheels and PCB
  ([example](https://www.conrad.ch/de/p/toolcraft-din7981-c-h-2-9c4-5-linsenblechschrauben-2-9-mm-4-5-mm-kreuzschlitz-phillips-din-7981-stahl-verzinkt-1-st-889176.html?searchType=SearchRedirect)).
* (optional) Two large nuts that can be glued to the back of the chassis for better stability.

## Electronic design

Schematics and PCB layout are provided as a Kicad project in this directory.
Component list is summarized in `bom.csv`.

## Firmware

Source of the firmware, which runs on the AVR microcontroller of the robot, is
in the directory `fw`. You need reasonable new avr-g++ in order to compile the
firmware, which uses C++20. Development was done using avr-g++ 11.2.0.

The AVR firmware can be flashed using avrdude and an UPDI-capable programmer by
running `make write`.

A smoke test is in `fw/test`. It uses simavr library to simulate the AVR code.

Note: To compile the firmware, you need to first generate the input data, see
the next section for details. Beyond that, the tests depend on simavr, gmock,
and gtest libraries.

## Image data

The image data for the robot are prepared using a tool in `gendata` directory.
Source images are in `gendata/input` in svg file format. The tool depend on
absl, cairo, xcb, cairo-pdf, and xcb-iccm libraries.
