// Model + slicer_modifier() regions exported as ONE PrusaSlicer object
color("red") cube([20, 20, 5]);
slicer_modifier(fill_density = "100%", perimeters = 4, name = "dense")
  translate([5, 5, -1]) cube([10, 10, 7]);
slicer_modifier(type = "support_blocker") translate([0, 0, 5]) cube(3);
