#include "io/export_line_drawing.h"

#include <algorithm>
#include <cctype>
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

// One orthographic view: which world axes map to screen x / y, and which axis is
// depth. depthSign orients depth so that smaller = closer to the viewer.
struct ViewDef {
  const char *name;
  int ax;          // world axis -> screen x
  int ay;          // world axis -> screen y (drawn upward)
  int az;          // world axis -> depth
  double depthSign;
  Vector3d dir;    // view direction for the silhouette test
};

struct ProjSeg {
  double x0, y0, x1, y1;
};

// A CPU depth buffer of one view, used for hidden-line removal: the mesh is
// rasterised into it, then each feature edge is sampled against it.
class DepthBuffer
{
public:
  DepthBuffer(const PolySet& ps, const ViewDef& v)
  {
    // 2D bounds of the model in screen coords.
    double minx = std::numeric_limits<double>::max(), miny = minx;
    double maxx = std::numeric_limits<double>::lowest(), maxy = maxx;
    for (const auto& p : ps.vertices) {
      minx = std::min(minx, p[v.ax]); maxx = std::max(maxx, p[v.ax]);
      miny = std::min(miny, p[v.ay]); maxy = std::max(maxy, p[v.ay]);
    }
    if (maxx <= minx || maxy <= miny) { valid_ = false; return; }
    minx_ = minx; miny_ = miny;
    const double extent = std::max(maxx - minx, maxy - miny);
    scale_ = kTargetPx / extent;  // pixels per mm
    W_ = static_cast<int>(std::ceil((maxx - minx) * scale_)) + 1;
    H_ = static_cast<int>(std::ceil((maxy - miny) * scale_)) + 1;
    buf_.assign(static_cast<size_t>(W_) * H_, std::numeric_limits<double>::max());

    // Rasterise every face (fan-triangulated) into the depth buffer.
    for (const auto& face : ps.indices) {
      for (size_t i = 1; i + 1 < face.size(); ++i) {
        const int ia = face[0], ib = face[i], ic = face[i + 1];
        if (ia < 0 || ib < 0 || ic < 0) continue;
        if (ia >= (int)ps.vertices.size() || ib >= (int)ps.vertices.size() ||
            ic >= (int)ps.vertices.size())
          continue;
        rasterTri(ps.vertices[ia], ps.vertices[ib], ps.vertices[ic], v);
      }
    }
    depthEps_ = extent * 5e-3;
    valid_ = true;
  }

  bool valid() const { return valid_; }

  // Is the world point visible (at or in front of the nearest rasterised face)?
  bool visible(const Vector3d& p, const ViewDef& v) const
  {
    int px, py;
    if (!toPixel(p[v.ax], p[v.ay], px, py)) return true;  // outside buffer: keep
    const double d = v.depthSign * p[v.az];
    const double buf = buf_[static_cast<size_t>(py) * W_ + px];
    if (buf == std::numeric_limits<double>::max()) return true;  // nothing there
    return d <= buf + depthEps_;
  }

private:
  static constexpr double kTargetPx = 900.0;

  bool toPixel(double sx, double sy, int& px, int& py) const
  {
    px = static_cast<int>((sx - minx_) * scale_ + 0.5);
    py = static_cast<int>((sy - miny_) * scale_ + 0.5);
    if (px < 0 || px >= W_ || py < 0 || py >= H_) return false;
    return true;
  }

  void rasterTri(const Vector3d& a, const Vector3d& b, const Vector3d& c, const ViewDef& v)
  {
    const double ax = (a[v.ax] - minx_) * scale_, ay = (a[v.ay] - miny_) * scale_;
    const double bx = (b[v.ax] - minx_) * scale_, by = (b[v.ay] - miny_) * scale_;
    const double cx = (c[v.ax] - minx_) * scale_, cy = (c[v.ay] - miny_) * scale_;
    const double da = v.depthSign * a[v.az], db = v.depthSign * b[v.az], dc = v.depthSign * c[v.az];
    const double denom = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy);
    if (std::abs(denom) < 1e-9) return;
    int x0 = std::max(0, (int)std::floor(std::min({ax, bx, cx})));
    int x1 = std::min(W_ - 1, (int)std::ceil(std::max({ax, bx, cx})));
    int y0 = std::max(0, (int)std::floor(std::min({ay, by, cy})));
    int y1 = std::min(H_ - 1, (int)std::ceil(std::max({ay, by, cy})));
    for (int py = y0; py <= y1; ++py) {
      for (int px = x0; px <= x1; ++px) {
        const double l1 = ((by - cy) * (px - cx) + (cx - bx) * (py - cy)) / denom;
        const double l2 = ((cy - ay) * (px - cx) + (ax - cx) * (py - cy)) / denom;
        const double l3 = 1.0 - l1 - l2;
        if (l1 < -1e-6 || l2 < -1e-6 || l3 < -1e-6) continue;
        const double d = l1 * da + l2 * db + l3 * dc;
        double& slot = buf_[static_cast<size_t>(py) * W_ + px];
        if (d < slot) slot = d;
      }
    }
  }

  bool valid_{false};
  int W_{0}, H_{0};
  double minx_{0}, miny_{0}, scale_{1};
  double depthEps_{0};
  std::vector<double> buf_;
};

// Project one view's feature edges to 2D (mm, y-up), removing the parts hidden
// behind the mesh, then shift to the origin and report width/height.
std::vector<ProjSeg> projectView(const PolySet& ps, const ViewDef& v, double creaseAngle,
                                 double& outW, double& outH)
{
  const auto segs = FeatureEdges::select(ps, v.dir, creaseAngle);
  const DepthBuffer depth(ps, v);

  double minx = std::numeric_limits<double>::max(), miny = minx;
  double maxx = std::numeric_limits<double>::lowest(), maxy = maxx;
  std::vector<ProjSeg> proj;
  auto emit = [&](const Vector3d& p0, const Vector3d& p1) {
    const double x0 = p0[v.ax], y0 = p0[v.ay];
    const double x1 = p1[v.ax], y1 = p1[v.ay];
    proj.push_back({x0, y0, x1, y1});
    minx = std::min({minx, x0, x1}); miny = std::min({miny, y0, y1});
    maxx = std::max({maxx, x0, x1}); maxy = std::max({maxy, y0, y1});
  };

  for (const auto& s : segs) {
    if (!depth.valid()) { emit(s.p0, s.p1); continue; }
    // Sample the edge and keep the contiguous visible runs (hidden-line removal).
    const double lenPx = (Vector3d(s.p1[v.ax] - s.p0[v.ax], s.p1[v.ay] - s.p0[v.ay], 0)).norm();
    const int n = std::clamp(static_cast<int>(lenPx * 2.0), 8, 400);
    bool runOpen = false;
    Vector3d runStart;
    Vector3d prev;
    for (int i = 0; i <= n; ++i) {
      const double t = static_cast<double>(i) / n;
      const Vector3d p = s.p0 + t * (s.p1 - s.p0);
      const bool vis = depth.visible(p, v);
      if (vis && !runOpen) { runOpen = true; runStart = p; }
      else if (!vis && runOpen) { emit(runStart, prev); runOpen = false; }
      prev = p;
    }
    if (runOpen) emit(runStart, prev);
  }

  if (proj.empty()) { outW = outH = 0; return proj; }
  for (auto& p : proj) { p.x0 -= minx; p.x1 -= minx; p.y0 -= miny; p.y1 -= miny; }
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

// The three ortho views projected to y-up mm, plus the sheet layout. Views are
// laid out like a drawing sheet: front top-left, side top-right, top below front.
struct Sheet {
  std::vector<std::vector<ProjSeg>> proj;  // per view, y-up, origin at (0,0)
  std::vector<double> vw, vh;              // per-view size
  std::vector<double> ox, oy;              // per-view origin on the sheet (y-up)
  std::vector<const char *> names;
  double W{0}, H{0};                       // sheet size
  double labelH{6.0};
};

bool buildSheet(const PolySet& ps, double creaseAngleDeg, Sheet& sh)
{
  const double creaseAngle = creaseAngleDeg * M_PI / 180.0;
  const std::vector<ViewDef> views = {
    {"Front", 0, 2, 1, +1.0, {0, 1, 0}},   // XZ plane, depth +Y (viewer at -Y)
    {"Side", 1, 2, 0, -1.0, {1, 0, 0}},     // YZ plane, depth -X (viewer at +X)
    {"Top", 0, 1, 2, -1.0, {0, 0, 1}},      // XY plane, depth -Z (viewer at +Z)
  };
  const int cellCol[3] = {0, 1, 0};
  const int cellRow[3] = {0, 0, 1};  // row 0 = upper, row 1 = lower

  sh.proj.resize(views.size());
  sh.vw.resize(views.size());
  sh.vh.resize(views.size());
  sh.ox.resize(views.size());
  sh.oy.resize(views.size());
  double cellW = 0, cellH = 0;
  for (size_t i = 0; i < views.size(); ++i) {
    sh.proj[i] = projectView(ps, views[i], creaseAngle, sh.vw[i], sh.vh[i]);
    sh.names.push_back(views[i].name);
    cellW = std::max(cellW, sh.vw[i]);
    cellH = std::max(cellH, sh.vh[i]);
  }
  if (cellW <= 0 || cellH <= 0) return false;

  const double margin = 10.0, gap = 15.0;
  sh.W = 2 * margin + 2 * cellW + gap;
  sh.H = 2 * margin + 2 * (cellH + sh.labelH) + gap;
  for (size_t i = 0; i < views.size(); ++i) {
    const double cellX = margin + cellCol[i] * (cellW + gap);
    // Upper row (cellRow 0) sits higher on the sheet (larger y, since y is up).
    const double cellBottom = margin + (1 - cellRow[i]) * (cellH + sh.labelH + gap);
    sh.ox[i] = cellX + (cellW - sh.vw[i]) / 2.0;
    sh.oy[i] = cellBottom + (cellH - sh.vh[i]) / 2.0;
  }
  return true;
}

bool writeSvg(const Sheet& sh, const std::string& path)
{
  std::ofstream out(path);
  if (!out.is_open()) return false;
  // SVG y is down: flip each y-up sheet coordinate to (H - y).
  auto Y = [&](double y) { return sh.H - y; };
  out << "<?xml version=\"1.0\" standalone=\"no\"?>\n";
  out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << fmt(sh.W) << "mm\" height=\""
      << fmt(sh.H) << "mm\" viewBox=\"0 0 " << fmt(sh.W) << " " << fmt(sh.H) << "\">\n";
  out << "<rect x=\"0\" y=\"0\" width=\"" << fmt(sh.W) << "\" height=\"" << fmt(sh.H)
      << "\" fill=\"white\"/>\n";
  for (size_t i = 0; i < sh.proj.size(); ++i) {
    out << "<text x=\"" << fmt(sh.ox[i]) << "\" y=\"" << fmt(Y(sh.oy[i] + sh.vh[i]) - 2)
        << "\" font-family=\"sans-serif\" font-size=\"4\" fill=\"black\">" << sh.names[i]
        << "</text>\n";
    out << "<g stroke=\"black\" stroke-width=\"0.25\" stroke-linecap=\"round\">\n";
    for (const auto& s : sh.proj[i]) {
      out << "<line x1=\"" << fmt(sh.ox[i] + s.x0) << "\" y1=\"" << fmt(Y(sh.oy[i] + s.y0))
          << "\" x2=\"" << fmt(sh.ox[i] + s.x1) << "\" y2=\"" << fmt(Y(sh.oy[i] + s.y1))
          << "\"/>\n";
    }
    out << "</g>\n";
  }
  out << "</svg>\n";
  return out.good();
}

bool writeDxf(const Sheet& sh, const std::string& path)
{
  std::ofstream out(path);
  if (!out.is_open()) return false;
  // DXF uses y-up, so the sheet coordinates go out as-is. One LINE per segment.
  out << "0\nSECTION\n2\nENTITIES\n";
  for (size_t i = 0; i < sh.proj.size(); ++i) {
    for (const auto& s : sh.proj[i]) {
      out << "0\nLINE\n8\n0\n"
          << "10\n" << fmt(sh.ox[i] + s.x0) << "\n20\n" << fmt(sh.oy[i] + s.y0) << "\n30\n0\n"
          << "11\n" << fmt(sh.ox[i] + s.x1) << "\n21\n" << fmt(sh.oy[i] + s.y1) << "\n31\n0\n";
    }
    // View label as a TEXT entity above the view.
    out << "0\nTEXT\n8\n0\n10\n" << fmt(sh.ox[i]) << "\n20\n" << fmt(sh.oy[i] + sh.vh[i] + 2)
        << "\n30\n0\n40\n4\n1\n" << sh.names[i] << "\n";
  }
  out << "0\nENDSEC\n0\nEOF\n";
  return out.good();
}

bool endsWithNoCase(const std::string& s, const std::string& suffix)
{
  if (s.size() < suffix.size()) return false;
  for (size_t i = 0; i < suffix.size(); ++i) {
    if (std::tolower(s[s.size() - suffix.size() + i]) != suffix[i]) return false;
  }
  return true;
}

}  // namespace

bool exportLineDrawing(const PolySet& ps, double creaseAngleDeg, const std::string& path)
{
  Sheet sh;
  if (!buildSheet(ps, creaseAngleDeg, sh)) return false;
  return endsWithNoCase(path, ".dxf") ? writeDxf(sh, path) : writeSvg(sh, path);
}
