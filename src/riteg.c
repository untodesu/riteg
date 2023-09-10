#define GLFW_INCLUDE_NONE
#include <errno.h>
#include <getopt.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <libgen.h>
#include <limits.h>
#include <parson.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef float r_uargs_t[8];
typedef float r_uparams_t[16];

struct r_texture {
    int width;
    int height;
    char ident[64];
    unsigned int handle;
    unsigned int framebuffer;
    unsigned char *pixels; /* RGBA8888 */
};

struct r_frame {
    int width;
    int height;
    unsigned char *pixels;
};

struct r_pass {
    unsigned int program;
    struct r_texture *output;
    struct r_texture **samplers;
    r_uparams_t params;
};

static const char *vert_source_minified =
    "#version 450 core\n"
    "layout(location=0)out vec2 uv;"
    "const vec2 verts[8]=vec2[8]("
    "vec2(-1.0,-1.0),vec2(+1.0,-1.0),vec2(+1.0,+1.0),vec2(-1.0,-1.0),"
    "vec2(+1.0,+1.0),vec2(-1.0,+1.0),vec2(-1.0,-1.0),vec2(+1.0,+1.0));"
    "void main(void){uv=vec2(0.5,0.5)+0.5*verts[gl_VertexID];"
    "gl_Position=vec4(verts[gl_VertexID],0.0,1.0);}\n";

static size_t num_passes;
static size_t num_textures;

static struct r_pass *passes;
static struct r_texture *textures;
static struct r_texture *blit_tex;

static struct r_texture image = {0};
static struct r_frame frame = {0};

static unsigned int vao = 0;
static unsigned int vert = 0;
static unsigned int uargs = 0;
static unsigned int uparams = 0;

static float curtime = 0.0f;
static float lasttime = 0.0f;
static float frametime = 0.0f;

static size_t kstrnlen(const char *restrict s, size_t n)
{
    size_t i;
    for(i = 0; *s++ && i < n; i++);
    return i;
}

static char *kstrncat(char *restrict s1, const char *restrict s2, size_t n)
{
    size_t nc;
    char *save = s1;
    while(*s1 && n--)
        s1++;
    nc = kstrnlen(s2, --n);
    s1[nc] = 0;
    memcpy(s1, s2, nc);
    return save;
}

static char *kstrncpy(char *restrict s1, const char *restrict s2, size_t n)
{
    char *save = s1;
    while(*s2 && n--)
        *s1++ = *s2++;
    if(n)
        *s1 = 0;
    return save;
}

static void info(const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc(0x0A, stderr);
    va_end(ap);
}

static void panic(const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc(0x0A, stderr);
    va_end(ap);
    abort();
}

static void *malloc_safe(size_t n)
{
    void *block = malloc(n);
    if(block == NULL)
        panic("malloc: %s", strerror(errno));
    memset(block, 0x00, n);
    return block;
}

static char *malloc_file(const char *restrict filename)
{
    char *data;
    size_t length;
    FILE *file = fopen(filename, "rb");

    if(file == NULL) {
        info("fopen: %s: %s", filename, strerror(errno));
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    length = ftell(file) + 1;
    fseek(file, 0, SEEK_SET);
    info("length=%lu", (unsigned long)length);

    data = malloc_safe(length);
    fread(data, 1, length, file);
    fclose(file);

    return data;
}

static unsigned int compile_shader(unsigned int stage, const char *source)
{
    int status;
    int length;
    char *info_log;
    unsigned int shader;

    shader = glCreateShader(stage);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

    if(length > 1) {
        info_log = malloc_safe(length + 1);
        glGetShaderInfoLog(shader, length, NULL, info_log);
        info("compile_shader: %s", info_log);
        info("compile_shader: %s", source);
        free(info_log);
    }

    if(!status)
        panic("compile_shader: compile failed");
    return shader;
}

static unsigned int make_program(unsigned int vert, unsigned int frag)
{
    int status;
    int length;
    char *info_log;
    unsigned int program;

    program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &status);
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

    if(length > 1) {
        info_log = malloc_safe(length + 1);
        glGetProgramInfoLog(program, length, NULL, info_log);
        info("make_program: %s", info_log);
        free(info_log);
    }

    if(!status)
        panic("make_program: link failed");
    return program;
}

static unsigned int make_pass_program(const char *restrict frag_filename)
{
    char *source = malloc_file(frag_filename);
    unsigned int frag;

    if(source == NULL)
        panic("make_pass_program: %s: %s", frag_filename, strerror(errno));
    frag = compile_shader(GL_FRAGMENT_SHADER, source);

    return make_program(vert, frag);
}

static struct r_texture *find_texture(const char *restrict ident)
{
    size_t i;

    if(ident) {
        for(i = 0; i < num_textures; ++i) {
            if(strcmp(ident, textures[i].ident))
                continue;
            return &textures[i];
        }
    }

    return NULL;
}

static void parse_textures(JSON_Object *root, const char *restrict filename)
{
    size_t i;
    JSON_Object *node = json_object_get_object(root, "textures");
    JSON_Object *object;
    JSON_Value *value;
    struct r_texture *texture;

    if(node == NULL) {
        /* Textures list should be an object */
        panic("parse_textures: %s: invalid type", filename);
    }

    num_textures = json_object_get_count(node);
    textures = malloc_safe(num_textures * sizeof(struct r_texture));

    for(i = 0; i < num_textures; ++i) {
        value = json_object_get_value_at(node, i);
        object = json_value_get_object(value);
        texture = &textures[i];

        if(!object) {
            /* Textures list is a list of objects */
            panic("parse_textures: %s: invalid entry format", filename);
        }

        kstrncpy(texture->ident, json_object_get_name(node, i), sizeof(texture->ident));

        if(!strcmp(texture->ident, "image")) {
            panic("parse_textures: %s: `image' is a reserved name", filename);
        }

        texture->width = json_object_get_number(object, "width");
        texture->height = json_object_get_number(object, "height");

        if(texture->width == 0 || texture->height == 0) {
            /* Textures with zero area/volume cannot exist */
            panic("parse_textures: %s: invalid size (%s)", filename, texture->ident);
        }

        texture->pixels = malloc_safe(4 * texture->width * texture->height);

        glCreateTextures(GL_TEXTURE_2D, 1, &texture->handle);
        glTextureStorage2D(texture->handle, 1, GL_RGBA16F, texture->width, texture->height);
        glTextureParameteri(texture->handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(texture->handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        if(json_object_get_boolean(object, "filter")) {
            glTextureParameteri(texture->handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteri(texture->handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }
        else {
            glTextureParameteri(texture->handle, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTextureParameteri(texture->handle, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        }

        glCreateFramebuffers(1, &texture->framebuffer);
        glNamedFramebufferTexture(texture->framebuffer, GL_COLOR_ATTACHMENT0, texture->handle, 0);
    }

    info("parse_textures: parsed %lu textures", (unsigned long)num_textures);
}

static void parse_passes(JSON_Object *restrict root, const char *restrict filename)
{
    size_t i, j, count;
    JSON_Array *node = json_object_get_array(root, "passes");
    JSON_Array *array;
    JSON_Object *object;
    struct r_pass *pass;
    const char *str;

    if(node == NULL) {
        /* Pass list should be an array */
        panic("parse_passes: %s: invalid type", filename);
    }

    num_passes = json_array_get_count(node);
    passes = malloc_safe(num_passes * sizeof(struct r_pass));

    for(i = 0; i < num_passes; ++i) {
        object = json_array_get_object(node, i);
        pass = &passes[i];

        if(!object) {
            /* Pass list is an array of objects */
            panic("parse_passes: %s: invalid entry format", filename);
        }

        if((array = json_object_get_array(object, "params"))) {
            count = json_array_get_count(array);
            if(count > 16)
                count = 16;
            for(j = 0; j < count; ++j) {
                pass->params[j] = json_array_get_number(array, j);
            }
        }

        if((array = json_object_get_array(object, "samplers"))) {
            count = json_array_get_count(array);
            pass->samplers = malloc_safe((count + 1) * sizeof(struct r_texture *));
            for(j = 0; j < count; ++j) {
                str = json_array_get_string(array, j);
                if(!strcmp(str, "image")) {
                    pass->samplers[j] = &image;
                    continue;
                }
                else {
                    if(!(pass->samplers[j] = find_texture(str))) {
                        /* We cannot bind NULL to the sampler! */
                        panic("parse_passes: %s: %s: invalid texture", filename, str);
                    }
                }
            }
        }

        str = json_object_get_string(object, "output");
        if(!(pass->output = find_texture(str)))
            panic("parse_passes: %s: %s: invalid texture", filename, str);
        pass->program = make_pass_program(json_object_get_string(object, "shader"));
    }

    info("parse_passes: parsed %lu passes", (unsigned long)num_passes);
}

static void parse_file(const char *restrict filename)
{
    JSON_Value *root = json_parse_file(filename);
    JSON_Object *root_obj;
    JSON_Value *blit_val;

    if(root == NULL) {
        panic("parse_file: %s: parse failed", filename);
    }

    if(!(root_obj = json_value_get_object(root))) {
        panic("parse_file: %s: root should be an object", filename);
    }

    parse_textures(root_obj, filename);
    parse_passes(root_obj, filename);

    if(!(blit_val = json_object_get_value(root_obj, "blit"))) {
        panic("parse_file: %s: blit: no blit texture", filename);
    }

    if(!(blit_tex = find_texture(json_value_get_string(blit_val)))) {
        panic("parse_file: %s: blit: invalid texture", filename);
    }

    json_value_free(root);
}

static void load_image(const char *restrict filename)
{
    image.pixels = stbi_load(filename, &image.width, &image.height, NULL, STBI_rgb_alpha);

    if(!image.pixels) {
        info("image: %s: load failed", filename);
        return;
    }

    glCreateTextures(GL_TEXTURE_2D, 1, &image.handle);
    glTextureStorage2D(image.handle, 1, GL_RGBA8, image.width, image.height);
    glTextureSubImage2D(image.handle, 0, 0, 0, image.width, image.height, GL_RGBA, GL_UNSIGNED_BYTE, image.pixels);
}

static void unload_image(void)
{
    if(image.pixels) {
        stbi_image_free(image.pixels);
        glDeleteTextures(1, &image.handle);
        image.pixels = NULL;
    }
}

static void render_pass(struct r_pass *restrict pass)
{
    size_t i;
    r_uargs_t args;

    glBindFramebuffer(GL_FRAMEBUFFER, pass->output->handle);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    args[0] = pass->output->width;
    args[1] = pass->output->height;
    args[2] = image.width;
    args[3] = image.height;

    args[4] = frametime;
    args[5] = curtime;
    args[6] = 0.0f;
    args[7] = 0.0f;

    glNamedBufferSubData(uargs, 0, sizeof(r_uargs_t), args);
    glNamedBufferSubData(uparams, 0, sizeof(r_uparams_t), pass->params);

    for(i = 0; pass->samplers[i]; ++i)
        glBindTextureUnit(i, pass->samplers[i]->handle);
    glUseProgram(pass->program);

    glViewport(0, 0, pass->output->width, pass->output->height);
    glDrawArrays(GL_TRIANGLES, 0, 8);
}

static void on_glfw_error(int code, const char *restrict message)
{
    fprintf(stderr, "glfw: [%d] %s", code, message);
}

static void on_framebuffer_size(GLFWwindow *restrict window, int width, int height)
{
    (void)window;
    if(frame.pixels)
        free(frame.pixels);
    frame.width = width;
    frame.height = height;
    frame.pixels = malloc_safe(3 * width * height);
}

static void usage(void)
{
    info("usage: riteg [-B] [-o <prefix>] [-L <count>] [-F <fps>] [-Q <qval>] [-s <W>x<H>] <pipeline> [path...]");
    info("flags:");
    info("  -B          : Set batch mode, treat prefix as a dirname");
    info("  -o <prefix> : Set output file prefix (directory in batch mode)");
    info("  -L <count>  : Specify maximum amount of frames to render");
    info("  -F <fps>    : Specify the fixed framerate");
    info("  -Q <qval>   : Specify export JPEG quality (0..100)");
    info("  -s <W>x<H>  : Set window size (makes it non-resizable)");
    info("  <pipeline>  : Set the JSON pipeline");
    info("  [path...]   : Load image/images");
}

int main(int argc, char **argv)
{
    int c;
    size_t i;
    int batchmode;
    int fixframetime;
    GLFWwindow *window;
    const char *pipeline;
    const char *pathptr;
    char outprefix[512] = {0};
    char outpath[1024] = {0};
    int width, height;
    int resizable = GLFW_TRUE;
    unsigned long jpeg_quality = 80UL;
    unsigned long long nframe = 0ULL;
    unsigned long long maxframe = 0ULL;
    char *imgpath;

    batchmode = 0;
    fixframetime = 0;

    width = -1;
    height = -1;

    while((c = getopt(argc, argv, "Bo:L:F:Q:s:h")) != -1) {
        switch(c) {
            case 'B':
                batchmode = 1;
                break;
            case 'o':
                kstrncpy(outprefix, optarg, sizeof(outprefix));
                break;
            case 'L':
                maxframe = strtoull(optarg, NULL, 10);
                break;
            case 'F':
                fixframetime = 1;
                frametime = 1.0f / atof(optarg);
                break;
            case 'Q':
                jpeg_quality = strtoul(optarg, NULL, 10);
                if(jpeg_quality > 100UL)
                    jpeg_quality = 100UL;
                break;
            case 's':
                if(sscanf(optarg, "%dx%d", &width, &height) < 2)
                    panic("argv: invalid -s argument format");
                resizable = GLFW_FALSE;
                break;
            case 'h':
                usage();
                return 0;
            default:
                info("argv: unrecognized option `%c'", c);
                usage();
                return 1;
        }
    }

    if(!(pipeline = argv[optind])) {
        info("argv: no pipeline defined");
        return 1;
    }

    if(maxframe == 0ULL) {
        /* Batch mode allows the application to idle
         * and display noise for whatever the fuck seconds.
         * To process a single image though you'd need the
         * window to simply flash open and then close... */
        maxframe = batchmode ? ULLONG_MAX : 1ULL;
    }

    if(batchmode) {
        kstrncat(outprefix, "/", sizeof(outprefix));
        pathptr = outpath;
    }
    else {
        /* In single-image mode the -o parameter
         * is treated as a complete filepath */
        pathptr = outprefix;
    }

    optind++;

    if(width <= 16)
        width = 640;
    if(height <= 16)
        height = 480;
    info("size: %dx%d", width, height);

    glfwSetErrorCallback(&on_glfw_error);

    if(!glfwInit()) {
        panic("glfw: init failed");
    }

    glfwWindowHint(GLFW_RESIZABLE, resizable);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);

    window = glfwCreateWindow(width, height, "riteg", NULL, NULL);

    if(!window) {
        panic("glfw: unable to create a window");
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwSetFramebufferSizeCallback(window, &on_framebuffer_size);
    on_framebuffer_size(window, width, height);

    if(!gladLoadGL(&glfwGetProcAddress)) {
        panic("glad: unable to load function pointers");
    }

    vert = compile_shader(GL_VERTEX_SHADER, vert_source_minified);

    glCreateBuffers(1, &uargs);
    glCreateBuffers(1, &uparams);
    glCreateVertexArrays(1, &vao);

    glNamedBufferStorage(uargs, sizeof(r_uargs_t), NULL, GL_DYNAMIC_STORAGE_BIT);
    glNamedBufferStorage(uparams, sizeof(r_uparams_t), NULL, GL_DYNAMIC_STORAGE_BIT);

    info("pipeline: %s", pipeline);
    parse_file(pipeline);

    stbi_set_flip_vertically_on_load(1);
    stbi_flip_vertically_on_write(1);

    curtime = glfwGetTime();
    lasttime = curtime;

    while(!glfwWindowShouldClose(window)) {
        if(argv[optind]) {
            unload_image();
            info("reading %s", argv[optind]);
            load_image(argv[optind]);
            imgpath = argv[optind];
            optind++;
        }

        if(fixframetime) {
            curtime += frametime;
            lasttime = curtime;
        }
        else {
            curtime = glfwGetTime();
            frametime = curtime - lasttime;
            lasttime = curtime;
        }

        glBindBufferBase(GL_UNIFORM_BUFFER, 0, uargs);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, uparams);
        glBindVertexArray(vao);

        for(i = 0; i < num_passes; render_pass(&passes[i++]));

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, frame.width, frame.height);
        glBlitNamedFramebuffer(blit_tex->framebuffer, 0, 0, 0, blit_tex->width, blit_tex->height, 0, 0, frame.width, frame.height, GL_COLOR_BUFFER_BIT, GL_LINEAR);

        if(outprefix[0] && nframe < maxframe) {
            if(batchmode)
                snprintf(outpath, sizeof(outpath), "%sRITEG%03llu.%s.jpg", outprefix, nframe, (imgpath && imgpath[0]) ? basename(imgpath) : "out");
            glReadPixels(0, 0, frame.width, frame.height, GL_RGB, GL_UNSIGNED_BYTE, frame.pixels);
            c = stbi_write_jpg(pathptr, frame.width, frame.height, 3, frame.pixels, jpeg_quality);
            info("writing %s %s", pathptr, c ? "SUCCESS" : "FAILED");
        }
    
        glfwSwapBuffers(window);
        glfwPollEvents();
        
        nframe++;
    }

    unload_image();

    for(i = 0; i < num_textures; ++i) {
        glDeleteFramebuffers(1, &textures[i].framebuffer);
        glDeleteTextures(1, &textures[i].handle);
        free(textures[i].pixels);
    }

    free(textures);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &uparams);
    glDeleteBuffers(1, &uargs);

    glDeleteShader(vert);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
