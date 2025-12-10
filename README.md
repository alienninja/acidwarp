# Acidwarp

A mesmerizing visual effects program originally created by Noah Spurrier in 1992-1993, ported to Linux by Steven Wills, and now available as a WebGL browser application.

## 🌐 Try It Online

**[Launch Acidwarp Web](https://acidwarp.bithash.cc/)**

## Features

### Classic Mode (41 Original Patterns)
- All original acidwarp visual patterns
- Palette cycling and color effects
- Faithful recreation of the DOS-era experience

### Modern Mode (6 GPU-Accelerated Effects)
- **Plasma** - Classic plasma waves with shifting colors
- **Tunnel** - Infinite twisting tunnel zoom
- **Rings** - Expanding concentric rings
- **Swirl** - Endless spiral vortex
- **Mandelbrot** - Animated fractal with rainbow colors
- **Burning Ship** - Burning Ship fractal visualization

## Controls

| Key | Action |
|-----|--------|
| `SPACE` | Pause/Resume |
| `LEFT/RIGHT` | Previous/Next effect |
| `M` | Toggle Classic/Modern mode |
| `L` | Lock current pattern |
| `F` | Toggle fullscreen |
| `ESC` | Quit |

## Building

### Web Version (Emscripten/WebGL)

```bash
cd acidwarp.web

# Install Emscripten SDK (first time only)
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk && ./emsdk install latest && ./emsdk activate latest && cd ..

# Build
source emsdk/emsdk_env.sh
./build.sh

# Test locally
python3 -m http.server 9090
# Open http://localhost:9090
```

### CLI Version (Linux/SDL2 + OpenGL)

```bash
cd acidwarp

# Install dependencies (Debian/Ubuntu)
sudo apt install libsdl2-dev libglew-dev libgl-dev

# Build
make
# or with CMake:
mkdir build && cd build && cmake .. && make

# Run
./acidwarp
```

## Project Structure

```
acidwarp/
├── acidwarp/           # Original CLI version
│   ├── acidwarp.c      # Main source
│   ├── bit_map.c/h     # Pattern generation
│   ├── lut.c/h         # Lookup tables
│   ├── palinit.c/h     # Palette initialization
│   ├── rolnfade.c/h    # Palette rolling/fading
│   └── warp_text.c/h   # Text effects
└── acidwarp.web/       # WebGL version
    ├── acidwarp.c      # Modified main with WebGL shaders
    ├── build.sh        # Emscripten build script
    ├── shell.html      # HTML template
    └── index.html      # Built output (for GitHub Pages)
```

## History

- **1992-1993**: Original DOS version by Noah Spurrier
- **1996**: Linux port by Steven Wills
- **2025**: WebGL port with modern GPU effects

## License

Original Acidwarp is Copyright (c) 1992, 1993 by Noah Spurrier.
Linux port by Steven Wills.
WebGL port and modern effects added 2025.

## Credits

- **Noah Spurrier** - Original author
- **Steven Wills** - Linux port
- https://www.noah.org/acidwarp/
- https://github.com/dreamlayers/acidwarp

[](http://api.bithash.cc) 
