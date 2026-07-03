#include "gui/DrawingViewDock.h"

#include <algorithm>
#include <cmath>

#include <QFileDialog>
#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
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
  QPointF dir = d.b - d.a;
  const double len = std::hypot(dir.x(), dir.y());
  if (len < 1e-9) return QPointF(0, -1);
  dir /= len;
  return QPointF(dir.y(), -dir.x());  // rotate -90°
}

QPointF OrthoViewWidget::dimLabelPos(const OrthoDimension& d) const
{
  // Image-space label anchor: midpoint of the offset dimension line, plus the
  // label's own offset along the line and perpendicular to it.
  const QPointF perp = dimPerp(d);
  QPointF dir = d.b - d.a;
  const double len = std::hypot(dir.x(), dir.y());
  if (len > 1e-9) dir /= len;
  else dir = QPointF(1, 0);
  const QPointF mid = (d.a + d.b) / 2.0 + perp * d.offsetPx;
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
    const QPointF off = perp * (dim.offsetPx * this->fitScale);
    const QPointF a2 = a + off;
    const QPointF b2 = b + off;

    // Witness (extension) lines when the dimension is pulled off the object.
    if (std::abs(dim.offsetPx) > 0.5) {
      painter.setPen(witnessPen);
      painter.drawLine(a, a2);
      painter.drawLine(b, b2);
    }

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
    const QPointF off = perp * (dim.offsetPx * this->fitScale);
    const QPointF a2 = mapToWidget(dim.a) + off;
    const QPointF b2 = mapToWidget(dim.b) + off;
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
  double best = 12.0;
  QString hit;
  for (const auto& anchor : this->data.anchors) {
    const QPointF p = mapToWidget(anchor.pos);
    const double d = std::hypot(p.x() - click.x(), p.y() - click.y());
    if (d < best) {
      best = d;
      hit = anchor.name;
    }
  }
  if (!hit.isEmpty()) emit anchorClicked(hit);
}

void OrthoViewWidget::mouseMoveEvent(QMouseEvent *event)
{
  // Label drag: decompose the mouse position (relative to the dimension-line
  // midpoint) into the along/perp frame of the dimension.
  if (this->draggingLabel >= 0 && this->draggingLabel < this->data.dims.size()) {
    OrthoDimension& dim = this->data.dims[this->draggingLabel];
    const QPointF perp = dimPerp(dim);
    QPointF dir = dim.b - dim.a;
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

  this->materializeButton = new QPushButton(tr("Materialize kvóty"), central);
  this->materializeButton->setToolTip(
    tr("Write a // @dim line for each kvota= pair, making them editable."));

  this->saveButton = new QPushButton(tr("Save Sheet…"), central);
  this->saveButton->setEnabled(false);

  auto *buttonRow = new QWidget(central);
  auto *buttonLayout = new QHBoxLayout(buttonRow);
  buttonLayout->setContentsMargins(0, 0, 0, 0);
  buttonLayout->addWidget(this->materializeButton);
  buttonLayout->addWidget(this->saveButton);

  layout->addWidget(sheet, 1);
  layout->addWidget(this->placeholder);
  layout->addWidget(this->showRefsCheck);
  layout->addWidget(buttonRow);
  setWidget(central);

  connect(this->saveButton, &QPushButton::clicked, this, &DrawingViewDock::saveSheet);
  connect(this->materializeButton, &QPushButton::clicked, this,
          &DrawingViewDock::materializeRequested);
  connect(this->showRefsCheck, &QCheckBox::toggled, this, &DrawingViewDock::setShowAnchors);
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
