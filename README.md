# GENajam Pico v1.19

A Raspberry Pi Pico (RP2040) port of the GENajam MIDI controller for Little-scale's GENMDM module. This is a modernized version of the original Arduino Mega 2560 GENajam by JAMATAR, featuring enhanced file browsing, real-time MIDI display, and improved responsiveness.

## Features

### Core Functionality
- **6-channel FM synthesis control** via GENMDM module
- **Polyphonic and monophonic modes** with intelligent voice allocation
- **Real-time parameter editing** with 4 potentiometers
- **TFI file management** (load, save, delete) with SD card support
- **OLED display** (128x32) for visual feedback
- **MIDI panic button** for emergency note-off

### Enhancements Over Original
- **Recursive directory scanning** - finds TFI files in subdirectories
- **Delayed loading system** - smooth file browsing without MIDI lag
- **Accelerated navigation** - hold left/right for fast file scrolling
- **Real-time MIDI display** - shows incoming channel and note names (C4, F#3, etc.)
- **Bar Graph Viz** - Bar Graph shows incoming MIDI CH. 1-11
- **Modern hardware support** - optimized for RP2040 architecture

## Hardware Requirements

### Core Components
- Raspberry Pi Pico (RP2040)
- 128x32 OLED display (SSD1306, I2C)
- MicroSD card module (SPI)
- TBD

### Pin Configuration
```
OLED (I2C):
- SDA: Pin 8
- SCL: Pin 9

SD Card (SPI):
- CS:   Pin 5
- MISO: Pin 4
- SCK:  Pin 6
- MOSI: Pin 7

MIDI:
- TX: Pin 0
- RX: Pin 1

Potentiometers (Analog):
- Multiplexer: Pin 28
- Multiplexer: C0-C3 to Pots
- Multiplexer S0-S3: Pin 10-13

Buttons (Digital, with pullups):
- Preset/Edit: Pin 20
- Left:        Pin 14
- Right:       Pin 15
- CH Up:       Pin 16
- CH Down:     Pin 17
- Mono/Poly:   Pin 18
- Delete:      Pin 19
- Panic:       Pin 21
```

## Operation Modes

### Mode 1: Mono Preset
- Browse and load TFI files per channel
- Use UP/DOWN to select FM channel (1-6)
- Use LEFT/RIGHT to browse presets
- Immediate loading for responsive channel switching

### Mode 2: Mono Edit
- Real-time FM parameter editing
- 4 pots control selected parameter screen
- 13 parameter screens available
- Changes apply to current channel only

### Mode 3: Poly Preset
- Browse presets for polyphonic play
- All 6 channels loaded with same preset
- 2-second delayed loading prevents browsing lag
- DELETE button removes files

### Mode 4: Poly Edit
- Real-time editing in poly mode
- Parameter changes affect all 6 channels
- Same 13 parameter screens as mono mode

## MIDI Features

### Polyphony System
- **6-voice polyphony** with intelligent voice stealing
- **Sustain pedal support** (CC#64)
- **Modulation wheel control** (CC#1) - controls LFO on/off
- **Pitch bend** - affects all voices in poly mode
- **Velocity curve** - musical response across dynamic range

### Voice Management
- Protects lowest note from voice stealing
- Random stereo positioning in poly mode
- Proper note-off handling with sustain
- MIDI panic clears all stuck notes

## File Management

### TFI File Support
- **Recursive directory scanning** - finds files in any folder depth
- **Fast file browsing** with acceleration (hold buttons for turbo speed)
- **Save new patches** - automatically numbered (newpatch001.tfi, etc.)
- **Overwrite existing** - update files in place
- **Delete files** - with confirmation prompt

### SD Card Structure
```
/
├── presets/
│   ├── bass/
│   │   ├── bass001.tfi
│   │   └── bass002.tfi
│   └── lead/
│       ├── lead001.tfi
│       └── lead002.tfi
├── drums/
│   ├── kick.tfi
│   └── snare.tfi
└── patch001.tfi
```

## Getting Started

### Arduino IDE Setup
1. Install RP2040 board package
2. Install required libraries:
   - Adafruit GFX Library
   - Adafruit SSD1306
   - MIDI Library
   - Wire (built-in)
   - SPI (built-in)
   - SD (built-in)
   - EEPROM (built-in)

### Initial Configuration
1. **Boot menu**: Hold SELECT on startup to configure MIDI channel and region (NTSC/PAL)
2. **Load TFI files** onto SD card in root directory or subfolders
3. **Connect GENMDM** via MIDI OUT
4. **Power on** and start making music!

## Controls

### Navigation
- **SELECT**: Toggle between Preset/Edit modes
- **MONO/POLY**: Toggle between Mono/Poly modes
- **LEFT/RIGHT**: Browse files or parameter screens
- **UP/DOWN**: Select channel (mono) or save options (poly)
- **DELETE**: Delete files (poly mode only)
- **PANIC**: Emergency all-notes-off

### Editing
- **4 Potentiometers**: Control selected parameters
- Real-time visual feedback on OLED
- Parameter values update immediately
- Changes affect current channel (mono) or all channels (poly)

## Parameter Screens

1. **Algorithm, Feedback, Pan** - Core FM structure
2. **OP Volume** - Individual operator levels
3. **Frequency Multiple** - Harmonic ratios
4. **Detune** - Fine frequency adjustments
5. **Rate Scaling** - Envelope scaling with pitch
6. **Attack** - Envelope attack rates
7. **Decay 1** - Initial decay rates
8. **Sustain** - Sustain levels
9. **Decay 2** - Secondary decay rates
10. **Release** - Release rates
11. **SSG-EG** - Special envelope modes
12. **Amp Modulation** - Amplitude modulation on/off
13. **LFO/FM/AM** - Global modulation controls

## Technical Notes

### Performance Optimizations
- **Delayed loading** prevents MIDI lag during file browsing
- **MIDI processing** during parameter updates maintains responsiveness
- **Efficient file scanning** with progress display
- **Button acceleration** for fast navigation

### Memory Management
- **EEPROM storage** for MIDI channel and region settings
- **Efficient file indexing** supports 999 TFI files
- **Real-time parameter tracking** for all 6 channels

## Credits

- **Original GENajam**: JAMATAR (2021)
- **GENMDM Module**: Little-scale
- **Velocity Curve**: impbox
- **Base Project**: Catskull Electronics

## License

Open source hardware and software project. Use and modify as needed for your musical creations.

---

*Built for musicians who want hands-on control of FM synthesis with modern reliability and features.*
