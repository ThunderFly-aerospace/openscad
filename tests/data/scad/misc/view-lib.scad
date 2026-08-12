// Helper for view-scopes.scad: a view() inside a module of a use<>d file. The
// enumeration behind --default-model-views has to resolve the module like the
// runtime does (local scopes first, then use<>d files) to find it.
module lib_part() {
  view(name = "libview", model_export = true) cube(7);
}
