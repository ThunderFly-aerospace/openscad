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
#include "gui/FlowLayout.h"

#include <QMargins>
#include <QSizePolicy>
#include <QStyle>
#include <QWidget>
#include <utility>

FlowLayout::FlowLayout(QWidget *parent, int margin, int hSpacing, int vSpacing)
  : QLayout(parent), hSpace(hSpacing), vSpace(vSpacing)
{
  setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::FlowLayout(int margin, int hSpacing, int vSpacing)
  : hSpace(hSpacing), vSpace(vSpacing)
{
  setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::~FlowLayout()
{
  while (QLayoutItem *item = takeAt(0)) {
    delete item;
  }
}

void FlowLayout::addItem(QLayoutItem *item)
{
  itemList.append(item);
}

int FlowLayout::count() const
{
  return itemList.size();
}

QLayoutItem *FlowLayout::itemAt(int index) const
{
  return itemList.value(index);
}

QLayoutItem *FlowLayout::takeAt(int index)
{
  if (index < 0 || index >= itemList.size()) {
    return nullptr;
  }
  return itemList.takeAt(index);
}

int FlowLayout::horizontalSpacing() const
{
  return hSpace >= 0 ? hSpace : smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
}

int FlowLayout::verticalSpacing() const
{
  return vSpace >= 0 ? vSpace : smartSpacing(QStyle::PM_LayoutVerticalSpacing);
}

Qt::Orientations FlowLayout::expandingDirections() const
{
  return {};
}

bool FlowLayout::hasHeightForWidth() const
{
  return true;
}

int FlowLayout::heightForWidth(int width) const
{
  return doLayout(QRect(0, 0, width, 0), true);
}

QSize FlowLayout::minimumSize() const
{
  // The whole point of wrapping: one item wide is narrow enough. The height
  // that goes with it is heightForWidth()'s business, so only the widest
  // item's height enters here.
  QSize size;
  for (const QLayoutItem *item : std::as_const(itemList)) {
    size = size.expandedTo(item->minimumSize());
  }
  const QMargins m = contentsMargins();
  return size + QSize(m.left() + m.right(), m.top() + m.bottom());
}

QSize FlowLayout::sizeHint() const
{
  // Preferred = everything on one line, so a dock that has the room looks
  // exactly like the QHBoxLayout it replaced.
  QSize size;
  int width = 0;
  bool first = true;
  for (const QLayoutItem *item : std::as_const(itemList)) {
    if (item->isEmpty()) {
      continue;
    }
    const QSize hint = item->sizeHint();
    width += hint.width() + (first ? 0 : horizontalSpacing());
    size = size.expandedTo(QSize(0, hint.height()));
    first = false;
  }
  size.setWidth(width);
  const QMargins m = contentsMargins();
  return size + QSize(m.left() + m.right(), m.top() + m.bottom());
}

void FlowLayout::setGeometry(const QRect& rect)
{
  QLayout::setGeometry(rect);
  doLayout(rect, false);
}

int FlowLayout::doLayout(const QRect& rect, bool testOnly) const
{
  int left, top, right, bottom;
  getContentsMargins(&left, &top, &right, &bottom);
  const QRect effectiveRect = rect.adjusted(+left, +top, -right, -bottom);
  int x = effectiveRect.x();
  int y = effectiveRect.y();
  int lineHeight = 0;

  for (QLayoutItem *item : std::as_const(itemList)) {
    if (item->isEmpty()) {  // hidden widgets take no room and no spacing
      continue;
    }
    const QSize hint = item->sizeHint();
    int nextX = x + hint.width() + horizontalSpacing();
    if (nextX - horizontalSpacing() > effectiveRect.right() && lineHeight > 0) {
      x = effectiveRect.x();
      y = y + lineHeight + verticalSpacing();
      nextX = x + hint.width() + horizontalSpacing();
      lineHeight = 0;
    }

    if (!testOnly) {
      // Clamp to the available width so a single item wider than the dock is
      // squeezed rather than drawn outside it.
      const int w = qMin(hint.width(), effectiveRect.width());
      item->setGeometry(QRect(QPoint(x, y), QSize(w, hint.height())));
    }

    x = nextX;
    lineHeight = qMax(lineHeight, hint.height());
  }
  return y + lineHeight - rect.y() + bottom;
}

int FlowLayout::smartSpacing(int pixelMetric) const
{
  QObject *parent = this->parent();
  if (!parent) {
    return -1;
  }
  if (parent->isWidgetType()) {
    auto *pw = static_cast<QWidget *>(parent);
    return pw->style()->pixelMetric(static_cast<QStyle::PixelMetric>(pixelMetric), nullptr, pw);
  }
  return static_cast<QLayout *>(parent)->spacing();
}
