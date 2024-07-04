penDiameter = 8.2;  // Including tolerance.
// Stabilo pen has 8.4mm diameter.
// We want the collar to fit tightly.

collarDiameter = 40;
collarHeight = 10;

wallThickness = 2.5;
roundingRadius = 1.1;

cutWidth = 1.5;

$fa = 3;
$fs = 0.3;

module simpleCylinder(h, r) {
  cylinder(h, r, r, center = false);
}


r0 = roundingRadius;
r1 = penDiameter / 2;
r2 = collarDiameter / 2;
h1 = wallThickness;
h2 = collarHeight;

pts = [
  [r1, 0], [r2 - r0, 0], [r2 - r0, r0], [r2, r0],
  [r2, h1 - r0], [r2 - r0, h1 - r0], [r2 - r0, h1],
  [r1 + r0 + h1, h1], [r1 + r0 + h1, h1 + r0], [r1 + h1, h1 + r0],
  [r1 + h1, h2 - r0], [r1 - r0 + h1, h2 - r0], [r1 - r0 + h1, h2],
  [r1 + r0, h2], [r1 + r0, h2 - r0], [r1, h2 - r0]
];

module smoothing() {
  union() {
    difference() {
      children();
      // Drill expanded cut
      translate([0, collarDiameter / 2, collarHeight / 2])
        cube([cutWidth + 2*roundingRadius , collarDiameter, collarHeight + 2], center=true);
    }

    for (i = [-1, 1]) intersection() {
      children();
      translate([i * (cutWidth / 2 + roundingRadius), 0, roundingRadius]) rotate(-90, [1, 0, 0])
        simpleCylinder(collarDiameter, roundingRadius);
    }

    difference() {
      children();

      // Drill strict cut
      translate([0, collarDiameter / 2, collarHeight / 2])
        cube([cutWidth, collarDiameter, collarHeight + 2], center=true);

      // Drill bottom
      translate([0, 0, roundingRadius / 2])
        cube([collarDiameter + 2, collarDiameter + 2, roundingRadius], center=true);

    }
  }
};

module PenCollar() {
  smoothing()
    rotate_extrude()
      difference() {
        union() {
          polygon(points = pts);
          translate(pts[2]) circle(r0);
          translate(pts[5]) circle(r0);
          translate(pts[11]) circle(r0);
          translate(pts[14]) circle(r0);
        };
        translate(pts[8]) circle(r0);
      };
}

PenCollar();
