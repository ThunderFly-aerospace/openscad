#include <string>
#include <vector>

#include "glview/Camera.h"
class BuiltinContext;
template <typename T>
class ContextHandle;

class RenderVariables
{
public:
  bool preview;
  double time;
  Camera camera;
  // Active view path for view() (Fáze 15): the chain of view names from the root
  // down to the selected node. Empty = root (only geometry outside any view()
  // shows). Set $view to this so view() blocks re-instantiate accordingly.
  std::vector<std::string> viewPath;
  void applyToContext(ContextHandle<BuiltinContext>& context) const;
};
