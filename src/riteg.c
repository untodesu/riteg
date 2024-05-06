#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <GL/gl.h>
#include <GLFW/glfw3.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include "parson.h"
#include "stb_image.h"
#include "stb_image_write.h"

#define unused_argument(x) ((void)(x))

/* Each pass can handle up to 16 floating
 * point parameters passed in form of four
 * float 4D vectors (vec4) packed inside a
 * uniform buffer at binding unit 1. */
#define MAX_PARAM 16

/* Reserved texture name for a built-in texture
 * referencing previously rendered frame. */
#define TEXNAME_FRAME "!frame"

/* Reserved texture name for a built-in texture
 * referencing the currently loaded input frame */
#define TEXNAME_IMAGE "!image"

/* Reserved texture name for a blank black texture */
#define TEXNAME_BLANK "!blank"

/* 8 additional parameters are packed in form
 * of two float 4D vectors (vec4) and contain
 * information about resolutions and timings */
typedef float pass_params_t[MAX_PARAM + 8];

enum {
    INPUT_MORE = 0,
    INPUT_DONE = 1,
    INPUT_FAIL = 2,
};

typedef struct {
    int width;
    int height;
    char name[64];
    unsigned int tex;
    unsigned int fbo;
    unsigned char *pixels;
} texture_t;

typedef struct {
    unsigned int prog;
    texture_t *output;
    texture_t **samplers;
    pass_params_t params;
} pass_t;

static const char vert_src[] = {
    0x23, 0x76, 0x65, 0x72, 0x73, 0x69,
    0x6F, 0x6E, 0x20, 0x34, 0x35, 0x30,
    0x20, 0x63, 0x6F, 0x72, 0x65, 0x0A,
    0x6C, 0x61, 0x79, 0x6F, 0x75, 0x74,
    0x28, 0x6C, 0x6F, 0x63, 0x61, 0x74,
    0x69, 0x6F, 0x6E, 0x3D, 0x30, 0x29,
    0x6F, 0x75, 0x74, 0x20, 0x76, 0x65,
    0x63, 0x32, 0x20, 0x75, 0x76, 0x3B,
    0x63, 0x6F, 0x6E, 0x73, 0x74, 0x20,
    0x76, 0x65, 0x63, 0x32, 0x20, 0x76,
    0x65, 0x72, 0x74, 0x73, 0x5B, 0x38,
    0x5D, 0x3D, 0x76, 0x65, 0x63, 0x32,
    0x5B, 0x38, 0x5D, 0x28, 0x76, 0x65,
    0x63, 0x32, 0x28, 0x2D, 0x31, 0x2E,
    0x30, 0x2C, 0x2D, 0x31, 0x2E, 0x30,
    0x29, 0x2C, 0x76, 0x65, 0x63, 0x32,
    0x28, 0x2B, 0x31, 0x2E, 0x30, 0x2C,
    0x2D, 0x31, 0x2E, 0x30, 0x29, 0x2C,
    0x76, 0x65, 0x63, 0x32, 0x28, 0x2B,
    0x31, 0x2E, 0x30, 0x2C, 0x2B, 0x31,
    0x2E, 0x30, 0x29, 0x2C, 0x76, 0x65,
    0x63, 0x32, 0x28, 0x2D, 0x31, 0x2E,
    0x30, 0x2C, 0x2D, 0x31, 0x2E, 0x30,
    0x29, 0x2C, 0x76, 0x65, 0x63, 0x32,
    0x28, 0x2B, 0x31, 0x2E, 0x30, 0x2C,
    0x2B, 0x31, 0x2E, 0x30, 0x29, 0x2C,
    0x76, 0x65, 0x63, 0x32, 0x28, 0x2D,
    0x31, 0x2E, 0x30, 0x2C, 0x2B, 0x31,
    0x2E, 0x30, 0x29, 0x2C, 0x76, 0x65,
    0x63, 0x32, 0x28, 0x2D, 0x31, 0x2E,
    0x30, 0x2C, 0x2D, 0x31, 0x2E, 0x30,
    0x29, 0x2C, 0x76, 0x65, 0x63, 0x32,
    0x28, 0x2B, 0x31, 0x2E, 0x30, 0x2C,
    0x2B, 0x31, 0x2E, 0x30, 0x29, 0x29,
    0x3B, 0x76, 0x6F, 0x69, 0x64, 0x20,
    0x6D, 0x61, 0x69, 0x6E, 0x28, 0x76,
    0x6F, 0x69, 0x64, 0x29, 0x7B, 0x75,
    0x76, 0x3D, 0x76, 0x65, 0x63, 0x32,
    0x28, 0x30, 0x2E, 0x35, 0x2C, 0x30,
    0x2E, 0x35, 0x29, 0x2B, 0x30, 0x2E,
    0x35, 0x2A, 0x76, 0x65, 0x72, 0x74,
    0x73, 0x5B, 0x67, 0x6C, 0x5F, 0x56,
    0x65, 0x72, 0x74, 0x65, 0x78, 0x49,
    0x44, 0x5D, 0x3B, 0x67, 0x6C, 0x5F,
    0x50, 0x6F, 0x73, 0x69, 0x74, 0x69,
    0x6F, 0x6E, 0x3D, 0x76, 0x65, 0x63,
    0x34, 0x28, 0x76, 0x65, 0x72, 0x74,
    0x73, 0x5B, 0x67, 0x6C, 0x5F, 0x56,
    0x65, 0x72, 0x74, 0x65, 0x78, 0x49,
    0x44, 0x5D, 0x2C, 0x30, 0x2E, 0x30,
    0x2C, 0x31, 0x2E, 0x30, 0x29, 0x3B,
    0x7D, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static size_t num_textures;
static texture_t *textures;
static texture_t *blit;
static texture_t frame;
static texture_t image;
static texture_t blank;

static AVFormatContext *in_ctx = NULL;
static const AVCodecParameters *in_params = NULL;
static const AVCodec *in_vcodec = NULL;
static AVCodecContext *in_vctx = NULL;
static AVPacket *in_vpacket = NULL;
static AVFrame *in_vframe = NULL;
static AVFrame *in_target = NULL;
static int in_vcodec_stream_id = 0;

static size_t num_passes;
static pass_t *passes;

static unsigned int vert;
static unsigned int ubo;
static unsigned int vao;

static float curtime;
static float lasttime;
static float frametime;

/* https://github.com/vxsys/vx/blob/master/lib/kstrncpy.c */
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
    fputs("\033[1;32minfo:\033[0m ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc(0x0A, stderr);
    va_end(ap);
}

static void warn(const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("\033[1;33mwarn:\033[0m ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc(0x0A, stderr);
    va_end(ap);
}

static void error(const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("\033[1;31merror:\033[0m ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc(0x0A, stderr);
    va_end(ap);
}

static void *malloc_safe(size_t size)
{
    void *pointer;

    if(!(pointer = calloc(1, size))) {
        error("calloc: %s", strerror(errno));
        abort();
    }

    return pointer;
}

static void *malloc_file(const char *restrict filename)
{
    char *buffer;
    size_t length;
    FILE *file = fopen(filename, "r");

    if(!file) {
        error("fopen: %s: %s", filename, strerror(errno));
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    length = ftell(file) + 1;
    fseek(file, 0, SEEK_SET);

    buffer = malloc_safe(length);
    fread(buffer, length, 1, file);
    fclose(file);

    return buffer;
}

static unsigned int compile_shader(unsigned int stage, const char *source)
{
    int status;
    int length;
    char *buffer;
    unsigned int shader;

    shader = glCreateShader(stage);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

    if(length > 1) {
        buffer = malloc_safe(length);
        glGetShaderInfoLog(shader, length, NULL, buffer);
        info("%s", source);
        info("%s", buffer);
        free(buffer);
    }

    if(!status) {
        glDeleteShader(shader);
        error("shader compilation failed");
        abort();
    }

    return shader;
}

static unsigned int link_program(const char *frag_filename)
{
    int status;
    int length;
    char *buffer;
    unsigned int frag;
    unsigned int prog;

    if(!(buffer = malloc_file(frag_filename))) {
        error("program linking failed");
        abort();
    }

    frag = compile_shader(GL_FRAGMENT_SHADER, buffer);
    free(buffer);

    prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    glDeleteShader(frag);

    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &length);

    if(length > 1) {
        buffer = malloc_safe(length);
        glGetProgramInfoLog(prog, length, NULL, buffer);
        info("%s", buffer);
        free(buffer);
    }

    if(!status) {
        glDeleteProgram(prog);
        error("program linking failed");
        abort();
    }

    return prog;
}

static texture_t *find_texture(const char *restrict name, int reserved)
{
    size_t i;

    if(reserved) {
        if(!strcmp(name, TEXNAME_FRAME)) {
            return &frame;
        }

        if(!strcmp(name, TEXNAME_IMAGE)) {
            return &image;
        }

        if(!strcmp(name, TEXNAME_BLANK)) {
            return &blank;
        }
    }

    for(i = 0; i < num_textures; ++i) {
        if(strcmp(name, textures[i].name))
            continue;
        return &textures[i];
    }

    return NULL;
}

static void parse_textures(JSON_Object *restrict root, const char *restrict filename)
{
    size_t i;
    JSON_Object *node = json_object_get_object(root, "textures");
    JSON_Object *object;
    JSON_Value *value;
    texture_t *texture;

    if(!node) {
        error("%s: textures must be an object");
        abort();
    }

    num_textures = json_object_get_count(node);
    textures = malloc_safe(num_textures * sizeof(texture_t));

    for(i = 0; i < num_textures; ++i) {
        value = json_object_get_value_at(node, i);
        object = json_value_get_object(value);
        texture = &textures[i];

        if(!object) {
            error("%s: textures[%zu] must be an object", i);
            abort();
        }

        kstrncpy(texture->name, json_object_get_name(node, i), sizeof(texture->name));

        if(texture->name[0] == '!') {
            error("%s: textures[%zu]: %s is a reserved name", i, texture->name);
            abort();
        }

        texture->width = json_object_get_number(object, "width");
        texture->height = json_object_get_number(object, "height");

        if(texture->width <= 0) {
            error("%s: textures[%zu / %s]: invalid width (%d)", filename, i, texture->name, texture->width);
            abort();
        }

        if(texture->height <= 0) {
            error("%s: textures[%zu / %s]: invalid height (%d)", filename, i, texture->name, texture->height);
            abort();
        }

        glCreateTextures(GL_TEXTURE_2D, 1, &texture->tex);
        glTextureStorage2D(texture->tex, 1, GL_RGBA16F, texture->width, texture->height);
        glTextureParameteri(texture->tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(texture->tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        if(json_object_get_boolean(object, "filter")) {
            glTextureParameteri(texture->tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteri(texture->tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }
        else {
            glTextureParameteri(texture->tex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTextureParameteri(texture->tex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        }

        glCreateFramebuffers(1, &texture->fbo);
        glNamedFramebufferTexture(texture->fbo, GL_COLOR_ATTACHMENT0, texture->tex, 0);
    }

    info("parse_textures: created %zu textures", num_textures);
}

static void parse_passes(JSON_Object *restrict root, const char *filename)
{
    size_t i, j;
    size_t count;
    const char *str;
    JSON_Array *node = json_object_get_array(root, "passes");
    JSON_Object *object;
    JSON_Array *array;
    pass_t *pass;

    if(!node) {
        error("%s: passes field must be an array");
        abort();
    }

    num_passes = json_array_get_count(node);
    passes = malloc_safe(num_passes * sizeof(pass_t));

    for(i = 0; i < num_passes; ++i) {
        object = json_array_get_object(node, i);
        pass = &passes[i];

        if(!object) {
            error("%s: passes[%zu] must be an object", i);
            abort();
        }

        if(!(str = json_object_get_string(object, "shader"))) {
            error("%s: passes[%zu]: shader field must be a string", i);
            abort();
        }

        pass->prog = link_program(str);

        if((array = json_object_get_array(object, "samplers"))) {
            count = json_array_get_count(array);
            pass->samplers = malloc_safe((count + 1) * sizeof(texture_t *));

            for(j = 0; j < count; ++j) {
                if(!(str = json_array_get_string(array, j))) {
                    error("%s: passes[%zu]: samplers[%zu]: value must be a string", filename, i, j);
                    abort();
                }

                if(!(pass->samplers[j] = find_texture(str, 1))) {
                    error("%s: passes[%zu]: samplers[%zu]: %s is not a valid texture", filename, i, j, str);
                    abort();
                }
            }

            pass->samplers[count] = NULL;
        }

        if(!(str = json_object_get_string(object, "output"))) {
            error("%s: passes[%zu]: output field must be a string", filename, i);
            abort();
        }

        if(!(pass->output = find_texture(str, 0))) {
            error("%s: passes[%zu]: %s is either an invalid or a reserved texture", filename, i, str);
            abort();
        }

        if((array = json_object_get_array(object, "params"))) {
            count = json_array_get_count(array);

            if(count > MAX_PARAM) {
                warn("%s: passes[%zu]: too much params (%zu/%zu)", filename, i, count, MAX_PARAM);
                count = MAX_PARAM;
            }

            for(j = 0; j < count; ++j) {
                pass->params[j] = json_array_get_number(array, j);
            }
        }
    }

    info("parse_passes: created %zu passes", num_passes);
}

static void parse_file(const char *filename)
{
    JSON_Value *json = json_parse_file_with_comments(filename);
    JSON_Object *root;
    JSON_Value *vblit;
    const char *str;

    if(!json) {
        error("%s: syntax error", filename);
        abort();
    }

    if(!(root = json_value_get_object(json))) {
        error("%s: root must be an object", filename);
        abort();
    }

    parse_textures(root, filename);
    parse_passes(root, filename);

    if(!(vblit = json_object_get_value(root, "blit"))) {
        blit = &textures[num_textures - 1];
        warn("%s: defaulting blit to %s", filename, blit->name);
        warn("%s: expect unpredictable results!", filename);
    }
    else {
        if(!(str = json_value_get_string(vblit))) {
            error("%s: blit field must be a string", filename);
            abort();
        }

        if(!(blit = find_texture(str, 0))) {
            error("%s: blit: %s is either an invalid or a reserved texture", filename, str);
            abort();
        }

        info("%s: setting blit to %s", filename, blit->name);
    }

    json_value_free(json);
}

static void draw_pass(pass_t *restrict pass)
{
    size_t i;

    glBindFramebuffer(GL_FRAMEBUFFER, pass->output->fbo);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    pass->params[MAX_PARAM + 0] = pass->output->width;
    pass->params[MAX_PARAM + 1] = pass->output->height;
    pass->params[MAX_PARAM + 2] = frame.width;
    pass->params[MAX_PARAM + 3] = frame.height;
    pass->params[MAX_PARAM + 4] = frametime;
    pass->params[MAX_PARAM + 5] = curtime;
    pass->params[MAX_PARAM + 6] = 0.0f;
    pass->params[MAX_PARAM + 7] = 0.0f;

    glNamedBufferSubData(ubo, 0, sizeof(pass_params_t), pass->params);

    for(i = 0; pass->samplers && pass->samplers[i]; ++i)
        glBindTextureUnit(i, pass->samplers[i]->tex);
    glUseProgram(pass->prog);

    glViewport(0, 0, pass->output->width, pass->output->height);
    glDrawArrays(GL_TRIANGLES, 0, 8);
}

static void load_input(const char *restrict filename)
{
    unsigned i;
    int errnum;
    char str[1024] = { 0 };
    AVCodecParameters *params;
    const AVCodec *codec;

    if(filename == NULL) {
        info("no input specified");
        return;
    }

    in_ctx = avformat_alloc_context();
    errnum = avformat_open_input(&in_ctx, filename, NULL, NULL);

    if(errnum) {
        av_strerror(errnum, str, sizeof(str));
        error("%s: %s", filename, str);
        abort();
    }

    for(i = 0; i < in_ctx->nb_streams; ++i) {
        params = in_ctx->streams[i]->codecpar;
        codec = avcodec_find_decoder(params->codec_id);

        if(codec == NULL) {
            /* Silently ignore unknown codecs; if it's
             * not supported by the system installation
             * of FFmpeg's army of libraries, it's obscure
             * enough to not give a damn in RITEG as well */
            continue;
        }

        if(!in_vcodec && (codec->type == AVMEDIA_TYPE_VIDEO)) {
            in_vcodec_stream_id = i;
            in_params = params;
            in_vcodec = codec;
            continue;
        }
    }

    if(!in_vcodec) {
        warn("%s: container has no supported video", filename);
        return;
    }

    info("%s: video: %s", filename, in_vcodec->long_name);
    info("%s: video: %dx%d", filename, in_params->width, in_params->height);

    in_vctx = avcodec_alloc_context3(in_vcodec);
    avcodec_parameters_to_context(in_vctx, in_params);
    errnum = avcodec_open2(in_vctx, in_vcodec, NULL);

    if(errnum) {
        av_strerror(errnum, str, sizeof(str));
        error("%s: video: %s", filename, str);
        abort();
    }

    in_vpacket = av_packet_alloc();
    in_vframe = av_frame_alloc();
}

static void unload_input(void)
{
    if(in_ctx) {
        av_frame_free(&in_target);
        av_frame_free(&in_vframe);
        av_packet_free(&in_vpacket);

        avformat_close_input(&in_ctx);
        avcodec_free_context(&in_vctx);
    }
}

static void process_input(void)
{
    int response;
    char str[1024] = { 0 };
    struct SwsContext *sws;

    while(in_ctx && (av_read_frame(in_ctx, in_vpacket) >= 0)) {
        if(in_vpacket->stream_index != in_vcodec_stream_id) {
            av_packet_unref(in_vpacket);
            continue;
        }

        response = avcodec_send_packet(in_vctx, in_vpacket);

        if(response) {
            av_strerror(response, str, sizeof(str));
            warn("video: %s", str);
            unload_input();
            return;
        }

        while(response >= 0) {
            response = avcodec_receive_frame(in_vctx, in_vframe);

            if(response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                /* Not enough data for a full frame, we
                 * should keep reading until it's enough */
                break;
            }

            if(response < 0) {
                av_strerror(response, str, sizeof(str));
                warn("video: %s", str);
                unload_input();
                return;
            }

            sws = sws_getContext(
                in_vframe->width, in_vframe->height, in_vframe->format,
                in_target->width, in_target->height, in_target->format,
                SWS_BILINEAR, NULL, NULL, NULL);
            sws_scale(sws, (const uint8_t * const*)in_vframe->data,
                in_vframe->linesize, 0, in_vframe->height, in_target->data,
                in_target->linesize);
            sws_freeContext(sws);

            if(in_target->linesize[0] != (image.width * 3)) {
                warn("video: linesize sanity check failed");
                warn("video: linesize[0] is not %zu", (size_t)(image.width * 3));
                unload_input();
                return;
            }

            glTextureSubImage2D(image.tex, 0, 0, 0, image.width, image.height, GL_RGB, GL_UNSIGNED_BYTE, in_target->data[0]);
            av_packet_unref(in_vpacket);
            return;
        }

        av_packet_unref(in_vpacket);
    }
}

static void on_glfw_error(int code, const char *restrict message)
{
    error("glfw: error[%d]: %s", code, message);
}

static void on_framebuffer_size(GLFWwindow *window, int width, int height)
{
    unused_argument(window);

    if(frame.tex) {
        glDeleteTextures(1, &frame.tex);
        glDeleteFramebuffers(1, &frame.fbo);
    }

    free(frame.pixels);

    frame.width = width;
    frame.height = height;
    frame.pixels = malloc_safe(3 * width * height);

    glCreateTextures(GL_TEXTURE_2D, 1, &frame.tex);
    glTextureStorage2D(frame.tex, 1, GL_RGBA32F, frame.width, frame.height);
    glTextureParameteri(frame.tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(frame.tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(frame.tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(frame.tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glCreateFramebuffers(1, &frame.fbo);
    glNamedFramebufferTexture(frame.fbo, GL_COLOR_ATTACHMENT0, frame.tex, 0);

    if(in_target)
        av_frame_free(&in_target);
    in_target = av_frame_alloc();

    av_image_alloc(in_target->data, in_target->linesize, width, height, AV_PIX_FMT_RGB24, 1);
    in_target->width = width;
    in_target->height = height;
    in_target->format = AV_PIX_FMT_RGB24;
}

static void on_key(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    unused_argument(scancode);
    unused_argument(mods);

    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        return;
    }
}

static void usage(void)
{
    /*
    
    riteg -o radiosonde_vhs.mp4 -Q 15 -s 640:480 json/vhs.v5.json radiosonde.mp4
    riteg -o fx.mp4 -Q 15 -s 640:480 -f 30 -c 300 json/vhs.v5.json media/vhs43_SMPTE.png

    */
   fprintf(stderr, "usage: UNDEFINED YET\n");
    /*
    fprintf(stderr, "usage: riteg [-h] [-o <path>] [-s <w>:<h>] [-f <fps>] [-c <count>] <pipeline> [paths...]\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, "   -h          : print this message and exit\n");
    fprintf(stderr, "   -o <path>   : specify output path. {} is substituted to frame index.\n");
    fprintf(stderr, "   -s <w>:<h>  : specify window size (makes the window non-resizable).\n");
    fprintf(stderr, "   -f <fps>    : specify fixed framerate (frametime = 1 / FPS).\n");
    fprintf(stderr, "   -c <count>  : specify maximum amount of frames to export.\n");
    */
}

int main(int argc, char **argv)
{
    int c;
    size_t i;
    int resizable = GLFW_TRUE;
    const char *pipeline_path;
    GLFWwindow *window;

    frame.width = -1;
    frame.height = -1;

    while((c = getopt(argc, argv, "ho:s:f:c:")) != -1) {
        switch(c) {
            case 'h':
                usage();
                return 0;
            case 'o':
                /* batch = make_pathfmt(output_fmt, sizeof(output_fmt), optarg); */
                break;
            case 's':
                sscanf(optarg, "%d:%d", &frame.width, &frame.height);
                resizable = GLFW_FALSE;
                break;
            default:
                error("unrecognized option: %c", c);
                usage();
                return 1;
        }
    }

    if(!argv[optind]) {
        error("no pipeline specified");
        abort();
    }

    pipeline_path = argv[optind++];

    if(frame.width <= 16) {
        frame.width = 640;
        info("window width is too small, setting to %d", frame.width);
    }

    if(frame.height <= 16) {
        frame.height = 480;
        info("window height is too small, setting to %d", frame.height);
    }

    info("size: %dx%d", frame.width, frame.height);
    info("resizable: %s", resizable ? "yes" : "no");

    glfwSetErrorCallback(&on_glfw_error);

    if(!glfwInit()) {
        error("glfw: init failed");
        abort();
    }

    glfwWindowHint(GLFW_RESIZABLE, resizable);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);

    if(!(window = glfwCreateWindow(frame.width, frame.height, "RITEG", NULL, NULL))) {
        error("glfw: creating a window failed");
        abort();
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwSetFramebufferSizeCallback(window, &on_framebuffer_size);
    glfwSetKeyCallback(window, &on_key);

    /* Refresh the framebuffer parameters */
    on_framebuffer_size(window, frame.width, frame.height);

    vert = compile_shader(GL_VERTEX_SHADER, vert_src);

    glCreateBuffers(1, &ubo);
    glNamedBufferStorage(ubo, sizeof(pass_params_t), NULL, GL_DYNAMIC_STORAGE_BIT);

    glCreateVertexArrays(1, &vao);

    info("loading %s", pipeline_path);
    parse_file(pipeline_path);

    stbi_set_flip_vertically_on_load(1);
    stbi_flip_vertically_on_write(1);

    blank.width = 16;
    blank.height = 16;
    blank.pixels = NULL;

    glCreateTextures(GL_TEXTURE_2D, 1, &blank.tex);
    glTextureStorage2D(blank.tex, 1, GL_RGBA16F, blank.width, blank.height);
    glTextureParameteri(blank.tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(blank.tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(blank.tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(blank.tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glCreateFramebuffers(1, &blank.fbo);
    glNamedFramebufferTexture(blank.fbo, GL_COLOR_ATTACHMENT0, blank.tex, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, blank.fbo);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    image.width = frame.width;
    image.height = frame.height;

    glCreateTextures(GL_TEXTURE_2D, 1, &image.tex);
    glTextureStorage2D(image.tex, 1, GL_RGBA16F, image.width, image.height);
    glTextureParameteri(image.tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(image.tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(image.tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(image.tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    load_input(argv[optind]);

    curtime = glfwGetTime();
    lasttime = curtime - 0.16f;

    while(!glfwWindowShouldClose(window)) {
        process_input();

        curtime = glfwGetTime();
        frametime = curtime - lasttime;
        lasttime = curtime;

        glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);

        glBindVertexArray(vao);

        for(i = 0; i < num_passes; draw_pass(&passes[i++]));

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, frame.width, frame.height);

        glBlitNamedFramebuffer(blit->fbo, frame.fbo, 0, 0, blit->width, blit->height, 0, 0, frame.width, frame.height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBlitNamedFramebuffer(blit->fbo, 0, 0, 0, blit->width, blit->height, 0, 0, frame.width, frame.height, GL_COLOR_BUFFER_BIT, GL_LINEAR);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    unload_input();

    for(i = 0; i < num_passes; ++i) {
        glDeleteProgram(passes[i].prog);
        free(passes[i].samplers);        
    }

    for(i = 0; i < num_textures; ++i) {
        glDeleteFramebuffers(1, &textures[i].fbo);
        glDeleteTextures(1, &textures[i].tex);
        free(textures[i].pixels);
    }

    free(passes);
    free(textures);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &ubo);
    glDeleteShader(vert);

    glDeleteFramebuffers(1, &frame.fbo);

    glDeleteTextures(1, &image.tex);
    glDeleteTextures(1, &blank.tex);
    glDeleteTextures(1, &frame.tex);

    free(frame.pixels);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
