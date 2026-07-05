#pragma once

#include <string>
#include <vector>

#include "geometry/linalg.h"

class PolySet;

/*!
   Vector line drawing of a mesh: the three orthographic views (front / side /
   top) drawn from the mesh's silhouette + crease + boundary edges with hidden
   lines removed (see FeatureEdges and the CPU depth buffer in
   export_line_drawing.cc).

   The per-view projector below is the shared source of truth for both the file
   export and the WYSIWYG drawing dock, so the dock shows exactly what exports.
 */

// One orthographic view: which world axes map to the screen right / up
// directions, which axis is depth (depthSign orients it so smaller = closer to
// the viewer), and the view direction used for the silhouette test.
struct LineDrawingView {
  const char *name;
  int ax;          // world axis -> screen right (x)
  int ay;          // world axis -> screen up (y)
  int az;          // world axis -> depth
  double depthSign;
  Vector3d dir;    // view direction for the silhouette test
};

// The three standard views (Front, Side, Top). Same convention as the GL ortho
// views and MainWindow::measureAxes so dimension projection stays consistent.
const std::vector<LineDrawingView>& lineDrawingViews();

// A projected 2D segment in a view's screen plane: coordinates are the raw
// world-plane values sx = world[ax], sy = world[ay] (millimetres, y-up), NOT
// shifted to an origin. Hidden parts have been removed.
struct LineDrawingSeg2D {
  double x0, y0, x1, y1;
};

// Project the feature edges of `ps` for one view to 2D and remove the hidden
// parts (CPU depth buffer). Coordinates are world-plane mm, y-up (see above).
std::vector<LineDrawingSeg2D> projectViewEdges(const PolySet& ps, const LineDrawingView& v,
                                               double creaseAngleDeg);

/*!
   Write a compact vector line drawing of `ps` to `path`: the three orthographic
   views laid out like a technical sheet. `creaseAngleDeg` is the dihedral
   threshold above which an edge counts as a feature edge. `scale` multiplies the
   model geometry (drawing-mm = model-mm * scale); text and line weights stay at
   an absolute size. The format is chosen from the extension: ".dxf" writes DXF
   LINE entities, anything else writes SVG. Returns false on write failure.
 */
bool exportLineDrawing(const PolySet& ps, double creaseAngleDeg, const std::string& path,
                       double scale = 1.0);
