#pragma once

#include <QFont>
#include <QImage>
#include <QLineF>
#include <QList>
#include <QPointF>
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

// Everything needed to render one ortho view as a vector line drawing with its
// dimension overlay. All 2D coordinates are view-local model millimetres, y-down,
// with the origin at the view's bounding-box top-left corner (see
// MainWindow::refreshDrawingViews). The dock is a WYSIWYG editor of the export:
// what it paints is what the vector export writes.
struct OrthoViewData {
  QList<QLineF> edges;          // silhouette + feature edges (model-mm, y-down)
  QList<QLineF> hatch;          // section cut-face hatching (model-mm, y-down); empty for plain views
  QSizeF sizeMm;                // view bounding box (model-mm) used to fit
  QList<OrthoAnchor> anchors;
  QList<OrthoDimension> dims;
  QList<OrthoAngle> angles;
  QList<OrthoSectionLine> sectionLines;
  QString title;
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

  // Replace the whole sheet: placed views, drawing scale (drawing-mm per model-mm)
  // and sheet size in drawing-mm. Zoom/pan are preserved across refreshes.
  void setSheet(const QVector<PlacedView>& placements, double scale,
                double sheetWmm, double sheetHmm);
  void clear();
  void setShowAnchors(bool show);
  bool hasContent() const { return !this->places.isEmpty(); }

signals:
  void anchorClicked(int viewIndex, const QString& name);
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
  // Index of the dimension under a widget point within placement pl (label or
  // line). editableOnly restricts to draggable // @dim dims (id >= 0).
  int dimAt(const PlacedView& pl, const QPointF& widgetPt, bool editableOnly = true) const;
  void paintPlacement(QPainter& painter, const PlacedView& pl);

  QVector<PlacedView> places;
  double scale{1.0};
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
  bool panning{false};
  QPointF panLast;
  bool showAnchors{true};
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
  // Section-view tiles ("Řez A–A"), shown below the standard views and appended
  // to the export. Each carries edges + cut-face hatch; titles come from the data.
  void setSectionViews(const QList<OrthoViewData>& sections);
  void clearViews();
  // Drawing scale as drawing-mm per model-mm (1/denominator from the spinbox).
  double exportScale() const;
  // Is standard view `viewIndex` (0..5 = Front/Back/Left/Right/Top/Bottom) shown?
  bool isViewSelected(int viewIndex) const;

signals:
  // Emitted when an anchor is clicked in one of the views (view index + name).
  void anchorClicked(int viewIndex, const QString& name);
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
  QLabel *placeholder;
  QPushButton *saveButton;
  QPushButton *materializeButton;
  QPushButton *exportButton;
  QCheckBox *showRefsCheck;
  QCheckBox *separateFilesCheck;
  QDoubleSpinBox *scaleSpin;
  QList<OrthoViewData> views;
};
