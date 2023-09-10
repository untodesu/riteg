# RITEG
A realtime-ish GLSL image processor

# CLI usage:
```
usage: riteg [-B] [-o <prefix>] [-L <count>] [-F <fps>] [-Q <qval>] [-s <W>x<H>] <pipeline> [path...]
flags:
  -B          : Set batch mode, treat prefix as a dirname
  -o <prefix> : Set output file prefix (directory in batch mode)
  -L <count>  : Specify maximum amount of frames to render
  -F <fps>    : Specify the fixed framerate
  -Q <qval>   : Specify export JPEG quality (0..100)
  -s <W>x<H>  : Set window size (makes it non-resizable)
  <pipeline>  : Set the JSON pipeline
  [path...]   : Load image/images
```

# Documentation
* [Here](https://github.com/untodesu/riteg/wiki)

# Requirements (normal systems)
* A GPU
* An ISO C99 compliant C compiler
* GPU drivers that comprehend OpenGL 4.5
* [GLFW](https://www.glfw.org/)

# Requirements (Windows)
* Tons of luck and WSL or whatever the hell windows users have these days

# Building
```
make all
```

# Cleaning
```
make clean
```

# Installing
![](2023-09-10_11-03.png)  

# Examples (vhs.v2.json)
![](RITEG-5.jpg)  
![](RITEG-6.jpg)  
![](RITEG-7.jpg)  
![](RITEG-8.jpg)  
![](RITEG-9.jpg)  

# Video example (vhs.v2.json) (click on the image)
[![](https://img.youtube.com/vi/X7Hk8FhaFpU/0.jpg)](https://www.youtube.com/watch?v=X7Hk8FhaFpU)

# Example (checker.json)
![](RITEG-checkerboard.jpg)  
