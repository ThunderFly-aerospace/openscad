// Regression test for the section() marker node.
//
// section() must parse and produce an empty node (no geometry), so the CSG dump
// keeps working and the model renders/exports exactly as if the section were not
// there. A section marks a cutting plane (via its parent transforms) for a
// section view in the technical drawing. Here we exercise a bare name, the flip=
// and hatch= options, and placement under translate()/rotate().

cube([10, 10, 10], center = true);

section("A");

translate([0, 0, 5])
  rotate([90, 0, 0])
    section("B", flip = true);

section("C", hatch = 2.5);

echo(section_node = "ok");
