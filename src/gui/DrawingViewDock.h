#pragma once

#include <QImage>
#include <QList>
#include <QStringList>
#include <QVector>

#include "gui/Dock.h"

class QLabel;
class QPushButton;
class QResizeEvent;

/*!
   Dock that shows orthographic views of the model (front / side / top =
   nárys / bokorys / půdorys), laid out like a technical drawing sheet. It is a
   passive viewer: MainWindow renders the images (after each preview) and hands
   them here via setViews(). Phase 2A is view-only; dimension overlays (Phase 2B)
   will be drawn on top of these views later.
 */
class DrawingViewDock : public Dock
{
  Q_OBJECT

public:
  DrawingViewDock(QWidget *parent = nullptr);
  ~DrawingViewDock() override;

  void setViews(const QList<QImage>& images, const QStringList& titles);
  void clearViews();

protected:
  void resizeEvent(QResizeEvent *event) override;

private slots:
  void saveSheet();

private:
  void rescale();

  QVector<QLabel *> viewLabels;
  QLabel *placeholder;
  QPushButton *saveButton;
  QList<QImage> images;
};
