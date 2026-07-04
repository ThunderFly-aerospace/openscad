#pragma once

#include <QImage>
#include <QList>
#include <QPointF>
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
  int id{-1};
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

// Everything needed to render one ortho view with its overlay.
struct OrthoViewData {
  QImage image;
  QList<OrthoAnchor> anchors;
  QList<OrthoDimension> dims;
  QList<OrthoAngle> angles;
  QString title;
  double pxPerMm{1.0};  // render-image pixels per model unit (for scaled export)
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

signals:
  void anchorClicked(const QString& name);
  // Emitted while dragging a dimension line and once more on release, with the
  // dimension id and its new perpendicular offset in image pixels.
  void dimensionOffsetChanged(int id, double offsetPx, bool finished);
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
  // Index of the editable dimension under a widget point (label or line), or -1.
  int dimAt(const QPointF& widgetPt) const;

private:
  // Maps a render-image pixel point to a widget point using the current fit.
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
  void clearViews();
  // Black-and-white line-drawing mode (vs. colored). Read by MainWindow when
  // rendering the ortho views.
  bool lineMode() const;

signals:
  // Emitted when an anchor is clicked in one of the views (view index + name).
  void anchorClicked(int viewIndex, const QString& name);
  // Emitted when a dimension's offset is dragged (view index, dim id, offset px,
  // finished=true on mouse release).
  void dimensionOffsetChanged(int viewIndex, int id, double offsetPx, bool finished);
  // Emitted when a dimension's label is dragged (view index, dim id, along/perp
  // px in the dimension's frame, finished=true on release).
  void labelOffsetChanged(int viewIndex, int id, double alongPx, double perpPx, bool finished);
  // Emitted on double-click of a dimension (its source id) to open its editor.
  void dimensionDoubleClicked(int id);
  // Emitted on right-click of a dimref anchor or dimension: 0-based source line.
  void jumpToSource(int line);
  // Emitted when the user clicks "Materialize kvóty".
  void materializeRequested();
  // Emitted when the colored/line-drawing mode is toggled (needs a re-render).
  void renderModeChanged();

private slots:
  void saveSheet();
  void exportDrawing();
  void setShowAnchors(bool show);

private:
  QVector<OrthoViewWidget *> viewWidgets;
  QLabel *placeholder;
  QPushButton *saveButton;
  QPushButton *materializeButton;
  QPushButton *exportButton;
  QCheckBox *showRefsCheck;
  QCheckBox *lineModeCheck;
  QDoubleSpinBox *scaleSpin;
  QList<OrthoViewData> views;
};
