// Tests for the shared vector line-drawing projector used by the drawing dock
// and the file export (see export_line_drawing.{h,cc}). Placed under src/gui/ so
// the unit-test glob picks it up; it exercises only io/geometry (no GUI).

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_all.hpp>

#include "geometry/PolySet.h"
#include "io/export_line_drawing.h"

namespace {

// A faceted cylinder of radius R and length L along the +X axis: two rings in the
// YZ plane (x=0 and x=L), side quads, and an n-gon cap at each end.
std::unique_ptr<PolySet> cylinderAlongX(int n, double R, double L)
{
  auto ps = std::make_unique<PolySet>(3);
  std::vector<int> ring0, ring1;
  for (int i = 0; i < n; ++i) {
    const double a = 2.0 * M_PI * i / n;
    ring0.push_back(static_cast<int>(ps->vertices.size()));
    ps->vertices.emplace_back(0.0, R * std::cos(a), R * std::sin(a));
  }
  for (int i = 0; i < n; ++i) {
    const double a = 2.0 * M_PI * i / n;
    ring1.push_back(static_cast<int>(ps->vertices.size()));
    ps->vertices.emplace_back(L, R * std::cos(a), R * std::sin(a));
  }
  for (int i = 0; i < n; ++i) {
    const int j = (i + 1) % n;
    ps->indices.push_back({ring0[i], ring0[j], ring1[j], ring1[i]});
  }
  ps->indices.push_back(IndexedFace(ring0.rbegin(), ring0.rend()));  // -X cap (outward)
  ps->indices.push_back(IndexedFace(ring1.begin(), ring1.end()));    // +X cap
  return ps;
}

// A UV sphere built exactly like OpenSCAD's builtin (primitives.cc): num_rings
// latitude rings of num_fragments points, side quads, and a flat n-gon cap at
// each pole. num_rings = (num_fragments+1)/2.
std::unique_ptr<PolySet> uvSphere(int num_fragments, double r)
{
  auto ps = std::make_unique<PolySet>(3);
  const int num_rings = (num_fragments + 1) / 2;
  for (int i = 0; i < num_rings; ++i) {
    const double phi = (M_PI * (i + 0.5)) / num_rings;
    const double radius = r * std::sin(phi);
    const double z = r * std::cos(phi);
    for (int k = 0; k < num_fragments; ++k) {
      const double a = 2.0 * M_PI * k / num_fragments;
      ps->vertices.emplace_back(radius * std::cos(a), radius * std::sin(a), z);
    }
  }
  // Top cap (ring 0).
  {
    IndexedFace cap;
    for (int i = 0; i < num_fragments; ++i) cap.push_back(i);
    ps->indices.push_back(cap);
  }
  // Side quads.
  for (int i = 0; i < num_rings - 1; ++i) {
    for (int k = 0; k < num_fragments; ++k) {
      ps->indices.push_back({i * num_fragments + (k + 1) % num_fragments,
                             i * num_fragments + k,
                             (i + 1) * num_fragments + k,
                             (i + 1) * num_fragments + (k + 1) % num_fragments});
    }
  }
  // Bottom cap (last ring).
  {
    IndexedFace cap;
    for (int i = 0; i < num_fragments; ++i) cap.push_back(num_rings * num_fragments - i - 1);
    ps->indices.push_back(cap);
  }
  return ps;
}

double svgWidthMm(const std::string& path)
{
  std::ifstream in(path);
  std::stringstream ss;
  ss << in.rdbuf();
  const std::string s = ss.str();
  const auto p = s.find("width=\"");
  if (p == std::string::npos) return -1.0;
  return std::atof(s.c_str() + p + 7);
}

}  // namespace

// Largest angular gap (degrees) in the outline of one projected view: outline
// segments (midpoint far from the centre) are binned by midpoint angle; the max
// span of empty consecutive bins is the biggest break in the contour. A clean
// circle has gaps no larger than a couple of facets; a dashed one has big gaps.
double maxOutlineGapDeg(const std::vector<LineDrawingSeg2D>& segs)
{
  double cx = 0, cy = 0;
  int n = 0;
  for (const auto& s : segs) { cx += s.x0 + s.x1; cy += s.y0 + s.y1; n += 2; }
  if (n == 0) return 360.0;
  cx /= n; cy /= n;
  double maxR = 0;
  for (const auto& s : segs)
    maxR = std::max({maxR, std::hypot(s.x0 - cx, s.y0 - cy), std::hypot(s.x1 - cx, s.y1 - cy)});
  std::vector<char> bins(360, 0);
  for (const auto& s : segs) {
    const double mx = (s.x0 + s.x1) / 2 - cx, my = (s.y0 + s.y1) / 2 - cy;
    if (std::hypot(mx, my) < 0.9 * maxR) continue;  // interior edge, not the outline
    const int a = ((int)std::lround(std::atan2(my, mx) * 180 / M_PI) % 360 + 360) % 360;
    bins[a] = 1;
  }
  int best = 0, cur = 0;
  for (int i = 0; i < 720; ++i) {  // two turns to catch a gap spanning 0 deg
    if (bins[i % 360]) cur = 0; else if (++cur > best) best = cur;
  }
  return std::min(best, 360);
}

TEST_CASE("line drawing outline of a sphere is continuous (no HLR dashes)", "[linedrawing]")
{
  // A UV sphere's tessellated silhouette sits just inside the true rim, so the
  // sphere's own nearer front cap used to occlude its outline in the depth buffer
  // and it dropped out in dashes. The outline must stay continuous.
  for (int fn : {12, 30, 48}) {
    auto ps = uvSphere(fn, 10.0);
    const auto segs = projectViewEdges(*ps, lineDrawingViews().at(0), 20.0);
    REQUIRE(!segs.empty());
    CHECK(maxOutlineGapDeg(segs) < 20.0);
  }
}

TEST_CASE("line drawing keeps the cylinder's top/bottom silhouette", "[linedrawing]")
{
  const double R = 10.0, L = 20.0;
  auto ps = cylinderAlongX(32, R, L);

  // Front view: screen x = X, screen y = Z. The top and bottom silhouettes are
  // long horizontal edges at Z = +R and Z = -R (the regression this guards: they
  // must not drop out — see the grazing tolerance in FeatureEdges).
  const LineDrawingView& front = lineDrawingViews().at(0);
  const auto segs = projectViewEdges(*ps, front, 20.0);
  REQUIRE(!segs.empty());

  double maxY = segs.front().y0, minY = segs.front().y0;
  for (const auto& s : segs) {
    maxY = std::max({maxY, s.y0, s.y1});
    minY = std::min({minY, s.y0, s.y1});
  }
  CHECK(maxY == Catch::Approx(R).margin(0.5));
  CHECK(minY == Catch::Approx(-R).margin(0.5));

  // There must be an edge lying along the top (both ends near maxY) and one along
  // the bottom (both ends near minY), i.e. the silhouette contour is unbroken.
  auto hasHorizontalAt = [&](double y) {
    for (const auto& s : segs) {
      if (std::abs(s.y0 - y) < 0.5 && std::abs(s.y1 - y) < 0.5 &&
          std::abs(s.x1 - s.x0) > 1.0)
        return true;
    }
    return false;
  };
  CHECK(hasHorizontalAt(maxY));
  CHECK(hasHorizontalAt(minY));
}

TEST_CASE("line drawing export honours the drawing scale", "[linedrawing]")
{
  auto ps = cylinderAlongX(24, 10.0, 20.0);
  const std::string p1 = std::tmpnam(nullptr) + std::string(".svg");
  const std::string p2 = std::tmpnam(nullptr) + std::string(".svg");

  REQUIRE(exportLineDrawing(*ps, 20.0, p1, 1.0));
  REQUIRE(exportLineDrawing(*ps, 20.0, p2, 0.5));

  const double w1 = svgWidthMm(p1);
  const double w2 = svgWidthMm(p2);
  std::remove(p1.c_str());
  std::remove(p2.c_str());

  REQUIRE(w1 > 0.0);
  REQUIRE(w2 > 0.0);
  // Geometry is scaled but the fixed margins are not, so the half-scale sheet is
  // narrower but by less than 2x; it must sit strictly between (w1+margins)/2 and w1.
  CHECK(w2 < w1);
  CHECK(w2 > w1 * 0.4);
}
