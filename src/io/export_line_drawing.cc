#include "io/export_line_drawing.h"

#include <algorithm>
#include <array>
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

#ifdef ENABLE_MANIFOLD
#include "geometry/manifold/ManifoldGeometry.h"
#include "geometry/manifold/manifoldutils.h"
#include "manifold/manifold.h"
#endif

namespace {

// The view definition is shared with the drawing dock (see export_line_drawing.h).
using ViewDef = LineDrawingView;

// Screen coordinates of a world point in a view (signed, so mirrored views read
// correctly). Depth keeps its own sign (smaller = closer to the viewer).
inline double screenX(const ViewDef& v, const Vector3d& p) { return v.sx * p[v.ax]; }
inline double screenY(const ViewDef& v, const Vector3d& p) { return v.sy * p[v.ay]; }

// A sheet-space segment (origin-shifted and scaled), millimetres, y-up.
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
      const double sx = screenX(v, p), sy = screenY(v, p);
      minx = std::min(minx, sx); maxx = std::max(maxx, sx);
      miny = std::min(miny, sy); maxy = std::max(maxy, sy);
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
    if (!toPixel(screenX(v, p), screenY(v, p), px, py)) return true;  // outside buffer: keep
    const double d = v.depthSign * p[v.az];
    const double buf = buf_[static_cast<size_t>(py) * W_ + px];
    if (buf == std::numeric_limits<double>::max()) return true;  // nothing there
    return d <= buf + depthEps_;
  }

  // Is there empty space (background, or off the model) within a few pixels of the
  // screen point along the unit screen direction (ux, uy)? Used to recognise an
  // outline edge: the tessellated silhouette of a curved object sits a fraction
  // inside the true rim, so the object's own nearer front surface occludes it in
  // the depth test and it drops out in dashes. But an edge with empty space just
  // to one side is on the object's boundary against nothing — it is genuinely
  // visible and must be drawn regardless of the depth test. Interior hidden edges
  // have covered pixels on both sides and are unaffected.
  bool backgroundToward(double sx, double sy, double ux, double uy) const
  {
    for (int k = 1; k <= 3; ++k) {
      int px, py;
      if (!toPixel(sx + ux * k / scale_, sy + uy * k / scale_, px, py)) return true;
      if (buf_[static_cast<size_t>(py) * W_ + px] == std::numeric_limits<double>::max())
        return true;
    }
    return false;
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
    const double ax = (screenX(v, a) - minx_) * scale_, ay = (screenY(v, a) - miny_) * scale_;
    const double bx = (screenX(v, b) - minx_) * scale_, by = (screenY(v, b) - miny_) * scale_;
    const double cx = (screenX(v, c) - minx_) * scale_, cy = (screenY(v, c) - miny_) * scale_;
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

// Project one view's feature edges to sheet space: origin-shifted and multiplied
// by `scale`; report the scaled width/height. Built on the shared raw projector.
std::vector<ProjSeg> projectViewShifted(const PolySet& ps, const ViewDef& v, double creaseAngleDeg,
                                        double scale, double& outW, double& outH)
{
  const auto raw = projectViewEdges(ps, v, creaseAngleDeg);
  double minx = std::numeric_limits<double>::max(), miny = minx;
  double maxx = std::numeric_limits<double>::lowest(), maxy = maxx;
  for (const auto& r : raw) {
    minx = std::min({minx, r.x0, r.x1}); miny = std::min({miny, r.y0, r.y1});
    maxx = std::max({maxx, r.x0, r.x1}); maxy = std::max({maxy, r.y0, r.y1});
  }
  std::vector<ProjSeg> proj;
  if (raw.empty()) { outW = outH = 0; return proj; }
  proj.reserve(raw.size());
  for (const auto& r : raw) {
    proj.push_back({(r.x0 - minx) * scale, (r.y0 - miny) * scale,
                    (r.x1 - minx) * scale, (r.y1 - miny) * scale});
  }
  outW = (maxx - minx) * scale;
  outH = (maxy - miny) * scale;
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

bool buildSheet(const PolySet& ps, double creaseAngleDeg, double scale, Sheet& sh)
{
  const std::vector<LineDrawingView>& views = lineDrawingViews();
  const int cellCol[3] = {0, 1, 0};
  const int cellRow[3] = {0, 0, 1};  // row 0 = upper, row 1 = lower

  sh.proj.resize(views.size());
  sh.vw.resize(views.size());
  sh.vh.resize(views.size());
  sh.ox.resize(views.size());
  sh.oy.resize(views.size());
  double cellW = 0, cellH = 0;
  for (size_t i = 0; i < views.size(); ++i) {
    sh.proj[i] = projectViewShifted(ps, views[i], creaseAngleDeg, scale, sh.vw[i], sh.vh[i]);
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

const std::vector<LineDrawingView>& lineDrawingViews()
{
  static const std::vector<LineDrawingView> views = {
    {"Front", 0, 2, 1, +1, +1, +1.0, {0, 1, 0}},  // XZ plane, depth +Y (viewer at -Y)
    {"Side", 1, 2, 0, +1, +1, -1.0, {1, 0, 0}},   // YZ plane, depth -X (viewer at +X)
    {"Top", 0, 1, 2, +1, +1, -1.0, {0, 0, 1}},    // XY plane, depth -Z (viewer at +Z)
  };
  return views;
}

const std::vector<LineDrawingView>& standardViews()
{
  // Screen frame per view derived from screen_right = cross(viewDir, up), up as
  // noted. Front/Right/Top match lineDrawingViews(); Back/Left/Bottom mirror the
  // marked screen axis so a multi-view sheet is laterally consistent.
  static const std::vector<LineDrawingView> views = {
    {"Front", 0, 2, 1, +1, +1, +1.0, {0, 1, 0}},   // look +Y, right +X, up +Z
    {"Back", 0, 2, 1, -1, +1, -1.0, {0, 1, 0}},    // look -Y, right -X (mirror)
    {"Left", 1, 2, 0, -1, +1, +1.0, {1, 0, 0}},    // look +X, right -Y (mirror)
    {"Right", 1, 2, 0, +1, +1, -1.0, {1, 0, 0}},   // look -X, right +Y
    {"Top", 0, 1, 2, +1, +1, -1.0, {0, 0, 1}},     // look -Z, right +X, up +Y
    {"Bottom", 0, 1, 2, +1, -1, +1.0, {0, 0, 1}},  // look +Z, up -Y (mirror)
  };
  return views;
}

std::vector<LineDrawingSeg2D> projectViewEdges(const PolySet& ps, const LineDrawingView& v,
                                               double creaseAngleDeg)
{
  const double creaseAngle = creaseAngleDeg * M_PI / 180.0;
  const auto segs = FeatureEdges::select(ps, v.dir, creaseAngle);
  const DepthBuffer depth(ps, v);

  std::vector<LineDrawingSeg2D> proj;
  auto emit = [&](const Vector3d& p0, const Vector3d& p1) {
    proj.push_back({screenX(v, p0), screenY(v, p0), screenX(v, p1), screenY(v, p1)});
  };
  for (const auto& s : segs) {
    if (!depth.valid()) { emit(s.p0, s.p1); continue; }
    // Sample the edge and keep the contiguous visible runs (hidden-line removal).
    const double dx = screenX(v, s.p1) - screenX(v, s.p0), dy = screenY(v, s.p1) - screenY(v, s.p0);
    const double lenPx = std::hypot(dx, dy);
    // Unit screen-space perpendicular of the edge, for the outline (background)
    // test that rescues silhouette edges from self-occlusion.
    const double perpx = lenPx > 1e-9 ? -dy / lenPx : 0.0;
    const double perpy = lenPx > 1e-9 ? dx / lenPx : 0.0;
    const int n = std::clamp(static_cast<int>(lenPx * 2.0), 8, 400);
    bool runOpen = false;
    Vector3d runStart;
    Vector3d prev;
    for (int i = 0; i <= n; ++i) {
      const double t = static_cast<double>(i) / n;
      const Vector3d p = s.p0 + t * (s.p1 - s.p0);
      bool vis = depth.visible(p, v);
      if (!vis && (depth.backgroundToward(screenX(v, p), screenY(v, p), perpx, perpy) ||
                   depth.backgroundToward(screenX(v, p), screenY(v, p), -perpx, -perpy))) {
        vis = true;  // edge lies on the object's outline against empty space
      }
      if (vis && !runOpen) { runOpen = true; runStart = p; }
      else if (!vis && runOpen) { emit(runStart, prev); runOpen = false; }
      prev = p;
    }
    if (runOpen) emit(runStart, prev);
  }
  return proj;
}

std::vector<LineDrawingFace2D> projectViewFaces(const PolySet& ps, const LineDrawingView& v)
{
  std::vector<LineDrawingFace2D> out;
  if (ps.color_indices.empty() || ps.colors.empty()) return out;

  // Build one filled polygon per coloured face, with a painter's-algorithm depth
  // key (average depth; smaller = nearer, so we sort descending = far first).
  struct FaceRec { LineDrawingFace2D face; double depth; };
  std::vector<FaceRec> recs;
  recs.reserve(ps.indices.size());
  for (size_t fi = 0; fi < ps.indices.size(); ++fi) {
    const int ci = fi < ps.color_indices.size() ? ps.color_indices[fi] : -1;
    if (ci < 0 || ci >= static_cast<int>(ps.colors.size())) continue;  // uncoloured: blank
    const auto& face = ps.indices[fi];
    if (face.size() < 3) continue;
    FaceRec rec;
    const Color4f& col = ps.colors[ci];
    rec.face.r = col.r(); rec.face.g = col.g(); rec.face.b = col.b(); rec.face.a = col.a();
    double depthSum = 0.0;
    int n = 0;
    for (const int idx : face) {
      if (idx < 0 || idx >= static_cast<int>(ps.vertices.size())) { n = 0; break; }
      const Vector3d& p = ps.vertices[idx];
      rec.face.pts.emplace_back(screenX(v, p), screenY(v, p));
      depthSum += v.depthSign * p[v.az];
      ++n;
    }
    if (n < 3) continue;
    rec.depth = depthSum / n;
    recs.push_back(std::move(rec));
  }
  std::sort(recs.begin(), recs.end(),
            [](const FaceRec& a, const FaceRec& b) { return a.depth > b.depth; });  // far first
  out.reserve(recs.size());
  for (auto& r : recs) out.push_back(std::move(r.face));
  return out;
}

namespace {
// Clip the 45° line {x - y = c} to triangle (a,b,c2) and, if it crosses, append
// the interior segment to `out`.
void hatchTriangle(const std::array<double, 2>& a, const std::array<double, 2>& b,
                   const std::array<double, 2>& c2, double cval, std::vector<LineDrawingSeg2D>& out)
{
  const std::array<std::array<double, 2>, 3> tri = {a, b, c2};
  std::array<double, 2> hit[3];
  int nh = 0;
  for (int e = 0; e < 3 && nh < 3; ++e) {
    const auto& p = tri[e];
    const auto& q = tri[(e + 1) % 3];
    const double kp = p[0] - p[1], kq = q[0] - q[1];
    const double denom = kq - kp;
    if (std::abs(denom) < 1e-12) continue;  // edge parallel to the hatch line
    const double t = (cval - kp) / denom;
    if (t < -1e-9 || t > 1.0 + 1e-9) continue;
    hit[nh++] = {p[0] + t * (q[0] - p[0]), p[1] + t * (q[1] - p[1])};
  }
  if (nh < 2) return;
  // Take the two most separated hits (guards against a vertex-grazing third hit).
  int i0 = 0, i1 = 1;
  double best = -1;
  for (int i = 0; i < nh; ++i)
    for (int j = i + 1; j < nh; ++j) {
      const double d = std::hypot(hit[i][0] - hit[j][0], hit[i][1] - hit[j][1]);
      if (d > best) { best = d; i0 = i; i1 = j; }
    }
  if (best < 1e-9) return;
  out.push_back({hit[i0][0], hit[i0][1], hit[i1][0], hit[i1][1]});
}
}  // namespace

namespace {
// Clip a polygon (local frame) to the kept half-space and, if a triangle or more
// survives, append it to `out` as a projected coloured fill (screen x=X, y=Y),
// with a painter's depth key = average local z. keep(z) selects the half; the
// section always projects along +Z, so nearer = smaller z (far-first = z desc).
struct SectionFaceRec { LineDrawingFace2D face; double depth; };
void clipColorFace(const std::vector<Vector3d>& poly, float r, float g, float b, float a,
                   bool flip, std::vector<SectionFaceRec>& out)
{
  const double s = flip ? -1.0 : 1.0;          // keep side: s*z >= 0
  std::vector<Vector3d> clipped;
  const size_t n = poly.size();
  for (size_t i = 0; i < n; ++i) {
    const Vector3d& A = poly[i];
    const Vector3d& B = poly[(i + 1) % n];
    const double da = s * A.z(), db = s * B.z();
    if (da >= -1e-9) clipped.push_back(A);
    if ((da > 1e-9 && db < -1e-9) || (da < -1e-9 && db > 1e-9)) {
      const double t = da / (da - db);         // crossing point on z=0
      clipped.push_back(A + t * (B - A));
    }
  }
  if (clipped.size() < 3) return;
  SectionFaceRec rec;
  rec.face.r = r; rec.face.g = g; rec.face.b = b; rec.face.a = a;
  double zsum = 0.0;
  for (const auto& p : clipped) { rec.face.pts.emplace_back(p.x(), p.y()); zsum += p.z(); }
  rec.depth = zsum / clipped.size();
  out.push_back(std::move(rec));
}
}  // namespace

SectionDrawing projectSection(const PolySet& ps, const Transform3d& worldFrame, bool flip,
                              double creaseAngleDeg, double hatchSpacingMm, bool wantColorFills)
{
  SectionDrawing out;
#ifdef ENABLE_MANIFOLD
  // Move the mesh into the section frame: cut plane -> z=0, view along +Z.
  const Transform3d L = worldFrame.inverse();
  PolySet local(3);
  local.vertices.reserve(ps.vertices.size());
  for (const auto& v : ps.vertices) local.vertices.push_back(L * v);
  local.indices = ps.indices;

  auto mg = ManifoldUtils::createManifoldFromPolySet(local);
  if (!mg) return out;
  // Keep the half the +Z view looks onto (z >= 0); flip keeps the near half.
  // TrimByPlane(normal) keeps the half on the negative-normal side, so +Z keeps z>=0.
  const double s = flip ? -1.0 : 1.0;
  manifold::Manifold trimmed = mg->getManifold().TrimByPlane(manifold::vec3(0, 0, s), 0.0);
  if (trimmed.IsEmpty()) return out;
  auto cutPs = ManifoldGeometry(trimmed).toPolySet();
  if (!cutPs || cutPs->vertices.empty()) return out;

  // Project like a +Z view (screen x=X, y=Y, depth +Z: smaller z nearer).
  const LineDrawingView view{"Section", 0, 1, 2, +1, +1, +1.0, {0, 0, 1}};
  out.edges = projectViewEdges(*cutPs, view, creaseAngleDeg);

  // Hatch the cut face (faces lying on z=0). Spacing auto = cut size / 18.
  double minx = 1e30, maxx = -1e30, miny = 1e30, maxy = -1e30;
  for (const auto& e : out.edges) {
    minx = std::min({minx, e.x0, e.x1}); maxx = std::max({maxx, e.x0, e.x1});
    miny = std::min({miny, e.y0, e.y1}); maxy = std::max({maxy, e.y0, e.y1});
  }
  const double diag = std::hypot(maxx - minx, maxy - miny);
  const double spacing = hatchSpacingMm > 0 ? hatchSpacingMm : std::max(diag / 18.0, 1e-3);
  const double dc = spacing * std::sqrt(2.0);  // Δ(x−y) between adjacent 45° lines
  const double zEps = std::max(1e-4, diag * 1e-4);
  for (const auto& face : cutPs->indices) {
    for (size_t i = 1; i + 1 < face.size(); ++i) {
      const int ia = face[0], ib = face[i], ic = face[i + 1];
      if (ia < 0 || ib < 0 || ic < 0) continue;
      const Vector3d& A = cutPs->vertices[ia];
      const Vector3d& B = cutPs->vertices[ib];
      const Vector3d& C = cutPs->vertices[ic];
      if (std::abs(A.z()) > zEps || std::abs(B.z()) > zEps || std::abs(C.z()) > zEps) continue;
      const std::array<double, 2> a{A.x(), A.y()}, b{B.x(), B.y()}, c{C.x(), C.y()};
      double kmin = std::min({a[0] - a[1], b[0] - b[1], c[0] - c[1]});
      double kmax = std::max({a[0] - a[1], b[0] - b[1], c[0] - c[1]});
      for (double cval = std::ceil(kmin / dc) * dc; cval <= kmax; cval += dc)
        hatchTriangle(a, b, c, cval, out.hatch);
    }
  }
  // Coloured fills of the kept solid: clip each coloured original face to the kept
  // half-space (in the section frame) and project along +Z. Independent of the
  // manifold trim (which loses per-face colour), so it always shows the colours.
  if (wantColorFills && !ps.color_indices.empty() && !ps.colors.empty()) {
    std::vector<SectionFaceRec> recs;
    for (size_t fi = 0; fi < local.indices.size(); ++fi) {
      const int ci = fi < ps.color_indices.size() ? ps.color_indices[fi] : -1;
      if (ci < 0 || ci >= static_cast<int>(ps.colors.size())) continue;
      const auto& face = local.indices[fi];
      if (face.size() < 3) continue;
      std::vector<Vector3d> poly;
      poly.reserve(face.size());
      bool ok = true;
      for (const int idx : face) {
        if (idx < 0 || idx >= static_cast<int>(local.vertices.size())) { ok = false; break; }
        poly.push_back(local.vertices[idx]);
      }
      if (!ok) continue;
      const Color4f& col = ps.colors[ci];
      clipColorFace(poly, col.r(), col.g(), col.b(), col.a(), flip, recs);
    }
    std::sort(recs.begin(), recs.end(),
              [](const SectionFaceRec& a, const SectionFaceRec& b) { return a.depth > b.depth; });
    out.fills.reserve(recs.size());
    for (auto& r : recs) out.fills.push_back(std::move(r.face));
  }

  out.valid = !out.edges.empty();
#else
  (void)ps; (void)worldFrame; (void)flip; (void)creaseAngleDeg; (void)hatchSpacingMm;
  (void)wantColorFills;
#endif
  return out;
}

bool exportLineDrawing(const PolySet& ps, double creaseAngleDeg, const std::string& path,
                       double scale)
{
  Sheet sh;
  if (!buildSheet(ps, creaseAngleDeg, scale, sh)) return false;
  return endsWithNoCase(path, ".dxf") ? writeDxf(sh, path) : writeSvg(sh, path);
}
