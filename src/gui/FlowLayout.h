/*
 *  OpenSCAD (www.openscad.org)
 *  Copyright The OpenSCAD Developers.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, see
 *  <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <QLayout>
#include <QLayoutItem>
#include <QList>
#include <QRect>
#include <QSize>
#include <QWidget>

/**
 * A horizontal layout that wraps to the next line when it runs out of width.
 * Unlike QHBoxLayout, whose minimum width is the sum of all its items, this
 * layout's minimum width is that of its *widest single item* — which is what
 * lets a dock full of buttons and checkboxes be narrowed down without either
 * clipping the controls or hiding them behind an overflow menu.
 *
 * Items keep their sizeHint width (no stretching); this is not ColorLayout,
 * which lays its swatches out in equal-width columns.
 */
class FlowLayout : public QLayout
{
public:
  explicit FlowLayout(QWidget *parent, int margin = -1, int hSpacing = -1, int vSpacing = -1);
  explicit FlowLayout(int margin = -1, int hSpacing = -1, int vSpacing = -1);
  ~FlowLayout() override;

  int horizontalSpacing() const;
  int verticalSpacing() const;

  void addItem(QLayoutItem *item) override;
  int count() const override;
  QLayoutItem *itemAt(int index) const override;
  QLayoutItem *takeAt(int index) override;

  Qt::Orientations expandingDirections() const override;
  bool hasHeightForWidth() const override;
  int heightForWidth(int width) const override;
  QSize minimumSize() const override;
  QSize sizeHint() const override;
  void setGeometry(const QRect& rect) override;

private:
  // Places the items, returning the total height needed. testOnly skips the
  // actual geometry updates, which is what heightForWidth() needs.
  int doLayout(const QRect& rect, bool testOnly) const;
  // Falls back to the style's layout spacing when the spacing was left at -1.
  int smartSpacing(int pixelMetric) const;

  QList<QLayoutItem *> itemList;
  int hSpace;
  int vSpace;
};
