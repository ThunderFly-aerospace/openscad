// Regression test for the label() marker node.
//
// label() must parse and produce an empty node (no geometry), so the CSG dump
// keeps working and the model renders/exports exactly as if the label were not
// there. A label is a named anchor point (via its parent transforms) that carries
// annotation text; a // @label line then draws that text in a drawing view or a
// camera snapshot. Here we exercise a bare name (text defaults to the name), an
// explicit text, and placement under translate()/rotate().

cube([10, 10, 10]);

translate([10, 0, 5])
  label("A", "Motor mount");

translate([0, 20, 0])
  rotate([0, 0, 90])
    label("B");

echo(label_node = "ok");
