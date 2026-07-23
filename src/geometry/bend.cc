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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "geometry/GeometryUtils.h"
#include "geometry/PolySet.h"
#include "geometry/PolySetUtils.h"
#include "geometry/Reindexer.h"
#include "geometry/linalg.h"

namespace {

constexpr double EPS = 1e-9;

struct Tri
{
  Vector3d v[3];
  int32_t color{-1};

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

void emitFan(const std::vector<Vector3d>& poly, int32_t color, std::vector<Tri>& out)
{
  for (size_t i = 1; i + 1 < poly.size(); ++i) {
    if (poly[0] == poly[i] || poly[i] == poly[i + 1] || poly[0] == poly[i + 1]) continue;
    Tri tri;
    tri.v[0] = poly[0];
    tri.v[1] = poly[i];
    tri.v[2] = poly[i + 1];
    tri.color = color;
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
    emitFan(clipHalfspace(poly, axis, value, +1.0), tri.color, out);
    emitFan(clipHalfspace(poly, axis, value, -1.0), tri.color, out);
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

  Vector3d operator()(const Vector3d& v) const
  {
    const double u = v[0] - at;
    // Return the fixed part bit for bit: at + (x - at) does not round-trip
    // exactly, and a one-ulp shift would keep a vertex on a mask boundary from
    // being shared with the unmasked side, silently opening the mesh.
    if (u <= 0.0) return v;

    double theta;
    double s;
    if (u >= L) {
      theta = theta_total;
      s = u - L;
    } else {
      theta = u / R;
      s = 0.0;
    }
    const double ct = std::cos(theta);
    const double st = std::sin(theta);
    const double lz = v[2] - R;
    return {at + s * ct - lz * st, v[1], R + s * st + lz * ct};
  }

  // True while the point is on the arc proper (not in either rigid part).
  bool onArc(const Vector3d& v) const
  {
    const double u = v[0] - at;
    return u > 0.0 && u < L;
  }
};

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

  // Band planes, only where the bend actually applies.
  const int segments = std::max(1, params.segments);
  for (int k = 0; k <= segments; ++k) {
    splitByPlane(tris, 0, params.at + map.L * k / segments, affected);
  }

  // ---- diagnostics ------------------------------------------------------
  // A tear is an edge whose two adjacent triangles disagree about being bent,
  // provided the bent side really moves (in the x < at region the map is the
  // identity, so those edges are harmless).
  if (params.hasMask()) {
    Reindexer<Vector3d> index;
    std::map<std::pair<int, int>, uint8_t> edges;
    for (const auto& tri : tris) {
      const uint8_t flag = affected(tri) ? 1 : 2;
      int idx[3];
      for (int i = 0; i < 3; ++i) idx[i] = index.lookup(tri.v[i]);
      for (int i = 0; i < 3; ++i) {
        const int a = idx[i];
        const int b = idx[(i + 1) % 3];
        edges[std::minmax(a, b)] |= flag;
      }
    }
    const std::vector<Vector3d>& points = index.getArray();
    for (const auto& [edge, flags] : edges) {
      if (flags != 3) continue;
      const double gap = std::max((map(points[edge.first]) - points[edge.first]).norm(),
                                  (map(points[edge.second]) - points[edge.second]).norm());
      if (gap > EPS) {
        stats.torn_edges++;
        stats.max_gap = std::max(stats.max_gap, gap);
      }
    }
  }

  // The map folds through the bend axis where the material reaches past the
  // centre of curvature; that is the physical minimum bend radius.
  const double sign = (map.R > 0.0) ? 1.0 : -1.0;
  for (const auto& tri : tris) {
    if (!affected(tri)) continue;
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
    const bool bend = affected(tri);
    IndexedFace face;
    for (const auto& v : tri.v) face.push_back(outIndex.lookup(bend ? map(v) : v));
    if (face[0] == face[1] || face[1] == face[2] || face[0] == face[2]) continue;
    result->indices.push_back(face);
    if (hasColors) result->color_indices.push_back(tri.color);
  }
  outIndex.copy(std::back_inserter(result->vertices));

  result->setManifold(ps.isManifold() && stats.torn_edges == 0 && !stats.inverted);
  return result;
}
