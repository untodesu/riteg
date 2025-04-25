#include "riteg/pch.hh"
#include "riteg/project.hh"

#include "riteg/blank.hh"
#include "riteg/cmdline.hh"
#include "riteg/image.hh"
#include "riteg/shader.hh"
#include "riteg/timings.hh"

static std::vector<Source*> s_sources;
static Source *s_display_source = nullptr;
static Source *s_output_source = nullptr;
static lua_State *s_lua_state = nullptr;

static int ritegAPI_has_option(lua_State *L)
{
    auto option = luaL_checkstring(L, 1);

    riteg_force_assert(option != nullptr);

    if(cmdline::contains(option))
        lua_pushboolean(L, true);
    else lua_pushboolean(L, false);

    return 1;
}

static int ritegAPI_get_option_number(lua_State *L)
{
    auto option = luaL_checkstring(L, 1);
    auto fallback = luaL_optnumber(L, 2, 0.0);

    if(auto value = cmdline::get(option))
        lua_pushnumber(L, std::strtod(value, nullptr));
    else lua_pushnumber(L, fallback);

    return 1;
}

static int ritegAPI_get_option_string(lua_State *L)
{
    auto option = luaL_checkstring(L, 1);
    auto fallback = luaL_checkstring(L, 2);

    riteg_force_assert(option != nullptr);
    riteg_force_assert(fallback != nullptr);

    lua_pushstring(L, cmdline::get(option, fallback));

    return 1;
}

static int ritegAPI_get_blank_source(lua_State *L)
{
    assert(s_sources.size() > PROJECT_BLANK_SOURCE_ID);
    lua_pushinteger(L, PROJECT_BLANK_SOURCE_ID);
    return 1;
}

static int ritegAPI_get_image_source(lua_State *L)
{
    assert(s_sources.size() > PROJECT_IMAGE_SOURCE_ID);
    lua_pushinteger(L, PROJECT_IMAGE_SOURCE_ID);
    return 1;
}

static int ritegAPI_create_shader(lua_State *L)
{
    auto width = static_cast<int>(luaL_checknumber(L, 1));
    auto height = static_cast<int>(luaL_checknumber(L, 2));
    auto source = luaL_checkstring(L, 3);

    riteg_force_assert(width > 0);
    riteg_force_assert(height > 0);
    riteg_force_assert(source != nullptr);

    auto shader = new Shader(width, height, source);

    // There can be an additional argument which is a table
    // that contains shadertoy-styled channel input assignments.
    // It would look somewhat like this: { iChannel0 = previous_pass }
    if(lua_istable(L, 4)) {
        lua_gettable(L, 4);

        // For some reason lua_getfield refuses to work
        // in this specific case, so instead we iterate through
        // the table and cherry-pick key-value pairs we can handle
        // https://stackoverflow.com/questions/6137684/iterate-through-lua-table

        lua_pushnil(L);

        while(lua_next(L, -2)) {
            lua_pushvalue(L, -2);

            auto key = luaL_checkstring(L, -1);
            riteg_force_assert(key != nullptr);

            int channel_index;

            if(1 == std::sscanf(key, "iChannel%d", &channel_index)) {
                riteg_force_assert(channel_index >= 0);
                riteg_force_assert(channel_index < Shader::MAX_CHANNELS);

                auto channel_source_id = static_cast<int>(luaL_checknumber(L, -2));
                riteg_force_assert(channel_source_id >= 0);
                riteg_force_assert(channel_source_id < s_sources.size());

                auto channel_source = s_sources[channel_source_id];
                riteg_force_assert(channel_source != nullptr);

                shader->setChannel(channel_index, channel_source);

                lua_pop(L, 2);
                continue;
            }

            lua_pop(L, 2);
            continue;
        }
    }

    s_sources.push_back(shader);

    lua_pushinteger(L, static_cast<lua_Integer>(s_sources.size() - 1));

    return 1;
}

static int ritegAPI_set_display_source(lua_State *L)
{
    auto source_id = static_cast<int>(luaL_checknumber(L, 1));
    riteg_force_assert(source_id >= 0);
    riteg_force_assert(source_id < s_sources.size());

    s_display_source = s_sources[source_id];

    return 0;
}

static int ritegAPI_set_output_source(lua_State *L)
{
    auto source_id = static_cast<int>(luaL_checknumber(L, 1));
    riteg_force_assert(source_id >= 0);
    riteg_force_assert(source_id < s_sources.size());

    s_output_source = s_sources[source_id];

    return 0;
}

void project::init(void)
{
    s_sources.clear();
    s_sources.push_back(new Blank()); // PROJECT_BLANK_SOURCE_ID
    s_sources.push_back(new Image()); // PROJECT_IMAGE_SOURCE_ID
    s_display_source = nullptr;
    s_output_source = nullptr;

    static const luaL_Reg riteg_api_functions[] = {
        { "has_option",         &ritegAPI_has_option            },
        { "get_option_number",  &ritegAPI_get_option_number     },
        { "get_option_string",  &ritegAPI_get_option_string     },
        { "get_blank_source",   &ritegAPI_get_blank_source      },
        { "get_image_source",   &ritegAPI_get_image_source      },
        { "create_shader",      &ritegAPI_create_shader         },
        { "set_display_source", &ritegAPI_set_display_source    },
        { "set_output_source",  &ritegAPI_set_output_source     },
        { nullptr,              nullptr                         },
    };

    s_lua_state = luaL_newstate();

    assert(s_lua_state != nullptr);

    luaL_openlibs(s_lua_state);

    luaL_newlibtable(s_lua_state, riteg_api_functions);
    luaL_setfuncs(s_lua_state, riteg_api_functions, 0);
    lua_setglobal(s_lua_state, "riteg");
}

void project::deinit(void)
{
    for(auto source : s_sources)
        delete source;
    lua_close(s_lua_state);
    s_display_source = nullptr;
    s_output_source = nullptr;
    s_lua_state = nullptr;
    s_sources.clear();
}

void project::render(const Timings &timings)
{
    for(auto source : s_sources) {
        assert(source != nullptr);

        source->render(timings);
    }
}

bool project::load_input_RGBA(const std::filesystem::path &path)
{
    assert(!path.empty());
    assert(s_sources.size() > PROJECT_IMAGE_SOURCE_ID);

    // FIXME: we can probably use dynamic_cast here
    // but I really doubt it's worth it in this specific case
    auto image = static_cast<Image*>(s_sources[PROJECT_IMAGE_SOURCE_ID]);

    assert(image != nullptr);

    return image->load_RGBA(path);
}

void project::run_lua_script(const char *filename)
{
    assert(s_lua_state != nullptr);

    assert(filename != nullptr);

    if(luaL_dofile(s_lua_state, filename) != LUA_OK) {
        riteg_fatal << "Lua error: " << lua_tostring(s_lua_state, -1) << std::endl;
        std::terminate();
    }
}

const Source *project::get_source(int id)
{
    if(id < 0 || id >= s_sources.size())
        return nullptr;
    return s_sources[id];
}

const Source *project::get_display_source(void)
{
    return s_display_source ? s_display_source : s_output_source;
}

const Source *project::get_output_source(void)
{
    return s_output_source;
}
