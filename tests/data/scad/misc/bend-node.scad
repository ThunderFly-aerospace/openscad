// Regression test for the bend() node: parameter parsing and the CSG dump.
//
// bend() wraps its children around an axis parallel to Y at x = at. The dump
// has to carry every parameter (including the limits and the discretization
// specials), because two bends that differ only in those must not collide in
// the geometry cache.

bend(at = 10, r = 2, angle = 90) cube([20, 5, 1]);

// Limits, with undef on one side meaning "unbounded".
bend(at = 10, r = 2, angle = 90, xlim = [undef, 25], ylim = [-6, 6])
  cube([30, 20, 1]);

// A malformed limit is ignored with a warning.
bend(at = 1, r = 1, angle = 45, zlim = 5) cube([5, 5, 1]);

// An empty range warns; nothing gets bent.
bend(at = 1, r = 1, angle = 45, ylim = [5, -5]) cube([5, 5, 1]);

// glue only shows up in the dump when it is in use.
bend(at = 10, r = 2, angle = 90, zlim = [undef, 5], glue = 0.25) cube([20, 5, 1]);

// A negative glue is refused with a warning and treated as off.
bend(at = 10, r = 2, angle = 90, glue = -1) cube([20, 5, 1]);

// angle = 0 is the identity.
bend(at = 1, r = 1, angle = 0) cube([5, 5, 1]);

echo(bend_node = "ok");
