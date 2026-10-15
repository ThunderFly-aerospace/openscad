// Regression test for the camera() marker node.
//
// camera() must parse and produce an empty node (no geometry), so the CSG dump
// keeps working and the model renders/exports exactly as if the camera were not
// there. Cameras carry a pose (via their parent transforms) plus lens/sensor
// and output parameters. Here we exercise focal+sensor, explicit fov, an
// orthographic camera, a name, and placement under translate()/rotate().

cube([10, 10, 10]);

translate([20, 0, 5])
  rotate([0, 0, 45])
    camera(focal = 35, sensor = 24, width = 800, height = 600, name = "front");

camera(fov = 60);

translate([0, 30, 0])
  camera(projection = "orthogonal", name = "top");

// slicer_modifier() regions left out of this snapshot
camera(name = "plain", show_modifiers = false);

echo(camera_node = "ok");
