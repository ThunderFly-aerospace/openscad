#include "glview/CsgInfo.h"

#include <memory>
#include <vector>

#include "core/CSGNode.h"
#include "core/CSGTreeEvaluator.h"
#include "core/Tree.h"
#include "geometry/GeometryEvaluator.h"
#include "glview/RenderSettings.h"
#include "glview/preview/CSGTreeNormalizer.h"
#include "utils/printutils.h"

bool CsgInfo::compile_products(const Tree& tree, bool showSlicerModifiers)
{
  auto& root_node = tree.root();
  GeometryEvaluator geomevaluator(tree);
  CSGTreeEvaluator evaluator(tree, &geomevaluator);
  evaluator.setShowSlicerModifiers(showSlicerModifiers);
  const std::shared_ptr<CSGNode> csgRoot = evaluator.buildCSGTree(*root_node);
  std::vector<std::shared_ptr<CSGNode>> highlightNodes = evaluator.getHighlightNodes();
  std::vector<std::shared_ptr<CSGNode>> backgroundNodes = evaluator.getBackgroundNodes();
  const std::vector<std::shared_ptr<CSGNode>>& regionNodes = evaluator.getSlicerRegionNodes();

  LOG("Compiling design (CSG Products normalization)...");
  CSGTreeNormalizer normalizer(RenderSettings::inst()->openCSGTermLimit);
  if (csgRoot) {
    const std::shared_ptr<CSGNode> normalizedRoot = normalizer.normalize(csgRoot);
    if (normalizedRoot) {
      this->root_products = std::make_shared<CSGProducts>();
      this->root_products->import(normalizedRoot);
      LOG("Normalized CSG tree has %1$d elements", int(this->root_products->size()));
    } else {
      this->root_products.reset();
      LOG(message_group::Warning, "CSG normalization resulted in an empty tree");
    }
  }

  if (highlightNodes.size() > 0) {
    LOG("Compiling highlights (%1$i CSG Trees)...", highlightNodes.size());
    this->highlights_products = std::make_shared<CSGProducts>();
    for (auto& highlightNode : highlightNodes) {
      highlightNode = normalizer.normalize(highlightNode);
      this->highlights_products->import(highlightNode);
    }
  }

  if (backgroundNodes.size() > 0) {
    LOG("Compiling background (%1$i CSG Trees)...", backgroundNodes.size());
    this->background_products = std::make_shared<CSGProducts>();
    for (auto& backgroundNode : backgroundNodes) {
      backgroundNode = normalizer.normalize(backgroundNode);
      this->background_products->import(backgroundNode);
    }
  }

  this->slicer_region_products = compileSlicerRegions(regionNodes, normalizer);
  return true;
}

std::vector<std::shared_ptr<CSGProducts>> CsgInfo::compileSlicerRegions(
  const std::vector<std::shared_ptr<CSGNode>>& regionNodes, CSGTreeNormalizer& normalizer)
{
  std::vector<std::shared_ptr<CSGProducts>> regions;
  for (const auto& node : regionNodes) {
    if (auto normalized = normalizer.normalize(node)) {
      auto products = std::make_shared<CSGProducts>();
      products->import(normalized);
      regions.push_back(std::move(products));
    }
  }
  return regions;
}
