#include "io/export_line_drawing.h"

#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "geometry/FeatureEdges.h"
#include "geometry/PolySet.h"
#include "geometry/linalg.h"

namespace {

// One orthographic view: which world axes map to screen x / y, and the view
// direction used for the silhouette test.
struct ViewDef {
  const char *name;
  int ax;  // world axis index -> screen x
  int ay;  // world axis index -> screen y (drawn upward)
  Vector3d dir;
};

struct ProjSeg {
  double x0, y0, x1, y1;
};

// Project the feature edges of one view to 2D (mm, y-down), shifted so the view
// starts at the origin; also report the view's width/height.
std::vector<ProjSeg> projectView(const PolySet& ps, const ViewDef& v, double creaseAngle,
                                 double& outW, double& outH)
{
  const auto segs = FeatureEdges::select(ps, v.dir, creaseAngle);
  double minx = std::numeric_limits<double>::max(), miny = minx;
  double maxx = std::numeric_limits<double>::lowest(), maxy = maxx;
  std::vector<ProjSeg> proj;
  proj.reserve(segs.size());
  for (const auto& s : segs) {
    // Screen x = world[ax]; screen y = -world[ay] so world "up" is up on paper.
    const double x0 = s.p0[v.ax], y0 = -s.p0[v.ay];
    const double x1 = s.p1[v.ax], y1 = -s.p1[v.ay];
    proj.push_back({x0, y0, x1, y1});
    minx = std::min({minx, x0, x1});
    miny = std::min({miny, y0, y1});
    maxx = std::max({maxx, x0, x1});
    maxy = std::max({maxy, y0, y1});
  }
  if (proj.empty()) { outW = outH = 0; return proj; }
  for (auto& p : proj) {  // shift to origin
    p.x0 -= minx; p.x1 -= minx;
    p.y0 -= miny; p.y1 -= miny;
  }
  outW = maxx - minx;
  outH = maxy - miny;
  return proj;
}

std::string fmt(double v)
{
  std::ostringstream s;
  s.precision(4);
  s << v;
  return s.str();
}

}  // namespace

bool exportLineDrawingSvg(const PolySet& ps, double creaseAngleDeg, const std::string& path)
{
  const double creaseAngle = creaseAngleDeg * M_PI / 180.0;
  const std::vector<ViewDef> views = {
    {"Front", 0, 2, {0, 1, 0}},   // XZ plane, looking along Y
    {"Side", 1, 2, {1, 0, 0}},    // YZ plane, looking along X
    {"Top", 0, 1, {0, 0, 1}},     // XY plane, looking along Z
  };
  // Grid cells like the drawing sheet: front top-left, side top-right, top below.
  const int cellCol[3] = {0, 1, 0};
  const int cellRow[3] = {0, 0, 1};

  std::vector<std::vector<ProjSeg>> proj(views.size());
  double cellW = 0, cellH = 0;
  std::vector<double> vw(views.size()), vh(views.size());
  for (size_t i = 0; i < views.size(); ++i) {
    proj[i] = projectView(ps, views[i], creaseAngle, vw[i], vh[i]);
    cellW = std::max(cellW, vw[i]);
    cellH = std::max(cellH, vh[i]);
  }
  if (cellW <= 0 || cellH <= 0) return false;

  const double margin = 10.0;   // mm around the sheet
  const double gap = 15.0;      // mm between views
  const double labelH = 6.0;    // mm reserved above each view for its title
  const double sheetW = 2 * margin + 2 * cellW + gap;
  const double sheetH = 2 * margin + 2 * (cellH + labelH) + gap;

  std::ofstream out(path);
  if (!out.is_open()) return false;

  out << "<?xml version=\"1.0\" standalone=\"no\"?>\n";
  out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << fmt(sheetW) << "mm\" height=\""
      << fmt(sheetH) << "mm\" viewBox=\"0 0 " << fmt(sheetW) << " " << fmt(sheetH) << "\">\n";
  out << "<rect x=\"0\" y=\"0\" width=\"" << fmt(sheetW) << "\" height=\"" << fmt(sheetH)
      << "\" fill=\"white\"/>\n";

  for (size_t i = 0; i < views.size(); ++i) {
    const double cellX = margin + cellCol[i] * (cellW + gap);
    const double cellY = margin + cellRow[i] * (cellH + labelH + gap);
    // Center the view within its cell (below the label row).
    const double ox = cellX + (cellW - vw[i]) / 2.0;
    const double oy = cellY + labelH + (cellH - vh[i]) / 2.0;
    out << "<text x=\"" << fmt(cellX) << "\" y=\"" << fmt(cellY + labelH - 1)
        << "\" font-family=\"sans-serif\" font-size=\"4\" fill=\"black\">" << views[i].name
        << "</text>\n";
    out << "<g stroke=\"black\" stroke-width=\"0.25\" stroke-linecap=\"round\">\n";
    for (const auto& s : proj[i]) {
      out << "<line x1=\"" << fmt(ox + s.x0) << "\" y1=\"" << fmt(oy + s.y0) << "\" x2=\""
          << fmt(ox + s.x1) << "\" y2=\"" << fmt(oy + s.y1) << "\"/>\n";
    }
    out << "</g>\n";
  }
  out << "</svg>\n";
  return out.good();
}
