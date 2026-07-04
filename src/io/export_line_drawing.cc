#include "io/export_line_drawing.h"

#include <algorithm>
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

// Project one view's feature edges to 2D (mm, y-down), removing the parts hidden
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
    const double x0 = p0[v.ax], y0 = -p0[v.ay];
    const double x1 = p1[v.ax], y1 = -p1[v.ay];
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

}  // namespace

bool exportLineDrawingSvg(const PolySet& ps, double creaseAngleDeg, const std::string& path)
{
  const double creaseAngle = creaseAngleDeg * M_PI / 180.0;
  const std::vector<ViewDef> views = {
    {"Front", 0, 2, 1, +1.0, {0, 1, 0}},   // XZ plane, depth +Y (viewer at -Y)
    {"Side", 1, 2, 0, -1.0, {1, 0, 0}},     // YZ plane, depth -X (viewer at +X)
    {"Top", 0, 1, 2, -1.0, {0, 0, 1}},      // XY plane, depth -Z (viewer at +Z)
  };
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

  const double margin = 10.0, gap = 15.0, labelH = 6.0;  // mm
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
