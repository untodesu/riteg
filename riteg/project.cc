#include "riteg/pch.hh"

#include "riteg/project.hh"

#include "riteg/blank.hh"
#include "riteg/cmdline.hh"
#include "riteg/graph.hh"
#include "riteg/image.hh"
#include "riteg/node.hh"
#include "riteg/program.hh"
#include "riteg/render_target.hh"
#include "riteg/timings.hh"

namespace
{
std::unique_ptr<Graph> s_graph;
std::unique_ptr<Image> s_image;
std::unique_ptr<Blank> s_blank;
lua_State* s_lua_state = nullptr;
bool s_script_failed = false;
} // namespace

namespace
{
struct LuaProgram final {
    Program* program;
};

struct LuaSource final {
    Source* source;
    Node* node;
};

LuaProgram* check_program(lua_State* L, int index)
{
    return static_cast<LuaProgram*>(luaL_checkudata(L, index, "Riteg.Program"));
}

LuaSource* check_source(lua_State* L, int index)
{
    return static_cast<LuaSource*>(luaL_checkudata(L, index, "Riteg.Source"));
}

void push_program(lua_State* L, Program* program)
{
    auto userdata = static_cast<LuaProgram*>(lua_newuserdatauv(L, sizeof(LuaProgram), 0));
    userdata->program = program;
    luaL_setmetatable(L, "Riteg.Program");
}

void push_source(lua_State* L, Source* source, Node* node)
{
    auto userdata = static_cast<LuaSource*>(lua_newuserdatauv(L, sizeof(LuaSource), 0));
    userdata->source = source;
    userdata->node = node;
    luaL_setmetatable(L, "Riteg.Source");
}

Node* require_node(lua_State* L, int index)
{
    auto source = check_source(L, index);

    if(!source->node)
        luaL_error(L, "source is not a shader node");

    return source->node;
}

CompileOptions current_compile_options(void)
{
    CompileOptions options;
    options.reuse_targets = !cmdline::contains("no-target-reuse");
    return options;
}

TextureFormat parse_texture_format(lua_State* L, const char* value)
{
    if(0 == std::strcmp(value, "rgba16f"))
        return TextureFormat::RGBA16F;

    if(0 == std::strcmp(value, "rgba8"))
        return TextureFormat::RGBA8;

    if(0 == std::strcmp(value, "r16f"))
        return TextureFormat::R16F;

    if(0 == std::strcmp(value, "rg16f"))
        return TextureFormat::RG16F;

    luaL_error(L, "unknown texture format: %s", value);
    return TextureFormat::RGBA16F;
}

TextureFilter parse_texture_filter(lua_State* L, const char* value)
{
    if(0 == std::strcmp(value, "linear"))
        return TextureFilter::Linear;

    if(0 == std::strcmp(value, "nearest"))
        return TextureFilter::Nearest;

    luaL_error(L, "unknown texture filter: %s", value);
    return TextureFilter::Linear;
}

TextureWrap parse_texture_wrap(lua_State* L, const char* value)
{
    if(0 == std::strcmp(value, "clamp"))
        return TextureWrap::Clamp;

    if(0 == std::strcmp(value, "repeat"))
        return TextureWrap::Repeat;

    if(0 == std::strcmp(value, "mirror"))
        return TextureWrap::Mirror;

    luaL_error(L, "unknown texture wrap: %s", value);
    return TextureWrap::Clamp;
}

NodeOptions parse_node_options(lua_State* L, int index)
{
    NodeOptions options;

    if(lua_isnoneornil(L, index))
        return options;

    if(!lua_istable(L, index))
        luaL_error(L, "node options must be a table");

    lua_pushnil(L);

    while(lua_next(L, index)) {
        auto key = luaL_checkstring(L, -2);

        if(0 == std::strcmp(key, "name")) {
            options.name = luaL_checkstring(L, -1);
        }
        else if(0 == std::strcmp(key, "format")) {
            options.format = parse_texture_format(L, luaL_checkstring(L, -1));
        }
        else if(0 == std::strcmp(key, "filter")) {
            options.filter = parse_texture_filter(L, luaL_checkstring(L, -1));
        }
        else if(0 == std::strcmp(key, "wrap")) {
            options.wrap = parse_texture_wrap(L, luaL_checkstring(L, -1));
        }
        else {
            luaL_error(L, "unknown node option: %s", key);
        }

        lua_pop(L, 1);
    }

    return options;
}

int lua_gc_noop(lua_State* L)
{
    (void)L;
    return 0;
}

int source_tostring(lua_State* L)
{
    auto source = check_source(L, 1);
    std::string result = "Riteg.Source";

    if(source->node) {
        result = std::format("Riteg.Node(\"{}\", {}x{})", source->node->get_name(), source->node->get_texture_width(),
            source->node->get_texture_height());
    }

    lua_pushlstring(L, result.c_str(), result.size());
    return 1;
}
} // namespace

namespace
{
int ritegAPI_has_option(lua_State* L)
{
    auto option = luaL_checkstring(L, 1);

    riteg_force_assert(option != nullptr);

    if(cmdline::contains(option)) {
        lua_pushboolean(L, true);
    }
    else {
        lua_pushboolean(L, false);
    }

    return 1;
}

int ritegAPI_get_option_number(lua_State* L)
{
    auto option = luaL_checkstring(L, 1);
    auto fallback = luaL_optnumber(L, 2, 0.0);
    auto value = cmdline::get(option);

    if(value.size()) {
        lua_Number parsed_value;
        auto result = std::from_chars(value.data(), value.data() + value.size(), parsed_value);

        if(result.ec == std::errc()) {
            lua_pushnumber(L, parsed_value);
        }
        else {
            lua_pushnumber(L, fallback);
        }
    }
    else {
        lua_pushnumber(L, fallback);
    }

    return 1;
}

int ritegAPI_get_option_string(lua_State* L)
{
    auto option = luaL_checkstring(L, 1);
    auto fallback = luaL_checkstring(L, 2);

    riteg_force_assert(option != nullptr);
    riteg_force_assert(fallback != nullptr);

    lua_pushstring(L, cmdline::get_cstr(option, fallback));

    return 1;
}

int ritegAPI_get_blank_source(lua_State* L)
{
    push_source(L, s_blank.get(), nullptr);
    return 1;
}

int ritegAPI_get_image_source(lua_State* L)
{
    push_source(L, s_image.get(), nullptr);
    return 1;
}

int ritegAPI_program(lua_State* L)
{
    auto source = luaL_checkstring(L, 1);
    std::string name;

    if(lua_istable(L, 2)) {
        lua_getfield(L, 2, "name");

        if(lua_isstring(L, -1))
            name = lua_tostring(L, -1);

        lua_pop(L, 1);
    }

    std::string error;
    auto program = s_graph->get_program(source, error);

    if(!program)
        return luaL_error(L, "program: %s", error.c_str());

    if(!name.empty())
        program->set_name(name);

    push_program(L, program);
    return 1;
}

int ritegAPI_node(lua_State* L)
{
    auto program = check_program(L, 1);
    auto width = static_cast<int>(luaL_checkinteger(L, 2));
    auto height = static_cast<int>(luaL_checkinteger(L, 3));

    if(width <= 0 || height <= 0)
        return luaL_error(L, "node size must be positive");

    GLint max_texture_size = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);

    if(width > max_texture_size || height > max_texture_size)
        return luaL_error(L, "node size exceeds GL_MAX_TEXTURE_SIZE");

    auto options = parse_node_options(L, 4);
    auto node = s_graph->create_node(program->program, width, height, options);

    push_source(L, node, node);
    return 1;
}

int source_input(lua_State* L)
{
    auto self = check_source(L, 1);

    if(!self->node)
        return luaL_error(L, "input: source is not a shader node");

    auto channel = static_cast<int>(luaL_checkinteger(L, 2));

    if(channel < 0 || channel >= Node::MAX_CHANNELS)
        return luaL_error(L, "input: channel must be in range 0..3");

    Source* input = nullptr;

    if(!lua_isnil(L, 3))
        input = check_source(L, 3)->source;

    s_graph->connect(self->node, channel, input);
    return 0;
}

int source_set_number(lua_State* L)
{
    auto node = require_node(L, 1);
    auto name = luaL_checkstring(L, 2);

    UniformValue value;
    value.kind = UniformValue::Kind::Float;
    value.values[0] = static_cast<float>(luaL_checknumber(L, 3));

    node->set_uniform(name, value);
    return 0;
}

int source_set_int(lua_State* L)
{
    auto node = require_node(L, 1);
    auto name = luaL_checkstring(L, 2);

    UniformValue value;
    value.kind = UniformValue::Kind::Int;
    value.integer = static_cast<int>(luaL_checkinteger(L, 3));

    node->set_uniform(name, value);
    return 0;
}

int source_set_vector(lua_State* L, UniformValue::Kind kind, int count)
{
    auto node = require_node(L, 1);
    auto name = luaL_checkstring(L, 2);

    UniformValue value;
    value.kind = kind;

    for(int i = 0; i < count; ++i)
        value.values[i] = static_cast<float>(luaL_checknumber(L, 3 + i));

    node->set_uniform(name, value);
    return 0;
}

int source_set_vec2(lua_State* L)
{
    return source_set_vector(L, UniformValue::Kind::Vec2, 2);
}

int source_set_vec3(lua_State* L)
{
    return source_set_vector(L, UniformValue::Kind::Vec3, 3);
}

int source_set_vec4(lua_State* L)
{
    return source_set_vector(L, UniformValue::Kind::Vec4, 4);
}

int source_prev(lua_State* L)
{
    auto node = require_node(L, 1);
    push_source(L, node->get_prev(), nullptr);
    return 1;
}

int ritegAPI_output(lua_State* L)
{
    auto source = check_source(L, 1);

    if(!source->node)
        return luaL_error(L, "output: source is not a shader node");

    std::string error;

    if(!s_graph->set_output(source->source, error))
        return luaL_error(L, "%s", error.c_str());

    return 0;
}

int ritegAPI_display(lua_State* L)
{
    auto source = check_source(L, 1);

    if(!source->node)
        return luaL_error(L, "display: source is not a shader node");

    std::string error;

    if(!s_graph->set_display(source->source, error))
        return luaL_error(L, "%s", error.c_str());

    return 0;
}

int ritegAPI_bake(lua_State* L)
{
    std::string error;

    if(!s_graph->compile(current_compile_options(), error))
        return luaL_error(L, "%s", error.c_str());

    lua_pushboolean(L, true);
    return 1;
}

int ritegAPI_plan_dump(lua_State* L)
{
    std::string error;

    if(!s_graph->compile(current_compile_options(), error))
        return luaL_error(L, "%s", error.c_str());

    auto dump = s_graph->dump_plan();
    lua_pushlstring(L, dump.c_str(), dump.size());
    return 1;
}

int ritegAPI_log_info(lua_State* L)
{
    logging::detail::info(std::source_location::current(), lua_tostring(L, 1));
    return 0;
}

int ritegAPI_log_warning(lua_State* L)
{
    logging::detail::warning(std::source_location::current(), lua_tostring(L, 1));
    return 0;
}

int ritegAPI_log_error(lua_State* L)
{
    logging::detail::error(std::source_location::current(), lua_tostring(L, 1));
    return 0;
}

int ritegAPI_log_critical(lua_State* L)
{
    logging::detail::critical(std::source_location::current(), lua_tostring(L, 1));
    return 0;
}
} // namespace

namespace
{
const luaL_Reg SOURCE_METHODS[] = {
    { "input", &source_input },
    { "set_number", &source_set_number },
    { "set_int", &source_set_int },
    { "set_vec2", &source_set_vec2 },
    { "set_vec3", &source_set_vec3 },
    { "set_vec4", &source_set_vec4 },
    { "prev", &source_prev },
    { nullptr, nullptr },
};
} // namespace

void project::init(void)
{
    s_graph = std::make_unique<Graph>();
    s_image = std::make_unique<Image>();
    s_blank = std::make_unique<Blank>();
    s_script_failed = false;

    static const luaL_Reg riteg_api_functions[] = {
        { "has_option", &ritegAPI_has_option },
        { "get_option_number", &ritegAPI_get_option_number },
        { "get_option_string", &ritegAPI_get_option_string },
        { "get_blank_source", &ritegAPI_get_blank_source },
        { "get_image_source", &ritegAPI_get_image_source },
        { "program", &ritegAPI_program },
        { "node", &ritegAPI_node },
        { "output", &ritegAPI_output },
        { "display", &ritegAPI_display },
        { "bake", &ritegAPI_bake },
        { "plan_dump", &ritegAPI_plan_dump },
        { "log_info", &ritegAPI_log_info },
        { "log_warning", &ritegAPI_log_warning },
        { "log_error", &ritegAPI_log_error },
        { "log_critical", &ritegAPI_log_critical },
        { nullptr, nullptr },
    };

    s_lua_state = luaL_newstate();

    assert(s_lua_state != nullptr);

    luaL_openlibs(s_lua_state);

    luaL_newmetatable(s_lua_state, "Riteg.Program");
    lua_pushvalue(s_lua_state, -1);
    lua_setfield(s_lua_state, -2, "__index");
    lua_pushcfunction(s_lua_state, &lua_gc_noop);
    lua_setfield(s_lua_state, -2, "__gc");
    lua_pop(s_lua_state, 1);

    luaL_newmetatable(s_lua_state, "Riteg.Source");
    lua_pushvalue(s_lua_state, -1);
    lua_setfield(s_lua_state, -2, "__index");
    lua_pushcfunction(s_lua_state, &lua_gc_noop);
    lua_setfield(s_lua_state, -2, "__gc");
    lua_pushcfunction(s_lua_state, &source_tostring);
    lua_setfield(s_lua_state, -2, "__tostring");
    luaL_setfuncs(s_lua_state, SOURCE_METHODS, 0);
    lua_pop(s_lua_state, 1);

    luaL_newlibtable(s_lua_state, riteg_api_functions);
    luaL_setfuncs(s_lua_state, riteg_api_functions, 0);
    lua_setglobal(s_lua_state, "riteg");
}

void project::deinit(void)
{
    if(s_lua_state) {
        lua_close(s_lua_state);
        s_lua_state = nullptr;
    }

    s_graph.reset();
    s_image.reset();
    s_blank.reset();
    s_script_failed = false;
}

bool project::finalize(void)
{
    if(s_script_failed || !s_graph)
        return false;

    std::string error;

    if(!s_graph->compile(current_compile_options(), error)) {
        LOG_CRITICAL("{}", error);
        return false;
    }

    if(cmdline::contains("dump-plan"))
        LOG_INFO("{}", s_graph->dump_plan());

    return true;
}

void project::render(const Timings& timings)
{
    if(s_graph)
        s_graph->render(timings);
}

bool project::load_input_RGBA(const std::filesystem::path& path)
{
    assert(!path.empty());
    assert(s_image != nullptr);

    return s_image->load_RGBA(path);
}

void project::run_lua_script(const std::filesystem::path& filename)
{
    assert(s_lua_state != nullptr);

    if(luaL_dofile(s_lua_state, filename.string().c_str()) != LUA_OK) {
        LOG_CRITICAL("lua error: {}", lua_tostring(s_lua_state, -1));
        lua_pop(s_lua_state, 1);
        s_script_failed = true;
    }
}

const Source* project::get_display_source(void)
{
    if(!s_graph)
        return nullptr;

    if(auto display = s_graph->get_display())
        return display;

    return s_graph->get_output();
}

const Source* project::get_output_source(void)
{
    return s_graph ? s_graph->get_output() : nullptr;
}
