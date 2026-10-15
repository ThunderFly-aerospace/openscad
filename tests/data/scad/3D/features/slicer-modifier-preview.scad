// slicer_modifier(): the preview colors only the part of the model inside each
// region; #slicer_modifier highlights the whole region shape. F6 / render is the
// plain model.
color("gold") cube([40, 40, 10]);
translate([10, 10, -2]) slicer_modifier(fill_density = "80%", color = "red") cylinder(d = 12, h = 20, $fn = 32);
// default region color
slicer_modifier(perimeters = 4) translate([25, -5, 5]) cube([10, 20, 10]);
// overlapping regions: the later one wins
slicer_modifier(color = "blue") translate([28, 25, 0]) cube([8, 20, 20]);
#slicer_modifier(color = "green") translate([20, 30, 5]) cube([12, 15, 10]);
