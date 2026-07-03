#include "gui/DrawingViewDock.h"

#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QWidget>

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
    auto *label = new QLabel(box);
    label->setAlignment(Qt::AlignCenter);
    label->setMinimumSize(120, 90);
    label->setText(tr("(no preview)"));
    boxLayout->addWidget(label);
    grid->addWidget(box, kGridPos[i][0], kGridPos[i][1]);
    this->viewLabels.append(label);
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

void DrawingViewDock::setViews(const QList<QImage>& newImages, const QStringList& titles)
{
  this->images = newImages;

  for (int i = 0; i < this->viewLabels.size(); ++i) {
    if (i < titles.size() && !titles.at(i).isEmpty()) {
      // The label lives inside a QGroupBox; update the box title.
      if (auto *box = qobject_cast<QGroupBox *>(this->viewLabels.at(i)->parentWidget())) {
        box->setTitle(titles.at(i));
      }
    }
  }

  const bool any = !this->images.isEmpty();
  this->saveButton->setEnabled(any);
  this->placeholder->setVisible(!any);
  rescale();
}

void DrawingViewDock::clearViews()
{
  setViews({}, {});
}

void DrawingViewDock::rescale()
{
  for (int i = 0; i < this->viewLabels.size(); ++i) {
    QLabel *label = this->viewLabels.at(i);
    if (i >= this->images.size() || this->images.at(i).isNull()) {
      label->setText(tr("(no preview)"));
      continue;
    }
    label->setPixmap(QPixmap::fromImage(this->images.at(i))
                       .scaled(label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
}

void DrawingViewDock::resizeEvent(QResizeEvent *event)
{
  Dock::resizeEvent(event);
  rescale();
}

void DrawingViewDock::saveSheet()
{
  if (this->images.isEmpty()) return;
  const QString filename = QFileDialog::getSaveFileName(
    this, tr("Save Drawing Sheet"), "drawing.png", tr("PNG image (*.png)"));
  if (filename.isEmpty()) return;

  // Compose the three views side by side at their native resolution.
  int totalW = 0, maxH = 0;
  for (const auto& img : this->images) {
    if (img.isNull()) continue;
    totalW += img.width();
    maxH = std::max(maxH, img.height());
  }
  if (totalW == 0 || maxH == 0) return;
  QImage sheet(totalW, maxH, QImage::Format_ARGB32);
  sheet.fill(Qt::white);
  QPainter painter(&sheet);
  int x = 0;
  for (const auto& img : this->images) {
    if (img.isNull()) continue;
    painter.drawImage(x, 0, img);
    x += img.width();
  }
  painter.end();
  sheet.save(filename, "PNG");
}
