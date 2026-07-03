// Regression test for the dimref() marker node.
//
// dimref() must parse and produce an empty node (no geometry), so the CSG dump
// keeps working and the model renders/exports exactly as if the dimref were not
// there. A dimref is a named anchor point (via its parent transforms) used as an
// endpoint of a dimension (kvóta). Here we exercise a bare name, the kvota=
// auto-pairing sugar, and placement under translate()/rotate().

cube([10, 10, 10]);

translate([10, 0, 0])
  dimref("A");

translate([10, 20, 0])
  rotate([0, 0, 90])
    dimref("B");

// kvota= sugar: two dimrefs sharing a kvota name auto-form a dimension.
translate([0, 0, 0]) dimref("W1", kvota = "width");
translate([10, 0, 0]) dimref("W2", kvota = "width");

echo(dimref_node = "ok");
