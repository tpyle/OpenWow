#pragma once

#include <string>
#include <vector>

struct lua_State;

namespace openwow::ui::framexml {
struct UiFrame;
}

namespace openwow::ui::game::frame_api {

struct TemplateResolveResult {
  std::vector<const openwow::ui::framexml::UiFrame*> base_to_derived;
  std::string failed_name;
  bool recursive{false};

  [[nodiscard]] bool ok() const noexcept { return failed_name.empty(); }
};

[[nodiscard]] TemplateResolveResult ResolveTemplateNodes(
    lua_State* lua, const char* inherits, const char* expected_kind);
/// Pushes the inherited-template failure message for `result` onto the Lua
/// stack. The caller raises it with lua_error() once every C++ object that
/// owns resources (including `result`) has gone out of scope, since
/// lua_error() unwinds with longjmp and skips destructors.
void PushInheritedTemplateError(lua_State* lua, int owner_index,
                                const char* method_name,
                                const TemplateResolveResult& result);

}
