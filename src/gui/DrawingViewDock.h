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
  QString title;
  // Drawing scale (drawing-mm per model-mm). Geometry is drawn in model-mm; text
  // and line weights are absolute drawing-mm, so their size relative to the
  // geometry reflects the scale (1:2 -> text twice as large relative to the part).
  double scale{1.0};
  bool valid() const { return sizeMm.isValid() && sizeMm.width() > 0 && sizeMm.height() > 0; }
};

/*!
   Widget that shows one orthographic view (the rendered image) with a dimension
   overlay drawn on top: dimref anchor dots and dimension lines with labels. The
   image is scaled to fit while keeping aspect; overlay coordinates are given in
   the render image's native pixel space and mapped through the same fit
   transform. Clicking near an anchor emits anchorClicked() (used in Phase 2B-3).
 */
class OrthoViewWidget : public QWidget
{
  Q_OBJECT

public:
  OrthoViewWidget(QWidget *parent = nullptr);

  void setData(const OrthoViewData& data);
  void clear();
  void setShowAnchors(bool show);
  // Draw a leader line from a dragged-away label to its dimension line (all dim
  // types). Off = the label floats free with no connector.
  void setShowLeaders(bool show);
  // Update only the drawing scale (drawing-mm per model-mm) and repaint; used
  // when the user changes the scale spinbox without re-rendering the geometry.
  void setScale(double scale);

signals:
  void anchorClicked(const QString& name);
  // Emitted while dragging a dimension line and once more on release, with the
  // dimension id and its new perpendicular offset in image pixels.
  void dimensionOffsetChanged(int id, double offsetPx, bool finished);
  // Emitted while rotating a radial (⌀/R) dimension and once more on release,
  // with the dimension id and the new angle in degrees.
  void dimensionAngleChanged(int id, double angleDeg, bool finished);
  // Emitted while dragging a dimension's label, with the new label offset in the
  // dimension's (along, perp) frame in image pixels.
  void labelOffsetChanged(int id, double alongPx, double perpPx, bool finished);
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
  // Index of the dimension under a widget point (label or line), or -1. When
  // editableOnly is true only draggable // @dim dimensions are considered (used
  // for double-click / drag); false also matches kvota= dims (used for the
  // right-click jump-to-source).
  int dimAt(const QPointF& widgetPt, bool editableOnly = true) const;
  // Widget pixels per model-mm at the current fit + zoom.
  double mmToPx() const;
  // An absolute drawing-mm size in widget pixels (drawing-mm -> model-mm via the
  // scale, then model-mm -> widget px). Used for pen widths, arrows and fonts so
  // they read at a fixed size relative to the scaled drawing.
  double absPx(double drawingMm) const;
  // The dimension-label font at the current zoom/scale.
  QFont overlayFont() const;

private:
  // Maps a view-local model-mm point to a widget point using the current fit.
  QPointF mapToWidget(const QPointF& imgPt) const;
  QPointF mapToImage(const QPointF& widgetPt) const;
  // Unit perpendicular (image space) of dimension i, pointing along +offset.
  QPointF dimPerp(const OrthoDimension& d) const;
  // Label anchor point (image space): midpoint of the offset dimension line plus
  // the label's own along/perp offset.
  QPointF dimLabelPos(const OrthoDimension& d) const;

  OrthoViewData data;
  // Fit transform (recomputed each paint): widget = image*(fitScale*userZoom) +
  // fitOffset + userPan. fitScale/fitOffset fit the image; userZoom/userPan are
  // the user's wheel-zoom and drag-pan on top.
  double fitScale{1.0};
  QPointF fitOffset;
  double userZoom{1.0};
  QPointF userPan;
  // Index into data.dims currently being dragged (line), or -1.
  int draggingDim{-1};
  // Index into data.dims whose label is being dragged, or -1.
  int draggingLabel{-1};
  // Panning the view with an empty-area left drag.
  bool panning{false};
  QPointF panLast;
  // Whether to draw reference-point dots and their names.
  bool showAnchors{true};
  // Whether to draw a leader line from a dragged-away label to its dimension line.
  bool showLeaders{true};
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
  void setShowLeaders(bool show);

private:
  // Enable/disable the export + save buttons based on whether any shown view has
  // content.
  void updateExportEnabled();

  QVector<OrthoViewWidget *> viewWidgets;
  QVector<class QGroupBox *> viewBoxes;  // parallel to viewWidgets; checkable = shown
  QVector<OrthoViewWidget *> sectionWidgets;  // dynamic; one per section() marker
  QVector<class QGroupBox *> sectionBoxes;
  QList<OrthoViewData> sectionViews;
  class QGridLayout *sheetGrid{nullptr};  // to add section tiles below the standard grid
  class QWidget *sheetWidget{nullptr};
  QLabel *placeholder;
  QPushButton *saveButton;
  QPushButton *materializeButton;
  QPushButton *exportButton;
  QCheckBox *showRefsCheck;
  QCheckBox *showLeadersCheck;
  QCheckBox *separateFilesCheck;
  QDoubleSpinBox *scaleSpin;
  QList<OrthoViewData> views;
};
