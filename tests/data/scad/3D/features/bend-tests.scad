// Tests for the bend() module.
//
// bend() bends its children around an axis parallel to Y at x = at, with the
// neutral fibre at z = 0; a positive angle curves towards +Z. The optional
// xlim/ylim/zlim ranges select which material takes part, which is what makes
// several bends on different tabs of the same board independent.

$fn = 16;

// Plain 90 degree bend: the part in front of x = 6 stays put.
bend(at = 6, r = 2, angle = 90) cube([20, 6, 0.6]);

// A negative angle curves the other way.
translate([0, 10, 0])
  bend(at = 6, r = 2, angle = -60) cube([20, 6, 0.6]);

// ylim picks one of two tabs: only that one bends, the other stays flat even
// though it reaches past the bend line. The mask boundary runs through free air
// between the tabs, so nothing tears.
translate([0, 22, 0])
  bend(at = 8, r = 1.5, angle = 90, ylim = [-1, 5]) {
    cube([8, 16, 0.6]);
    translate([8, 0, 0]) cube([12, 4, 0.6]);
    translate([8, 12, 0]) cube([12, 4, 0.6]);
  }

// Chained bends: reposition the result back into XY between them. The inverse
// of the arc end frame is rotate([0, angle, 0]) translate(-p).
translate([0, 45, 0])
  bend(at = 5, r = 2, angle = 90)
    rotate([0, 90, 0]) translate([-(5 + 2 * sin(90)), 0, -(2 - 2 * cos(90))])
      bend(at = 5, r = 2, angle = 90)
        cube([25, 6, 0.6]);

// angle = 0 is the identity.
translate([0, 55, 0])
  bend(at = 5, r = 2, angle = 0) cube([10, 6, 0.6]);
