#pragma once

#include <limits>
#include <string>
#include <utility>

#include "core/BaseVisitable.h"
#include "core/CurveDiscretizer.h"
#include "core/ModuleInstantiation.h"
#include "core/node.h"

/*!
   A bend() node: bends its children around an axis parallel to Y.

   The sheet is expected to lie in the XY plane along +X, with the neutral
   fibre at z = 0. Material at x < at stays put, material in the band
   [at, at + r*|angle|] is wrapped onto a circular arc of radius r, and
   everything beyond the band follows as a rigid body. A positive angle
   curves towards +Z. Any other orientation is obtained by wrapping the node
   in a rotate().

   The optional xlim/ylim/zlim ranges restrict which material takes part in
   the bend at all: a vertex outside any of them is left exactly where it is.
   That is what makes several bends on different tabs of the same board
   independent of each other.
 */
class BendNode : public AbstractNode
{
public:
  VISITABLE();
  BendNode(const ModuleInstantiation *mi, CurveDiscretizer discretizer)
    : AbstractNode(mi), discretizer(std::move(discretizer))
  {
  }
  std::string toString() const override;
  std::string name() const override { return "bend"; }

  static constexpr double INF = std::numeric_limits<double>::infinity();

  double at{0.0};      // x of the bend line
  double r{1.0};       // radius of the neutral fibre, > 0
  double angle{0.0};   // bend angle in degrees, positive curves towards +Z

  // Region mask, evaluated in the node's input coordinates. Infinite bounds
  // mean "unrestricted"; undef on either side of a range keeps that bound.
  double xmin{-INF}, xmax{INF};
  double ymin{-INF}, ymax{INF};
  double zmin{-INF}, zmax{INF};

  CurveDiscretizer discretizer;
};
