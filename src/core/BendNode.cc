/*
 *  OpenSCAD (www.openscad.org)
 *  Copyright (C) 2009-2011 Clifford Wolf <clifford@clifford.at> and
 *                          Marius Kintel <marius@kintel.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  As a special exception, you have permission to link this program
 *  with the CGAL library and distribute executables, as long as you
 *  follow the requirements of the GNU GPL in regard to all of the
 *  software in the executable aside from CGAL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "core/BendNode.h"

#include <cmath>
#include <cstddef>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "core/Builtins.h"
#include "core/Children.h"
#include "core/CurveDiscretizer.h"
#include "core/ModuleInstantiation.h"
#include "core/Parameters.h"
#include "core/module.h"
#include "utils/printutils.h"

namespace {

/*!
   Reads an optional [lo, hi] range parameter. Either entry may be undef, which
   leaves that side unbounded; a missing parameter leaves both sides unbounded.
   Returns false (and warns) if the value is present but not a usable range.
 */
bool read_limit(const Parameters& parameters, const ModuleInstantiation *inst, const char *name,
                double& lo, double& hi)
{
  const Value& value = parameters[name];
  if (value.type() == Value::Type::UNDEFINED) return true;
  if (value.type() != Value::Type::VECTOR || value.toVector().size() != 2) {
    LOG(message_group::Warning, inst->location(), parameters.documentRoot(),
        "bend() expects %1$s = [lo, hi], ignoring", name);
    return false;
  }
  const auto& vec = value.toVector();
  if (vec[0].type() == Value::Type::NUMBER) lo = vec[0].toDouble();
  if (vec[1].type() == Value::Type::NUMBER) hi = vec[1].toDouble();
  if (lo > hi) {
    LOG(message_group::Warning, inst->location(), parameters.documentRoot(),
        "bend() got an empty %1$s range, nothing will be bent", name);
  }
  return true;
}

void write_limit(std::ostringstream& stream, const char *name, double lo, double hi)
{
  if (std::isinf(lo) && std::isinf(hi)) return;
  stream << ", " << name << " = [";
  if (std::isinf(lo)) stream << "undef";
  else stream << lo;
  stream << ", ";
  if (std::isinf(hi)) stream << "undef";
  else stream << hi;
  stream << "]";
}

}  // namespace

static std::shared_ptr<AbstractNode> builtin_bend(const ModuleInstantiation *inst,
                                                  Arguments arguments, const Children& children)
{
  Parameters parameters = Parameters::parse(std::move(arguments), inst->location(),
                                            {"at", "r", "angle"},
                                            {"xlim", "ylim", "zlim", "glue"});

  auto node = std::make_shared<BendNode>(inst, CurveDiscretizer(parameters, inst->location()));

  parameters.validate_number("at", node->at);
  parameters.validate_number("r", node->r);
  parameters.validate_number("angle", node->angle);
  parameters.validate_number("glue", node->glue);
  if (node->glue < 0.0) {
    LOG(message_group::Warning, inst->location(), parameters.documentRoot(),
        "bend() expects glue >= 0, ignoring");
    node->glue = 0.0;
  }

  read_limit(parameters, inst, "xlim", node->xmin, node->xmax);
  read_limit(parameters, inst, "ylim", node->ymin, node->ymax);
  read_limit(parameters, inst, "zlim", node->zmin, node->zmax);

  return children.instantiate(node);
}

std::string BendNode::toString() const
{
  std::ostringstream stream;
  stream << "bend(" << this->discretizer << ", at = " << this->at << ", r = " << this->r
         << ", angle = " << this->angle;
  write_limit(stream, "xlim", this->xmin, this->xmax);
  write_limit(stream, "ylim", this->ymin, this->ymax);
  write_limit(stream, "zlim", this->zmin, this->zmax);
  // Only when in use, so that models which never asked for it keep their dump.
  if (this->glue > 0.0) stream << ", glue = " << this->glue;
  stream << ")";
  return stream.str();
}

void register_builtin_bend()
{
  Builtins::init("bend", new BuiltinModule(builtin_bend),
                 {
                   "bend(at = 0, r = 1, angle = 90)",
                   "bend(at = 0, r = 1, angle = 90, xlim = [lo, hi], ylim = [lo, hi], zlim = [lo, "
                   "hi])",
                   "bend(at = 0, r = 1, angle = 90, glue = 0)",
                 });
}
