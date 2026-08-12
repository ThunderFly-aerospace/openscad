// Regression test for where view() blocks are found (Fáze 17).
//
// The instantiated tree treats a view name as a *label*, not an identity: every
// matching block is active at once (a module used twice, every iteration of a for,
// two blocks with the same name). The enumeration behind --list-model-views /
// --all-model-views / --default-model-views therefore has to (a) find views inside
// user modules — including modules from use<>d files — and both branches of
// if/else, and (b) report each distinct view *path* only once, since one path maps
// to one output file.
//
// Pinned by list-view-scopes (--list-model-views over this file).

use <view-lib.scad>

// A view() in a module: found once even though the module is called twice.
module part(n) {
  view(name = "part", model_export = true) cube(n);
}
part(1);
translate([10, 0, 0]) part(2);

// A view() in a for loop: one AST occurrence, all iterations active.
for (i = [0:2]) view(name = "loop", camera_export = true) translate([0, i * 5, 0]) cube(1);

// Two blocks with the same name: one entry (both are active together).
view(name = "dup", model_export = true) cube(3);
translate([0, 0, 10]) view(name = "dup", model_export = true) cube(4);

// Both branches of if/else are reachable views, so both are listed.
if (true) view(name = "then", model_export = true) cube(5);
else view(name = "else", model_export = true) cube(6);

// A module from a use<>d file.
lib_part();

// A recursive module must not hang the walk.
module recur(n) {
  if (n > 0) recur(n - 1);
  view(name = "recursed") cube(1);
}
recur(3);

// A module called inside a view(): its view nests under the caller's path, so the
// same module contributes both "part" (top-level calls above) and "outer/part".
view(name = "outer") part(9);

echo(view_scopes = "ok");
