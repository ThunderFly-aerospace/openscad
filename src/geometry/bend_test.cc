#include "geometry/bend.h"

#include <catch2/catch_all.hpp>
#include <functional>
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

/*!
   A closed prismatic plate of arbitrary orthogonal outline, built on a shared
   vertex grid so that neighbouring cells really are edge-connected. Boxes
   merely placed next to each other would not be, and connectivity is exactly
   what these tests are about.
 */
std::unique_ptr<PolySet> makePlate(int nx, int ny, const Vector3d& origin, double cell,
                                   double thickness,
                                   const std::function<bool(int, int)>& inside)
{
  auto ps = std::make_unique<PolySet>(3);
  const int stride = ny + 1;
  const int top = (nx + 1) * (ny + 1);
  for (int layer = 0; layer < 2; ++layer) {
    for (int i = 0; i <= nx; ++i) {
      for (int j = 0; j <= ny; ++j) {
        ps->vertices.emplace_back(origin.x() + i * cell, origin.y() + j * cell,
                                  origin.z() + layer * thickness);
      }
    }
  }
  const auto b = [&](int i, int j) { return i * stride + j; };
  const auto t = [&](int i, int j) { return top + i * stride + j; };
  const auto quad = [&](int a, int bb, int c, int d) {
    ps->indices.push_back({a, bb, c});
    ps->indices.push_back({a, c, d});
  };

  for (int i = 0; i < nx; ++i) {
    for (int j = 0; j < ny; ++j) {
      if (!inside(i, j)) continue;
      quad(b(i, j), b(i, j + 1), b(i + 1, j + 1), b(i + 1, j));  // -z
      quad(t(i, j), t(i + 1, j), t(i + 1, j + 1), t(i, j + 1));  // +z
      // Walls wherever the neighbour is not part of the plate. The corner pairs
      // run counter-clockwise seen from +z, which makes the walls face outwards.
      const int corners[4][2] = {{i, j}, {i + 1, j}, {i + 1, j + 1}, {i, j + 1}};
      const int neighbour[4][2] = {{i, j - 1}, {i + 1, j}, {i, j + 1}, {i - 1, j}};
      for (int e = 0; e < 4; ++e) {
        const int ni = neighbour[e][0];
        const int nj = neighbour[e][1];
        const bool outside =
          ni < 0 || nj < 0 || ni >= nx || nj >= ny || !inside(ni, nj);
        if (!outside) continue;
        const int *A = corners[e];
        const int *B = corners[(e + 1) % 4];
        quad(b(A[0], A[1]), b(B[0], B[1]), t(B[0], B[1]), t(A[0], A[1]));
      }
    }
  }
  ps->setTriangular(true);
  return ps;
}

//! Appends one mesh into another as a separate, unconnected shell.
void appendShell(PolySet& into, const PolySet& shell)
{
  const int base = static_cast<int>(into.vertices.size());
  into.vertices.insert(into.vertices.end(), shell.vertices.begin(), shell.vertices.end());
  for (const auto& face : shell.indices) {
    IndexedFace moved;
    for (const auto& idx : face) moved.push_back(idx + base);
    into.indices.push_back(moved);
  }
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

namespace {

//! Board of two wide plates joined by a narrow rib, the shape the bend region
//! cannot cover on its own: x 0..10 and x 30..45 span the full width, the rib
//! between them is only y -3..3.
std::unique_ptr<PolySet> makeRibBoard()
{
  return makePlate(45, 30, {0, -15, 0}, 1.0, 0.4, [](int i, int j) {
    if (i < 10 || i >= 30) return true;
    return j >= 12 && j < 18;
  });
}

BendParams ribBend()
{
  BendParams params;
  params.at = 15;
  params.r = 2;
  params.angle = 90;
  params.ymin = -4;
  params.ymax = 4;
  params.segments = 8;
  return params;
}

}  // namespace

TEST_CASE("bend() carries material connected past the bend along with it", "[bend]")
{
  auto board = makeRibBoard();
  const BendParams params = ribBend();
  BendStats stats;

  auto bent = bendPolySet(*board, params, stats);

  // Four pieces sit outside the region: the two wings of each wide plate. The
  // near ones hang off the fixed side, the far ones off the moving side.
  CHECK(stats.groups == 4);
  CHECK(stats.groups_fixed == 2);
  CHECK(stats.groups_carried == 2);
  CHECK(stats.groups_conflict == 0);
  CHECK(stats.groups_on_arc == 0);
  CHECK(stats.groups_isolated == 0);
  // Carried material joins the bent end seamlessly, so nothing tears.
  CHECK(stats.torn_edges == 0);
  CHECK(unpairedHalfEdges(*bent) == 0);

  // The far plate stood up: it used to reach x = 45 and z = 0.4 only.
  const BoundingBox box = bbox(*bent);
  CHECK(box.max().x() < 20.0);
  CHECK(box.max().z() == Catch::Approx(2.0 + (45.0 - 15.0 - 2.0 * M_PI / 2.0)));
}

TEST_CASE("bend() reports a part that bridges the bend", "[bend]")
{
  // A plate of constant width: the material outside the region runs from the
  // fixed side all the way to the moving side, so no rigid motion fits it.
  auto plate = makePlate(40, 30, {0, -15, 0}, 1.0, 0.4, [](int, int) { return true; });
  const BendParams params = ribBend();
  BendStats stats;

  auto bent = bendPolySet(*plate, params, stats);

  CHECK(stats.groups == 2);
  CHECK(stats.groups_conflict == 2);
  CHECK(stats.groups_fixed == 2);  // frozen, since neither motion is right
  CHECK(stats.groups_carried == 0);
  CHECK(stats.torn_edges > 0);
}

TEST_CASE("bend() places a disconnected part by its own position", "[bend]")
{
  // A part that merely rests on the board - a separate shell, as a component in
  // an imported STL usually is - has nothing to inherit from.
  const BendParams params = ribBend();
  const double past = params.at + params.r * M_PI / 2.0;

  SECTION("past the bend it travels with the moving end")
  {
    auto board = makeRibBoard();
    appendShell(*board, *makePlate(4, 4, {past + 5, 5, 0.5}, 1.0, 3.0,
                                   [](int, int) { return true; }));
    BendStats stats;
    auto bent = bendPolySet(*board, params, stats);
    CHECK(stats.groups == 5);
    CHECK(stats.groups_isolated == 1);
    CHECK(stats.groups_carried == 3);
  }

  SECTION("in front of the bend line it stays put")
  {
    auto board = makeRibBoard();
    appendShell(*board, *makePlate(4, 4, {5, 5, 0.5}, 1.0, 3.0, [](int, int) { return true; }));
    BendStats stats;
    auto bent = bendPolySet(*board, params, stats);
    CHECK(stats.groups == 5);
    CHECK(stats.groups_isolated == 1);
    CHECK(stats.groups_fixed == 3);
  }

  SECTION("lying across the bend is reported, not silently frozen")
  {
    // Spans x = 13..20, i.e. from in front of the bend line to past the arc.
    // No single rigid motion describes where such a part should end up.
    auto board = makeRibBoard();
    appendShell(*board, *makePlate(7, 4, {13, 5, 0.5}, 1.0, 3.0, [](int, int) { return true; }));
    BendStats stats;
    auto bent = bendPolySet(*board, params, stats);
    CHECK(stats.groups == 5);
    CHECK(stats.groups_isolated == 1);
    CHECK(stats.groups_conflict == 1);
    CHECK(stats.groups_fixed == 3);
  }
}

namespace {

//! A plain board, entirely inside the bend region, for the glue tests. The
//! region is bounded in z only, which is how components get excluded in
//! practice: the board takes part in the bend, anything standing on it does not.
std::unique_ptr<PolySet> makeGlueBoard()
{
  return makePlate(40, 6, {0, -3, 0}, 1.0, 0.4, [](int, int) { return true; });
}

BendParams glueBend()
{
  BendParams params;
  params.at = 15;
  params.r = 2;
  params.angle = 90;  // arc ends at x = 15 + 2*pi/2 = 18.14
  params.zmax = 0.45;
  params.segments = 8;
  return params;
}

//! A component resting on the board with a 0.1 gap, straddling the end of the
//! arc. Judged on its own position it is a conflict and freezes - while the
//! board underneath drives away, which is the case an imported STL full of
//! separate shells runs into.
std::unique_ptr<PolySet> makeComponent(double x0, double x1)
{
  return makeBox({x0, -2, 0.5}, {x1, 2, 3.5});
}

}  // namespace

TEST_CASE("glue attaches a part that merely rests on the bent material", "[bend]")
{
  auto board = makeGlueBoard();
  appendShell(*board, *makeComponent(16, 24));
  BendParams params = glueBend();

  SECTION("without glue it is judged on its own and left behind")
  {
    BendStats stats;
    auto bent = bendPolySet(*board, params, stats);
    CHECK(stats.groups == 1);
    CHECK(stats.groups_isolated == 1);
    CHECK(stats.groups_conflict == 1);
    CHECK(stats.groups_glued == 0);
    CHECK(stats.groups_carried == 0);
  }

  SECTION("with glue it inherits from the material it stands on")
  {
    params.glue = 0.2;  // wider than the 0.1 gap
    BendStats stats;
    auto bent = bendPolySet(*board, params, stats);
    CHECK(stats.groups == 1);
    CHECK(stats.groups_isolated == 0);
    CHECK(stats.groups_conflict == 0);
    CHECK(stats.groups_glued == 1);
    CHECK(stats.groups_carried == 1);
    // Standing on material that is being deformed cannot stay rigid; say so.
    CHECK(stats.groups_on_arc == 1);
  }
}

TEST_CASE("glue measures vertex against face, not vertex against vertex", "[bend]")
{
  // One box for the board, so its top is two huge triangles and the nearest
  // board vertex to the component is a corner tens of millimetres away. Only a
  // point-to-face distance can tell that the component is resting on it.
  auto board = makeBox({0, -10, 0}, {40, 10, 0.4});
  appendShell(*board, *makeComponent(16, 24));
  BendParams params = glueBend();

  BendStats loose;
  auto bent = bendPolySet(*board, params, loose);
  CHECK(loose.groups_glued == 0);
  CHECK(loose.groups_carried == 0);

  params.glue = 0.2;
  BendStats stats;
  auto bent2 = bendPolySet(*board, params, stats);
  CHECK(stats.groups_glued == 1);
  CHECK(stats.groups_carried == 1);
}

TEST_CASE("glue unites shells of a solid that arrived split", "[bend]")
{
  // Two halves of one component sharing a face but not a single vertex, the way
  // a converted STL often comes out. Together they must act as one piece.
  auto board = makeGlueBoard();
  appendShell(*board, *makeComponent(16, 20));
  appendShell(*board, *makeComponent(20.05, 24));  // a hairline gap, not a shared face
  BendParams params = glueBend();

  BendStats loose;
  auto bent = bendPolySet(*board, params, loose);
  CHECK(loose.groups == 2);  // each half on its own, and they disagree
  CHECK(loose.groups_fixed == 1);
  CHECK(loose.groups_carried == 1);

  params.glue = 0.2;
  BendStats stats;
  auto bent2 = bendPolySet(*board, params, stats);
  CHECK(stats.groups == 1);  // the halves became one group
  CHECK(stats.groups_carried == 1);
}

TEST_CASE("glue still reports a part that spans the bend", "[bend]")
{
  // A bar resting on the board from well before the bend line to past the arc.
  // Gluing gives it two anchors that move differently, which is the same
  // predicament as a bridge built out of connected material: freeze and report.
  auto board = makeGlueBoard();
  appendShell(*board, *makeComponent(5, 24));
  BendParams params = glueBend();
  params.glue = 0.2;

  BendStats stats;
  auto bent = bendPolySet(*board, params, stats);
  CHECK(stats.groups == 1);
  CHECK(stats.groups_conflict == 1);
  CHECK(stats.groups_carried == 0);
}

TEST_CASE("glue does not reach further than it is told", "[bend]")
{
  // The same component, but floating 0.5 above the board instead of 0.1.
  auto board = makeGlueBoard();
  appendShell(*board, *makeBox({16, -2, 0.9}, {24, 2, 3.9}));
  BendParams params = glueBend();
  params.glue = 0.2;

  BendStats stats;
  auto bent = bendPolySet(*board, params, stats);
  CHECK(stats.groups_glued == 0);
  CHECK(stats.groups_isolated == 1);
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
