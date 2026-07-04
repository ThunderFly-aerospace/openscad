#include "gui/DrawingViewDock.h"

#include <algorithm>
#include <cmath>

#include <QFileDialog>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMarginsF>
#include <QPageSize>
#include <QPdfWriter>
#include <QSvgGenerator>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

// ---------------------------------------------------------------------------
// OrthoViewWidget
// ---------------------------------------------------------------------------

namespace {
// Distance from point p to segment [a,b].
double distToSegment(const QPointF& p, const QPointF& a, const QPointF& b)
{
  const QPointF ab = b - a;
  const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
  double t = 0.0;
  if (len2 > 1e-9) {
    t = ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len2;
    t = std::clamp(t, 0.0, 1.0);
  }
  const QPointF proj(a.x() + t * ab.x(), a.y() + t * ab.y());
  return std::hypot(p.x() - proj.x(), p.y() - proj.y());
}
}  // namespace

OrthoViewWidget::OrthoViewWidget(QWidget *parent) : QWidget(parent)
{
  setMinimumSize(120, 90);
  setMouseTracking(false);
}

void OrthoViewWidget::setData(const OrthoViewData& newData)
{
  this->data = newData;
  this->draggingDim = -1;
  this->draggingLabel = -1;
  update();
}

void OrthoViewWidget::clear()
{
  this->data = OrthoViewData{};
  this->draggingDim = -1;
  this->draggingLabel = -1;
  update();
}

void OrthoViewWidget::setShowAnchors(bool show)
{
  this->showAnchors = show;
  update();
}

QPointF OrthoViewWidget::mapToWidget(const QPointF& imgPt) const
{
  return QPointF(imgPt.x() * this->fitScale + this->fitOffset.x(),
                 imgPt.y() * this->fitScale + this->fitOffset.y());
}

QPointF OrthoViewWidget::mapToImage(const QPointF& widgetPt) const
{
  if (this->fitScale < 1e-9) return QPointF();
  return QPointF((widgetPt.x() - this->fitOffset.x()) / this->fitScale,
                 (widgetPt.y() - this->fitOffset.y()) / this->fitScale);
}

QPointF OrthoViewWidget::dimPerp(const OrthoDimension& d) const
{
  QPointF dir = d.dir;
  const double len = std::hypot(dir.x(), dir.y());
  if (len < 1e-9) return QPointF(0, -1);
  dir /= len;
  return QPointF(dir.y(), -dir.x());  // rotate -90°
}

// Foot of an image-space point projected onto the (offset) dimension line: the
// line through midpoint(a,b)+perp*offset running along the measurement
// direction dir. Used for both drawing and hit-testing so they stay in sync.
static QPointF dimFoot(const OrthoDimension& d, const QPointF& perp, const QPointF& t)
{
  QPointF dir = d.dir;
  const double len = std::hypot(dir.x(), dir.y());
  if (len > 1e-9) dir /= len;
  else dir = QPointF(1, 0);
  const QPointF base = (d.a + d.b) / 2.0 + perp * d.offsetPx;
  const QPointF rel = t - base;
  const double s = rel.x() * dir.x() + rel.y() * dir.y();
  return base + dir * s;
}

QPointF OrthoViewWidget::dimLabelPos(const OrthoDimension& d) const
{
  // Image-space label anchor: midpoint of the dimension line (feet of a and b),
  // plus the label's own offset along the line and perpendicular to it.
  const QPointF perp = dimPerp(d);
  QPointF dir = d.dir;
  const double len = std::hypot(dir.x(), dir.y());
  if (len > 1e-9) dir /= len;
  else dir = QPointF(1, 0);
  const QPointF mid = (dimFoot(d, perp, d.a) + dimFoot(d, perp, d.b)) / 2.0;
  return mid + dir * d.labAlongPx + perp * d.labPerpPx;
}

void OrthoViewWidget::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  painter.fillRect(rect(), Qt::white);

  if (this->data.image.isNull()) {
    painter.setPen(Qt::gray);
    painter.drawText(rect(), Qt::AlignCenter, tr("(no preview)"));
    return;
  }

  // Fit the image into the widget while keeping aspect ratio, centered.
  const QSize imgSize = this->data.image.size();
  this->fitScale = std::min(static_cast<double>(width()) / imgSize.width(),
                            static_cast<double>(height()) / imgSize.height());
  const double drawW = imgSize.width() * this->fitScale;
  const double drawH = imgSize.height() * this->fitScale;
  this->fitOffset = QPointF((width() - drawW) / 2.0, (height() - drawH) / 2.0);

  painter.drawImage(QRectF(this->fitOffset.x(), this->fitOffset.y(), drawW, drawH),
                    this->data.image);

  painter.setRenderHint(QPainter::Antialiasing, true);

  // Dimensions: witness lines from the anchors out to an offset dimension line
  // with arrowheads, and a length label riding on it.
  QPen dimPen(QColor(20, 20, 20));
  dimPen.setWidthF(1.4);
  QPen witnessPen(QColor(120, 120, 120));
  witnessPen.setWidthF(0.8);
  for (const auto& dim : this->data.dims) {
    const QPointF a = mapToWidget(dim.a);
    const QPointF b = mapToWidget(dim.b);
    const QPointF perp = dimPerp(dim);
    // Endpoints of the dimension line: feet of the anchors on the offset line
    // along the measurement direction (image space, then mapped to widget).
    const QPointF a2 = mapToWidget(dimFoot(dim, perp, dim.a));
    const QPointF b2 = mapToWidget(dimFoot(dim, perp, dim.b));

    // Witness (extension) lines from each anchor to its foot on the dimension
    // line (skip the degenerate case where the foot coincides with the anchor).
    painter.setPen(witnessPen);
    if (std::hypot((a2 - a).x(), (a2 - a).y()) > 0.5) painter.drawLine(a, a2);
    if (std::hypot((b2 - b).x(), (b2 - b).y()) > 0.5) painter.drawLine(b, b2);

    // Dimension line with arrowheads.
    painter.setPen(dimPen);
    painter.drawLine(a2, b2);
    QPointF dir = b2 - a2;
    const double len = std::hypot(dir.x(), dir.y());
    if (len > 1e-6) {
      dir /= len;
      const QPointF nrm(-dir.y(), dir.x());
      const double s = 6.0;
      auto arrow = [&](const QPointF& tip, const QPointF& along) {
        QPolygonF head;
        head << tip << (tip + along * s + nrm * (s * 0.4))
             << (tip + along * s - nrm * (s * 0.4));
        painter.setBrush(QColor(20, 20, 20));
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(head);
        painter.setPen(dimPen);
      };
      if (dim.arrowsOut) {
        // Arrowheads outside the endpoints pointing inward (for tight dims);
        // extend the dimension line a little past each end so they have a stem.
        painter.drawLine(a2, a2 - dir * (s * 1.5));
        painter.drawLine(b2, b2 + dir * (s * 1.5));
        arrow(a2, -dir);
        arrow(b2, dir);
      } else {
        arrow(a2, dir);
        arrow(b2, -dir);
      }
    }

    // Label at its own position (dimension midpoint plus label offset).
    const QPointF labelCenter = mapToWidget(dimLabelPos(dim));
    const QString label = dim.label;
    const QRectF textRect = painter.fontMetrics().boundingRect(label).adjusted(-3, -2, 3, 2);
    QRectF box(labelCenter.x() - textRect.width() / 2.0,
               labelCenter.y() - textRect.height() / 2.0, textRect.width(), textRect.height());
    // Keep the label inside the widget so it never clips at the view border.
    const double bx = std::clamp(box.x(), 1.0, width() - box.width() - 1.0);
    const double by = std::clamp(box.y(), 1.0, height() - box.height() - 1.0);
    box.moveTo(bx, by);
    painter.fillRect(box, QColor(255, 255, 255, 220));
    painter.setPen(QColor(150, 30, 30));
    painter.drawText(box, Qt::AlignCenter, label);
  }

  // Angle dimensions: two rays from the vertex, an arc between them, and a label.
  for (const auto& ang : this->data.angles) {
    const QPointF v = mapToWidget(ang.vertex);
    const QPointF a = mapToWidget(ang.a);
    const QPointF b = mapToWidget(ang.b);
    QPointF ua = a - v, ub = b - v;
    const double la = std::hypot(ua.x(), ua.y());
    const double lb = std::hypot(ub.x(), ub.y());
    if (la < 1e-6 || lb < 1e-6) continue;
    ua /= la;
    ub /= lb;
    // Arc radius: a fixed fraction of the shorter ray, clamped to a sane range.
    const double arcR = std::clamp(std::min(la, lb) * 0.5, 14.0, 42.0);

    painter.setPen(witnessPen);
    painter.drawLine(v, v + ua * std::max(la, arcR + 6));
    painter.drawLine(v, v + ub * std::max(lb, arcR + 6));

    // Arc from ray a to ray b (Qt angles: degrees CCW from +x, y up; widget y is
    // down, so negate). Sweep the short way between the two rays.
    double angA = std::atan2(-ua.y(), ua.x()) * 180.0 / M_PI;
    double angB = std::atan2(-ub.y(), ub.x()) * 180.0 / M_PI;
    double span = angB - angA;
    while (span <= -180.0) span += 360.0;
    while (span > 180.0) span -= 360.0;
    const QRectF arcRect(v.x() - arcR, v.y() - arcR, 2 * arcR, 2 * arcR);
    painter.setPen(dimPen);
    painter.drawArc(arcRect, static_cast<int>(angA * 16), static_cast<int>(span * 16));

    // Label along the bisector, just outside the arc.
    QPointF bis = ua + ub;
    const double lbis = std::hypot(bis.x(), bis.y());
    bis = lbis > 1e-6 ? bis / lbis : QPointF(0, -1);
    const QPointF labelCenter = v + bis * (arcR + 12);
    const QRectF tr = painter.fontMetrics().boundingRect(ang.label).adjusted(-3, -2, 3, 2);
    QRectF box(labelCenter.x() - tr.width() / 2.0, labelCenter.y() - tr.height() / 2.0,
               tr.width(), tr.height());
    const double bx = std::clamp(box.x(), 1.0, width() - box.width() - 1.0);
    const double by = std::clamp(box.y(), 1.0, height() - box.height() - 1.0);
    box.moveTo(bx, by);
    painter.fillRect(box, QColor(255, 255, 255, 220));
    painter.setPen(QColor(150, 30, 30));
    painter.drawText(box, Qt::AlignCenter, ang.label);
  }

  // Anchor dots with names (reference points) — optional.
  if (this->showAnchors)
  for (const auto& anchor : this->data.anchors) {
    const QPointF p = mapToWidget(anchor.pos);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(30, 90, 200));
    painter.drawEllipse(p, 3.5, 3.5);
    painter.setPen(QColor(30, 90, 200));
    painter.drawText(p + QPointF(5, -4), anchor.name);
  }
}

void OrthoViewWidget::mousePressEvent(QMouseEvent *event)
{
  if (this->data.image.isNull()) return;
  const QPointF click = event->position();

  // First: start dragging a label if the click is on one (editable dims only).
  const QFontMetrics fm = fontMetrics();
  for (int i = 0; i < this->data.dims.size(); ++i) {
    const auto& dim = this->data.dims.at(i);
    if (dim.id < 0) continue;
    QRectF box = fm.boundingRect(dim.label).adjusted(-3, -2, 3, 2);
    box.moveCenter(mapToWidget(dimLabelPos(dim)));
    if (box.contains(click)) {
      this->draggingLabel = i;
      return;
    }
  }

  // Next: start dragging a dimension line if the click is near one (editable
  // dimensions only, i.e. those with a source id).
  double bestLine = 8.0;
  int hitDim = -1;
  for (int i = 0; i < this->data.dims.size(); ++i) {
    const auto& dim = this->data.dims.at(i);
    if (dim.id < 0) continue;
    const QPointF perp = dimPerp(dim);
    const QPointF a2 = mapToWidget(dimFoot(dim, perp, dim.a));
    const QPointF b2 = mapToWidget(dimFoot(dim, perp, dim.b));
    const double d = distToSegment(click, a2, b2);
    if (d < bestLine) {
      bestLine = d;
      hitDim = i;
    }
  }
  if (hitDim >= 0) {
    this->draggingDim = hitDim;
    return;
  }

  // Otherwise: report an anchor click (used to create dimensions). Hidden
  // reference points are not clickable.
  if (!this->showAnchors) return;
  const double radius = 12.0;
  QStringList hits;  // distinct anchor refs within the hit radius
  for (const auto& anchor : this->data.anchors) {
    const QPointF p = mapToWidget(anchor.pos);
    if (std::hypot(p.x() - click.x(), p.y() - click.y()) <= radius &&
        !hits.contains(anchor.ref)) {
      hits << anchor.ref;
    }
  }
  if (hits.isEmpty()) return;
  if (hits.size() == 1) {
    emit anchorClicked(hits.first());
    return;
  }
  // Several reference points coincide here: let the user pick which one (the
  // menu shows the ref token, e.g. A#0 / A#1, so duplicates are distinguishable).
  QMenu menu(this);
  for (const QString& ref : hits) menu.addAction(ref);
  if (QAction *chosen = menu.exec(event->globalPosition().toPoint())) {
    emit anchorClicked(chosen->text());
  }
}

void OrthoViewWidget::mouseMoveEvent(QMouseEvent *event)
{
  // Label drag: decompose the mouse position (relative to the dimension-line
  // midpoint) into the along/perp frame of the dimension.
  if (this->draggingLabel >= 0 && this->draggingLabel < this->data.dims.size()) {
    OrthoDimension& dim = this->data.dims[this->draggingLabel];
    const QPointF perp = dimPerp(dim);
    QPointF dir = dim.dir;
    const double len = std::hypot(dir.x(), dir.y());
    if (len > 1e-9) dir /= len;
    else dir = QPointF(1, 0);
    const QPointF base = (dim.a + dim.b) / 2.0 + perp * dim.offsetPx;
    const QPointF rel = mapToImage(event->position()) - base;
    dim.labAlongPx = rel.x() * dir.x() + rel.y() * dir.y();
    dim.labPerpPx = rel.x() * perp.x() + rel.y() * perp.y();
    update();
    emit labelOffsetChanged(dim.id, dim.labAlongPx, dim.labPerpPx, false);
    return;
  }

  if (this->draggingDim < 0 || this->draggingDim >= this->data.dims.size()) return;
  OrthoDimension& dim = this->data.dims[this->draggingDim];
  const QPointF perp = dimPerp(dim);
  const QPointF mouseImg = mapToImage(event->position());
  const QPointF mid = (dim.a + dim.b) / 2.0;
  const QPointF rel = mouseImg - mid;
  dim.offsetPx = rel.x() * perp.x() + rel.y() * perp.y();  // project onto perp
  update();
  emit dimensionOffsetChanged(dim.id, dim.offsetPx, false);
}

int OrthoViewWidget::dimAt(const QPointF& pt) const
{
  const QFontMetrics fm = fontMetrics();
  // Label box first, then the dimension line.
  for (int i = 0; i < this->data.dims.size(); ++i) {
    const auto& dim = this->data.dims.at(i);
    if (dim.id < 0) continue;
    QRectF box = fm.boundingRect(dim.label).adjusted(-3, -2, 3, 2);
    box.moveCenter(mapToWidget(dimLabelPos(dim)));
    if (box.contains(pt)) return i;
  }
  for (int i = 0; i < this->data.dims.size(); ++i) {
    const auto& dim = this->data.dims.at(i);
    if (dim.id < 0) continue;
    const QPointF perp = dimPerp(dim);
    if (distToSegment(pt, mapToWidget(dimFoot(dim, perp, dim.a)),
                      mapToWidget(dimFoot(dim, perp, dim.b))) < 8.0) return i;
  }
  return -1;
}

void OrthoViewWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
  if (this->data.image.isNull()) return;
  const int i = dimAt(event->position());
  if (i >= 0) emit dimensionDoubleClicked(this->data.dims.at(i).id);
}

void OrthoViewWidget::mouseReleaseEvent(QMouseEvent *)
{
  if (this->draggingLabel >= 0 && this->draggingLabel < this->data.dims.size()) {
    const OrthoDimension& dim = this->data.dims[this->draggingLabel];
    emit labelOffsetChanged(dim.id, dim.labAlongPx, dim.labPerpPx, true);
    this->draggingLabel = -1;
    return;
  }
  if (this->draggingDim < 0 || this->draggingDim >= this->data.dims.size()) return;
  const OrthoDimension& dim = this->data.dims[this->draggingDim];
  emit dimensionOffsetChanged(dim.id, dim.offsetPx, true);
  this->draggingDim = -1;
}

// ---------------------------------------------------------------------------
// DrawingViewDock
// ---------------------------------------------------------------------------

namespace {
// Grid position (row, col) for each ortho view, laid out like a drawing sheet:
//   nárys (front)  | bokorys (side)
//   půdorys (top)  |
constexpr int kViewCount = 3;
constexpr int kGridPos[kViewCount][2] = {{0, 0}, {0, 1}, {1, 0}};
}  // namespace

DrawingViewDock::DrawingViewDock(QWidget *parent) : Dock(parent)
{
  auto *central = new QWidget(this);
  auto *layout = new QVBoxLayout(central);
  layout->setContentsMargins(4, 4, 4, 4);

  auto *sheet = new QWidget(central);
  auto *grid = new QGridLayout(sheet);
  grid->setContentsMargins(0, 0, 0, 0);

  const char *defaultTitles[kViewCount] = {
    QT_TR_NOOP("Front (nárys)"), QT_TR_NOOP("Side (bokorys)"), QT_TR_NOOP("Top (půdorys)")};

  for (int i = 0; i < kViewCount; ++i) {
    auto *box = new QGroupBox(tr(defaultTitles[i]), sheet);
    auto *boxLayout = new QVBoxLayout(box);
    boxLayout->setContentsMargins(2, 2, 2, 2);
    auto *view = new OrthoViewWidget(box);
    boxLayout->addWidget(view);
    grid->addWidget(box, kGridPos[i][0], kGridPos[i][1]);
    this->viewWidgets.append(view);

    connect(view, &OrthoViewWidget::anchorClicked, this,
            [this, i](const QString& name) { emit anchorClicked(i, name); });
    connect(view, &OrthoViewWidget::dimensionOffsetChanged, this,
            [this, i](int id, double offsetPx, bool finished) {
              emit dimensionOffsetChanged(i, id, offsetPx, finished);
            });
    connect(view, &OrthoViewWidget::labelOffsetChanged, this,
            [this, i](int id, double alongPx, double perpPx, bool finished) {
              emit labelOffsetChanged(i, id, alongPx, perpPx, finished);
            });
    connect(view, &OrthoViewWidget::dimensionDoubleClicked, this,
            &DrawingViewDock::dimensionDoubleClicked);
  }
  grid->setRowStretch(0, 1);
  grid->setRowStretch(1, 1);
  grid->setColumnStretch(0, 1);
  grid->setColumnStretch(1, 1);

  this->placeholder = new QLabel(tr("Render (F5) to generate orthographic views."), central);
  this->placeholder->setAlignment(Qt::AlignCenter);

  this->showRefsCheck = new QCheckBox(tr("Reference points"), central);
  this->showRefsCheck->setChecked(true);
  this->showRefsCheck->setToolTip(
    tr("Show the dimref anchor dots and their names (dimension labels stay)."));

  this->lineModeCheck = new QCheckBox(tr("Line drawing (b&&w)"), central);
  this->lineModeCheck->setChecked(false);
  this->lineModeCheck->setToolTip(
    tr("Render a black-and-white line drawing (uniform faces + visible edges) "
       "instead of the colored model."));

  this->materializeButton = new QPushButton(tr("Materialize kvóty"), central);
  this->materializeButton->setToolTip(
    tr("Write a // @dim line for each kvota= pair, making them editable."));

  this->scaleSpin = new QDoubleSpinBox(central);
  this->scaleSpin->setPrefix(tr("Scale 1:"));
  this->scaleSpin->setRange(0.01, 10000.0);
  this->scaleSpin->setDecimals(2);
  this->scaleSpin->setValue(1.0);
  this->scaleSpin->setToolTip(tr("Drawing scale for export (model units : drawing mm)."));

  this->exportButton = new QPushButton(tr("Export SVG/PDF…"), central);
  this->exportButton->setEnabled(false);
  this->exportButton->setToolTip(
    tr("Export the sheet as scaled vector SVG or PDF (labels stay readable)."));

  this->saveButton = new QPushButton(tr("Save Sheet…"), central);
  this->saveButton->setEnabled(false);

  auto *buttonRow = new QWidget(central);
  auto *buttonLayout = new QHBoxLayout(buttonRow);
  buttonLayout->setContentsMargins(0, 0, 0, 0);
  buttonLayout->addWidget(this->materializeButton);
  buttonLayout->addWidget(this->saveButton);

  auto *checkRow = new QWidget(central);
  auto *checkLayout = new QHBoxLayout(checkRow);
  checkLayout->setContentsMargins(0, 0, 0, 0);
  checkLayout->addWidget(this->showRefsCheck);
  checkLayout->addWidget(this->lineModeCheck);
  checkLayout->addStretch(1);
  checkLayout->addWidget(this->scaleSpin);
  checkLayout->addWidget(this->exportButton);

  layout->addWidget(sheet, 1);
  layout->addWidget(this->placeholder);
  layout->addWidget(checkRow);
  layout->addWidget(buttonRow);
  setWidget(central);

  connect(this->saveButton, &QPushButton::clicked, this, &DrawingViewDock::saveSheet);
  connect(this->exportButton, &QPushButton::clicked, this, &DrawingViewDock::exportDrawing);
  connect(this->materializeButton, &QPushButton::clicked, this,
          &DrawingViewDock::materializeRequested);
  connect(this->showRefsCheck, &QCheckBox::toggled, this, &DrawingViewDock::setShowAnchors);
  connect(this->lineModeCheck, &QCheckBox::toggled, this, &DrawingViewDock::renderModeChanged);
}

bool DrawingViewDock::lineMode() const
{
  return this->lineModeCheck->isChecked();
}

void DrawingViewDock::setShowAnchors(bool show)
{
  for (auto *view : this->viewWidgets) view->setShowAnchors(show);
}

DrawingViewDock::~DrawingViewDock() = default;

void DrawingViewDock::setViews(const QList<OrthoViewData>& newViews, const QStringList& titles)
{
  this->views = newViews;

  for (int i = 0; i < this->viewWidgets.size(); ++i) {
    if (i < this->views.size()) {
      this->viewWidgets.at(i)->setData(this->views.at(i));
    } else {
      this->viewWidgets.at(i)->clear();
    }
    if (i < titles.size() && !titles.at(i).isEmpty()) {
      if (auto *box = qobject_cast<QGroupBox *>(this->viewWidgets.at(i)->parentWidget())) {
        box->setTitle(titles.at(i));
      }
    }
  }

  bool any = false;
  for (const auto& v : this->views) {
    if (!v.image.isNull()) any = true;
  }
  this->saveButton->setEnabled(any);
  this->exportButton->setEnabled(any);
  this->placeholder->setVisible(!any);
}

void DrawingViewDock::clearViews()
{
  setViews({}, {});
}

void DrawingViewDock::saveSheet()
{
  if (this->viewWidgets.isEmpty()) return;
  const QString filename = QFileDialog::getSaveFileName(
    this, tr("Save Drawing Sheet"), "drawing.png", tr("PNG image (*.png)"));
  if (filename.isEmpty()) return;

  // Compose the three views (with overlays) side by side by grabbing each widget.
  QVector<QPixmap> shots;
  int totalW = 0, maxH = 0;
  for (auto *view : this->viewWidgets) {
    QPixmap shot = view->grab();
    shots.append(shot);
    totalW += shot.width();
    maxH = std::max(maxH, shot.height());
  }
  if (totalW == 0 || maxH == 0) return;
  QImage sheet(totalW, maxH, QImage::Format_ARGB32);
  sheet.fill(Qt::white);
  QPainter painter(&sheet);
  int x = 0;
  for (const auto& shot : shots) {
    painter.drawPixmap(x, 0, shot);
    x += shot.width();
  }
  painter.end();
  sheet.save(filename, "PNG");
}

void DrawingViewDock::exportDrawing()
{
  bool any = false;
  for (const auto& v : this->views) {
    if (!v.image.isNull()) any = true;
  }
  if (!any) return;

  const double denom = this->scaleSpin->value();
  const double scale = denom > 1e-6 ? 1.0 / denom : 1.0;  // model-mm -> drawing-mm

  const QString filename = QFileDialog::getSaveFileName(
    this, tr("Export drawing"), "drawing.svg", tr("SVG (*.svg);;PDF (*.pdf)"));
  if (filename.isEmpty()) return;

  const double dpi = 96.0;
  const double mmToPx = dpi / 25.4;
  const double marginMm = 10.0, gapMm = 8.0, titleMm = 5.0;
  const double fontPt = 9.0;

  // Vertical layout: title + view, stacked. Sizes computed in mm.
  QVector<double> viewYmm, viewWmm, viewHmm;
  double sheetWmm = 0.0, sheetHmm = marginMm;
  for (const auto& v : this->views) {
    if (v.image.isNull()) {
      viewYmm << 0; viewWmm << 0; viewHmm << 0;
      continue;
    }
    const double vw = v.image.width() / v.pxPerMm * scale;
    const double vh = v.image.height() / v.pxPerMm * scale;
    sheetHmm += titleMm;
    viewYmm << sheetHmm;
    viewWmm << vw;
    viewHmm << vh;
    sheetHmm += vh + gapMm;
    sheetWmm = std::max(sheetWmm, vw);
  }
  sheetWmm += 2 * marginMm;
  sheetHmm += marginMm;
  const int sheetWpx = qRound(sheetWmm * mmToPx);
  const int sheetHpx = qRound(sheetHmm * mmToPx);

  auto paintSheet = [&](QPainter& p) {
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(QRectF(0, 0, sheetWpx, sheetHpx), Qt::white);
    QFont font = p.font();
    font.setPointSizeF(fontPt);
    p.setFont(font);

    for (int i = 0; i < this->views.size(); ++i) {
      const auto& v = this->views[i];
      if (v.image.isNull()) continue;
      const double ox = marginMm * mmToPx;
      const double oy = viewYmm[i] * mmToPx;

      p.setPen(Qt::black);
      p.drawText(QPointF(ox, oy - 2.0), v.title);
      p.drawImage(QRectF(ox, oy, viewWmm[i] * mmToPx, viewHmm[i] * mmToPx), v.image);

      // image-px -> sheet-px (same factor the raster is drawn at)
      const double k = (1.0 / v.pxPerMm) * scale * mmToPx;
      auto map = [&](const QPointF& ip) { return QPointF(ox + ip.x() * k, oy + ip.y() * k); };

      QPen dimPen(Qt::black);
      dimPen.setWidthF(std::max(0.5, 0.3 * mmToPx));
      for (const auto& d : v.dims) {
        QPointF dir = d.b - d.a;
        const double len = std::hypot(dir.x(), dir.y());
        if (len < 1e-6) continue;
        dir /= len;
        const QPointF perp(dir.y(), -dir.x());
        const QPointF a = map(d.a + perp * d.offsetPx);
        const QPointF b = map(d.b + perp * d.offsetPx);
        p.setPen(dimPen);
        p.drawLine(a, b);
        if (std::abs(d.offsetPx) > 0.5) {
          QPen wit(QColor(120, 120, 120));
          wit.setWidthF(0.5);
          p.setPen(wit);
          p.drawLine(map(d.a), a);
          p.drawLine(map(d.b), b);
          p.setPen(dimPen);
        }
        // Arrowheads (fixed size, in px).
        QPointF sdir = b - a;
        const double sl = std::hypot(sdir.x(), sdir.y());
        if (sl > 1e-6) {
          sdir /= sl;
          const QPointF nrm(-sdir.y(), sdir.x());
          const double s = 3.5 * mmToPx * 0.35;
          auto arrow = [&](const QPointF& tip, const QPointF& along) {
            QPolygonF h;
            h << tip << (tip + along * s + nrm * s * 0.4) << (tip + along * s - nrm * s * 0.4);
            p.setBrush(Qt::black);
            p.setPen(Qt::NoPen);
            p.drawPolygon(h);
            p.setPen(dimPen);
          };
          if (d.arrowsOut) { arrow(a, -sdir); arrow(b, sdir); }
          else { arrow(a, sdir); arrow(b, -sdir); }
        }
        // Label at the offset-line midpoint plus its own offset (fixed font).
        const QPointF midImg = (d.a + d.b) / 2.0 + perp * d.offsetPx +
                               dir * d.labAlongPx + perp * d.labPerpPx;
        const QPointF lp = map(midImg);
        const QRectF tr = p.fontMetrics().boundingRect(d.label).adjusted(-2, -1, 2, 1);
        QRectF box(lp.x() - tr.width() / 2, lp.y() - tr.height() / 2, tr.width(), tr.height());
        p.fillRect(box, QColor(255, 255, 255, 220));
        p.setPen(QColor(150, 30, 30));
        p.drawText(box, Qt::AlignCenter, d.label);
      }
    }
  };

  if (filename.endsWith(".pdf", Qt::CaseInsensitive)) {
    QPdfWriter pdf(filename);
    pdf.setResolution(static_cast<int>(dpi));
    pdf.setPageSize(QPageSize(QSizeF(sheetWmm, sheetHmm), QPageSize::Millimeter));
    pdf.setPageMargins(QMarginsF(0, 0, 0, 0));
    QPainter p(&pdf);
    paintSheet(p);
  } else {
    QSvgGenerator svg;
    svg.setFileName(filename);
    svg.setResolution(static_cast<int>(dpi));
    svg.setSize(QSize(sheetWpx, sheetHpx));
    svg.setViewBox(QRect(0, 0, sheetWpx, sheetHpx));
    QPainter p(&svg);
    paintSheet(p);
  }
}
