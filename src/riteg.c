/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023..2024, Kirill GPRB */
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <GL/gl.h>
#include <GLFW/glfw3.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>

#include "parson.h"

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

/* Reserver texture name for a built-in texture
 * referencing the currently loaded input frame
 * that is vertically flipped because opengl is stupid */
#define TEXNAME_VFLIP "!vflip"

/* Reserved texture name for a blank black texture */
#define TEXNAME_BLANK "!blank"

/* 8 additional parameters are packed in form
 * of two float 4D vectors (vec4) and contain
 * information about resolutions and timings */
typedef float pass_params_t[MAX_PARAM + 8];

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
static texture_t vflip;
static texture_t blank;

static AVFormatContext *in_format = NULL;
static const AVCodecParameters *in_params = NULL;
static const AVCodec *in_decoder = NULL;
static AVCodecContext *in_context = NULL;
static AVPacket *in_packet = NULL;
static AVFrame *in_frame = NULL;
static int in_stream_id = INT_MAX;

static AVFormatContext *out_format = NULL;
static AVCodecParameters *out_params = NULL;
static const AVCodec *out_encoder = NULL;
static AVCodecContext *out_context = NULL;
static AVPacket *out_packet = NULL;
static AVFrame *out_frame = NULL;

static size_t num_passes;
static pass_t *passes;

static unsigned int vert;
static unsigned int ubo;
static unsigned int vao;

static float curtime;
static float lasttime;
static float frametime;
static unsigned long framenum;
static unsigned long maxframe;
static unsigned long framerate;

static void close_output();
static void unload_input();

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
        if(!strcmp(name, TEXNAME_FRAME))
            return &frame;
        if(!strcmp(name, TEXNAME_IMAGE))
            return &image;
        if(!strcmp(name, TEXNAME_VFLIP))
            return &vflip;
        if(!strcmp(name, TEXNAME_BLANK))
            return &blank;
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

    in_format = avformat_alloc_context();
    errnum = avformat_open_input(&in_format, filename, NULL, NULL);

    if(errnum) {
        av_strerror(errnum, str, sizeof(str));
        error("%s: %s", filename, str);
        abort();
    }

    errnum = avformat_find_stream_info(in_format, NULL);

    if(errnum) {
        av_strerror(errnum, str, sizeof(str));
        error("%s: %s", filename, str);
        abort();
    }

    for(i = 0; i < in_format->nb_streams; ++i) {
        params = in_format->streams[i]->codecpar;
        codec = avcodec_find_decoder(params->codec_id);

        if(codec == NULL) {
            /* Silently ignore unknown codecs; if it's
             * not supported by the system installation
             * of FFmpeg's army of libraries, it's obscure
             * enough to not give a damn in RITEG as well */
            continue;
        }

        if(!in_decoder && (codec->type == AVMEDIA_TYPE_VIDEO)) {
            in_stream_id = i;
            in_params = params;
            in_decoder = codec;
            continue;
        }
    }

    if(!in_decoder) {
        warn("%s: container has no supported video", filename);
        return;
    }

    info("%s: video: %s", filename, in_decoder->long_name);
    info("%s: video: %dx%d", filename, in_params->width, in_params->height);

    in_context = avcodec_alloc_context3(in_decoder);
    avcodec_parameters_to_context(in_context, in_params);
    errnum = avcodec_open2(in_context, in_decoder, NULL);

    if(errnum) {
        av_strerror(errnum, str, sizeof(str));
        error("%s: video: %s", filename, str);
        abort();
    }

    in_packet = av_packet_alloc();
    in_frame = av_frame_alloc();
}

static void unload_input(void)
{
    if(in_format) {
        av_frame_free(&in_frame);
        av_packet_free(&in_packet);

        avformat_close_input(&in_format);
        avcodec_free_context(&in_context);
    }
}

static int process_input(void)
{
    int response;
    int vflip_linesize;
    int image_linesize;
    unsigned char *vflip_pixels;
    char str[1024] = { 0 };
    struct SwsContext *sws;

    if(in_format) {
        for(;;) {
            response = av_read_frame(in_format, in_packet);

            if(response == AVERROR_EOF) {
                unload_input();
                return 0;
            }

            if(response < 0) {
                av_packet_unref(in_packet);
                break;
            }

            if(in_packet->stream_index != in_stream_id) {
                av_packet_unref(in_packet);
                continue;
            }

            response = avcodec_send_packet(in_context, in_packet);

            if(response) {
                av_strerror(response, str, sizeof(str));
                warn("video: %s", str);
                unload_input();
                return 0;
            }

            while(response >= 0) {
                response = avcodec_receive_frame(in_context, in_frame);

                if(response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                    /* Not enough data for a full frame, we
                    * should keep reading until it's enough */
                    break;
                }

                if(response < 0) {
                    av_strerror(response, str, sizeof(str));
                    warn("video: %s", str);
                    unload_input();
                    return 0;
                }

                vflip_linesize = 3 * vflip.width;
                image_linesize = 3 * image.width;

                sws = sws_getContext(in_frame->width, in_frame->height, in_frame->format, vflip.width, vflip.height, AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR, NULL, NULL, NULL);
                sws_scale(sws, (void *)(in_frame->data), in_frame->linesize, 0, in_frame->height, (void *)(&vflip.pixels), &vflip_linesize);
                sws_freeContext(sws);

                vflip_pixels = vflip.pixels;
                vflip_pixels += vflip_linesize * (vflip.height - 1);
                vflip_linesize = -vflip_linesize;

                sws = sws_getContext(vflip.width, vflip.height, AV_PIX_FMT_RGB24, image.width, image.height, AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR, NULL, NULL, NULL);
                sws_scale(sws, (void *)(&vflip_pixels), &vflip_linesize, 0, vflip.height, (void *)(&image.pixels), &image_linesize);
                sws_freeContext(sws);

                glTextureSubImage2D(vflip.tex, 0, 0, 0, vflip.width, vflip.height, GL_RGB, GL_UNSIGNED_BYTE, vflip.pixels);
                glTextureSubImage2D(image.tex, 0, 0, 0, image.width, image.height, GL_RGB, GL_UNSIGNED_BYTE, image.pixels);
                av_packet_unref(in_packet);

                return 1;
            }

            av_packet_unref(in_packet);
        }

        return 1;
    }

    return 0;
}

static void init_output(const char *restrict filename)
{
    double guess;
    int errnum, cid;
    char str[1024] = { 0 };
    AVStream *ostream = NULL;

    if(!filename || !filename[0]) {
        info("no output specified");
        return;
    }

    errnum = avformat_alloc_output_context2(&out_format, NULL, NULL, filename);

    if(errnum < 0) {
        av_strerror(errnum, str, sizeof(str));
        error("%s: %s", filename, str);
        abort();
    }

    ostream = avformat_new_stream(out_format, NULL);

    if(!ostream) {
        error("%s: AVStream allocation failed", filename);
        abort();
    }

    cid = av_guess_codec(out_format->oformat, NULL, filename, NULL, AVMEDIA_TYPE_VIDEO);
    out_encoder = avcodec_find_encoder(cid);

    if(!out_encoder) {
        error("%s: AVCodec lookup [%d] failed", filename, cid);
        abort();
    }

    out_context = avcodec_alloc_context3(out_encoder);

    if(!out_context) {
        error("%s: AVCodecContext allocation failed", filename);
        abort();
    }

    if(framerate == 0UL) {
        if(in_context) {
            out_context->framerate = av_guess_frame_rate(in_format, in_format->streams[in_stream_id], NULL);
            out_context->time_base.den = out_context->framerate.num;
            out_context->time_base.num = out_context->framerate.den;
        }
        else {
            out_context->time_base = ((AVRational){1, 60});
            out_context->framerate = ((AVRational){60, 1});
        }
    }
    else {
        out_context->framerate.den = 1;
        out_context->framerate.num = framerate;
        out_context->time_base.den = out_context->framerate.num;
        out_context->time_base.num = out_context->framerate.den;
    }

    out_context->gop_size = 1;
    out_context->max_b_frames = 0;

    if(out_encoder->pix_fmts)
        out_context->pix_fmt = out_encoder->pix_fmts[0];
    else out_context->pix_fmt = AV_PIX_FMT_YUV420P;

    out_context->width = frame.width;
    out_context->height = frame.height;
    out_context->strict_std_compliance = FF_COMPLIANCE_UNOFFICIAL;

    switch(out_context->pix_fmt) {
        case AV_PIX_FMT_YUVJ420P:
            out_context->pix_fmt = AV_PIX_FMT_YUV420P;
            break;
        case AV_PIX_FMT_YUVJ422P:
            out_context->pix_fmt = AV_PIX_FMT_YUV422P;
            break;
        case AV_PIX_FMT_YUVJ444P:
            out_context->pix_fmt = AV_PIX_FMT_YUV444P;
            break;
        case AV_PIX_FMT_YUVJ440P:
            out_context->pix_fmt = AV_PIX_FMT_YUV440P;
            break;
        default:
            break;
    }

    av_opt_set_double(out_context->priv_data, "qp", 0.0, 0);
    av_opt_set(out_context->priv_data, "preset", "ultrafast", 0);

    errnum = avcodec_open2(out_context, out_encoder, NULL);

    if(errnum < 0) {
        av_strerror(errnum, str, sizeof(str));
        error("%s: codec: %s", filename, str);
        abort();
    }

    out_params = ostream->codecpar;
    errnum = avcodec_parameters_from_context(out_params, out_context);

    if(errnum < 0) {
        av_strerror(errnum, str, sizeof(str));
        error("%s: codecpar: %s", filename, str);
        abort();
    }

    if(!(out_format->oformat->flags & AVFMT_NOFILE)) {
        errnum = avio_open(&out_format->pb, filename, AVIO_FLAG_WRITE);

        if(errnum < 0) {
            av_strerror(errnum, str, sizeof(str));
            error("%s: avio: %s", filename, str);
            abort();
        }
    }

    errnum = avformat_write_header(out_format, NULL);

    if(errnum < 0) {
        av_strerror(errnum, str, sizeof(str));
        error("%s: header: %s", filename, str);
        abort();
    }

    out_packet = av_packet_alloc();
    out_frame = av_frame_alloc();

    out_frame->width = out_context->width;
    out_frame->height = out_context->height;
    out_frame->format = out_context->pix_fmt;

    errnum = av_frame_get_buffer(out_frame, 1);

    if(errnum < 0) {
        av_strerror(errnum, str, sizeof(str));
        error("%s: frame: %s", filename, str);
        abort();
    }

    if(in_format && (maxframe == ULONG_MAX)) {
        guess = in_format->duration;
        guess /= 1000000.0; /* AVFormatContext::duration is in useconds */
        guess *= out_context->framerate.num;
        guess += out_context->framerate.num; /* CRUTCH: add extra second */
        maxframe = ceil(guess);
    }
}

static void close_output(void)
{
    if(out_format) {
        av_write_trailer(out_format);

        av_frame_free(&out_frame);
        av_packet_free(&out_packet);

        avformat_close_input(&out_format);
        avcodec_free_context(&out_context);
    }
}

static void process_output_video()
{
    char str[1024];
    int linesize = 3 * frame.width;
    int response;
    struct SwsContext *sws;
    unsigned char *pixptr;

    if((maxframe != ULONG_MAX) && (framenum >= maxframe)) {
        close_output();
        return;
    }

    if(out_format) {
        av_frame_make_writable(out_frame);

        glBindFramebuffer(GL_FRAMEBUFFER, frame.fbo);
        glReadPixels(0, 0, frame.width, frame.height, GL_RGB, GL_UNSIGNED_BYTE, frame.pixels);

        pixptr = frame.pixels;
        pixptr += linesize * (frame.height - 1);
        linesize = -linesize;

        sws = sws_getContext(frame.width, frame.height, AV_PIX_FMT_RGB24, out_frame->width, out_frame->height, out_frame->format, SWS_FAST_BILINEAR, NULL, NULL, NULL);
        sws_scale(sws, (void *)(&pixptr), &linesize, 0, frame.height, out_frame->data, out_frame->linesize);
        sws_freeContext(sws);

        out_frame->pts = framenum;

        response = avcodec_send_frame(out_context, out_frame);

        if(response < 0) {
            av_strerror(response, str, sizeof(str));
            warn("[1] output: video: %s", str);
            close_output();
            return;
        }

        while(response >= 0) {
            response = avcodec_receive_packet(out_context, out_packet);

            if(response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                av_packet_unref(out_packet);
                break;
            }

            if(response < 0) {
                av_strerror(response, str, sizeof(str));
                warn("[2] output: video: %s", str);
                close_output();
                return;
            }

            av_packet_rescale_ts(out_packet, out_context->time_base, out_format->streams[0]->time_base);

            response = av_interleaved_write_frame(out_format, out_packet);

            if(response) {
                av_strerror(response, str, sizeof(str));
                warn("[3] output: video: %s", str);
                close_output();
                return;
            }

            av_packet_unref(out_packet);
        }
    }
}

static void on_glfw_error(int code, const char *restrict message)
{
    (error)("glfw: error[%d]: %s", code, message);
}

static void on_framebuffer_size(GLFWwindow *window, int width, int height)
{
    unused_argument(window);
    unused_argument(width);
    unused_argument(height);
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
    fprintf(stderr, "usage: riteg [-h] [-o <path>] [-s <w>:<h>] [-f <fps>] [-c <count>] [-Q] <pipeline> [paths...]\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, "   -h          : print this message and exit\n");
    fprintf(stderr, "   -o <path>   : specify output path. FFmpeg path formatting works.\n");
    fprintf(stderr, "   -s <w>:<h>  : specify rendering frame size.\n");
    fprintf(stderr, "   -f <fps>    : specify fixed framerate (frametime = 1 / FPS).\n");
    fprintf(stderr, "   -c <count>  : specify maximum amount of frames to export.\n");
    fprintf(stderr, "   -Q          : quit as soon as input video feed ends\n");
}

int main(int argc, char **argv)
{
    int c;
    size_t i;
    int fbw, fbh;
    int quit_on_end;
    const char *pipeline_path;
    char output_path[BUFSIZ] = { 0 };
    GLFWwindow *window;

    frame.width = -1;
    frame.height = -1;
    maxframe = ULONG_MAX;
    framerate = 0UL;
    quit_on_end = 0;

    while((c = getopt(argc, argv, "ho:s:f:c:Q")) != -1) {
        switch(c) {
            case 'h':
                usage();
                return 0;
            case 'o':
                kstrncpy(output_path, optarg, sizeof(output_path));
                break;
            case 's':
                sscanf(optarg, "%d:%d", &frame.width, &frame.height);
                break;
            case 'f':
                framerate = strtoul(optarg, NULL, 10);
                break;
            case 'c':
                maxframe = strtoul(optarg, NULL, 10);
                break;
            case 'Q':
                quit_on_end = 1;
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

    glfwSetErrorCallback(&on_glfw_error);

    if(!glfwInit()) {
        error("glfw: init failed");
        abort();
    }

    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
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

    load_input(argv[optind++]);
    init_output(output_path);

    image.width = 1 << av_log2(in_format ? in_context->width : frame.width);
    image.height = 1 << av_log2(in_format ? in_context->height : frame.height);
    image.pixels = malloc_safe(3 * image.width * image.height);

    vflip.width = image.width;
    vflip.height = image.height;
    vflip.pixels = malloc_safe(3 * vflip.width * vflip.height);

    frame.pixels = malloc_safe(3 * frame.width * frame.height);

    blank.width = 16;
    blank.height = 16;
    blank.pixels = NULL;

    glCreateTextures(GL_TEXTURE_2D, 1, &image.tex);
    glTextureStorage2D(image.tex, 1, GL_RGBA32F, image.width, image.height);
    glTextureParameteri(image.tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(image.tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(image.tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(image.tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glCreateTextures(GL_TEXTURE_2D, 1, &vflip.tex);
    glTextureStorage2D(vflip.tex, 1, GL_RGBA32F, vflip.width, vflip.height);
    glTextureParameteri(vflip.tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(vflip.tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(vflip.tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(vflip.tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glCreateTextures(GL_TEXTURE_2D, 1, &blank.tex);
    glTextureStorage2D(blank.tex, 1, GL_RGBA16F, blank.width, blank.height);
    glTextureParameteri(blank.tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(blank.tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(blank.tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(blank.tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glCreateTextures(GL_TEXTURE_2D, 1, &frame.tex);
    glTextureStorage2D(frame.tex, 1, GL_RGBA32F, frame.width, frame.height);
    glTextureParameteri(frame.tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(frame.tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(frame.tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(frame.tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glCreateFramebuffers(1, &frame.fbo);
    glNamedFramebufferTexture(frame.fbo, GL_COLOR_ATTACHMENT0, frame.tex, 0);

    glCreateFramebuffers(1, &blank.fbo);
    glNamedFramebufferTexture(blank.fbo, GL_COLOR_ATTACHMENT0, blank.tex, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, blank.fbo);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    curtime = glfwGetTime();
    lasttime = curtime - 0.16f;
    framenum = 0UL;

    while(!glfwWindowShouldClose(window)) {
        if(!process_input()) {
            if(quit_on_end) {
                if(maxframe != ULONG_MAX) {
                    if(framenum >= maxframe) {
                        glfwSetWindowShouldClose(window, GLFW_TRUE);
                        /* Don't break here: we still need to process
                        * the remaining frame that probably exists */
                    }
                }
                else {
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                    /* Don't break here: we still need to process
                    * the remaining frame that probably exists */
                }
            }
        }

        curtime = glfwGetTime();
        frametime = curtime - lasttime;
        lasttime = curtime;

        glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);

        glBindVertexArray(vao);

        for(i = 0; i < num_passes; draw_pass(&passes[i++]));

        glfwGetFramebufferSize(window, &fbw, &fbh);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, fbw, fbh);

        glBlitNamedFramebuffer(blit->fbo, frame.fbo, 0, 0, blit->width, blit->height, 0, 0, frame.width, frame.height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBlitNamedFramebuffer(blit->fbo, 0, 0, 0, blit->width, blit->height, 0, 0, fbw, fbh, GL_COLOR_BUFFER_BIT, GL_LINEAR);

        process_output_video();

        glfwSwapBuffers(window);
        glfwPollEvents();

        framenum += 1UL;
    }

    close_output();
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

    glDeleteFramebuffers(1, &blank.fbo);
    glDeleteFramebuffers(1, &frame.fbo);

    glDeleteTextures(1, &blank.tex);
    glDeleteTextures(1, &frame.tex);
    glDeleteTextures(1, &vflip.tex);
    glDeleteTextures(1, &image.tex);

    free(frame.pixels);
    free(vflip.pixels);
    free(image.pixels);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
