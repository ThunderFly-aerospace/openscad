#include "geometry/FeatureEdges.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "geometry/PolySet.h"

namespace FeatureEdges {

namespace {

// Weld vertices by quantized position so edges shared by faces that happen to
// use different vertex indices are still recognised as the same edge.
int weld(std::map<std::tuple<int64_t, int64_t, int64_t>, int>& canon,
         std::vector<Vector3d>& points, const Vector3d& p)
{
  constexpr double kGrid = 1e6;  // 1e-6 mm resolution
  const auto key = std::make_tuple(static_cast<int64_t>(std::llround(p.x() * kGrid)),
                                   static_cast<int64_t>(std::llround(p.y() * kGrid)),
                                   static_cast<int64_t>(std::llround(p.z() * kGrid)));
  const auto it = canon.find(key);
  if (it != canon.end()) return it->second;
  const int idx = static_cast<int>(points.size());
  canon.emplace(key, idx);
  points.push_back(p);
  return idx;
}

// Newell's method: a robust polygon normal (unnormalised) over welded points.
Vector3d faceNormal(const std::vector<int>& face, const std::vector<Vector3d>& points)
{
  Vector3d n = Vector3d::Zero();
  const size_t m = face.size();
  for (size_t i = 0; i < m; ++i) {
    const Vector3d& a = points[face[i]];
    const Vector3d& b = points[face[(i + 1) % m]];
    n.x() += (a.y() - b.y()) * (a.z() + b.z());
    n.y() += (a.z() - b.z()) * (a.x() + b.x());
    n.z() += (a.x() - b.x()) * (a.y() + b.y());
  }
  return n;
}

}  // namespace

std::vector<Segment> select(const PolySet& ps, const Vector3d& viewDir, double creaseAngle)
{
  // 1. Weld vertices and remap every face to canonical indices.
  std::map<std::tuple<int64_t, int64_t, int64_t>, int> canon;
  std::vector<Vector3d> points;
  points.reserve(ps.vertices.size());
  std::vector<std::vector<int>> faces;
  faces.reserve(ps.indices.size());
  for (const auto& face : ps.indices) {
    std::vector<int> f;
    f.reserve(face.size());
    for (int vi : face) {
      if (vi < 0 || vi >= static_cast<int>(ps.vertices.size())) continue;
      f.push_back(weld(canon, points, ps.vertices[vi]));
    }
    if (f.size() >= 3) faces.push_back(std::move(f));
  }

  // 2. Per-face normals.
  std::vector<Vector3d> normals(faces.size());
  for (size_t i = 0; i < faces.size(); ++i) {
    const Vector3d n = faceNormal(faces[i], points);
    const double len = n.norm();
    if (len > 1e-12) normals[i] = n / len;
    else normals[i] = Vector3d::Zero();
  }

  // 3. Edge -> adjacent faces (undirected key = sorted vertex pair).
  std::map<std::pair<int, int>, std::vector<int>> edgeFaces;
  for (size_t fi = 0; fi < faces.size(); ++fi) {
    const auto& f = faces[fi];
    for (size_t i = 0; i < f.size(); ++i) {
      int a = f[i], b = f[(i + 1) % f.size()];
      if (a == b) continue;
      if (a > b) std::swap(a, b);
      edgeFaces[{a, b}].push_back(static_cast<int>(fi));
    }
  }

  Vector3d view(0, 0, 1);
  if (viewDir.norm() > 1e-12) view = viewDir.normalized();
  const double cosCrease = std::cos(creaseAngle);
  // Grazing band (|n·view| below this counts as edge-on) for the silhouette test.
  // ~1.1°: small enough not to thicken the contour, large enough to swallow the
  // tessellation/numeric noise that otherwise leaves gaps in a curved outline.
  constexpr double kGrazeTol = 0.02;

  // 4. Classify each edge.
  std::vector<Segment> out;
  out.reserve(edgeFaces.size() / 4 + 1);
  for (const auto& [edge, adj] : edgeFaces) {
    bool keep = false;
    if (adj.size() != 2) {
      keep = true;  // boundary (1) or non-manifold (>2): always a feature edge
    } else {
      const Vector3d& nA = normals[adj[0]];
      const Vector3d& nB = normals[adj[1]];
      const double da = nA.dot(view);
      const double db = nB.dot(view);
      // Crease: dihedral sharper than the threshold (view-independent).
      if (nA.dot(nB) < cosCrease) {
        keep = true;
      }
      // Silhouette: the two faces face opposite ways relative to the view. A
      // grazing tolerance (a face whose normal is nearly perpendicular to the
      // view is on neither side) closes gaps in the contour of a tessellated
      // curved surface: without it a facet that is almost edge-on — or whose
      // neighbour is a flat face nearly parallel to the projection plane — can
      // land a hair on the "wrong" side and drop a segment of the outline.
      else if (da * db <= 0.0 || std::abs(da) <= kGrazeTol || std::abs(db) <= kGrazeTol) {
        keep = true;
      }
    }
    if (keep) out.push_back({points[edge.first], points[edge.second]});
  }
  return out;
}

}  // namespace FeatureEdges
