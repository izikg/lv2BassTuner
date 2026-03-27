# BassTuner

A bass guitar pitch detector LV2 plugin for Ubuntu and Darkglass Anagram.

Detects the fundamental note played on a bass guitar (30–350 Hz range) using
the YIN algorithm and exposes three output control ports:

| Port        | Range      | Description                        |
|-------------|------------|------------------------------------|
| `midi_note` | 0–127      | Nearest MIDI note number           |
| `cents`     | -50..+50   | Cents deviation from equal temp.   |
| `frequency` | 0–500 Hz   | Raw detected frequency             |

---

## Building for Ubuntu (native, for testing)

```sh
# 1. Install dependencies
sudo apt install cmake build-essential lv2-dev

# 2. Configure and build
cmake -S . -B build-ubuntu -DCMAKE_BUILD_TYPE=Release
cmake --build build-ubuntu -- -j$(nproc)

# 3. Install locally for testing
mkdir -p ~/.lv2
cp -r build-ubuntu/BassTuner_artefacts/Release/LV2/BassTuner.lv2 ~/.lv2/

# 4. Test in a host (e.g. jalv or Carla)
jalv https://yourwebsite.com/plugins/BassTuner
```

---

## Building for Darkglass Anagram (cross-compile, ARM64)

### Step 1: Bootstrap the toolchain (one-time, ~1 hour, ~5 GB)

```sh
git clone https://github.com/mod-audio/mod-plugin-builder
sudo apt install acl bc curl cvs git mercurial rsync subversion wget \
    bison bzip2 flex gawk gperf gzip help2man nano perl patch tar texinfo unzip \
    automake binutils build-essential cpio libtool libncurses-dev pkg-config \
    python-is-python3 libtool-bin
./mod-plugin-builder/bootstrap.sh darkglass-anagram
```

### Step 2: Build

```sh
source /path/to/mod-plugin-builder/local.env darkglass-anagram
cmake -S . -B build-anagram
$(which cmake) --build build-anagram
```

### Step 3: Deploy to Anagram (Developer Mode required)

```sh
cd build-anagram
scp -O -r *.lv2 root@192.168.51.1:/root/.lv2/
ssh root@192.168.51.1 "systemctl restart jack2 lvgl-app"
```

---

## Architecture

```
PluginProcessor.h/.cpp
├── YinDetector         — pitch detection (30–350 Hz, ~93 ms update rate)
├── NoteUtil            — freq → MIDI note, note name, cents deviation
└── BassTunerProcessor  — JUCE AudioProcessor, Anagram-compatible
```

## Key Anagram constraints respected

- `createEditor()` returns `nullptr` — no desktop GUI (Anagram has no X11/Wayland)
- `getBypassParameter()` implemented — required for smooth bypass
- `getAlternateDisplayNames()` returns `"BTN"` — abbreviation for bindings screen
- All output values exposed as LV2 Control Ports (not Patch Parameters)
- Plugin passes audio through unmodified — pure analyser

---

## Customise

- Change `COMPANY_NAME`, `COMPANY_WEBSITE`, `LV2_URI` in `CMakeLists.txt`
- Replace `images/block-on.png` and `images/block-off.png` with your artwork (200×200 px PNG)
- Tune `YinDetector::kThreshold` (default `0.15`) for more/less aggressive detection
