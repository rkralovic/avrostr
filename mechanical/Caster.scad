// Original: http://www.thingiverse.com/thing:8959/
// Marble Caster by HipsterLogic
// License: https://creativecommons.org/licenses/by-sa/3.0/

// Modified by 648.ken@gmail.com
// MakersBox.blogspot.com

// Added tapper the top lip to keep it from snagging on things.
// Added recess for screw heads if baseThickness > 3mm
// Added pedistal base so you can adjust platform hieght.

// Modified by Richard Kralovic

deckHeight = 18.5;    // from chassis bottom to ground, (must be at lease ballDiameter)
ballDiameter = 15.875;    // 7/16" ball bearing = 12.25mm, WAS 16.5  16mm = 5/8" ball bearing
ballTolerance = 0.25 + 0.25;  // play between ball and surface
wallThickness = 2.5;  // was 2.5
gapWidth = ballDiameter / 3;
baseThickness = 2.5; // was 3

pedistalWidth = ballDiameter + wallThickness * 2 + ballTolerance * 2;

baseWidth = 30; // distance between mounting holes

M3holeDia = 3.5;
M3nutDia = 6.5;

baseHeight = deckHeight - ballDiameter / 2 + ballTolerance;

module mainShape() {
  // mounting cross-piece
  translate([-baseWidth/2, -wallThickness - M3holeDia / 2, 0]) 
    cube([baseWidth, wallThickness * 2 + M3holeDia, baseThickness]);

  // bolt ends
  translate([baseWidth / 2, 0, 0]) 
    cylinder(baseThickness, M3holeDia / 2 + wallThickness, M3holeDia / 2 + wallThickness, $fn=40);
  translate([-baseWidth / 2, 0, 0]) 
    cylinder(baseThickness, M3holeDia / 2 + wallThickness, M3holeDia / 2 + wallThickness, $fn=40);


  // main cylindrical body
  translate([0, 0, 0])
    cylinder(baseHeight, pedistalWidth / 2, pedistalWidth / 2, $fn=90);

  difference(){
    // tapper shere at top
    translate([0, 0, baseHeight])
      sphere(r = pedistalWidth / 2, $fn=90);

    // shave the top flat
    R = ballDiameter / 2;
    insertRestraint = 0.5;        
    shaveHeight = sqrt((R+ballTolerance)^2 - (R - insertRestraint)^2);
    echo(shaveHeight);
    translate([0, 0, baseHeight + shaveHeight])
      cylinder(25, 25, 25, $fn=90);
  }
}

module cutout() {
  // ball hole
  translate([0, 0, baseHeight]) 
    sphere(ballDiameter / 2 + ballTolerance, $fn=90);
      
  for (r = [0:90:90]) rotate([0, 0, 45 + r]) {
    // slot to remove ball
    slotOffset = ballDiameter / 2.5;  // distance below baseHeight to cut slot
    if (baseHeight - slotOffset > baseThickness) {
      translate([-gapWidth / 2, -baseWidth / 2, baseHeight - slotOffset]) 
        cube([gapWidth, baseWidth, ballDiameter]);
    } else { // don't cut into base
      translate([-gapWidth / 2, -baseWidth / 2, baseThickness]) 
       cube([gapWidth, baseWidth, ballDiameter]);        
    }
  }
}

module mountingHoles(){
  // m3 holes
  for (i = [-1, 1]) translate([ i * baseWidth / 2, 0, -1])
    cylinder(baseThickness + 2, M3holeDia / 2, M3holeDia / 2, $fn=30);
}

module Caster() {
  rotate([0, 0, 90])
  difference() {
    mainShape();
    cutout();
    mountingHoles();
  }

  // show ball
  %translate([0, 0, baseHeight - ballTolerance]) 
    sphere(ballDiameter / 2, $fn=50);
}

Caster();
