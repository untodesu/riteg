# RITEG
A realtime data-driven image processor

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

# Custom pipelines
* [Existing shaders](docs/shaders/README.md)
* [Pipeline JSON](docs/pipelines/README.md)

# Demo: empty frame
![](media/black.png)  
![](docs/media/black.png.RITEG0000.jpg)  

# Demo: PM5544 test pattern
![](media/test_PM5544.png)  
![](docs/media/test_PM5544.png.RITEG0008.jpg)

# Demo: SMPTE test pattern
![](media/test_SMPTE.png)  
![](docs/media/test_SMPTE.png.RITEG0009.jpg)  
