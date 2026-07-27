// Regression test for the view() marker node (Fáze 15).
//
// view("name") { ... } is a switchable-view container. With the default active
// path ($view == [], i.e. the root) every view() is inactive and instantiates no
// children, so only geometry outside any view() block ("common") shows — this
// dump therefore also pins the backward-compatible behaviour of a model with
// view() blocks when no view is selected.
//
// Exercises: a common object, two top-level views (positional and name=), a
// nested view (accumulation), and a view placed under translate()/rotate().

cube([10, 10, 10]);            // common: always shown

view("a") sphere(5);           // positional name

view(name = "b") {             // name= form, with a nested view
  cylinder(h = 8, r = 2, $fn = 6);
  view("b_detail") cube(1);
}

translate([20, 0, 0])
  rotate([0, 0, 90])
    view("placed") cube([3, 4, 5]);

echo(view_node = "ok");
