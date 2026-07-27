#include "core/RenderVariables.h"

#include "core/BuiltinContext.h"
#include "core/Context.h"
#include "core/Value.h"

void RenderVariables::applyToContext(ContextHandle<BuiltinContext>& context) const
{
  context->set_variable("$preview", preview);
  context->set_variable("$t", time);

  VectorType view(context->session());
  for (const auto& name : viewPath) view.emplace_back(Value(name.c_str()));
  context->set_variable("$view", Value(std::move(view)));

  const auto vpr = camera.getVpr();
  context->set_variable("$vpr", VectorType(context->session(), vpr.x(), vpr.y(), vpr.z()));
  const auto vpt = camera.getVpt();
  context->set_variable("$vpt", VectorType(context->session(), vpt.x(), vpt.y(), vpt.z()));
  const auto vpd = camera.zoomValue();
  context->set_variable("$vpd", vpd);
  const auto vpf = camera.fovValue();
  context->set_variable("$vpf", vpf);
}
