# Raymarching in 500 Bytes of C

*A flat pixel buffer becomes 3D with zero geometry, zero GPU. The framebuffer IS the render pipeline.*

---

## Forget Everything About 3D Rendering

Traditional 3D rendering is a pipeline factory. You load vertex buffers, assemble triangles, rasterize them onto a depth buffer, run fragment shaders, and finally write to a framebuffer. Each stage has its own data structure, its own coordinate space, its own bugs.

SDF raymarching throws all of that away. The entire renderer is:

```c
for (int y = 0; y < 100; y++) {
    for (int x = 0; x < 160; x++) {
        uint32_t color = march(ray(x, y));
        rb_pixel(s, x*2, y*2, color);
    }
}
```

That's it. No vertex buffers. No triangle lists. No rasterizer. No depth buffer. The framebuffer IS the pipeline — you write directly to a `uint32_t` array, and each pixel is an independent computation. You could shuffle the pixel order, skip every other one, or run them on different cores. There is no dependency between pixels.

The entire "GPU" is a nested for-loop and some `sinf()` calls. The `rawbuf.h` surface — the same one that renders plasma and fire — is the only output target.

---

## Step 1 — A Sphere From Nothing

Every object in SDF world is defined by a single function: given a point in space, return how far that point is from the surface. Negative means inside. Positive means outside. Zero is the surface.

A sphere centered at the origin with radius `r`:

```c
float sd_sphere(vec3 p, float r) {
    return length(p) - r;
}
```

One line. That's the entire sphere. No mesh, no vertices, no UV mapping.

To see it, we "march" a ray from the camera. Start at the camera position, step forward by the distance the SDF reports, and repeat. When the distance drops below a threshold (0.002), you've hit something:

```c
float t = 0;
for (int i = 0; i < 64; i++) {
    vec3 p = cam_pos + rd * t;
    float d = sd_sphere(p, 1.0);
    if (d < 0.002) break;  // hit!
    t += d;                 // step by the safe distance
}
```

This is ~15 lines of C and produces a white circle on black. Each pixel sends its own ray and marches independently — the buffer is just a 2D grid of independent ray results.

---

## Step 2 — Add a Floor

A floor is the simplest possible SDF:

```c
float sd_plane(float py, float h) {
    return py - h;
}
```

To combine sphere and floor, just take the minimum. The closest surface wins:

```c
float d = fminf(sd_sphere(p, 1.0), sd_plane(p.y, -0.5));
```

That's `min()`. The sphere floats above a plane. Still no lighting, but you can see the geometry because the march terminates at different depths for different pixels.

---

## Step 3 — Lighting Is Just a Dot Product

To light a surface, you need its normal — the direction it faces. With SDFs, the normal is computed by sampling the distance field in 6 nearby points and taking the gradient:

```c
vec3 scene_normal(vec3 p, float t, int scene) {
    const float e = 0.001;
    float d = scene_sdf(p, t, scene);
    return normalize(vec3(
        scene_sdf(p + vec3(e,0,0), t, scene) - d,
        scene_sdf(p + vec3(0,e,0), t, scene) - d,
        scene_sdf(p + vec3(0,0,e), t, scene) - d
    ));
}
```

Six SDF evaluations. The normal falls out of the math — no per-vertex normal storage, no smooth groups, no tangent space.

Diffuse lighting is then one dot product:

```c
float diff = fmaxf(0, dot(normal, light_dir));
```

Three multiplies, two adds. That's diffuse shading.

For the checkerboard floor, we use the hit position as a world-space lookup table — same concept as a palette LUT, but in 3D:

```c
int check = ((int)floorf(hx) + (int)floorf(hz)) & 1;
```

---

## Step 4 — Shadows Are a Second March

Here's where it gets elegant. To test if a point is in shadow, you march a second ray — from the hit point toward the light. If it hits something before reaching the light, the point is in shadow.

```c
float shadow_t = 0.05;  // offset to avoid self-intersection
for (int si = 0; si < 32; si++) {
    vec3 sp = hit_pos + light_dir * shadow_t;
    float sd = scene_sdf(sp, t, scene);
    if (sd < 0.002) { diff *= 0.3; break; }  // in shadow
    shadow_t += sd;
}
```

Same `scene_sdf()` function. Same march loop. Different starting point and direction. The shadow is a second *read pass* over the exact same distance field — no shadow maps, no shadow volumes, no separate data structure. The SDF IS the shadow caster and the shadow receiver.

---

## Step 5 — Smooth Blending = Buffer Blending

The `min(d1, d2)` union creates sharp intersections. Smooth union creates organic, melted-together shapes:

```c
float op_smooth_union(float d1, float d2, float k) {
    float h = fmaxf(k - fabsf(d1 - d2), 0.0) / k;
    return fminf(d1, d2) - h * h * k * 0.25;
}
```

This is analogous to alpha blending in a framebuffer. Where alpha blending interpolates two pixel colors based on transparency, smooth union interpolates two distances based on proximity. The parameter `k` controls the blend width — larger `k` means a wider, softer transition, exactly like a wider alpha gradient.

The blob scene in the playground uses this to merge four spheres into a single organic shape:

```c
float d = sd_sphere(p, 0.5);
d = op_smooth_union(d, sd_sphere(p2, 0.35), 0.4);
d = op_smooth_union(d, sd_sphere(p3, 0.30), 0.4);
d = op_smooth_union(d, sd_sphere(p4, 0.25), 0.4);
```

Each call blends another sphere into the existing field. The result is a pulsing, breathing blob with no hard edges — and the entire geometry is four function calls.

---

## The Full Raymarcher in ~500 Bytes

Here's the complete core logic. SDF primitives, scene composition, normal computation, shadow casting, and shading — all in about 44 lines of functional C:

```c
float sd_sphere(vec3 p, float r) { return v3_len(p) - r; }
float sd_torus(vec3 p, float R, float r) {
    float q = sqrtf(p.x*p.x + p.z*p.z) - R;
    return sqrtf(q*q + p.y*p.y) - r;
}
float sd_plane(float py, float h) { return py - h; }
float op_smooth_union(float d1, float d2, float k) {
    float h = fmaxf(k - fabsf(d1-d2), 0) / k;
    return fminf(d1, d2) - h*h*k*0.25f;
}

float scene_sdf(vec3 p, float t) {
    vec3 rp = rotate_y(p, t);
    float d = op_smooth_union(
        sd_torus(rp, 0.8, 0.25),
        sd_sphere(offset(p, t), 0.35), 0.3);
    return fminf(d, sd_plane(p.y, -0.5));
}

// Per-pixel: march → normal → shadow → shade → write
for (int y = 0; y < 100; y++)
  for (int x = 0; x < 160; x++) {
    vec3 rd = ray_dir(x, y, fov, cam);
    float t = 0;
    for (int i = 0; i < 64; i++) {
        float d = scene_sdf(cam + rd*t, time);
        if (d < 0.002) break;
        t += d;
    }
    vec3 n = normal(cam + rd*t, time);
    float light = dot(n, light_dir);
    light *= shadow(cam + rd*t, light_dir, time);
    rb_pixel(s, x*2, y*2, shade(light));
  }
```

For comparison, a traditional rasterizer rendering the same scene would need: a torus mesh (~2000 triangles), a sphere mesh (~500 triangles), a floor quad, vertex and index buffers, a vertex shader, a fragment shader, a depth buffer, shadow map generation (another full render pass with its own framebuffer), and shadow map sampling. That's thousands of lines of setup code before you draw a single pixel.

The SDF version fits in a comment block.

---

## Try It

The playground's **Raymarch** tab implements this entire article. Drag to orbit, switch scenes (Torus+Sphere, Pillars, Blob), adjust FOV and rotation speed. Every pixel you see was independently ray-marched through pure math — no geometry was harmed in the making of this demo.

The full C implementation is in `demos/raymarch_sdf.c` — arrow keys to orbit, 1/2/3 to switch scenes.
