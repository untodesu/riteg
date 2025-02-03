# Tips and Tricks
### Add sound to the video
Say, you have processed some video container. Now it has no sound.  
The solution is simple: supply two inputs into commandline `ffmpeg` utility and map channels accordingly:  

```
ffmpeg -i processed.mp4 -i source.mp4 -map 0:v:0 -map 1:a:0 processed_with_sound.mp4
```

### Changing H.264's CRF, preset and more
**UNDONE:** treat all other command-line options as `key=value` pairs to feed into the output `AVCodecContext` instance.  
**UNDONE:** write me!  
