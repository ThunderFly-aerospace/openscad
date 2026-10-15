// Tests for bend(glue = ...).
//
// An imported mesh is routinely not edge-connected: a component merely rests on
// the board, or a conversion split one solid into shells that touch without
// sharing a vertex. Connectivity alone then judges such a piece on its own
// position and leaves it behind while the board drives away. glue is the slack
// within which a piece is taken to hang off its neighbour after all.

$fn = 16;

// The board takes part in the bend, the component standing on it does not:
// zlim cuts the region just above the board. The component straddles the end of
// the arc, so on its own position it is a conflict and freezes.
module board() {
  cube([40, 6, 0.4]);
  translate([16, 1, 0.5]) cube([8, 4, 3]);  // separate body, 0.1 gap
}

// Without glue the component is left hanging in mid-air.
bend(at = 15, r = 2, angle = 90, zlim = [undef, 0.45]) board();

// With glue it travels with the material it stands on.
translate([0, 10, 0])
  bend(at = 15, r = 2, angle = 90, zlim = [undef, 0.45], glue = 0.2) board();

// Two shells of one part, separated by a hairline gap: glue makes them act as
// a single piece rather than each being judged on its own.
translate([0, 20, 0])
  bend(at = 15, r = 2, angle = 90, zlim = [undef, 0.45], glue = 0.2) {
    cube([40, 6, 0.4]);
    translate([16, 1, 0.5]) cube([4, 4, 3]);
    translate([20.05, 1, 0.5]) cube([4, 4, 3]);
  }

// glue reaches only as far as it is told: a 0.5 gap is out of range, so this
// component is still placed by its own position.
translate([0, 30, 0])
  bend(at = 15, r = 2, angle = 90, zlim = [undef, 0.45], glue = 0.2) {
    cube([40, 6, 0.4]);
    translate([16, 1, 0.9]) cube([8, 4, 3]);
  }
