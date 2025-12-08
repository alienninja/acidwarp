# Acidwarp Web

WebGL/Emscripten port of the classic Acidwarp visual effects program.

## Building

### Prerequisites

Install the Emscripten SDK:

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
cd ..
```

### Build

```bash
source emsdk/emsdk_env.sh
./build.sh
```

### Test Locally

```bash
python3 -m http.server 9090
# Open http://localhost:9090
```

## Deploying to GitHub Pages

The `index.html` file is included in the repo. To enable GitHub Pages:

1. Go to your repo Settings → Pages
2. Set Source to "Deploy from a branch"
3. Select `main` branch and `/acidwarp.web` folder
4. Save

Your site will be available at `https://YOUR_USERNAME.github.io/acidwarp/`

## Files

- `acidwarp.c` - Main source with WebGL shaders
- `build.sh` - Build script
- `shell.html` - Emscripten HTML template
- `index.html` - Built output (commit this for GitHub Pages)
- `index.js` / `index.wasm` - Build artifacts (gitignored)
