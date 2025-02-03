#include "riteg/precompiled.hh"
#include "riteg/project.hh"

#include "riteg/cmdline.hh"
#include "riteg/debug_out.hh"
#include "riteg/shader_pass.hh"
#include "riteg/timings.hh"

std::filesystem::path project::workdir;
std::filesystem::path project::projdir;

std::string project::about;
std::string project::author;

int project::input_mode = LOADSAVE_DISABLED;
long project::input_maxframes = 0;
std::string project::input_filename;
BasicImage project::input_image;

int project::output_mode = LOADSAVE_DISABLED;
int project::output_filetype = SAVE_FILETYPE_INV;
long project::output_maxframes = 0;
int project::output_jpeg_quality = 95;
std::string project::output_filename;
BasicImage project::output_image;
GLuint project::output_frame;

ShaderPass *project::final_pass;
std::unordered_map<std::string, ShaderPass*> project::passes_map;
std::vector<ShaderPass*> project::passes;

static std::vector<std::byte> cached_output_pixels;

static std::string resolve_path(const std::string &path)
{
    if(auto semicolon = std::strchr(path.c_str(), ':')) {
        auto substring = path.substr(0, std::size_t(semicolon - path.c_str()));
        auto subpath = std::string(semicolon + 1);

        if(substring == "cwd")
            return std::filesystem::path(project::workdir / subpath).string();
        if(substring == "project")
            return std::filesystem::path(project::projdir / subpath).string();
        return std::filesystem::path(subpath).string();
    }

    return std::filesystem::path(path).string();
}

static void guess_output_filetype(const std::string &extension)
{
    if(extension == ".png")
        project::output_filetype = SAVE_FILETYPE_PNG;
    else if(extension == ".jpg")
        project::output_filetype = SAVE_FILETYPE_JPG;
    else if(extension == ".jpeg")
        project::output_filetype = SAVE_FILETYPE_JPG;
    else if(extension == ".tga")
        project::output_filetype = SAVE_FILETYPE_TGA;
    else throw std::invalid_argument("cannot guess output_filetype");
}

static void parse_project_table(const toml_table_t *table, std::string &final_pass_name)
{
    toml_datum_t datum;

    if((datum = toml_string_in(table, "about")).ok) {
        project::about = datum.u.s;
        std::free(datum.u.s);
    }

    if((datum = toml_string_in(table, "author")).ok) {
        project::author = datum.u.s;
        std::free(datum.u.s);
    }

    if((datum = toml_string_in(table, "input_mode")).ok) {
        if(!std::strcmp(datum.u.s, "disabled"))
            project::input_mode = LOADSAVE_DISABLED;
        else if(!std::strcmp(datum.u.s, "oneshot"))
            project::input_mode = LOADSAVE_ONESHOT;
        else if(!std::strcmp(datum.u.s, "sprintf"))
            project::input_mode = LOADSAVE_SPRINTF;
        else throw std::invalid_argument("invalid input_mode value");
        std::free(datum.u.s);
    }

    if((datum = toml_string_in(table, "input_maxframes")).ok) {
        if(!std::strcmp(datum.u.s, "probe"))
            project::input_maxframes = LOAD_MAXFRAMES_PROBE;
        else if(!std::strcmp(datum.u.s, "guess"))
            project::input_maxframes = LOAD_MAXFRAMES_PROBE;
        else throw std::invalid_argument("invalid input_maxframes value [0]");
        std::free(datum.u.s);
    }
    else if((datum = toml_int_in(table, "input_maxframes")).ok) {
        if(datum.u.i < 0)
            throw std::invalid_argument("invalid input_maxframes value [1]");
        project::input_maxframes = datum.u.i;
    }

    if(project::input_mode == LOADSAVE_ONESHOT) {
        if((datum = toml_string_in(table, "input_path")).ok) {
            project::input_filename = resolve_path(datum.u.s);
            std::free(datum.u.s);
        }
    }
    else if(project::input_mode == LOADSAVE_SPRINTF) {
        if((datum = toml_string_in(table, "input_format")).ok) {
            project::input_filename = resolve_path(datum.u.s);
            std::free(datum.u.s);
        }
    }

    if((datum = toml_string_in(table, "output_mode")).ok) {
        if(!std::strcmp(datum.u.s, "disabled"))
            project::output_mode = LOADSAVE_DISABLED;
        else if(!std::strcmp(datum.u.s, "oneshot"))
            project::output_mode = LOADSAVE_ONESHOT;
        else if(!std::strcmp(datum.u.s, "sprintf"))
            project::output_mode = LOADSAVE_SPRINTF;
        else throw std::invalid_argument("invalid output_mode value");
        std::free(datum.u.s);
    }

    if((datum = toml_string_in(table, "output_maxframes")).ok) {
        if(!std::strcmp(datum.u.s, "input"))
            project::output_maxframes = SAVE_MAXFRAMES_INPUT;
        else throw std::invalid_argument("invalid output_maxframes value [0]");
        std::free(datum.u.s);
    }
    else if((datum = toml_int_in(table, "output_maxframes")).ok) {
        if(datum.u.i < 0)
            throw std::invalid_argument("invalid output_maxframes value [1]");
        project::output_maxframes = datum.u.i;
    }

    if(project::output_mode == LOADSAVE_ONESHOT) {
        if((datum = toml_string_in(table, "output_path")).ok) {
            project::output_filename = resolve_path(datum.u.s);
            std::free(datum.u.s);
        }
    }
    else if(project::output_mode == LOADSAVE_SPRINTF) {
        if((datum = toml_string_in(table, "output_format")).ok) {
            project::output_filename = resolve_path(datum.u.s);
            std::free(datum.u.s);
        }
    }

    if((datum = toml_string_in(table, "output_filetype")).ok) {
        if(!std::strcmp(datum.u.s, "guess"))
            guess_output_filetype(std::filesystem::path(project::output_filename).extension().string());
        else guess_output_filetype(std::string(".") + std::string(datum.u.s));
        std::free(datum.u.s);
    }

    if((datum = toml_int_in(table, "output_jpeg_quality")).ok) {
        project::output_jpeg_quality = datum.u.i;
        if(project::output_jpeg_quality < 1) project::output_jpeg_quality = 95;
        if(project::output_jpeg_quality > 100) project::output_jpeg_quality = 95;
    }

    if(auto array = toml_array_in(table, "output_resolution")) {
        int width = 0;
        int height = 0;

        if((datum = toml_int_at(array, 0)).ok) {
            width = datum.u.i;
        }

        if((datum = toml_int_at(array, 1)).ok) {
            height = datum.u.i;
        }

        if(width <= 0 || height <= 0) {
            throw std::runtime_error("invalid output_resolution");
        }

        project::output_image.resolution[0] = width;
        project::output_image.resolution[1] = height;
        project::output_image.resolution[2] = project::output_image.resolution[0] / project::output_image.resolution[1];
    }

    if((datum = toml_string_in(table, "final_pass")).ok) {
        final_pass_name.assign(datum.u.s);
        std::free(datum.u.s);
    }
}

static bool do_load_image(const Timings &timings)
{
    static char load_filename[4096];

    if(project::input_mode == LOADSAVE_DISABLED) {
        // We either started off disabled or
        // reached the stopping condition last frame
        return false;
    }

    switch(project::input_mode) {
        case LOADSAVE_ONESHOT:
            std::strncpy(load_filename, project::input_filename.c_str(), sizeof(load_filename));
            break;
        case LOADSAVE_SPRINTF:
            stbsp_snprintf(load_filename, sizeof(load_filename), project::input_filename.c_str(), timings.frame_number);
            break;
        default:
            debug_out << "disabled input_mode fell through";
            return false;
    }

    if(project::input_mode == LOADSAVE_SPRINTF) {
        if(project::input_maxframes != LOAD_MAXFRAMES_PROBE && timings.frame_number >= project::input_maxframes) {
            // We're not in the probe mode and this frame
            // is going to be the last, so we set load mode
            // to disabled; won't be loading new images next frame
            project::input_mode = LOADSAVE_DISABLED;
        }
    }

    if(project::input_mode == LOADSAVE_ONESHOT) {
        // One-shot mode - we're not supposed to load
        // new images after we loaded a single one
        project::input_mode = LOADSAVE_DISABLED;
    }

    stbi_set_flip_vertically_on_load(true);

    int width, height;
    auto pixels = stbi_load(load_filename, &width, &height, nullptr, STBI_rgb_alpha);

    if(pixels == nullptr) {
        if(project::input_maxframes == LOAD_MAXFRAMES_PROBE) {
            // We're in a probing mode; a single failed
            // load means we're done reading input frames
            project::input_mode = LOADSAVE_DISABLED;
        }

        debug_out << load_filename << ": " << stbi_failure_reason();
        return false;
    }

    project::input_image.resolution[0] = width;
    project::input_image.resolution[1] = width;
    project::input_image.resolution[2] = project::input_image.resolution[0] / project::input_image.resolution[1];

    if(project::input_image.texture == 0)
        glGenTextures(1, &project::input_image.texture);
    glBindTexture(GL_TEXTURE_2D, project::input_image.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(pixels);

    return true;
}

static void do_save_image(const Timings &timings, bool load_succeeded)
{
    static char save_filename[4096];

    if(project::output_mode == LOADSAVE_DISABLED) {
        // We either started off disabled or
        // reached the stopping condition last frame
        return;
    }

    if(project::output_maxframes == SAVE_MAXFRAMES_INPUT && !load_succeeded) {
        // We're saving as much frames as the loader figures
        // out, so when it fails we should fail as well
        project::output_mode = LOADSAVE_DISABLED;
        return;
    }

    switch(project::output_mode) {
        case LOADSAVE_ONESHOT:
            std::strncpy(save_filename, project::output_filename.c_str(), sizeof(save_filename));
            break;
        case LOADSAVE_SPRINTF:
            stbsp_snprintf(save_filename, sizeof(save_filename), project::output_filename.c_str(), timings.frame_number);
            break;
        default:
            debug_out << "disabled output_mode fell through";
            return;
    }

    stbi_flip_vertically_on_write(true);

    int width = project::output_image.resolution[0];
    int height = project::output_image.resolution[1];
    cached_output_pixels.resize(4 * width * height);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, project::output_frame);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, cached_output_pixels.data());

    bool success = false;

    switch(project::output_filetype) {
        case SAVE_FILETYPE_PNG:
            success = stbi_write_png(save_filename, width, height, 4, cached_output_pixels.data(), 4 * width);
            break;
        case SAVE_FILETYPE_JPG:
            success = stbi_write_jpg(save_filename, width, height, 4, cached_output_pixels.data(), project::output_jpeg_quality);
            break;
        case SAVE_FILETYPE_TGA:
            success = stbi_write_tga(save_filename, width, height, 4, cached_output_pixels.data());
            break;
    }

    if(!success) {
        debug_out << save_filename << ": failed";
        if(project::output_maxframes != SAVE_MAXFRAMES_INPUT) {
            project::output_mode = LOADSAVE_DISABLED;
        }
    }

    if(timings.frame_number >= project::output_maxframes) {
        if(project::output_maxframes != SAVE_MAXFRAMES_INPUT) {
            project::output_mode = LOADSAVE_DISABLED;
            return;
        }
    }
}

void project::init(const char *project_path, const char *preset)
{
    toml_datum_t datum;

    project::workdir = std::filesystem::current_path();
    project::projdir = std::filesystem::current_path() / project_path;

    auto toml_filename = project::projdir / "project.toml";
    auto toml_file = std::ifstream(toml_filename);

    if(toml_file.fail()) {
        throw std::system_error(errno, std::system_category(), toml_filename.string());
    }

    char toml_error[4096];
    auto toml_source = std::string(std::istreambuf_iterator<char>(toml_file), std::istreambuf_iterator<char>());
    auto toml_root = toml_parse(toml_source.data(), toml_error, sizeof(toml_error));

    if(toml_root == nullptr) {
        throw std::runtime_error(toml_error);
    }

    auto project_table = toml_table_in(toml_root, "project");

    if(project_table == nullptr) {
        throw std::runtime_error("cannot locate [project]");
    }

    auto project_base = toml_table_in(project_table, "base");

    if(project_base == nullptr) {
        throw std::runtime_error("cannot locate [project.base]");
    }

    std::string final_pass_name;
    parse_project_table(project_base, final_pass_name);

    if(preset) {
        auto preset_table = toml_table_in(project_table, preset);

        if(preset_table == nullptr) {
            throw std::runtime_error("cannot locate [project.preset]");
        }

        parse_project_table(preset_table, final_pass_name);
    }

    if(auto scale_string = cmdline::get("scale")) {
        auto scale_float = std::strtof(scale_string, nullptr);

        if(scale_float >= 0.25f && scale_float <= 5.0f) {
            project::output_image.resolution[0] *= scale_float;
            project::output_image.resolution[1] *= scale_float;
        }
    }

    if(project::output_image.resolution[0] <= 0.0f || project::output_image.resolution[1] <= 0.0f) {
        throw std::runtime_error("invalid output_resolution value");
    }

    glGenTextures(1, &project::output_image.texture);
    glBindTexture(GL_TEXTURE_2D, project::output_image.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, project::output_image.resolution[0], project::output_image.resolution[1], 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glGenFramebuffers(1, &project::output_frame);
    glBindFramebuffer(GL_FRAMEBUFFER, project::output_frame);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, project::output_image.texture, 0);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("framebuffer status check failed");
    }

    if(final_pass_name.empty()) {
        throw std::runtime_error("final_pass value is empty or missing");
    }

    if(project::output_mode != LOADSAVE_DISABLED && project::output_filetype == SAVE_FILETYPE_INV) {
        throw std::runtime_error("output is not disabled but filetype has not been set correctly");
    }

    auto passes_array = toml_array_in(toml_root, "shader_pass");

    if(passes_array == nullptr) {
        throw std::runtime_error("cannot locate [[shader_pass]] array");
    }

    for(int i = 0;; ++i) {
        auto pass_table = toml_table_at(passes_array, i);

        if(pass_table == nullptr) {
            debug_out << "parsed " << i << " shader passes";
            break;
        }

        auto pass_name = toml_string_in(pass_table, "name");
        auto pass_shader = toml_string_in(pass_table, "shader");

        if(!pass_name.ok) {
            throw std::runtime_error("invalid or missing shader_pass.name value");
        }

        if(!pass_shader.ok) {
            throw std::runtime_error("invalid or missing shader_pass.shader value");
        }

        if(pass_name.u.s[0] == '!') {
            throw std::runtime_error("pass names prefixed with '!' are reserved for internal use");
        }

        if(project::passes_map.find(pass_name.u.s) != project::passes_map.cend()) {
            throw std::runtime_error("duplicate shader passes are not allowed");
        }

        auto pass_width = project::output_image.resolution[0];
        auto pass_height = project::output_image.resolution[1];

        if(auto pass_resolution = toml_array_in(pass_table, "resolution")) {
            auto pass_wide_d = toml_int_at(pass_resolution, 0);
            auto pass_tall_d = toml_int_at(pass_resolution, 1);

            if(pass_wide_d.ok && pass_wide_d.u.i > 0 && pass_tall_d.ok && pass_tall_d.u.i > 0) {
                pass_width = pass_wide_d.u.i;
                pass_height = pass_tall_d.u.i;
            }
        }

        if(auto pass_scale = toml_array_in(pass_table, "scale")) {
            auto pass_sx_d = toml_double_at(pass_scale, 0);
            auto pass_sy_d = toml_double_at(pass_scale, 1);

            if(pass_sx_d.ok && pass_sx_d.u.d > 0.0 && pass_sy_d.ok && pass_sy_d.u.d > 0.0) {
                pass_width *= pass_sx_d.u.d;
                pass_height *= pass_sy_d.u.d;
            }
        }

        pass_width = std::ceilf(pass_width);
        pass_height = std::ceilf(pass_height);

        ShaderPass *pass = new ShaderPass;
        ShaderPass::create(pass, pass_name.u.s, pass_width, pass_height, resolve_path(pass_shader.u.s).c_str());

        std::free(pass_shader.u.s);
        std::free(pass_name.u.s);

        if(auto pass_parameters = toml_array_in(pass_table, "parameters")) {
            for(int j = 0; j < MAX_SHADER_PARAMETERS; ++j) {
                if((datum = toml_int_at(pass_parameters, j)).ok) {
                    pass->parameters[j] = datum.u.i;
                }
                else if((datum = toml_double_at(pass_parameters, j)).ok) {
                    pass->parameters[j] = datum.u.d;
                }
                else break;
            }
        }

        if(auto pass_channels = toml_array_in(pass_table, "channels")) {
            for(int j = 0; j < MAX_SHADER_CHANNELS; ++j) {
                if((datum = toml_string_at(pass_channels, j)).ok) {
                    if(!std::strcmp(datum.u.s, "!image")) {
                        pass->channels[j] = &project::input_image;
                        std::free(datum.u.s);
                        continue;
                    }

                    auto it = project::passes_map.find(datum.u.s);

                    if(it == project::passes_map.cend()) {
                        throw std::runtime_error("invalid or missing channel input");
                    }

                    pass->channels[j] = &it->second->output_image;
                    std::free(datum.u.s);
                    continue;
                }
                else break;
            }
        }
    
        project::passes_map.insert_or_assign(pass->name, pass);
        project::passes.push_back(pass);
    }

    toml_free(toml_root);

    auto final_it = project::passes_map.find(final_pass_name);

    if(final_it == project::passes_map.cend()) {
        throw std::runtime_error("shader pass specified in final_pass is missing or invalid");
    }

    project::final_pass = final_it->second;

    project::input_image.resolution[0] = 0.0f;
    project::input_image.resolution[1] = 0.0f;
    project::input_image.resolution[2] = 0.0f;
    project::input_image.texture = 0;
}

void project::deinit(void)
{
    for(ShaderPass *pass : project::passes)
        delete pass;
    project::passes_map.clear();
    project::passes.clear();

    glDeleteFramebuffers(1, &project::output_frame);
    glDeleteTextures(1, &project::input_image.texture);
    glDeleteTextures(1, &project::output_image.texture);

    project::final_pass = nullptr;
}

void project::render(GLFWwindow *window, const Timings &timings)
{
    auto load_succeeded = do_load_image(timings);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    for(const ShaderPass *pass : project::passes) {
        ShaderPass::render(pass, timings);
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, project::final_pass->framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, project::output_frame);
    glBlitFramebuffer(0, 0, project::final_pass->output_image.resolution[0], project::final_pass->output_image.resolution[1], 0, 0, project::output_image.resolution[0], project::output_image.resolution[1], GL_COLOR_BUFFER_BIT, GL_LINEAR);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glViewport(0, 0, width, height);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, project::final_pass->framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, project::final_pass->output_image.resolution[0], project::final_pass->output_image.resolution[1], 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    do_save_image(timings, load_succeeded);
}
