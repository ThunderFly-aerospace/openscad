#pragma once

#include <cmath>
#include <limits>
#include <memory>

class PolySet;

/*!
   Parameters of a single bend(), in the node's input coordinate system.

   The sheet is expected to lie in the XY plane along +X with the neutral fibre
   at z = 0; the bend axis is parallel to Y and passes through x = at. Material
   at x < at stays put, the band [at, at + r*|angle|] is wrapped onto an arc of
   radius r, and everything past the band follows as a rigid body.

   The x/y/z ranges restrict which material takes part at all: a triangle whose
   centroid falls outside any of them is left exactly where it is.
 */
struct BendParams
{
  static constexpr double INF = std::numeric_limits<double>::infinity();

  double at{0.0};
  double r{1.0};
  double angle{0.0};  // degrees, positive curves towards +Z

  double xmin{-INF}, xmax{INF};
  double ymin{-INF}, ymax{INF};
  double zmin{-INF}, zmax{INF};

  int segments{8};  // arc segments used to subdivide the band, >= 1

  // Slack used when deciding what hangs off what. Imported meshes are routinely
  // not edge-connected, so plain connectivity would judge such a piece on its
  // own and leave it behind. 0 disables the proximity pass entirely.
  double glue{0.0};

  bool hasMask() const
  {
    return std::isfinite(xmin) || std::isfinite(xmax) || std::isfinite(ymin) ||
           std::isfinite(ymax) || std::isfinite(zmin) || std::isfinite(zmax);
  }
};

/*!
   Diagnostics collected while bending, reported by the caller which knows the
   source location.
 */
struct BendStats
{
  // Edges whose two adjacent triangles end up under different transforms and
  // really do come apart. Such a mesh is no longer manifold.
  int torn_edges{0};
  double max_gap{0.0};

  // True when the radius is smaller than the distance of some bent material
  // from the neutral fibre on the inner side: the map folds through the bend
  // axis and the result self-intersects (the "minimum bend radius").
  bool inverted{false};

  // How the material outside the bend region was classified. Connected groups
  // inherit the fate of the bent material they hang off; groups that touch
  // nothing are judged by their own position instead.
  int groups{0};
  int groups_fixed{0};
  int groups_carried{0};
  int groups_isolated{0};
  int groups_conflict{0};  // hangs off both the fixed and the moving side
  int groups_on_arc{0};    // hangs off material that is being deformed
  int groups_glued{0};     // found its anchor only through glue, not a shared edge
};

/*!
   Subdivides the given PolySet along the bend band and the mask planes and
   applies the bend map. Returns a fresh PolySet; colors are preserved.
 */
std::unique_ptr<PolySet> bendPolySet(const PolySet& ps, const BendParams& params, BendStats& stats);
