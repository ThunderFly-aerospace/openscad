#pragma once

#include <memory>
#include <vector>

#include "core/CSGNode.h"
#include "core/Tree.h"

/*
   Small helper class for compiling and normalizing node trees into CSG products
 */
class CsgInfo
{
public:
  CsgInfo() = default;
  std::shared_ptr<class CSGProducts> root_products;
  std::shared_ptr<CSGProducts> highlights_products;
  std::shared_ptr<CSGProducts> background_products;
  // One product set per slicer_modifier() region (model * region, recolored),
  // drawn on top of the model, see OpenCSGRenderer::setSlicerRegionProducts.
  std::vector<std::shared_ptr<CSGProducts>> slicer_region_products;

  // showSlicerModifiers: color the slicer_modifier() regions (default) or leave
  // them out.
  bool compile_products(const Tree& tree, bool showSlicerModifiers = true);

  // Normalize the region terms of CSGTreeEvaluator::getSlicerRegionNodes().
  static std::vector<std::shared_ptr<CSGProducts>> compileSlicerRegions(
    const std::vector<std::shared_ptr<CSGNode>>& regionNodes, class CSGTreeNormalizer& normalizer);
};
