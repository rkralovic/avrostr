//$fa = 3;
//$fs = 0.3;
$fn = 50;

// Wheel diameter: 47.5 + 3 (gasket, see Wheel.scad) = 50.5
// Distance ground - shaft = 25.25
// Distance ground - bottom of chassis (Caster.scad) = 18.5
// Distance bottom of chassis - shaft = 6.75

wallThickness = 2.5;

baseThickness = wallThickness;
shaftPosition = 6.75;

module roundedCubeXYZR(xdim ,ydim ,zdim, rdim){
  hull() { // https://youtu.be/gKOkJWiTgAY
    translate([rdim, rdim, 0]) cylinder(zdim, rdim, rdim);
    translate([xdim-rdim, rdim, 0]) cylinder(zdim, rdim, rdim);
    translate([rdim, ydim-rdim, 0]) cylinder(zdim, rdim, rdim);
    translate([xdim-rdim, ydim-rdim, 0]) cylinder(zdim, rdim, rdim);
  }
}

// Interface more compatible with cube()
module roundedCube(dims, radius=2, center=false) {
  xdim = dims[0];
  ydim = dims[1];
  zdim = dims[2];
  if (center) {
    translate([-xdim  / 2, -ydim / 2, -zdim / 2])
      roundedCubeXYZR(xdim, ydim, zdim, radius);
  } else {
    roundedCubeXYZR(xdim, ydim, zdim, radius);
  }
}

module ellipsoidFillet(dims, h0, full = true) {
  module ell() {
    translate([0, 0, dims[2] / 2]) scale([1, 1, 2 * (dims[2] - h0) / dims[0]]) rotate([90, 0, 0])
      cylinder(h = dims[1] + 0.1, d = dims[0], center = true);
  }
  translate([0, 0, dims[2] / 2])
  difference() {
    cube(dims, center = true);
    if (full) {
      ell();
    } else {
      translate([-dims[0] / 2, 0, 0]) scale([2, 1, 1]) ell();
    }
  }
}

M3holeDia = 3.6;
M3nutDia = 6.5;

stepperBlockH = 17;  // distance from center to the top of the rectangular block
stepperBlockW = 14.6;
stepperBulkThickness = wallThickness + 1;

stepperShaftRadius = 5;
stepperShaftOffset = 8;
stepperDiameter = 28;
stepperTabWidth = 7;
stepperTolerance = 0.6;
module Stepper() {
  thickness = 3;

  tol= stepperTolerance;

  h1 = 19;  // body thickness, minimum 19
  h2 = 10;  // arbitrary, to have long enough drill holes
  blockH = stepperBlockH;
  blockW = stepperBlockW;
  d = stepperDiameter;
  screw_distance = 35;
  tab_width = stepperTabWidth;
  shaft_offset = stepperShaftOffset;
  shaft_radius = stepperShaftRadius;

  union() {
    rotate(-90, [0, 0, 1])
    rotate(90, [1, 0, 0])
    translate([0, shaft_offset, - h1 / 2 - tol]) {
      cylinder(h = h1 + 2 * tol, r = d / 2 + tol, center=true);  // stepper body

      for (i = [-1, 1])
        translate([i * screw_distance / 2, 0, 0]) {
          translate([0, 0, -thickness +h1 / 2 - tol])
          cylinder(h = thickness + 2*tol, r = tab_width / 2 + tol, center=false);
          translate([0, 0, -thickness +h1/2])
            cylinder(h = thickness + h2, d = M3holeDia, center=false);
        }

      translate([0, 0, h1 / 2 + tol - thickness / 2])
        cube([screw_distance, tab_width + 2*tol, thickness], center=true);

      // Not part of the motor body, but useful to keep too thin areas left
      translate([0, d/4, -thickness /2 + h1/2])
      cube([screw_distance + tab_width + 2*tol, d / 2, thickness + 2 * tol], center=true);

      // Shaft
      translate([0, -shaft_offset, h2 / 2])
        cylinder(h = h1 + h2, r = shaft_radius + tol, center=true);

        translate([0, blockH / 2, 0])
          cube([blockW + 2 * tol, blockH + 2 * tol, h1 + 2 * tol], center=true);

    }

    // Not part of the motor body, but useful to keep too thin areas left
    cutout = d * 0.25;
    translate([0, -cutout / 2, shaft_offset - d/2 - baseThickness])
      cube([h1 + 2*tol, cutout, d + 2*tol ]);
  }
}

stepperBulkW = 43;
stepperBulkH = stepperShaftOffset + shaftPosition + stepperTabWidth / 2;//19.5

module StepperBulk() {
  bulkW = stepperBulkW;
  bulkH = stepperBulkH;
  r = 2;
  bulkThickness = stepperBulkThickness;

  union() {
    translate([-(bulkW) / 2, 0, 0])
      rotate([90, 0, 0])
        union() {
          cube([bulkW, shaftPosition, bulkThickness]);
          for (i = [0, 2])
            translate([i * (bulkW / 2 + stepperShaftRadius + stepperTolerance) / 2, 0, 0])
              roundedCube([bulkW / 2 - stepperShaftRadius - stepperTolerance, bulkH, bulkThickness], r);
        }

      r2 = 5;
      translate([0, -r2/2 - bulkThickness, r2/2 + baseThickness ])
      difference() {
        filletW = bulkW - 2 * r2;
        union () {
          cube([filletW, r2, r2], center=true);
          for (i = [-1, 1])
          translate([i * filletW / 2, r2/2, -r2/2])
          intersection() {
            sphere(r2);
            translate([(i - 1) / 2 *r2, -r2, 0]) cube([r2, r2, r2]);
          }
        }
        translate([0, -r2/2, r2/2]) rotate([0, 90, 0])
          cylinder(h = 2*r2 + filletW + 0.1, r = r2, center = true);
      }
  }
}

railWidth = 2.9;
railOffset = 5.8;

penDiameter = 8.8;  // Including tolerance. Maximum 9.
// Stabilo pen has 8.4mm diameter, 0.5mm tolerance
barrelDiameter = penDiameter + 2 * wallThickness;
barrelHeight = 33;  // 26

// servo bracket variables
servoOverallHeight = 34.5;  // 34.5
servoBodyHeight = 24;
servoBodyWidth = 13;
servoScrewDia = 1.5;
servoScrewOffset = 2;
servoFwdOffset = penDiameter / 2 + wallThickness + 19;   // was 14.5
servoSideOffset = 3;
servoVerticalOffset = 8;  // distance between top of holder and servo body center, was 5

batteryOffset = 20.5;
batteryShift = 0;  // -2 is needed in the original position of servo
batteryScrewDistance = 15;

module HolderBulk() {
  baseWid = 19;
  baseLen = railWidth + 2 * railOffset;
  cornerRounding = 2;

  // pen holder
  translate([0, 0, baseThickness])
  cube([baseWid, baseLen, 2 * baseThickness], center = true);

  translate([0, 0, baseThickness])
    cylinder(barrelHeight - wallThickness / 2, r1 = baseLen / 2, r2 = barrelDiameter / 2);
  translate([0, 0, baseThickness + barrelHeight - wallThickness / 2])
    toroid(wallThickness / 2, (barrelDiameter - wallThickness) / 2);

  union() {
    bracketThickness = baseThickness + 0.5;
    bracketWidth = servoBodyWidth + 4 * baseThickness;
    // servo bracket
    rotate([0, 0, 180])
    translate([-servoFwdOffset, servoSideOffset, barrelHeight - servoVerticalOffset])
    rotate([90, 0, 90])
    union() {
      difference() {
        // base
        translate([ 0, -(barrelHeight - servoVerticalOffset - servoOverallHeight / 2) / 2, bracketThickness / 2 ])
            roundedCube([bracketWidth, servoOverallHeight / 2 + barrelHeight - servoVerticalOffset,
                         bracketThickness], cornerRounding, center = true);

        // body cutout
        translate([ 0, 0, bracketThickness / 2])
            cube([servoBodyWidth, servoBodyHeight, bracketThickness + 0.5], center = true);

        // vertical screw holes
        translate([ -0.1, -servoBodyHeight / 2 - servoScrewOffset, -0.1 ])
            cylinder(bracketThickness + 0.2, servoScrewDia / 2, servoScrewDia / 2);
        translate([ 0, servoBodyHeight / 2 + servoScrewOffset, -0.10 ])
            cylinder(bracketThickness + 0.2, servoScrewDia / 2, servoScrewDia / 2);

        // space for inserting m3 nut below
        shift = bracketThickness / 2;
        translate([-servoSideOffset, -0.1 + 1 + 2.5 + 1.5 - baseThickness / 2 - barrelHeight + servoVerticalOffset, shift])
          intersection() {
            roundedCube([5.7, 1 + 2.5 + 1.5 + baseThickness, bracketThickness + 0.1], radius = 1, center = true);
            // Ugly - we need to place the nut to the correct hole
            translate([0, 0, servoFwdOffset - shift - batteryOffset - batteryScrewDistance / 2]) rotate([90, 30, 0])
              cylinder(1 + 2*(baseThickness + 5), d = M3nutDia, center = true, $fn=6);
          }
      }

      // Servo body for checking
      %union() {
        mountOffset = 16 + 2.45;  // TODO: measure - 16 by datasheet, but that's probably not counting the thicnkess of the tab
        translate([0, 0, 26.9 / 2 - mountOffset])
          cube([12.6, 22.8, 26.9], center = true);
        translate([0, 22.8/4, 26.9 - mountOffset]) rotate([0, 0, 90])
          cylinder(h = 32.2 - 26.9, d = 7.3);
//        mountOffset = 16 + 2.45;  // TODO: measure - 16 by datasheet, but that's probably not counting the thicnkess of the tab
//        translate([0, 0, 26.7 / 2 - mountOffset])
//          cube([12.6, 22.8, 26.7], center = true);
//        translate([0, 22.8/4, 26.7 - mountOffset]) rotate([0, 0, 90])
//          cylinder(h = 34.5 - 26.7, d = 7);
      }
    }
    // fillets
    {
      w = chassisW / 2 - servoSideOffset - bracketWidth / 2 - stepperBulkThickness;
      translate([servoFwdOffset - bracketThickness / 2, w / 2 - chassisW / 2 + stepperBulkThickness, baseThickness])
      rotate([0, 0, 90]) 
        ellipsoidFillet([w + 0.01, bracketThickness, stepperBulkH], stepperBulkH / 3, false);

      translate([servoFwdOffset / 2 - bracketThickness / 4 + stepperBulkW / 4 - bracketThickness / 4,
                 -chassisW / 2 + stepperBulkThickness / 2, baseThickness])
        rotate([0, 0, 180]) 
        ellipsoidFillet([servoFwdOffset - bracketThickness / 2 - stepperBulkW / 2 - bracketThickness / 2 + 0.01,
                         stepperBulkThickness, stepperBulkH / 2 ],
                         stepperBulkH / 3, false);

      intersection() {
        translate([servoFwdOffset - bracketThickness, -chassisW / 2, baseThickness])
          cube([bracketThickness, stepperBulkThickness, stepperBulkH / 3]);
        translate([servoFwdOffset - bracketThickness, -chassisW / 2 + stepperBulkThickness, baseThickness])
          scale([1, stepperBulkThickness / bracketThickness, 1])
          cylinder(h = stepperBulkH / 3, r = bracketThickness);
      }
    }
    {
      w2 = chassisW / 2 + servoSideOffset - bracketWidth / 2 - stepperBulkThickness;
      translate([servoFwdOffset - bracketThickness / 2, chassisW / 2 - w2 /2 - stepperBulkThickness, baseThickness])
      rotate([0, 0, -90]) 
        ellipsoidFillet([w2 + 0.01, bracketThickness, stepperBulkH], stepperBulkH / 3, false);

      translate([servoFwdOffset / 2 - bracketThickness / 4 + stepperBulkW / 4 - bracketThickness / 4,
                 chassisW / 2 - stepperBulkThickness / 2, baseThickness])
        rotate([0, 0, 180]) 
        ellipsoidFillet([servoFwdOffset - bracketThickness / 2 - stepperBulkW / 2 - bracketThickness / 2 + 0.01,
                         stepperBulkThickness, stepperBulkH / 2 ],
                         stepperBulkH / 3, false);

      intersection() {
        translate([servoFwdOffset - bracketThickness, chassisW / 2 - stepperBulkThickness, baseThickness])
          cube([bracketThickness, stepperBulkThickness, stepperBulkH / 3]);
        translate([servoFwdOffset - bracketThickness, chassisW / 2 - stepperBulkThickness, baseThickness])
          scale([1, stepperBulkThickness / bracketThickness, 1])
          cylinder(h = stepperBulkH / 3, r = bracketThickness);
      }
    }
    {
      l3 = barrelDiameter / 2 - railOffset + railWidth / 2;
      translate([(servoFwdOffset - bracketThickness) / 2, l3 / 2 + railOffset - railWidth / 2, baseThickness])
        ellipsoidFillet([servoFwdOffset - bracketThickness,
            l3, barrelHeight - 2 * wallThickness], stepperBulkH / 2, true);
    }
  }

}

module HolderDrill() {
  translate([0, 0, -1])
    cylinder(barrelHeight + baseThickness + 2, d = penDiameter);

  difference() {
    translate([0, 0, -0.1])
      cylinder(wallThickness / 2 + 0.1, d = penDiameter + wallThickness);

    translate([0, 0, wallThickness / 2])
      toroid(wallThickness / 2, (penDiameter + wallThickness) / 2);
  }
}

// Bottom is at z = 0
chassisL = 104;
chassisW = 60;

  // standoffs are in 2 variants: downward facing items and upward facint items

  pcbH = 50.8;
  pcbD1 = 6.35;
  pcbD2 = 17.78;
  pcbX0 = -44.5;
  pcbY0 = -pcbH / 2;

  standoffDia = [8, 7];
  standoffThickness = [1.5, 4.5];
  smallScrewHoleDia = 2;

  casterFromEdge = standoffDia[0] / 2;
  casterScrewDistance = 30;  // must be equal to baseWidth in Caster.scad

  standoffs = [
    [ // downward facing items
      // battery standoffs
      for (i = [-1, 1])
        for (j = [-1, 1])
          [i * (batteryOffset + j * batteryScrewDistance / 2) + batteryShift, 0],

      // caster standoffs
      for (i = [-1, 1])
        [chassisL / 2 - casterFromEdge, i * casterScrewDistance / 2 ]
    ],
    [ // upward facing items
      // PCB
      [pcbX0, pcbY0],
      [pcbX0 + pcbD2, pcbY0],
      [pcbX0 + pcbD1, pcbY0 + pcbH]
    ]
  ];

module ChassisBase() {
  cornerRounding = 8;

  railRadius = 1.5;
  railThickness = railRadius;

  translate([0, 0, baseThickness / 2])
  difference() {
    union() {
      roundedCube([chassisL, chassisW, baseThickness], cornerRounding, center = true);

      // Rails
      for (i = [-1, 1])
        translate([0, i * railOffset, railThickness]) rotate([90, 0, 0])
          roundedCube([chassisL, 2 * railThickness, railWidth], railRadius, center = true);

      for (i = [0, 1]) {
        for (pos = standoffs[i]) {
          translate([pos[0], pos[1], baseThickness / 2])
            cylinder(standoffThickness[i], d = standoffDia[i]);
        }
      }
    }

    // Standoff holes, M3
    for (pos = standoffs[0]) {
      // screw holes
      translate([pos[0], pos[1], standoffThickness[0] / 2])
        cylinder(baseThickness + standoffThickness[0] + 1, d = M3holeDia, center = true);

      // nut holders
      translate([pos[0], pos[1], standoffThickness[0] + 0.01])
        cylinder(baseThickness, d = M3nutDia, center = true, $fn=6);
    }

    // Standoff holes, self-drilling M3
    for (pos = standoffs[1]) {
      translate([pos[0], pos[1], standoffThickness[1] / 2])
        cylinder(baseThickness + standoffThickness[1] + 0.02,
                 d = smallScrewHoleDia, center = true);
    }

  }
}

module PCB() {
  union() {
    translate([0, 0, 0.8])
      roundedCube([31.75, 58.42, 1.6], 3.81, center=true);
    translate([+31.75/2 - 9.6/2, 0, 1.6 + 5])
    cube([9.6, 48, 10], center=true);

  }
}

difference() {
  union() {
    ChassisBase();
    for (i = [1, -1]) {
      translate([0, -i * chassisW / 2, 0])
      rotate([0, 0, (i+1) * 90])
        StepperBulk();
    }

    HolderBulk();

    // PCB approximation
    %translate([-12.06 + pcbX0 + pcbD2, 0, baseThickness + standoffThickness[1]])
    PCB();
  }

  #for (i = [1, -1]) {
    translate([0, -i * (chassisW / 2 - wallThickness), shaftPosition])
    rotate([0, 0, i * 90])
      Stepper();
  }

  // Hole for battery cables
  {
    x = -batteryOffset - 32/2 - 2.5 + batteryShift;
    echo("Wire hole position", x);
    translate([x, -chassisW / 2 + 6, -1])
    cylinder(h = baseThickness + 2, d = 4);
  }

  HolderDrill();
}

// Batteries
%for (i = [-1, 1])
translate([i * batteryOffset, 0, -16.5/2])
cube([32, 59, 16.5], center=true);

use <PenCollar.scad>
translate([0, 0, barrelHeight + 4])
%PenCollar();

use <Caster.scad>

%translate([chassisL / 2 - casterFromEdge, 0, 0])
rotate([0, 180, 0])
Caster();

use <Wheel.scad>

*for (i = [0, 180])
rotate([0, 0, i])
translate([0, -chassisW / 2 - 1, shaftPosition]) rotate([90, 0, 0])
union() {
  Wheel();
  translate([0, 0, 2])
  toroid(1.5, 47.5 / 2);
}

*translate([-75, -75, -18.5 - 3])
cube([150, 150, 3]);
