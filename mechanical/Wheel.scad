wheelDiameter = 47.5;  // not including the gasket
gasketDiameter = 3;
innerWidth = 2.0;
outerWidth = max(gasketDiameter + 1, innerWidth);

hubDiameter = 11.5;
hubHeight = 9;
hubRounding = 1;
hubCutWidth = 3.25;

// spokeDiameter = (wheelDiameter / 2 - 6.5) - 9;
spokeDiameter = ((wheelDiameter - hubDiameter) / 2 - hubRounding - outerWidth) * 0.85;
spokeAspectRatio = 0.75;
numSpokes = 8;

// pilot hole for M3 self-tapping screw in hub 
drillHoleDiameter1 = 2.5;
drillHoleDiameter2 = 2.5;

$fa = 3;
$fs = 0.3;
//$fn = 20;

module toroid(r1, r2) {
  rotate_extrude()
    translate([r2, 0, 0]) circle(r = r1);
}

module simpleCylinder(h, r) {
  cylinder(h, r, r, center = false);
}

module basicWheel() {
  difference() {
    simpleCylinder(outerWidth, wheelDiameter / 2);
    translate([0, 0, outerWidth / 2])
      toroid(gasketDiameter / 2, wheelDiameter / 2);

    minkowski() {
      translate([0, 0, outerWidth])
        simpleCylinder(outerWidth, wheelDiameter / 2 - outerWidth);
      sphere(outerWidth - innerWidth);
    }
  }
}

module spoke() {
  scale([1, spokeAspectRatio, 1])
  difference() {
    simpleCylinder(innerWidth + 1, spokeDiameter / 2);
    translate([0, 0, innerWidth / 2])
      toroid(innerWidth / 2, spokeDiameter / 2);
  }
}

module hub() {
  union() {
    minkowski() {
      translate([0, 0, hubRounding])
        simpleCylinder(hubHeight - 2 * hubRounding, (hubDiameter / 2) - hubRounding);
      sphere(hubRounding);
    };

    // bottom rounding
    difference() {
      translate([0, 0, innerWidth])
        simpleCylinder(hubRounding, hubDiameter / 2 + hubRounding);
      translate([0, 0, innerWidth + hubRounding])
        toroid(hubRounding, hubDiameter / 2 + hubRounding);
    }
  }
}

module Wheel() {
  union() {
    difference() {
      union() {
        basicWheel();
        hub();
      }

      // Drill spokes
      for (i = [0:numSpokes])
        rotate( i * 360 / numSpokes, [0, 0, 1])
          translate([-(hubDiameter / 2 + hubRounding + wheelDiameter / 2 - outerWidth) / 2, 0, 0])
            spoke();

      // Drill axis
      translate([0, 0, -1])
        intersection() {
          R = 2.75;
          simpleCylinder(hubHeight + 2, R);
          cube([hubCutWidth, 2 * R, 2 * hubHeight + 3], center = true);
        };

      // Drill Screw hole
      #for (i = [-1, 1])
        translate([i * (hubDiameter / 4 + hubCutWidth /4), 0, 6]) 
        rotate([0, 90, 0])
        rotate([90*(i + 1), 0, 0])
          cylinder((hubDiameter - hubCutWidth)/ 2 + 0.01, drillHoleDiameter1 / 2, drillHoleDiameter2 / 2, center = true);
    }
    // screw
    %translate([1.62, 0, 6]) rotate([0, 90, 0])
    union() {
      screwHeight = 4.8; // 4.5 is better, but ~6 is what I have and acceptable, too
      cylinder(h = screwHeight, d = 2.9);
      difference() {
        translate([0, 0, screwHeight])
          sphere(d = 5.6);
        translate([0, 0, screwHeight/2])
          cube([6, 6, screwHeight], center=true);
      }
    }
  }
}

Wheel();
