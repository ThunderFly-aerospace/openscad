// Regression test for view() with a selected active path (Fáze 15).
//
// Run with --model-view assembly/exploded (see tests/CMakeLists.txt). The outer
// view "assembly" is on the active path, so its common child (the housing cube)
// is instantiated, and its nested view "exploded" is the next step on the path,
// so its part is instantiated too (accumulation down the view tree). The sibling
// branch "other" is not on the active path and must stay empty.

view("assembly") {
  cube([30, 10, 2]);
  view("exploded") translate([0, 0, 20]) cube(5);
}

view("other") sphere(3);

echo(view_active = "ok");
