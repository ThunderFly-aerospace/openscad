// Regression test for the per-view export defaults of view() (Fáze 17).
//
// A view() may declare which batch export it is a default for, with three
// independent optional flags (all false unless given):
//
//   camera_export   -> --export-cameras --default-model-views
//   drawings_export -> --export-drawing --default-model-views
//   model_export    -> -o <file> --default-model-views
//
// The dump pins how the flags reach the instantiated node (toString), while the
// companion list-model-views tests pin the AST walk that the CLI uses to select
// the views to export.
//
// Note the "computed" view below: the AST walk does not evaluate expressions, so a
// flag given as a variable reads as false there (and the view is therefore not
// selected by --default-model-views) even though the instantiated node sees true.
// Deliberate, and pinned here on purpose — use literal true/false.

flag = true;

cube([10, 10, 10]);  // common: shown at the root, outside any view

view(name = "pcb", drawings_export = true, model_export = true) sphere(5);

view(name = "case", camera_export = true, drawings_export = true) {
  cylinder(h = 8, r = 2, $fn = 6);
  view(name = "detail", camera_export = true) cube(1);
}

view(name = "plain") cube([3, 4, 5]);                     // no flags
view(name = "explicit", model_export = false) cube(2);    // explicitly off
view(name = "computed", camera_export = flag) cube(1);    // not a literal

echo(view_defaults = "ok");
