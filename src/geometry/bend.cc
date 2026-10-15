/*
 *  OpenSCAD (www.openscad.org)
 *  Copyright (C) 2009-2011 Clifford Wolf <clifford@clifford.at> and
 *                          Marius Kintel <marius@kintel.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  As a special exception, you have permission to link this program
 *  with the CGAL library and distribute executables, as long as you
 *  follow the requirements of the GNU GPL in regard to all of the
 *  software in the executable aside from CGAL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "geometry/bend.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "geometry/GeometryUtils.h"
#include "geometry/Grid.h"
#include "geometry/PolySet.h"
#include "geometry/PolySetUtils.h"
#include "geometry/Reindexer.h"
#include "geometry/linalg.h"

namespace {

constexpr double EPS = 1e-9;

//! What happens to a triangle: it is deformed, it stays put, or it travels
//! rigidly with the end of the bend.
enum class Zone : uint8_t { Bend, Fixed, Carried };

struct Tri
{
  Vector3d v[3];
  int32_t color{-1};
  Zone zone{Zone::Bend};

  Vector3d centroid() const { return (v[0] + v[1] + v[2]) / 3.0; }
};

/*!
   Total ordering on points, used to make edge splitting deterministic: both
   triangles sharing an edge must produce the bit-identical split point, or the
   two halves would no longer share a vertex and the mesh would come apart.
 */
bool lexLess(const Vector3d& a, const Vector3d& b)
{
  if (a[0] != b[0]) return a[0] < b[0];
  if (a[1] != b[1]) return a[1] < b[1];
  return a[2] < b[2];
}

Vector3d splitPoint(const Vector3d& a, const Vector3d& b, int axis, double value)
{
  const Vector3d *p = &a;
  const Vector3d *q = &b;
  if (lexLess(b, a)) std::swap(p, q);
  const double denom = (*q)[axis] - (*p)[axis];
  double t = (denom == 0.0) ? 0.0 : (value - (*p)[axis]) / denom;
  t = std::clamp(t, 0.0, 1.0);
  Vector3d res = *p + t * (*q - *p);
  res[axis] = value;  // land exactly on the plane so later tests stay stable
  return res;
}

/*!
   Sutherland-Hodgman clip of a convex polygon against a half-space.
   side = +1 keeps coord >= value, side = -1 keeps coord <= value.
 */
std::vector<Vector3d> clipHalfspace(const std::vector<Vector3d>& poly, int axis, double value,
                                    double side)
{
  std::vector<Vector3d> res;
  const size_t n = poly.size();
  for (size_t i = 0; i < n; ++i) {
    const Vector3d& a = poly[i];
    const Vector3d& b = poly[(i + 1) % n];
    const double da = side * (a[axis] - value);
    const double db = side * (b[axis] - value);
    if (da >= -EPS) res.push_back(a);
    if ((da > EPS && db < -EPS) || (da < -EPS && db > EPS)) {
      res.push_back(splitPoint(a, b, axis, value));
    }
  }
  return res;
}

void emitFan(const std::vector<Vector3d>& poly, const Tri& src, std::vector<Tri>& out)
{
  for (size_t i = 1; i + 1 < poly.size(); ++i) {
    if (poly[0] == poly[i] || poly[i] == poly[i + 1] || poly[0] == poly[i + 1]) continue;
    Tri tri = src;
    tri.v[0] = poly[0];
    tri.v[1] = poly[i];
    tri.v[2] = poly[i + 1];
    out.push_back(tri);
  }
}

/*!
   Cuts every triangle for which `predicate` holds by the plane coord[axis] ==
   value. Triangles that do not straddle the plane pass through untouched, so
   subdividing by the band planes only ever touches the band.
 */
template <typename Pred>
void splitByPlane(std::vector<Tri>& tris, int axis, double value, Pred predicate)
{
  if (!std::isfinite(value)) return;

  std::vector<Tri> out;
  out.reserve(tris.size());
  for (const auto& tri : tris) {
    if (!predicate(tri)) {
      out.push_back(tri);
      continue;
    }
    bool anyPos = false;
    bool anyNeg = false;
    for (const auto& v : tri.v) {
      const double d = v[axis] - value;
      if (d > EPS) anyPos = true;
      if (d < -EPS) anyNeg = true;
    }
    if (!anyPos || !anyNeg) {
      out.push_back(tri);
      continue;
    }
    const std::vector<Vector3d> poly{tri.v[0], tri.v[1], tri.v[2]};
    emitFan(clipHalfspace(poly, axis, value, +1.0), tri, out);
    emitFan(clipHalfspace(poly, axis, value, -1.0), tri, out);
  }
  tris.swap(out);
}

/*!
   The bend map itself. With u = x - at, Theta the total turn in radians,
   R = sign(Theta)*r the signed radius and L = r*|Theta| the arc length:

     u <= 0 -> theta = 0,     s = u       (fixed part, identity)
     u >= L -> theta = Theta, s = u - L   (rigid part past the arc)
     else   -> theta = u/R,   s = 0       (on the arc)

     bent = (at, 0, R) + Ry(-theta) * (s, y, z - R)

   which is continuous and C1 at both u = 0 and u = L.
 */
struct BendMap
{
  double at{0.0};
  double R{1.0};
  double L{0.0};
  double theta_total{0.0};

  Vector3d place(double theta, double s, double y, double z) const
  {
    const double ct = std::cos(theta);
    const double st = std::sin(theta);
    const double lz = z - R;
    return {at + s * ct - lz * st, y, R + s * st + lz * ct};
  }

  Vector3d operator()(const Vector3d& v) const
  {
    const double u = v[0] - at;
    // Return the fixed part bit for bit: at + (x - at) does not round-trip
    // exactly, and a one-ulp shift would keep a vertex on a mask boundary from
    // being shared with the unmasked side, silently opening the mesh.
    if (u <= 0.0) return v;
    if (u >= L) return place(theta_total, u - L, v[1], v[2]);
    return place(u / R, 0.0, v[1], v[2]);
  }

  //! The rigid motion the material past the arc undergoes. Deliberately the
  //! same expression as the u >= L branch above, so that carried material and
  //! bent material past the arc join bit for bit.
  Vector3d rigid(const Vector3d& v) const
  {
    return place(theta_total, v[0] - at - L, v[1], v[2]);
  }

  Vector3d apply(const Vector3d& v, Zone zone) const
  {
    switch (zone) {
      case Zone::Bend: return (*this)(v);
      case Zone::Carried: return rigid(v);
      default: return v;
    }
  }

  // True while the point is on the arc proper (not in either rigid part).
  bool onArc(const Vector3d& v) const
  {
    const double u = v[0] - at;
    return u > 0.0 && u < L;
  }
};

//! Triangles sharing a welded edge, as a map from the edge to the triangles on
//! it. Welding is tolerant (Grid3d checks neighbouring cells), so hairline
//! cracks from a STEP conversion do not break the mesh into pieces.
using EdgeMap = std::map<std::pair<int, int>, std::vector<int>>;

EdgeMap buildEdgeMap(const std::vector<Tri>& tris, std::vector<std::array<int, 3>>& welded)
{
  Grid3d<int> grid(GRID_FINE);
  welded.resize(tris.size());
  for (size_t i = 0; i < tris.size(); ++i) {
    for (int k = 0; k < 3; ++k) {
      Vector3d v = tris[i].v[k];  // align() snaps its argument, so pass a copy
      welded[i][k] = grid.align(v);
    }
  }

  EdgeMap edges;
  for (size_t i = 0; i < tris.size(); ++i) {
    for (int k = 0; k < 3; ++k) {
      const int a = welded[i][k];
      const int b = welded[i][(k + 1) % 3];
      if (a != b) edges[std::minmax(a, b)].push_back(static_cast<int>(i));
    }
  }
  return edges;
}

//! Which part of the bend the material at this x belongs to.
enum : uint8_t { TOUCH_FIXED = 1, TOUCH_ARC = 2, TOUCH_CARRIED = 4 };

uint8_t touchAt(double x, const BendMap& map)
{
  const double u = x - map.at;
  if (u <= 0.0) return TOUCH_FIXED;
  if (u >= map.L) return TOUCH_CARRIED;
  return TOUCH_ARC;
}

//! Two triangles closer than glue, and which part of the bend they touch at.
//! The touch is taken at the contact point rather than at the region
//! triangle's centroid: a coarsely tessellated import can have a single facet
//! spanning the whole arc, and its centroid would then answer for material
//! metres away.
struct GluePair
{
  int a;
  int b;
  uint8_t touch;
};

/*!
   Squared distance from a point to a triangle: closest point on the face, on
   one of the three edges, or one of the vertices, whichever wins.
 */
double pointTriangleDistSq(const Vector3d& p, const Vector3d& a, const Vector3d& b,
                           const Vector3d& c)
{
  const Vector3d ab = b - a;
  const Vector3d ac = c - a;
  const Vector3d ap = p - a;
  const double d1 = ab.dot(ap);
  const double d2 = ac.dot(ap);
  if (d1 <= 0.0 && d2 <= 0.0) return ap.squaredNorm();

  const Vector3d bp = p - b;
  const double d3 = ab.dot(bp);
  const double d4 = ac.dot(bp);
  if (d3 >= 0.0 && d4 <= d3) return bp.squaredNorm();

  const Vector3d cp = p - c;
  const double d5 = ab.dot(cp);
  const double d6 = ac.dot(cp);
  if (d6 >= 0.0 && d5 <= d6) return cp.squaredNorm();

  const double vc = d1 * d4 - d3 * d2;
  if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
    const double denom = d1 - d3;
    const double v = (denom != 0.0) ? d1 / denom : 0.0;
    return (ap - v * ab).squaredNorm();
  }
  const double vb = d5 * d2 - d1 * d6;
  if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
    const double denom = d2 - d6;
    const double w = (denom != 0.0) ? d2 / denom : 0.0;
    return (ap - w * ac).squaredNorm();
  }
  const double va = d3 * d6 - d5 * d4;
  if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0) {
    const double denom = (d4 - d3) + (d5 - d6);
    const double w = (denom != 0.0) ? (d4 - d3) / denom : 0.0;
    return (p - (b + w * (c - b))).squaredNorm();
  }

  const double sum = va + vb + vc;
  if (sum <= 0.0) {
    // Degenerate (collinear) triangle: the edge cases above already cover it.
    return std::min({ap.squaredNorm(), bp.squaredNorm(), cp.squaredNorm()});
  }
  return (p - (a + (vb / sum) * ab + (vc / sum) * ac)).squaredNorm();
}

struct CellKey
{
  int64_t x, y, z;
  bool operator==(const CellKey& o) const { return x == o.x && y == o.y && z == o.z; }
};

struct CellHash
{
  size_t operator()(const CellKey& k) const
  {
    // Three odd primes; the grid is sparse, so this is plenty.
    return static_cast<size_t>(k.x * 73856093LL ^ k.y * 19349663LL ^ k.z * 83492791LL);
  }
};

int64_t cellOf(double v, double h) { return static_cast<int64_t>(std::floor(v / h)); }

//! How many grid cells the triangle's glue-inflated bounding box spans.
double cellSpan(const Tri& tri, double h, double glue)
{
  double span = 1.0;
  for (int axis = 0; axis < 3; ++axis) {
    const double lo = std::min({tri.v[0][axis], tri.v[1][axis], tri.v[2][axis]}) - glue;
    const double hi = std::max({tri.v[0][axis], tri.v[1][axis], tri.v[2][axis]}) + glue;
    span *= std::floor(hi / h) - std::floor(lo / h) + 1.0;
    if (span > 1e12) return 1e12;
  }
  return span;
}

/*!
   Grid step for the proximity search. Sized by the triangles, not by glue: with
   a cell of glue, a 100 mm plate and glue = 0.01 would mean 10^8 cells. The
   doubling loop then caps the total work, because a mesh whose triangles differ
   wildly in size - two huge plate facets next to a finely tessellated part -
   would otherwise sweep a large empty area for every one of the big facets.
 */
double chooseCellSize(const std::vector<Tri>& tris, double glue)
{
  std::vector<double> lengths;
  lengths.reserve(tris.size() * 3);
  for (const auto& tri : tris) {
    for (int k = 0; k < 3; ++k) lengths.push_back((tri.v[(k + 1) % 3] - tri.v[k]).norm());
  }
  double h = glue;
  if (!lengths.empty()) {
    const size_t mid = lengths.size() / 2;
    std::nth_element(lengths.begin(), lengths.begin() + mid, lengths.end());
    h = std::max(h, lengths[mid]);  // median: robust against a few huge facets
  }
  if (!(h > 0.0)) h = 1.0;

  const double budget = 8.0 * static_cast<double>(std::max<size_t>(tris.size(), 1));
  for (int i = 0; i < 40; ++i) {
    double total = 0.0;
    for (const auto& tri : tris) {
      total += cellSpan(tri, h, glue);
      if (total > budget) break;
    }
    if (total <= budget) break;
    h *= 2.0;
  }
  return h;
}

/*!
   Pairs of triangles that are closer than glue, measured vertex against face.
   Vertex against vertex would not do: the typical case is a part standing in
   the middle of a large plate facet, whose nearest plate vertex is a corner far
   away.

   Only pairs with at least one triangle outside the region are of any use, so
   the material inside it - normally almost the whole mesh - is indexed once and
   then only ever queried against the handful of triangles outside. Without that
   split the board would be tested against itself, which is both the bulk of the
   work and entirely wasted.

   Duplicates are harmless - uniting is idempotent and the touch flags are
   OR-ed - so only the obvious per-triangle repeats are dropped.
 */
void findProximityPairs(const std::vector<Tri>& tris, const std::vector<bool>& inRegion,
                        double glue, const BendMap& map, std::vector<GluePair>& pairs)
{
  if (!(glue > 0.0) || tris.empty()) return;

  const double h = chooseCellSize(tris, glue);
  using CellMap = std::unordered_map<CellKey, std::vector<int>, CellHash>;
  CellMap everything;
  CellMap outside;
  everything.reserve(tris.size() * 2);
  for (size_t i = 0; i < tris.size(); ++i) {
    for (int k = 0; k < 3; ++k) {
      const Vector3d& v = tris[i].v[k];
      const CellKey key{cellOf(v[0], h), cellOf(v[1], h), cellOf(v[2], h)};
      everything[key].push_back(static_cast<int>(i) * 3 + k);
      if (!inRegion[i]) outside[key].push_back(static_cast<int>(i) * 3 + k);
    }
  }
  if (outside.empty()) return;  // nothing to attach, so nothing to look for

  const double glueSq = glue * glue;
  std::vector<std::pair<int, uint8_t>> hits;  // (triangle, zone of the contact)
  for (size_t i = 0; i < tris.size(); ++i) {
    const Tri& tri = tris[i];
    int64_t lo[3];
    int64_t hi[3];
    for (int axis = 0; axis < 3; ++axis) {
      lo[axis] = cellOf(std::min({tri.v[0][axis], tri.v[1][axis], tri.v[2][axis]}) - glue, h);
      hi[axis] = cellOf(std::max({tri.v[0][axis], tri.v[1][axis], tri.v[2][axis]}) + glue, h);
    }
    hits.clear();
    // A triangle inside the region only ever needs the ones outside it.
    const CellMap& haystack = inRegion[i] ? outside : everything;
    for (int64_t cx = lo[0]; cx <= hi[0]; ++cx) {
      for (int64_t cy = lo[1]; cy <= hi[1]; ++cy) {
        for (int64_t cz = lo[2]; cz <= hi[2]; ++cz) {
          const auto it = haystack.find({cx, cy, cz});
          if (it == haystack.end()) continue;
          for (const int entry : it->second) {
            const int other = entry / 3;
            if (other == static_cast<int>(i)) continue;
            const Vector3d& contact = tris[other].v[entry % 3];
            if (pointTriangleDistSq(contact, tri.v[0], tri.v[1], tri.v[2]) > glueSq) continue;
            hits.emplace_back(other, touchAt(contact[0], map));
          }
        }
      }
    }
    std::sort(hits.begin(), hits.end());
    hits.erase(std::unique(hits.begin(), hits.end()), hits.end());
    for (const auto& [other, touch] : hits) {
      pairs.push_back({other, static_cast<int>(i), touch});
    }
  }
}

//! Union-find over triangles.
struct DisjointSet
{
  std::vector<int> parent;

  explicit DisjointSet(size_t n) : parent(n)
  {
    for (size_t i = 0; i < n; ++i) parent[i] = static_cast<int>(i);
  }
  int find(int i)
  {
    while (parent[i] != i) i = parent[i] = parent[parent[i]];
    return i;
  }
  void unite(int a, int b)
  {
    a = find(a);
    b = find(b);
    if (a != b) parent[b] = a;
  }
};

/*!
   Decides what happens to the material the bend region does not cover.

   Triangles outside the region are flood-filled into groups through shared
   edges - and, when glue is set, through mere proximity as well - and each
   group inherits the fate of the bent material it hangs off: material attached
   in front of the bend line stays put, material attached past the arc travels
   with it. A group that touches nothing at all - typically a part that merely
   rests on the board without being fused to it - is judged by its own position
   instead, exactly as if it had been inside the region.
 */
void classifyOutside(std::vector<Tri>& tris, const std::vector<bool>& inRegion,
                     const EdgeMap& edges, const std::vector<GluePair>& glued,
                     const BendMap& map, BendStats& stats)
{
  DisjointSet groups(tris.size());
  for (const auto& [edge, list] : edges) {
    for (size_t p = 0; p < list.size(); ++p) {
      for (size_t q = p + 1; q < list.size(); ++q) {
        if (!inRegion[list[p]] && !inRegion[list[q]]) groups.unite(list[p], list[q]);
      }
    }
  }
  for (const auto& pair : glued) {
    if (!inRegion[pair.a] && !inRegion[pair.b]) groups.unite(pair.a, pair.b);
  }

  // Which part of the bend a region triangle belongs to. After the band planes
  // have been cut, no region triangle straddles at or at + L.
  const auto touchOf = [&map](const Tri& tri) { return touchAt(tri.centroid()[0], map); };

  // Kept apart only so that a group which owes its anchor purely to glue can be
  // reported as such; the decision below uses the union of the two.
  std::vector<uint8_t> touched(tris.size(), 0);
  std::vector<uint8_t> touchedByGlue(tris.size(), 0);
  for (const auto& [edge, list] : edges) {
    for (size_t p = 0; p < list.size(); ++p) {
      for (size_t q = 0; q < list.size(); ++q) {
        if (p == q || inRegion[list[p]] || !inRegion[list[q]]) continue;
        touched[groups.find(list[p])] |= touchOf(tris[list[q]]);
      }
    }
  }
  for (const auto& pair : glued) {
    if (!inRegion[pair.a] && inRegion[pair.b]) touchedByGlue[groups.find(pair.a)] |= pair.touch;
    if (inRegion[pair.a] && !inRegion[pair.b]) touchedByGlue[groups.find(pair.b)] |= pair.touch;
  }

  // x extent of each group, for the isolated case.
  std::vector<double> xmin(tris.size(), BendParams::INF);
  std::vector<double> xmax(tris.size(), -BendParams::INF);
  for (size_t i = 0; i < tris.size(); ++i) {
    if (inRegion[i]) continue;
    const int root = groups.find(static_cast<int>(i));
    for (const auto& v : tris[i].v) {
      xmin[root] = std::min(xmin[root], v[0]);
      xmax[root] = std::max(xmax[root], v[0]);
    }
  }

  std::map<int, Zone> decided;
  for (size_t i = 0; i < tris.size(); ++i) {
    if (inRegion[i]) {
      tris[i].zone = Zone::Bend;
      continue;
    }
    const int root = groups.find(static_cast<int>(i));
    auto it = decided.find(root);
    if (it == decided.end()) {
      const uint8_t t = touched[root] | touchedByGlue[root];
      Zone zone;
      stats.groups++;
      if (touched[root] == 0 && touchedByGlue[root] != 0) stats.groups_glued++;
      // Orthogonal to the outcome: hanging off deforming material always tears.
      if (t & TOUCH_ARC) stats.groups_on_arc++;
      if ((t & TOUCH_FIXED) && (t & TOUCH_CARRIED)) {
        // Bridges the bend: no single rigid motion fits, so freeze and say so.
        stats.groups_conflict++;
        zone = Zone::Fixed;
      } else if (t & TOUCH_CARRIED) {
        zone = Zone::Carried;
      } else if (t & (TOUCH_FIXED | TOUCH_ARC)) {
        zone = Zone::Fixed;
      } else {
        // Touches nothing that is being bent - typically a part that merely
        // rests on the board. Judge it by its own position instead.
        stats.groups_isolated++;
        if (xmin[root] >= map.at + map.L) {
          zone = Zone::Carried;
        } else {
          // Lying across the bend is the same predicament as bridging it: no
          // single rigid motion describes where it should end up.
          if (xmax[root] > map.at) stats.groups_conflict++;
          zone = Zone::Fixed;
        }
      }
      // fixed + carried always adds up to the group count; conflict, on_arc and
      // isolated annotate how a group got there.
      if (zone == Zone::Carried) stats.groups_carried++;
      else stats.groups_fixed++;
      it = decided.emplace(root, zone).first;
    }
    tris[i].zone = it->second;
  }
}

/*!
   Counts edges whose two sides end up under different transforms and really do
   come apart. Bent material past the arc and carried material use the same
   expression, and the bend map is the exact identity in front of the bend line,
   so a seam only shows up where the model genuinely tears.
 */
void findTears(const std::vector<Tri>& tris, const EdgeMap& edges,
               const std::vector<std::array<int, 3>>& welded, const BendMap& map, BendStats& stats)
{
  for (const auto& [edge, list] : edges) {
    for (size_t p = 0; p < list.size(); ++p) {
      for (size_t q = p + 1; q < list.size(); ++q) {
        const Tri& a = tris[list[p]];
        const Tri& b = tris[list[q]];
        if (a.zone == b.zone) continue;
        double gap = 0.0;
        for (int k = 0; k < 3; ++k) {
          const int id = welded[list[p]][k];
          if (id != edge.first && id != edge.second) continue;
          gap = std::max(gap, (map.apply(a.v[k], a.zone) - map.apply(a.v[k], b.zone)).norm());
        }
        if (gap > EPS) {
          stats.torn_edges++;
          stats.max_gap = std::max(stats.max_gap, gap);
        }
      }
    }
  }
}

}  // namespace

std::unique_ptr<PolySet> bendPolySet(const PolySet& ps, const BendParams& params, BendStats& stats)
{
  auto tess = PolySetUtils::tessellate_faces(ps);

  const double theta_total = params.angle * M_PI / 180.0;
  if (theta_total == 0.0 || params.r <= 0.0) return tess;

  BendMap map;
  map.at = params.at;
  map.R = (theta_total > 0.0) ? params.r : -params.r;
  map.theta_total = theta_total;
  map.L = params.r * std::fabs(theta_total);

  const auto inMask = [&params](const Vector3d& c) {
    return c[0] >= params.xmin && c[0] <= params.xmax && c[1] >= params.ymin &&
           c[1] <= params.ymax && c[2] >= params.zmin && c[2] <= params.zmax;
  };
  const auto affected = [&inMask](const Tri& tri) { return inMask(tri.centroid()); };
  const auto always = [](const Tri&) { return true; };

  // ---- gather triangles -------------------------------------------------
  std::vector<Tri> tris;
  tris.reserve(tess->indices.size());
  const bool hasColors = !tess->color_indices.empty();
  for (size_t i = 0; i < tess->indices.size(); ++i) {
    const auto& face = tess->indices[i];
    if (face.size() < 3) continue;
    const int32_t color = hasColors ? tess->color_indices[i] : -1;
    // tessellate_faces() gives triangles; fan anything else just in case.
    for (size_t k = 1; k + 1 < face.size(); ++k) {
      Tri tri;
      tri.v[0] = tess->vertices[face[0]];
      tri.v[1] = tess->vertices[face[k]];
      tri.v[2] = tess->vertices[face[k + 1]];
      tri.color = color;
      tris.push_back(tri);
    }
  }

  // ---- subdivide --------------------------------------------------------
  // Mask planes first, on everything: this turns a tear into a clean hole with
  // a straight edge instead of a triangle stretched across the gap.
  splitByPlane(tris, 0, params.xmin, always);
  splitByPlane(tris, 0, params.xmax, always);
  splitByPlane(tris, 1, params.ymin, always);
  splitByPlane(tris, 1, params.ymax, always);
  splitByPlane(tris, 2, params.zmin, always);
  splitByPlane(tris, 2, params.zmax, always);

  // Band planes, only where the bend actually applies. Cut after the mask
  // planes but before classifying: cutting only the region's triangles leaves
  // T-junctions along the region boundary, which would break edge adjacency.
  // Without a mask nothing is classified and every triangle keeps its default
  // Zone::Bend, so this one predicate covers both cases.
  const auto isBend = [](const Tri& tri) { return tri.zone == Zone::Bend; };

  // ---- classify the material outside the bend region ---------------------
  // Without a mask every triangle is in the region, so there is nothing to
  // classify and no two zones for a tear to open between - the whole pass is
  // provably a no-op and only costs time on large imports.
  if (params.hasMask()) {
    std::vector<bool> inRegion(tris.size());
    for (size_t i = 0; i < tris.size(); ++i) inRegion[i] = affected(tris[i]);

    std::vector<std::array<int, 3>> welded;
    const EdgeMap edges = buildEdgeMap(tris, welded);
    std::vector<GluePair> glued;
    findProximityPairs(tris, inRegion, params.glue, map, glued);
    classifyOutside(tris, inRegion, edges, glued, map, stats);
    findTears(tris, edges, welded, map, stats);
  }

  const int segments = std::max(1, params.segments);
  for (int k = 0; k <= segments; ++k) {
    splitByPlane(tris, 0, params.at + map.L * k / segments, isBend);
  }

  // The map folds through the bend axis where the material reaches past the
  // centre of curvature; that is the physical minimum bend radius.
  const double sign = (map.R > 0.0) ? 1.0 : -1.0;
  for (const auto& tri : tris) {
    if (tri.zone != Zone::Bend) continue;
    for (const auto& v : tri.v) {
      if (map.onArc(v) && (map.R - v[2]) * sign <= 0.0) stats.inverted = true;
    }
  }

  // ---- apply ------------------------------------------------------------
  auto result = std::make_unique<PolySet>(3);
  result->setConvexity(ps.getConvexity());
  result->setTriangular(true);
  result->colors = tess->colors;

  Reindexer<Vector3d> outIndex;
  outIndex.reserve(tess->vertices.size());
  result->indices.reserve(tris.size());
  for (const auto& tri : tris) {
    IndexedFace face;
    for (const auto& v : tri.v) face.push_back(outIndex.lookup(map.apply(v, tri.zone)));
    if (face[0] == face[1] || face[1] == face[2] || face[0] == face[2]) continue;
    result->indices.push_back(face);
    if (hasColors) result->color_indices.push_back(tri.color);
  }
  outIndex.copy(std::back_inserter(result->vertices));

  result->setManifold(ps.isManifold() && stats.torn_edges == 0 && !stats.inverted);
  return result;
}
