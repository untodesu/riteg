# JSON format
### Example: checker.json
```json
{
  "textures": {
    "final": {
      "width": 800,
      "height": 600,
      "filter": false
    }
  },
  "blit": "final",
  "passes": [
    {
      "shader": "shaders/checkerboard.frag",
      "samplers": [],
      "output": "final",
      "params": [ 32 ]
    }
  ]
}
```

### Textures
* A key-value list of named textures (framebuffers as well)
* Texture names must start with a letter (A-Z and a-z).
* Texture names starting with `!` are considered reserved.
1. `width`: texture's width (should be greater or equal to 1).
2. `height`: texture's height (should be greater or equal to 1).
3. `filter`: when true, linear filtering is applied when sampling the texture.

### Reserved texture names in use
* `!blank`: a blank texture.  
* `!image`: the source image.  
* `!vflip`: the source image flipped vertically.  
* `!frame`: the processed image blitted last frame.  

### Blit
After running all the passes, RITEG will blit (copy) the specified texture to the window's framebuffer.

### Passes
* An array of objects defining each render pass ran in the exact oreder.
1. `shader`: path to a fragment shader.
2. `samplers`: an array of texture names bound their respective slots. Reserved texture names can be used here.
3. `output`: name of the texture (framebuffer) this pass will render into
4. `params`: an array of up to 16 floating point parameters passed to the shader.
