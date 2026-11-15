# Geartowns Shaders

Geartowns desktop shader presets use `.gshader` files. A preset is an INI-style file that describes one or more GLSL fragment shader passes. Presets placed in this `shaders` directory, up to 64, are discovered once when the desktop app starts and appear in the `Video > Shader` menu after `Pixel Perfect`. Shader presets are not applied while the debugger is enabled.

GLSL files are normal fragment shaders. Geartowns supplies a fullscreen vertex shader and prepends the correct GLSL version, so shader files should not include a `#version` line.

## Minimal Preset

```ini
[Preset]
Name=Scanlines + Bilinear
Passes=1

[Pass0]
Path=scanlines.glsl
ScaleType=Viewport
Filter=Linear

[Parameters]
ScanlineIntensity=0.10

[Parameter.ScanlineIntensity]
Label=Intensity
Min=0.0
Max=1.0
Step=0.01
```

## Preset Sections

`[Preset]`

- `Name`: display name shown in the menu. If omitted, the filename without its extension is used.
- `Group`: optional submenu name. Presets with the same group are listed together in that submenu.
- `Passes`: number of passes, from 1 to 32.
- `Variants`: optional number of `[VariantN]` sections, up to 16.
- `DefaultVariant`: variant applied when the preset is loaded for the first time. Defaults to `0`.

`[PassN]`

- `Path`: GLSL fragment shader path. Relative paths are resolved from the `.gshader` file directory first, then from the `shaders` resource directory.
- `Alias`: optional name for the pass output. Later passes can sample it with that name. See Shader Inputs.
- `Filter`: `Nearest` by default. Use `Linear` or `Bilinear` for bilinear sampling on that pass input/output texture.
- `Feedback`: `true` when the pass needs the previous frame through `PassFeedback0`.
- `History`: `true` when the pass needs previous inputs to the same pass through `SourceHistory0` through `SourceHistory7`.
- `ScaleType`: sets both axes. Defaults to `Viewport`. Values are:
  - `Source` (or `Original`): relative to the original emulator frame, at native core resolution.
  - `Previous`: relative to the previous pass output. This is RetroArch's `source` scale type.
  - `Viewport` (or `FinalViewport`): relative to the final display viewport.
  - `Absolute`: fixed size from `AbsoluteWidth` and `AbsoluteHeight`.
- `ScaleTypeX`, `ScaleTypeY`: optional per-axis scale type overrides.
- `Scale`: output scale for both axes. Defaults to `1.0`.
- `ScaleX`, `ScaleY`: optional per-axis scale overrides.
- `AbsoluteWidth`, `AbsoluteHeight`: used with `ScaleType=Absolute`.
- `FloatFramebuffer`: `true` when the pass output needs signed or high-range intermediate values. Uses a floating-point render target.

The final pass is what Geartowns displays. Non-final passes render to intermediate textures. Only the final pass needs
`Viewport`; passes that work at core resolution should use `Source` or `Previous` to avoid rendering at display size.

`[Parameters]`

Each entry creates a float uniform with the same name:

```ini
[Parameters]
Amount=0.5
```

Optional metadata for sliders:

```ini
[Parameter.Amount]
Label=Amount
Min=0.0
Max=1.0
Step=0.01
```

`Label` defaults to the parameter name. `Min`, `Max` and `Step` default to `0.0`, `1.0` and `0.01`, and the `[Parameters]`
value is clamped to the `Min` to `Max` range.

A parameter with `Min=0.0`, `Max=1.0` and a `Step` of `1.0` or higher is displayed as a checkbox. Other parameters with
a `Step` of `1.0` or higher are displayed as integer sliders.

Optional `Group` puts the parameter in a collapsible section of the Shader Parameters window, which opens from
`Video > Shader > Parameters...`:

```ini
[Parameter.Amount]
Label=Amount
Group=Glow
```

A preset can expose up to 256 parameters.

`[VariantN]`

A variant is a named set of parameter values. Selecting it resets every parameter to its `[Parameters]` value and then
applies the variant values. Variants are listed as submenus of the preset in `Video > Shader` and in the Shader
Parameters window. `Restore Defaults` restores the selected variant.

```ini
[Preset]
Name=My CRT
Variants=2

[Variant0]
Name=Slot Mask

[Variant1]
Name=Aperture Grille
MaskType=2.0
```

## Shader Inputs

Available samplers:

- `uniform sampler2D Source`: current pass input.
- `uniform sampler2D Original`: original emulator frame.
- `uniform sampler2D PassFeedback0`: previous final output, for feedback presets.
- `uniform sampler2D SourceHistory0` through `SourceHistory7`: previous inputs to the same pass, newest first, for history presets.
- `uniform sampler2D PassOutputN`: output of any previous pass, where `PassOutput0` is pass 0 output, `PassOutput1` is pass 1 output, and so on.
- `uniform sampler2D <Alias>`: output of a previous pass with that `Alias`.
- `uniform sampler2D <Alias>Feedback`: previous frame output of the non-final pass with that `Alias`. A pass can sample its own feedback.

A pass can use up to 16 samplers.

Available size uniforms are `vec4(width, height, 1.0 / width, 1.0 / height)`:

- `SourceSize`: current pass input size.
- `OriginalSize`: original emulator frame size.
- `OutputSize`: current pass output size.
- `FinalViewportSize`: final display viewport size.
- `PassFeedback0Size`: feedback texture size.
- `SourceHistorySize`: source history texture size.
- `PassOutputSizeN` (or `PassOutputNSize`): previous pass output texture sizes.
- `<Alias>Size`: output size of the pass with that `Alias`.

Other uniforms:

- `FrameCount`: increments once per new emulated frame. It starts at `0` when a preset is loaded.
- `FrameDirection`: `1` normally, `-1` while rewinding.
- `SourceHistoryCount`: number of valid source history textures, from `0` to `8`.
- `OriginalAspect`: display width / display height of the picture in CRTC dots and lines at full resolution. It does
  not change with Native Resolution.
- `FramebufferScale`: display pixel density, as the rounded ratio between framebuffer pixels and window points, at
  least `1.0`. Use it to keep masks and grids at a fixed physical size on high-density displays.
- `BackgroundColor`: configured normal desktop background color as `vec4(r, g, b, 1)`.

FM Towns video uniforms, updated every frame. They describe the frame in `Original`, so they follow the video mode
the program sets and the `Video > Native Resolution` option:

- `float OriginalFPS`: emulated frame rate in Hz, following the CRTC vertical period: about `59.94` in the standard
  15.73 and 31.47 kHz modes, about `55.4` in the 24.37 kHz 640x400 mode, and `59.94` on the Marty in every mode. `60.0`
  while the CRTC is stopped.
- `float PixelClock`: rate of the `Original` texels in MHz: the CRTC dot clock divided by the dots per texel. The dot
  clock is `28.636`, `24.545`, `25.175` or `21.053`, chosen by the program, or `25.175` or `65.0` on the high-res CRTC.
  With `Native Resolution` on, a texel is one layer pixel, so the dot clock is divided by the horizontal layer zoom
  (the common zoom when both layers are visible): `25.175` for the 640 pixel 31.47 kHz modes, `21.053` for the 640
  pixel 24.37 kHz modes, `14.318` for the 720 pixel 15.73 kHz mode and `6.136` for the 320 pixel 15.73 kHz modes. With
  it off, a texel is one dot, or two dots when the picture is wider than 1024 dots. `0.0` while the CRTC is stopped.
- `float LineRate`: horizontal scan rate in kHz, the CRTC dot clock divided by the dots per line the program sets. It
  is `15.734` in the standard 15.73 kHz modes, `24.366` in 24.37 kHz modes and `31.469` in 31.47 kHz modes. The Marty
  fits its 24 and 31 kHz modes into its NTSC field, two CRTC lines per TV line, and reports them at `31.469`. `0.0`
  while the CRTC is stopped. CRT shaders that otherwise guess interlacing from the height of `Original` use
  `Interlaced` instead whenever `LineRate` is above `0.0`.
- `float SourceLines`: number of scanlines `Original` covers. Scanline shaders use it instead of `OriginalSize.y`, so
  the scanlines follow the CRTC lines when a row of `Original` spans several of them. It equals `OriginalSize.y` with
  `Native Resolution` off and in interlaced modes. With it on it is `OriginalSize.y` times the vertical layer zoom,
  for example `480` for a picture `240` rows tall that doubles every line.
- `int Interlaced`: `1` when `Original` holds both interlaced fields woven into one image, `0` otherwise. The CRTC
  interlaces when its frame has an odd number of half-lines, as in the standard 15.73 kHz modes (525 half-lines). Even
  `Original` rows then come from the first field and odd rows from the second, and `SourceLines` equals
  `OriginalSize.y`. The Marty counts 525 half-lines in all its 15.73 kHz modes, so they are always `1`; its standard 24
  and 31 kHz modes are `0` because `Original` holds the whole progressive frame.
- `int FieldParity`: `0` or `1`, the field the CRTC drew last. It flips every frame while `Interlaced` is `1`, and is
  `0` otherwise. Rows of `Original` with `row % 2 == FieldParity` belong to that field, which is what interlace
  shaders use it for.
- `int SystemType`: `0` FM Towns computers, `1` FM Towns Marty, which outputs NTSC video to a TV.
- `int VideoMode`: `0` standard CRTC, `1` high-resolution CRTC, on the models that have one.

`FrameCount`, `SourceHistory`, `PassFeedback0` and `<Alias>Feedback` only advance when the emulator produces a new
frame, or when rewinding steps back one. Redraws while paused, after a window resize or after a parameter change
render the same frame again with the same state.

Any parameter declared in `[Parameters]` is also available as a `float` uniform.

## Minimal GLSL Shader

```glsl
in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D Source;

void main()
{
    FragColor = texture(Source, vTexCoord);
}
```

## Feedback Example

```ini
[Preset]
Name=Feedback Blend
Passes=1

[Pass0]
Path=feedback_blend.glsl
ScaleType=Viewport
Filter=Nearest
Feedback=true

[Parameters]
FeedbackAmount=0.5
```

A feedback shader can sample the previous final frame. Save this one as `feedback_blend.glsl` next to the preset:

```glsl
in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D Source;
uniform sampler2D PassFeedback0;
uniform float FeedbackAmount;

void main()
{
    vec3 current = texture(Source, vTexCoord).rgb;
    vec3 previous = texture(PassFeedback0, vTexCoord).rgb;
    FragColor = vec4(mix(current, previous, FeedbackAmount), 1.0);
}
```
