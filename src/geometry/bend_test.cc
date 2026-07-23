#include "geometry/bend.h"

#include <catch2/catch_all.hpp>
#include <cmath>
#include <map>
#include <utility>
#include <vector>

#include "geometry/PolySet.h"
#include "geometry/linalg.h"

namespace {

//! A closed box mesh, so that watertightness of the result is meaningful.
std::unique_ptr<PolySet> makeBox(const Vector3d& lo, const Vector3d& hi)
{
  auto ps = std::make_unique<PolySet>(3);
  ps->vertices = {
    {lo.x(), lo.y(), lo.z()}, {hi.x(), lo.y(), lo.z()}, {hi.x(), hi.y(), lo.z()},
    {lo.x(), hi.y(), lo.z()}, {lo.x(), lo.y(), hi.z()}, {hi.x(), lo.y(), hi.z()},
    {hi.x(), hi.y(), hi.z()}, {lo.x(), hi.y(), hi.z()},
  };
  const int faces[6][4] = {
    {0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7},
  };
  for (const auto& f : faces) {
    ps->indices.push_back({f[0], f[1], f[2]});
    ps->indices.push_back({f[0], f[2], f[3]});
  }
  ps->setTriangular(true);
  ps->setManifold(true);
  return ps;
}

//! Number of half-edges without an opposite twin; 0 means the mesh is closed.
int unpairedHalfEdges(const PolySet& ps)
{
  std::map<std::pair<int, int>, int> half;
  for (const auto& face : ps.indices) {
    for (size_t i = 0; i < face.size(); ++i) {
      half[{face[i], face[(i + 1) % face.size()]}]++;
    }
  }
  int bad = 0;
  for (const auto& [edge, count] : half) {
    if (count != 1 || half[{edge.second, edge.first}] != 1) bad++;
  }
  return bad;
}

BoundingBox bbox(const PolySet& ps)
{
  BoundingBox box;
  for (const auto& v : ps.vertices) box.extend(v);
  return box;
}

}  // namespace

TEST_CASE("bend() with angle 0 is the identity", "[bend]")
{
  auto plate = makeBox({0, 0, 0}, {30, 10, 0.4});
  BendParams params;
  params.at = 10;
  params.r = 2;
  params.angle = 0;
  BendStats stats;

  auto bent = bendPolySet(*plate, params, stats);
  CHECK(bent->indices.size() == plate->indices.size());
  CHECK(bent->vertices == plate->vertices);
  CHECK(stats.torn_edges == 0);
  CHECK_FALSE(stats.inverted);
}

TEST_CASE("bend() keeps the part before the bend line and lifts the rest", "[bend]")
{
  // 30 long, bent 90 degrees at x = 10 around r = 2. The neutral fibre is the
  // bottom face (z = 0), so the arc consumes r*pi/2 of length and the straight
  // remainder stands up from z = r.
  const double r = 2.0;
  const double L = r * M_PI / 2.0;
  auto plate = makeBox({0, 0, 0}, {30, 10, 0.4});

  BendParams params;
  params.at = 10;
  params.r = r;
  params.angle = 90;
  params.segments = 8;
  BendStats stats;

  auto bent = bendPolySet(*plate, params, stats);

  CHECK(stats.torn_edges == 0);
  CHECK_FALSE(stats.inverted);
  CHECK(unpairedHalfEdges(*bent) == 0);

  const BoundingBox box = bbox(*bent);
  // Nothing moved in front of the bend line, and the arc reaches x = at + r.
  CHECK(box.min().x() == Catch::Approx(0.0));
  CHECK(box.max().x() == Catch::Approx(params.at + r));
  // Height: rise of the arc plus the straight remainder standing upright.
  CHECK(box.max().z() == Catch::Approx(r + (30.0 - params.at - L)));
  CHECK(box.min().y() == Catch::Approx(0.0));
  CHECK(box.max().y() == Catch::Approx(10.0));
}

TEST_CASE("bend() subdivides only inside the band", "[bend]")
{
  BendParams params;
  params.at = 10;
  params.r = 2;
  params.angle = 90;
  params.segments = 8;
  BendStats stats;

  auto shortPlate = makeBox({0, 0, 0}, {30, 10, 0.4});
  auto longPlate = makeBox({0, 0, 0}, {60, 10, 0.4});
  auto shortBent = bendPolySet(*shortPlate, params, stats);
  auto longBent = bendPolySet(*longPlate, params, stats);

  // The band itself gets cut...
  CHECK(shortBent->indices.size() > shortPlate->indices.size());
  // ...but the extra 30 units of rigid material past the band add nothing, so
  // subdivision really is local to the bend.
  CHECK(shortBent->indices.size() == longBent->indices.size());
}

TEST_CASE("bend() leaves masked-out material exactly in place", "[bend]")
{
  auto plate = makeBox({0, 0, 0}, {30, 10, 0.4});
  BendParams params;
  params.at = 10;
  params.r = 2;
  params.angle = 90;
  params.ymin = 100;  // nothing is inside the mask
  params.ymax = 200;
  BendStats stats;

  auto bent = bendPolySet(*plate, params, stats);
  CHECK(stats.torn_edges == 0);
  const BoundingBox box = bbox(*bent);
  CHECK(box.max().x() == Catch::Approx(30.0));
  CHECK(box.max().z() == Catch::Approx(0.4));
}

TEST_CASE("bend() reports a tear when the mask boundary cuts material", "[bend]")
{
  // The plate spans y = 0..20 and reaches past the bend line, so restricting the
  // bend to y <= 6 splits material that is being moved.
  auto plate = makeBox({0, 0, 0}, {30, 20, 0.4});
  BendParams params;
  params.at = 10;
  params.r = 2;
  params.angle = 90;
  params.ymax = 6;
  BendStats stats;

  auto bent = bendPolySet(*plate, params, stats);
  CHECK(stats.torn_edges > 0);
  CHECK(stats.max_gap > 1.0);
  CHECK(unpairedHalfEdges(*bent) > 0);  // a tear really does open the mesh
}

TEST_CASE("bend() does not report a tear when the mask boundary is in free air", "[bend]")
{
  // Only the region past the bend line matters: here the mask cuts the plate at
  // y = 6, but everything it separates sits in front of the bend line where the
  // map is the identity anyway.
  auto plate = makeBox({0, 0, 0}, {9, 20, 0.4});
  BendParams params;
  params.at = 10;
  params.r = 2;
  params.angle = 90;
  params.ymax = 6;
  BendStats stats;

  auto bent = bendPolySet(*plate, params, stats);
  CHECK(stats.torn_edges == 0);
  CHECK(unpairedHalfEdges(*bent) == 0);
}

TEST_CASE("bend() flags a radius below the minimum bend radius", "[bend]")
{
  // Material reaches z = 5 while the centre of curvature sits at z = r = 1, so
  // the inner surface folds through the bend axis.
  auto block = makeBox({0, 0, 0}, {20, 10, 5});
  BendParams params;
  params.at = 5;
  params.r = 1;
  params.angle = 90;
  params.segments = 8;
  BendStats stats;

  auto bent = bendPolySet(*block, params, stats);
  CHECK(stats.inverted);
}

TEST_CASE("bend() with a negative angle curves the other way", "[bend]")
{
  auto plate = makeBox({0, 0, 0}, {30, 10, 0.4});
  BendParams params;
  params.at = 10;
  params.r = 2;
  params.angle = -90;
  params.segments = 8;
  BendStats stats;

  auto bent = bendPolySet(*plate, params, stats);
  const BoundingBox box = bbox(*bent);
  CHECK(box.max().z() == Catch::Approx(0.4));
  CHECK(box.min().z() == Catch::Approx(-(2.0 + (30.0 - 10.0 - 2.0 * M_PI / 2.0))));
  CHECK(unpairedHalfEdges(*bent) == 0);
}
