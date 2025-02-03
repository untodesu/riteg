#ifndef RITEG_PROJECT_HH
#define RITEG_PROJECT_HH 1
#pragma once

#include "riteg/basic_image.hh"

constexpr static long LOAD_MAXFRAMES_PROBE = (-1L); // Image loader will try loading a new image until it fails once
constexpr static long SAVE_MAXFRAMES_INPUT = (-1L); // Image saver will save new images as long as loader succeeded this frame

constexpr static int LOADSAVE_DISABLED  = 0; // Disable loader/saver
constexpr static int LOADSAVE_ONESHOT   = 1; // Load/save a single still image
constexpr static int LOADSAVE_SPRINTF   = 2; // Load/save a sequence of images with sprintf format

constexpr static int SAVE_FILETYPE_INV = 0;
constexpr static int SAVE_FILETYPE_PNG = 1; // Save PNG images
constexpr static int SAVE_FILETYPE_JPG = 2; // Save JPG images
constexpr static int SAVE_FILETYPE_TGA = 3; // Save TGA images

class ShaderPass;
class Timings;

namespace project
{
extern std::filesystem::path workdir;
extern std::filesystem::path projdir;
} // namespace project

namespace project
{
extern std::string about;
extern std::string author;
} // namespace project

namespace project
{
extern int input_mode;
extern long input_maxframes;
extern std::string input_filename;
extern BasicImage input_image;
} // namespace project

namespace project
{
extern int output_mode;
extern int output_filetype;
extern long output_maxframes;
extern int output_jpeg_quality;
extern std::string output_filename;
extern BasicImage output_image;
extern GLuint output_frame;
} // namespace project

namespace project
{
extern ShaderPass *final_pass;
extern std::unordered_map<std::string, ShaderPass*> passes_map;
extern std::vector<ShaderPass*> passes;
} // namespace project

namespace project
{
void init(const char *project_path, const char *preset = nullptr);
void deinit(void);
} // namespace project

namespace project
{
void render(GLFWwindow *window, const Timings &timings);
} // namespace project

#endif /* RITEG_PROJECT_HH */
