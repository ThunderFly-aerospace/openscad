#pragma once

#include <utility>
#include <vector>

#include "geometry/linalg.h"

class PolySet;

/*!
   Selects the edges of a mesh worth drawing in a technical line drawing, so a
   compact vector projection can be produced without the tessellation facets of
   smooth surfaces cluttering it.

   Three edge classes are kept (see ROADMAP Phase 4C):
     - silhouette: an edge shared by two faces where one faces the viewer and the
       other faces away — a per-edge front/back-facing test (sign(nA·view) !=
       sign(nB·view)). This is view-dependent and is the main source of the
       curved contour of a cylinder standing in front of a box.
     - crease / feature: an edge whose two faces meet at a dihedral angle sharper
       than a threshold (view-independent). The top/bottom rims of a cylinder are
       creases; the many facets of its side are not, so they drop out.
     - boundary: an edge with a single adjacent face (open meshes).

   Vertices are welded by position first, so the classification is robust even
   when faces do not share vertex indices.
 */
namespace FeatureEdges {

// A 3D line segment (an edge to draw).
struct Segment {
  Vector3d p0;
  Vector3d p1;
};

// Return the silhouette + crease + boundary edges of `ps` for a view looking
// along `viewDir` (any non-zero vector; only its axis matters for silhouette).
// `creaseAngle` is the dihedral threshold in radians (edges sharper than this
// are kept as creases).
std::vector<Segment> select(const PolySet& ps, const Vector3d& viewDir, double creaseAngle);

}  // namespace FeatureEdges
