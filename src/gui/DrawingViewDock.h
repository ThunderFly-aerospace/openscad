#pragma once

#include <QColor>
#include <QFont>
#include <QHash>
#include <QImage>
#include <QLineF>
#include <QList>
#include <QPixmap>
#include <QPointF>
#include <QPolygonF>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include "gui/Dock.h"

class QLabel;
class QPushButton;
class QCheckBox;
class QDoubleSpinBox;
class QResizeEvent;
class QMouseEvent;
class QTimer;
class QPaintEvent;

// A dimref anchor projected into one ortho view, in render-image pixel coords.
// `name` is shown next to the dot; `ref` is the token emitted when it is
// clicked to build a dimension — the plain name when unique, or name#index
// (index = occurrence among same-named dimrefs in tree order) when duplicated,
// so a dimension pins the exact instance clicked.
struct OrthoAnchor {
  QPointF pos;
  QString name;
  QString ref;
  int sourceLine{-1};  // 0-based editor line of the dimref() (for jump-to-source)
};

// A dimension (kvóta) to draw in one ortho view: two anchor endpoints
// (render-image pixel coords), a text label, a perpendicular offset in image
// pixels (the dimension line is pulled this far off the anchor line), and an id
// used to write the offset back to source (source line index; -1 = not editable,
// e.g. kvota= sugar dimensions).
struct OrthoDimension {
  QPointF a;  // true projected anchor points (feature points)
  QPointF b;
  // Unit measurement direction in image space. The dimension line runs along
  // this direction; the drawn endpoints are the feet of a and b projected onto
  // it. For direct/planar it is (b-a) normalized (so the line is aligned with
  // the anchors); for h/v it is the view's horizontal/vertical axis, giving an
  // axis-aligned dimension with witness lines dropped from the anchors.
  QPointF dir{1.0, 0.0};
  QString label;
  double offsetPx{0.0};
  bool arrowsOut{false};  // arrowheads outside the extension lines
  // Label position offset from the dimension-line midpoint, in image px, in the
  // dimension's own frame (along the line, and perpendicular to it).
  double labAlongPx{0.0};
  double labPerpPx{0.0};
  // Draw a leader line to the dimension line when the label is dragged away.
  // Per-dimension (from leader=on|off in source); default on.
  bool leader{true};
  // Radial (diameter / radius) dimension: drawn straight through the feature
  // centre at an adjustable angle, with no witness lines. `center` is the circle
  // centre; for a diameter a/b are the two opposite rim points, for a radius
  // (radiusHalf==true) a==center and b is the single rim point. Dragging the line
  // rotates it about the centre (writes angle= back to source).
  bool radial{false};
  bool radiusHalf{false};
  QPointF center;
  int id{-1};
  // 0-based source line to navigate to on right-click. For an editable // @dim
  // dimension this equals id; for a kvota= sugar dimension (id = -1, not
  // draggable) it points at one of the two dimref() calls so right-click still
  // jumps to source.
  int jumpLine{-1};
};

// An angle dimension in one ortho view: a vertex and its two ray endpoints (all
// in render-image pixel coords) plus the label. Drawn as two short rays from the
// vertex, an arc between them, and the angle label near the arc.
struct OrthoAngle {
  QPointF vertex;
  QPointF a;
  QPointF b;
  QString label;
};

// A cutting-plane line drawn in a standard view where a section() plane is seen
// edge-on: a thick chain line across the view (p0..p1), arrowheads at both ends
// pointing in the section's viewing direction, and the section letter at each end.
struct OrthoSectionLine {
  QPointF p0;
  QPointF p1;
  QPointF arrowDir;  // unit screen direction the arrows point (viewing direction)
  QString label;
};

// A detail marker drawn on a parent view: a thin circle around the region that a
// // @detail enlarges, labelled with its letter. center/r are in the parent view's
// view-local model-mm.
struct OrthoDetailMark {
  QPointF center;
  double r{0.0};
  QString label;
};

// A text label (from a label() marker + // @label line) drawn in one view: the
// projected 3D anchor point, the text, its offset from the anchor (view-local
// model-mm, y-down), a leader line from the text box to the anchor, and the
// 0-based // @label source line for laboff= write-back / right-click jump.
struct OrthoLabel {
  QPointF anchor;
  QString text;
  QPointF off;         // text position = anchor + off
  bool leader{true};
  int id{-1};
};

// A projected filled face for the optional colour drawing mode: the face outline
// in view-local model-mm (y-down, same frame as edges) and its colour. Painted
// behind the black edges.
struct OrthoFill {
  QPolygonF poly;
  QColor color;
};

// Everything needed to render one ortho view as a vector line drawing with its
// dimension overlay. All 2D coordinates are view-local model millimetres, y-down,
// with the origin at the view's bounding-box top-left corner (see
// MainWindow::refreshDrawingViews). The dock is a WYSIWYG editor of the export:
// what it paints is what the vector export writes.
struct OrthoViewData {
  QList<OrthoFill> fills;       // coloured face fills (model-mm, y-down); empty unless fill mode on
  QList<QLineF> edges;          // silhouette + feature edges (model-mm, y-down)
  QList<QLineF> hatch;          // section cut-face hatching (model-mm, y-down); empty for plain views
  QSizeF sizeMm;                // view bounding box (model-mm) used to fit
  QList<OrthoAnchor> anchors;
  QList<OrthoDimension> dims;
  QList<OrthoAngle> angles;
  QList<OrthoSectionLine> sectionLines;
  QList<OrthoDetailMark> detailMarks;  // detail circles drawn on this (parent) view
  QList<OrthoLabel> labels;            // text callouts (// @label) drawn in this view
  QList<OrthoAnchor> labelAnchors;     // clickable label() markers (for "new label")
  QString title;
  // Manual placement: section / detail tiles can be dragged to free space. movable
  // marks a draggable tile; placeKey identifies it for write-back ("section:A" or
  // "detail:<0-based source line>"); hasAt/atMm is the explicit sheet-mm origin
  // parsed from source (overrides the auto grid cell).
  bool movable{false};
  QString placeKey;
  bool hasAt{false};
  QPointF atMm;
  // This tile carries its own resolved dimensions / labels (standard views and
  // named section tiles), so they are interactive here: draggable, right-click to
  // jump, double-click to edit. False for detail tiles, whose geometry is a clipped
  // copy of a parent — dragging a dimension there would be ambiguous.
  bool annotated{false};
  // The proj= token that targets this tile ("front".."bottom", "section<name>").
  // Emitted when an anchor here is clicked, so a new // @dim or // @label lands on
  // the tile the user actually clicked in. Empty for tiles that cannot be targeted.
  QString projKey;
  // Drawing scale (drawing-mm per model-mm). Geometry is drawn in model-mm; text
  // and line weights are absolute drawing-mm, so their size relative to the
  // geometry reflects the scale (1:2 -> text twice as large relative to the part).
  double scale{1.0};
  bool valid() const { return sizeMm.isValid() && sizeMm.width() > 0 && sizeMm.height() > 0; }
};

// One placed view on the drawing sheet: its data plus the drawing-mm origin where
// its view-local (0,0) sits, which standard view it is (0..5 for the interactive
// standard views, -1 for display-only section / detail tiles), and whether it is a
// section tile.
struct PlacedView {
  OrthoViewData data;
  QPointF originMm;
  int viewIndex{-1};
  bool isSection{false};
  // Drawing-mm per model-mm for THIS tile: the sheet scale for standard / section
  // views, the detail's own (enlarged) scale for a detail tile.
  double drawScale{1.0};
};

/*!
   Single-canvas widget that paints the whole drawing sheet: every selected view
   (plus section / detail tiles) placed at its first-angle position, all sharing
   one fit + zoom/pan — a true WYSIWYG of the vector export. Sheet coordinates are
   drawing-mm; each PlacedView's view-local model-mm points map onto the sheet
   through its origin and the drawing scale. Mouse interaction (drag dimension /
   label, rotate ⌀/R, click anchor, right-click jump, double-click edit) is
   dispatched to whichever standard view sits under the cursor; section / detail
   tiles are display-only. Signals carry the hit view's index directly.
 */
class DrawingSheetWidget : public QWidget
{
  Q_OBJECT

public:
  DrawingSheetWidget(QWidget *parent = nullptr);

  // Replace the whole sheet: placed views (each carrying its own drawScale) and the
  // sheet size in drawing-mm. Zoom/pan are preserved across refreshes.
  void setSheet(const QVector<PlacedView>& placements, double sheetWmm, double sheetHmm);
  // Bump the geometry version whenever the underlying mesh changed (owner detects
  // this — see MainWindow's projection cache). A dimension edit re-runs setSheet with
  // the SAME version, so the cached per-tile geometry pixmaps survive it; only a real
  // mesh change (or a scale change) invalidates them.
  void setGeometryVersion(int version);
  void clear();
  void setShowAnchors(bool show);
  // Enter/leave "new detail" mode: while on, a left-drag on a standard view rubber-
  // bands a circle and emits detailRequested() on release (then mode turns off).
  void setDetailMode(bool on);
  bool hasContent() const { return !this->places.isEmpty(); }

signals:
  void anchorClicked(const QString& projKey, const QString& name);
  // Emitted when a label() marker dot is clicked (to create a // @label): the hit
  // view index and the marker's reference name.
  void labelAnchorClicked(const QString& projKey, const QString& name);
  // Emitted while dragging a // @label text (its offset from the anchor in the
  // view's local model-mm, y-down) and once more on release.
  void labelTextMoved(int viewIndex, int id, double dxMm, double dyMm, bool finished);
  // Emitted on double-click of a // @label text (its source line) to open its editor.
  void labelDoubleClicked(int id);
  // Emitted when a detail circle is drawn over standard view `viewIndex`: centre and
  // radius in that view's view-local model-mm.
  void detailRequested(const QString& projKey, double cx, double cy, double r);
  // Emitted when the detail rubber-band gesture ends (so the owner can untick the
  // toolbar button), regardless of whether a detail was created.
  void detailModeFinished();
  // Emitted when a movable (section / detail) tile is dropped: its placeKey and new
  // top-left origin in sheet drawing-mm. The owner writes at=x,y back to source.
  void tilePlaced(const QString& placeKey, double x, double y);
  // Emitted while dragging a dimension line and once more on release, with the hit
  // view index, the dimension id and its new perpendicular offset in model-mm.
  void dimensionOffsetChanged(int viewIndex, int id, double offsetPx, bool finished);
  // Emitted while rotating a radial (⌀/R) dimension and once more on release.
  void dimensionAngleChanged(int viewIndex, int id, double angleDeg, bool finished);
  // Emitted while dragging a dimension's label (offset in the dimension's own
  // along/perp frame, model-mm).
  void labelOffsetChanged(int viewIndex, int id, double alongPx, double perpPx, bool finished);
  // Emitted on double-click of a dimension line or label (its source id).
  void dimensionDoubleClicked(int id);
  // Emitted on right-click of a dimref anchor or dimension: 0-based source line.
  void jumpToSource(int line);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

private:
  void recomputeFit();
  // Sheet drawing-mm <-> widget px (shared fit + zoom/pan).
  QPointF sheetToWidget(const QPointF& sheetMm) const;
  QPointF widgetToSheet(const QPointF& widgetPt) const;
  // View-local model-mm <-> widget px, through a placement's origin and the scale.
  QPointF mapToWidget(const PlacedView& pl, const QPointF& viewLocalMm) const;
  QPointF mapToViewLocal(const PlacedView& pl, const QPointF& widgetPt) const;
  // A drawing-mm length in widget px (pen widths, arrows, fonts).
  double absPx(double drawingMm) const;
  QFont overlayFont() const;
  // Index of the placement whose content box contains the widget point, or -1.
  // Prefers interactive standard views over display-only tiles on overlap.
  int placementAt(const QPointF& widgetPt) const;
  // Find the dimension under a widget point across ALL standard-view placements
  // (label box first, then the line within a few px) — not gated by the tile under
  // the cursor, since dimensions / dragged-away labels often sit outside their tile.
  // editableOnly restricts to draggable // @dim dims. On a hit returns true and sets
  // the placement + dim index and whether the label (vs the line) was hit.
  bool hitDim(const QPointF& widgetPt, bool editableOnly, int& place, int& dim,
              bool& onLabel) const;
  // Find the // @label text box under a widget point across all standard views.
  bool hitLabel(const QPointF& widgetPt, int& place, int& label) const;
  void paintPlacement(QPainter& painter, const PlacedView& pl);
  // Draw one tile's bulk geometry (fills + hatch + edges) with the painter already
  // transformed to that tile's view-local mm. `cull` clips to `visLocal` (view-local
  // mm) for the direct-draw path; the pixmap-build path passes the whole tile.
  void renderTileGeometry(QPainter& painter, const PlacedView& pl, bool cull,
                          const QRectF& visLocal) const;
  // Draw one tile's live overlay (title, dimensions, angles, section lines, detail
  // marks, // @label callouts, anchor dots) in widget coordinates. Always redrawn —
  // it is cheap (dozens of primitives) and it is what the user drags.
  void paintTileOverlay(QPainter& painter, const PlacedView& pl);
  // Ensure `tileLayers[id]` holds this tile's geometry rasterised at scale k (px per
  // model-mm) and blit it, or fall back to direct-draw when the tile has no stable id
  // or is too large at this zoom. Returns false if nothing was cached (caller then
  // direct-draws). While `zooming`, a stale-scale pixmap is blitted scaled instead of
  // rebuilt, so a wheel spin stays responsive.
  bool blitTileGeometry(QPainter& painter, const PlacedView& pl, double k);
  // A stable per-tile key that survives the setSheet() of a dimension edit: the
  // standard-view index, or the movable tile's placeKey. Empty = not cacheable.
  static QString tileId(const PlacedView& pl);
  // View-local model-mm -> widget px as an affine QTransform, so the geometry
  // (edges / hatch / fills) can be handed to QPainter unmapped and in one batch
  // instead of transforming every endpoint here. Same mapping as mapToWidget().
  QTransform tileTransform(const PlacedView& pl) const;
  // True while a drag / pan is in flight: antialiasing is dropped for those frames,
  // which matters on large imported meshes where a tile is tens of thousands of edges.
  bool interacting() const
  {
    return this->panning || this->tileDrag >= 0 || this->draggingDim >= 0 ||
           this->draggingLabel >= 0 || this->draggingText >= 0;
  }

  QVector<PlacedView> places;
  double sheetWmm{0.0}, sheetHmm{0.0};
  // Fit transform (recomputed each paint): widget = sheetMm*(fitScale*userZoom) +
  // fitOffset + userPan.
  double fitScale{1.0};
  QPointF fitOffset;
  double userZoom{1.0};
  QPointF userPan;
  // Drag state: which placement (index into places) + which dim index within it.
  int dragPlace{-1};
  int draggingDim{-1};
  int draggingLabel{-1};
  int draggingText{-1};  // index into places[dragPlace].data.labels (// @label drag)
  bool panning{false};
  QPointF panLast;
  bool showAnchors{true};
  // "New detail" rubber-band gesture.
  bool detailMode{false};
  bool detailDragging{false};
  int detailPlace{-1};     // placement being drawn over
  QPointF detailCenter;    // press point (widget px)
  QPointF detailCur;       // current point (widget px)
  // Dragging a movable (section / detail) tile to reposition it.
  int tileDrag{-1};        // placement being moved, or -1
  QPointF tileGrabMm;      // press point - tile origin, in sheet drawing-mm

  // Per-tile cached rasterisation of the geometry layer (fills + hatch + edges),
  // keyed on tileId() so it outlives the setSheet() that every dimension edit fires.
  // Compositing the sheet is then a blit per tile plus the live overlay; pan and
  // tile-drag move where a pixmap lands, they do not redraw geometry.
  struct TileLayer {
    QPixmap px;
    double builtScale{0.0};  // px per model-mm the pixmap was rendered at
    int builtVersion{-1};    // geometry version it was rendered from
  };
  QHash<QString, TileLayer> tileLayers;
  int geomVersion{0};        // owner bumps this when the mesh changes
  // Zoom is two-phase: a wheel step blits the existing pixmaps scaled (responsive but
  // soft) and (re)arms this timer; when it fires, the pixmaps are rebuilt at the new
  // scale (crisp). `zooming` is true between the wheel step and the rebuild.
  QTimer *zoomSettleTimer{nullptr};
  bool zooming{false};
};

/*!
   Dock that shows orthographic views of the model (front / side / top =
   nárys / bokorys / půdorys) laid out like a technical drawing sheet, each with
   a dimension overlay. Passive: MainWindow renders the images, projects the
   dimref anchors / dimensions, and hands the result here via setViews().
 */
class DrawingViewDock : public Dock
{
  Q_OBJECT

public:
  DrawingViewDock(QWidget *parent = nullptr);
  ~DrawingViewDock() override;

  void setViews(const QList<OrthoViewData>& views, const QStringList& titles);
  // Forwarded to the sheet: bump when the mesh changed so the per-tile geometry
  // pixmap cache survives a dimension edit but drops on a real geometry change.
  void setGeometryVersion(int version);
  // Section-view tiles ("Řez A–A"), shown below the standard views and appended
  // to the export. Each carries edges + cut-face hatch; titles come from the data.
  void setSectionViews(const QList<OrthoViewData>& sections);
  // Detail (enlargement) tiles ("Detail A (5:1)"), shown below the section tiles.
  // Each carries the clipped edges/hatch and its own (enlarged) scale in data.scale.
  void setDetailViews(const QList<OrthoViewData>& details);
  void clearViews();
  // Drawing scale as drawing-mm per model-mm (1/denominator from the spinbox).
  double exportScale() const;
  // Is standard view `viewIndex` (0..5 = Front/Back/Left/Right/Top/Bottom) shown?
  bool isViewSelected(int viewIndex) const;
  // Whether the "Fill colors" mode is on (fill views with model color() colours).
  bool fillColors() const;

signals:
  // Emitted when an anchor is clicked in one of the views (view index + name).
  void anchorClicked(const QString& projKey, const QString& name);
  // Emitted when a label() marker dot is clicked to create a // @label line.
  void labelAnchorClicked(const QString& projKey, const QString& name);
  // Emitted while dragging a // @label text (offset from anchor, view-local mm).
  void labelTextMoved(int viewIndex, int id, double dxMm, double dyMm, bool finished);
  // Emitted on double-click of a // @label text (its source line) to open its editor.
  void labelDoubleClicked(int id);
  // Emitted when a dimension's offset is dragged (view index, dim id, offset px,
  // finished=true on mouse release).
  void dimensionOffsetChanged(int viewIndex, int id, double offsetPx, bool finished);
  // Emitted when a radial dimension is rotated (view index, dim id, angle deg,
  // finished=true on release).
  void dimensionAngleChanged(int viewIndex, int id, double angleDeg, bool finished);
  // Emitted when a dimension's label is dragged (view index, dim id, along/perp
  // px in the dimension's frame, finished=true on release).
  void labelOffsetChanged(int viewIndex, int id, double alongPx, double perpPx, bool finished);
  // Emitted on double-click of a dimension (its source id) to open its editor.
  void dimensionDoubleClicked(int id);
  // Emitted on right-click of a dimref anchor or dimension: 0-based source line.
  void jumpToSource(int line);
  // Emitted when the user clicks "Materialize kvóty".
  void materializeRequested();
  // Emitted when the set of shown views changes (a view checkbox toggled), so the
  // owner can reproject only the now-visible views.
  void viewSelectionChanged();
  // Emitted when a detail circle is drawn over standard view `viewIndex`: centre and
  // radius in that view's view-local model-mm. The owner writes a // @detail line.
  void detailRequested(const QString& projKey, double cx, double cy, double r);
  // Emitted when a movable (section / detail) tile is dropped: its placeKey
  // ("section:A" / "detail:<line>") and new sheet-mm origin. The owner writes at=.
  void tilePlaced(const QString& placeKey, double x, double y);

private slots:
  void saveSheet();
  void exportDrawing();
  void setShowAnchors(bool show);

private:
  // Enable/disable the export + save buttons (and toggle the placeholder) based on
  // whether any shown view has content.
  void updateExportEnabled();
  // Recompute the first-angle sheet layout from the stored views + section tiles at
  // the current scale and selection, and hand the placed views to the canvas. Cheap
  // (no reprojection): called after setViews / setSectionViews and on scale change.
  void rebuildSheet();

  DrawingSheetWidget *sheet{nullptr};
  QVector<class QCheckBox *> viewChecks;  // per-view enable (0..5)
  QList<OrthoViewData> sectionViews;
  QList<OrthoViewData> detailViews;
  QLabel *placeholder;
  QPushButton *saveButton;
  QPushButton *materializeButton;
  QPushButton *detailButton;  // checkable "New detail" mode
  QPushButton *exportButton;
  QCheckBox *showRefsCheck;
  QCheckBox *fillColorsCheck;
  QCheckBox *separateFilesCheck;
  QDoubleSpinBox *scaleSpin;
  QList<OrthoViewData> views;
};
