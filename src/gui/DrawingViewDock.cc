#include "gui/DrawingViewDock.h"

#include <algorithm>
#include <cmath>

#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

// ---------------------------------------------------------------------------
// OrthoViewWidget
// ---------------------------------------------------------------------------

OrthoViewWidget::OrthoViewWidget(QWidget *parent) : QWidget(parent)
{
  setMinimumSize(120, 90);
}

void OrthoViewWidget::setData(const OrthoViewData& newData)
{
  this->data = newData;
  update();
}

void OrthoViewWidget::clear()
{
  this->data = OrthoViewData{};
  update();
}

QPointF OrthoViewWidget::mapToWidget(const QPointF& imgPt) const
{
  return QPointF(imgPt.x() * this->fitScale + this->fitOffset.x(),
                 imgPt.y() * this->fitScale + this->fitOffset.y());
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

  // Dimension lines.
  QPen dimPen(QColor(20, 20, 20));
  dimPen.setWidthF(1.4);
  for (const auto& dim : this->data.dims) {
    const QPointF a = mapToWidget(dim.a);
    const QPointF b = mapToWidget(dim.b);
    painter.setPen(dimPen);
    painter.drawLine(a, b);

    // Small perpendicular ticks at both ends.
    QPointF dir = b - a;
    const double len = std::hypot(dir.x(), dir.y());
    if (len > 1e-6) {
      dir /= len;
      const QPointF perp(-dir.y(), dir.x());
      const double t = 5.0;
      painter.drawLine(a - perp * t, a + perp * t);
      painter.drawLine(b - perp * t, b + perp * t);
    }

    // Label at the midpoint, on a light background for readability.
    const QPointF mid = (a + b) / 2.0;
    const QString label = dim.label;
    const QRectF textRect = painter.fontMetrics().boundingRect(label).adjusted(-3, -2, 3, 2);
    QRectF box(mid.x() - textRect.width() / 2.0, mid.y() - textRect.height() / 2.0,
               textRect.width(), textRect.height());
    painter.fillRect(box, QColor(255, 255, 255, 210));
    painter.setPen(QColor(150, 30, 30));
    painter.drawText(box, Qt::AlignCenter, label);
  }

  // Anchor dots with names.
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
  double best = 12.0;  // pixel hit radius
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
  }
  grid->setRowStretch(0, 1);
  grid->setRowStretch(1, 1);
  grid->setColumnStretch(0, 1);
  grid->setColumnStretch(1, 1);

  this->placeholder = new QLabel(tr("Render (F5) to generate orthographic views."), central);
  this->placeholder->setAlignment(Qt::AlignCenter);

  this->saveButton = new QPushButton(tr("Save Sheet…"), central);
  this->saveButton->setEnabled(false);

  layout->addWidget(sheet, 1);
  layout->addWidget(this->placeholder);
  layout->addWidget(this->saveButton);
  setWidget(central);

  connect(this->saveButton, &QPushButton::clicked, this, &DrawingViewDock::saveSheet);
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
