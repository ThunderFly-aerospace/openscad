// slicer_modifier(): PrusaSlicer modifier regions (not part of the model geometry)
cube([40, 40, 10]);

translate([10, 10, 0])
  slicer_modifier(fill_density = "80%", perimeters = 5, infill_overlap = 2.5, thin_walls = true,
                  infill_anchor = "50%", fill_pattern = "gyroid", name = "hole") {
    cylinder(d = 12, h = 10);
    cube(2);
  }

// a number (or numeric string) for a percent setting gets an explicit "%"
slicer_modifier(fill_density = 50, ironing_flowrate = "15", thin_walls = 0) cube(1);

slicer_modifier(type = "negative", color = "red") translate([30, 30, 5]) cube(5);
slicer_modifier(color = [0, 0.5, 1]) translate([0, 0, 5]) cube(2);
slicer_modifier(type = "support_enforcer") translate([0, 30, 0]) cube(5);

// warnings: per-object setting, unknown setting, bad values (PrusaSlicer would
// reject the file), positional argument, unknown type, settings on a
// non-modifier type, non-string name
slicer_modifier(layer_height = 0.1, bogus_key = 1, perimeters = [1, 2], 3) cube(1);
slicer_modifier(perimeters = 2.5, top_solid_layers = "abc", thin_walls = 5, fill_angle = "x",
                fill_pattern = 3) cube(1);
slicer_modifier(type = "wrong") cube(1);
slicer_modifier(type = "support_blocker", perimeters = 3) cube(1);
slicer_modifier(name = 5) cube(1);
slicer_modifier(color = "notacolor") cube(1);
slicer_modifier(color = [2, 0, 0]) cube(1);
