// Exact-output export test for bend(): the bent mesh has to survive a full
// render through both backends unchanged.
$fn = 8;
bend(at = 6, r = 2, angle = 90) cube([20, 6, 0.6]);
