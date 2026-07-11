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

#include "core/LabelNode.h"

#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "core/Builtins.h"
#include "core/Children.h"
#include "core/ModuleInstantiation.h"
#include "core/Parameters.h"
#include "core/TransformNode.h"
#include "core/module.h"
#include "geometry/linalg.h"

static std::shared_ptr<AbstractNode> builtin_label(const ModuleInstantiation *inst,
                                                   Arguments arguments, const Children& children)
{
  auto node = std::make_shared<LabelNode>(inst);

  Parameters parameters =
    Parameters::parse(std::move(arguments), inst->location(), {"name"}, {"text"});

  node->label_name = parameters.get("name", node->label_name);
  // Default the shown text to the name when text= is omitted.
  node->text = parameters.get("text", node->label_name);

  return children.instantiate(node);
}

std::string LabelNode::toString() const
{
  std::ostringstream stream;
  stream << "label(name = \"" << this->label_name << "\"";
  if (this->text != this->label_name) {
    stream << ", text = \"" << this->text << "\"";
  }
  stream << ")";
  return stream.str();
}

std::string LabelNode::name() const
{
  return "label";
}

static void collectLabelsRecursive(const AbstractNode& node, const Transform3d& parent,
                                   std::vector<LabelInfo>& out)
{
  Transform3d current = parent;
  if (const auto *transform = dynamic_cast<const TransformNode *>(&node)) {
    current = parent * transform->matrix;
  }
  if (const auto *label = dynamic_cast<const LabelNode *>(&node)) {
    out.push_back({current, label});
  }
  for (const auto& child : node.getChildren()) {
    if (child) collectLabelsRecursive(*child, current, out);
  }
}

std::vector<LabelInfo> collectLabels(const std::shared_ptr<const AbstractNode>& root)
{
  std::vector<LabelInfo> labels;
  if (root) {
    collectLabelsRecursive(*root, Transform3d::Identity(), labels);
  }
  return labels;
}

void register_builtin_label()
{
  Builtins::init("label", new BuiltinModule(builtin_label),
                 {
                   "label(name = \"A\")",
                   "label(name = \"A\", text = \"Motor mount\")",
                 });
}
