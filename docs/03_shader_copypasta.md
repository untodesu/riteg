# Shader copypasta
### GLSL version
```glsl
#version 450 core
```

### Fragment output and vertex input
Vertex shader passes a single 2D vector with UV coordinates:  

```glsl
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 target;
```

### Uniforms
* 16 pass parameters are split into four 4D vectors.
* Pass output framebuffer size is defined as `screen.xy` and window framebuffer size is defined as `screen.zw`.  
* Frametime is `timing.x`  
* Current time is `timing.y`  

```glsl
layout(binding = 0, std140) uniform params {
    vec4 param_a;
    vec4 param_b;
    vec4 param_c;
    vec4 param_d;
    vec4 screen;
    vec4 timing;
};
```

### Samplers
```json
"samplers": ["image", "noise"]
```
This will bind two samplers:  

```glsl
layout(binding = 0) uniform sampler2D s1; /* image */
layout(binding = 1) uniform sampler2D s2; /* noise */
```

### Full shader template
```glsl
#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 target;

layout(binding = 0, std140) uniform params {
    vec4 param_a;
    vec4 param_b;
    vec4 param_c;
    vec4 param_d;
    vec4 screen;
    vec4 timing;
};

void main(void)
{
    target = vec4(uv.x, uv.y, 1.0, 1.0);
}
```

### Example: checkerboard.frag
```glsl
#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 target;

layout(binding = 0, std140) uniform params {
    vec4 param_a;
    vec4 param_b;
    vec4 param_c;
    vec4 param_d;
    vec4 screen;
    vec4 timing;
};

void main(void)
{
    const vec2 pv = floor(screen.xy * vec2(uv.x, 1.0 - uv.y) / param_a.x);
    const float value = 1.0 - mod(pv.x + mod(pv.y, 2.0), 2.0);
    target.x = value;
    target.y = value;
    target.z = value;
    target.w = value;
}
```
