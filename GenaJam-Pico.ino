// GENajam v1.18 Arduino Pico Port - Crunchypotato 2025-SEPTEMBER
// Ported from Arduino Mega 2560 version
// originally by/forked from JAMATAR 2021-AUGUST
// --------------------
// This is a front end for Little-scale's GENMDM module for Mega Drive
// Now for: Raspberry Pi Pico (RP2040) using Arduino IDE
// OLED display via I2C, SD card via SPI

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SD.h>
#include <EEPROM.h>
#include <MIDI.h>

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

// Pin definitions
#define OLED_SDA_PIN 8
#define OLED_SCL_PIN 9
#define SD_CS_PIN 5
#define SD_MISO_PIN 4
#define SD_SCK_PIN 6
#define SD_MOSI_PIN 7
#define MIDI_RX_PIN 1
#define MIDI_TX_PIN 0
#define POT_OP1_PIN 26
#define POT_OP2_PIN 27
#define BTN_PRESET_PIN 20
#define BTN_PANIC_PIN 22
#define BTN_LEFT_PIN 14
#define BTN_RIGHT_PIN 15
#define BTN_CH_UP_PIN 16
#define BTN_CH_DOWN_PIN 17
#define BTN_MONO_POLY_PIN 18
#define BTN_DELETE_PIN 23
#define BTN_FUTURE_PIN 11

// Multiplexer control pins
#define MUX_S0_PIN 10
#define MUX_S1_PIN 11
#define MUX_S2_PIN 12
#define MUX_S3_PIN 13
#define MUX_SIG_PIN 28

// Display dimensions and setup
#define OLED_WIDTH 128
#define OLED_HEIGHT 32
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// Button definitions
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnPOLY   5
#define btnBLANK  6
#define btnNONE   7

// MIDI constants
#define MIDI_BAUD_RATE 31250

// Timing constants
const uint16_t debouncedelay = 200;
const uint16_t messagedelay = 700;

// Global variables
uint8_t lcd_key = 0;
unsigned long messagestart = 0;
uint8_t refreshscreen = 0;

// File handling
const uint8_t MaxNumberOfChars = 21;
uint16_t n = 0;
const uint16_t nMax = 999;
char filenames[nMax][MaxNumberOfChars + 1];
static const uint8_t FullNameChars = 96;
char fullnames[nMax][FullNameChars];
unsigned long tfi_select_time = 0;
bool tfi_pending_load = false;
uint16_t pending_tfi_channel = 1;
bool showing_loading = false;
unsigned long button_hold_start_time = 0;
uint8_t last_held_button = btnNONE;
bool button_is_held = false;
const uint16_t initial_debounce = 200;    // Initial slow debounce
const uint16_t fast_debounce = 50;        // Fast debounce after acceleration
const uint16_t turbo_debounce = 20;       // Turbo speed for long holds
const uint16_t hold_threshold_1 = 1000;   // 1 second to start acceleration
const uint16_t hold_threshold_2 = 3000;   // 3 seconds to go turbo

File dataFile;

// MIDI and settings
uint16_t tfifilenumber[6] = {0, 0, 0, 0, 0, 0};
uint8_t tfichannel = 1;
uint8_t mode = 3;
uint8_t region = 0;
uint8_t midichannel = 1;
uint8_t last_midi_channel = 0;
uint8_t last_midi_note = 0;
unsigned long last_midi_time = 0;
const uint16_t midi_display_timeout = 2000;
static bool display_needs_refresh = false;
static unsigned long last_display_update = 0;


// Visualizer
uint8_t voice_velocity[11] = {0,0,0,0,0,0,0,0,0,0,0};        // Current velocity per channel
uint8_t voice_peak[11] = {0,0,0,0,0,0,0,0,0,0,0};            // Peak hold values
unsigned long voice_peak_time[11] = {0,0,0,0,0,0,0,0,0,0,0}; // When peak was set
const uint16_t peak_hold_duration = 800;                      // Peak hold time in ms
const uint8_t velocity_decay_rate = 2;                        // How fast bars decay

// Polyphony settings
uint8_t polynote[6] = {0, 0, 0, 0, 0, 0};
bool polyon[6] = {0, 0, 0, 0, 0, 0};
bool sustainon[6] = {0, 0, 0, 0, 0, 0};
bool noteheld[6] = {0, 0, 0, 0, 0, 0};
bool sustain = 0;
uint8_t lowestnote = 0;      // Add this back - needed for voice stealing
uint8_t notecounter = 0;     // Add this back - needed for note counting

// FM parameter screen navigation
uint8_t fmscreen = 1;

// Global FM settings
uint8_t fmsettings[6][50];
uint8_t lfospeed = 64;
uint8_t polypan = 64;
uint8_t polyvoicenum = 6;

// Potentiometer values
uint8_t prevpotvalue[4];
bool menuprompt = 1;
bool booted = 0;

// Save file variables
uint16_t savenumber = 1;
char savefilefull[] = "newpatch001.tfi";

// Flash storage replacement with EEPROM
void flash_write_settings(uint8_t region_val, uint8_t midi_ch);
uint8_t flash_read_setting(uint8_t offset);

// Core functions
void setup_hardware(void);
void setup_midi(void);
void setup_sd(void);
void setup_oled(void);
uint8_t read_buttons(void);

// Mode and display functions
void modechange(int modetype);
void modechangemessage(void);
void bootprompt(void);

// File operations
void scandir(bool saved);
void saveprompt(void);
void savenew(void);
void saveoverwrite(void);
void deletefile(void);

void tfiselect(void);
void channelselect(void);
void tfisend(int opnarray[42], int sendchannel);

// FM synthesis functions
void fmparamdisplay(void);
void operatorparamdisplay(void);
void fmccsend(uint8_t potnumber, uint8_t potvalue);

// MIDI functions
void midi_send_cc(uint8_t channel, uint8_t cc, uint8_t value);
void midi_send_note_on(uint8_t channel, uint8_t note, uint8_t velocity);
void midi_send_note_off(uint8_t channel, uint8_t note, uint8_t velocity);
void midi_send_pitch_bend(uint8_t channel, int16_t bend);
void handle_midi_input(void);

// Here's the corrected handle_note_on function with proper visualizer integration:

void selectMuxChannel(uint8_t channel) {
    digitalWrite(MUX_S0_PIN, (channel & 0x01) ? HIGH : LOW);
    digitalWrite(MUX_S1_PIN, (channel & 0x02) ? HIGH : LOW);
    digitalWrite(MUX_S2_PIN, (channel & 0x04) ? HIGH : LOW);
    digitalWrite(MUX_S3_PIN, (channel & 0x08) ? HIGH : LOW);
    delayMicroseconds(10); // Small settling time
}

void handle_note_on(uint8_t channel, uint8_t note, uint8_t velocity) {
    // Only update displays when actually in those modes
    if (mode == 1 || mode == 3) {
        updateMidiDisplay(channel, note);
    }
    
    // Only update visualizer when in visualizer modes
    if ((mode == 5 || mode == 6) && channel >= 1 && channel <= 11) {
        updateVelocityViz(channel - 1, velocity);
    }
    
    // Apply velocity curve for more musical response
    velocity = (int)(pow((float)velocity / 127.0f, 0.17f) * 127.0f);
    
    bool repeatnote = false;
    
    if (mode == 3 || mode == 4 || mode == 6) { // if we're in poly mode
        if (channel == midichannel) {  // and is set to the global midi channel
            
            // Handle repeat notes - retrigger if same note is already playing
            for (int i = 0; i <= 5; i++) {
                if (note == polynote[i]) {
                    
                    if (polypan > 64) {
                        long randpan = random(33, 127);
                        midi_send_cc(i + 1, 77, randpan);
                    }
                    
                    midi_send_note_off(i + 1, polynote[i], velocity);
                    midi_send_note_on(i + 1, note, velocity);
                    noteheld[i] = true;
                    repeatnote = true;
                    break;
                }
                handle_midi_input();
            }
            
            if (!repeatnote) {
                // Find the lowest note to protect it from voice stealing
                lowestnote = polynote[0];
                for (int i = 0; i <= 5; i++) {
                    if (polynote[i] < lowestnote && polynote[i] != 0) {
                        lowestnote = polynote[i];
                    }
                    handle_midi_input();
                }
                
                // Pick a random channel for voice stealing
                long randchannel = random(0, 6);
                
                // Don't steal the lowest note
                if (polynote[randchannel] == lowestnote) {
                    randchannel++;
                    if (randchannel == 6) randchannel = 0;
                }
                
                // First, look for empty voice slots
                for (int i = 0; i <= 5; i++) {
                    if (polynote[i] == 0) {
                        randchannel = i;
                        break;
                    }
                    handle_midi_input();
                }
                
                if (polypan > 64) {
                    long randpan = random(33, 127);
                    midi_send_cc(randchannel + 1, 77, randpan);
                }
                
                if (polynote[randchannel] != 0) {
                    midi_send_note_off(randchannel + 1, polynote[randchannel], velocity);
                }
                midi_send_note_on(randchannel + 1, note, velocity);
                
                polynote[randchannel] = note;
                polyon[randchannel] = true;
                noteheld[randchannel] = true;
            }
            
        }
    } else {
        // Mono mode or pass-through
        if (channel >= 1 && channel <= 6) {
            polyon[channel-1] = true;
            polynote[channel-1] = note;
            noteheld[channel-1] = true;
            midi_send_note_on(channel, note, velocity);
        } else {
            // Pass other channels straight through to GENMDM
            MIDI.sendNoteOn(note, velocity, channel);
        }
    }
}
void handle_control_change(uint8_t channel, uint8_t cc, uint8_t value) {
    if (cc == 64) { // Sustain pedal
        if (value == 0) { // sustain pedal released
            sustain = false;
            
            if (mode == 3 || mode == 4) { // poly mode
                for (int i = 5; i >= 0; i--) { // scan for sustained channels
                    handle_midi_input();
                    if (!noteheld[i] && sustainon[i]) { // if key not held but sustained
                        midi_send_note_off(i + 1, polynote[i], 0); // turn that voice off
                        sustainon[i] = false; // turn off sustain on that channel
                        polyon[i] = false; // turn voice off
                        polynote[i] = 0; // clear the pitch on that channel
                    }
                }
            } else { // mono mode
                for (int i = 0; i < 6; i++) {
                    sustainon[i] = false;
                    if (!noteheld[i] && polyon[i]) {
                        midi_send_note_off(i + 1, polynote[i], 0);
                        polyon[i] = false;
                    }
                }
            }
        } else { // sustain pedal pressed
            sustain = true;
        }
    }
    
    // ADD THIS: Modulation wheel handling (same as original)
    if (cc == 1) { // Modulation wheel
        if (value <= 5) {
            midi_send_cc(1, 74, 0); // mod wheel below 5 turns off LFO
        } else {
            midi_send_cc(1, 74, 70); // mod wheel above 5 turns on LFO
        }
    }
    
    // Pass CC messages to appropriate channels
    if (mode == 3 || mode == 4) { // poly mode
        if (channel == midichannel) {
            // Send CC to all active FM channels in poly mode
            for (int i = 1; i <= 6; i++) {
                midi_send_cc(i, cc, value);
            }
        } else {
            // Pass other channels straight through
            MIDI.sendControlChange(cc, value, channel);
        }
    } else { // mono mode
        if (channel >= 1 && channel <= 6) {
            midi_send_cc(channel, cc, value);
        } else {
            // Pass other channels straight through
            MIDI.sendControlChange(cc, value, channel);
        }
    }
}

// Utility functions
void oled_print(int x, int y, const char* text);
void oled_clear(void);
void oled_refresh(void);

void setup() {
    setup_hardware();
    setup_oled();
    setup_sd();
    setup_midi();
    
    EEPROM.begin(512);
    
    region = flash_read_setting(0);
    midichannel = flash_read_setting(1);
    
    if (region == 255) region = 0;
    if (midichannel == 255) midichannel = 1;
    
    if (region == 0) {
        midi_send_cc(1, 83, 75);
    } else {
        midi_send_cc(1, 83, 1);
    }
    
    scandir(false);
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Setup complete!");
    display.setCursor(0, 16);
    display.print("Files found: ");
    display.print(n);
    display.display();
    
    // Initialize all channels with immediate loading (no delay during setup)
    for (int i = 6; i > 0; i--) {
        tfichannel = i;
        tfiLoadImmediate(); 
    }
}

void loop() {
    if (booted == 0) {
        lcd_key = read_buttons();
        if (lcd_key == btnSELECT) {
            bootprompt();
        }
    }
    
    booted = 1;
    
    // CRITICAL: Process MIDI first for lowest latency
    handle_midi_input();
    
    // Static timers for non-critical updates
    static unsigned long last_slow_update = 0;
    static unsigned long last_fast_update = 0;
    unsigned long current_time = millis();
    
    // Fast updates every 20ms (50 FPS max)
    if (current_time - last_fast_update >= 100) {
        last_fast_update = current_time;
        
        // Only run visualizer animation in visualizer modes
        if (mode == 5 || mode == 6) {
            updateVisualizerAnimation();
        }
        
        // Handle cached display updates
        if (display_needs_refresh && (mode == 1 || mode == 3)) {
            updateFileDisplay();
        }
    }
    
    // Slow updates every 100ms
    if (current_time - last_slow_update >= 100) {
        last_slow_update = current_time;
        
        // MIDI display timeout check (non-critical)
        if (last_midi_time > 0 && (current_time - last_midi_time) > midi_display_timeout) {
            last_midi_time = 0;
            if (mode == 1 || mode == 3) {
                display_needs_refresh = true; // Mark for update instead of immediate
            }
        }
    }
    
    // TFI loading system (keep at original speed for responsiveness)
    if (tfi_pending_load && !showing_loading && (current_time - tfi_select_time) > 500) {
        showing_loading = true;
        if (mode == 1 || mode == 3) {
            updateFileDisplay();
        }
    }
    
    if (tfi_pending_load && (current_time - tfi_select_time) > 2000) {
        loadPendingTFI();
    }
    
    // Button reading and mode handling (keep responsive)
    lcd_key = read_buttons();
    modechangemessage();
    
    switch(mode) {
        case 1: // MONO / PRESET
            switch (lcd_key) {
                case btnRIGHT:
                    tfifilenumber[tfichannel-1]++;
                    if(tfifilenumber[tfichannel-1] >= n) {
                        tfifilenumber[tfichannel-1] = 0;
                    }
                    tfiselect();
                    break;
                    
                case btnLEFT:
                    if(tfifilenumber[tfichannel-1] == 0) {
                        tfifilenumber[tfichannel-1] = n-1;
                    } else {
                        tfifilenumber[tfichannel-1]--;
                    }
                    tfiselect();
                    break;
                    
                case btnUP:
                    tfichannel++;
                    if(tfichannel > 6) {
                        tfichannel = 1;
                    }
                    channelselect();
                    break;
                    
                case btnDOWN:
                    tfichannel--;
                    if(tfichannel == 0) {
                        tfichannel = 6;
                    }
                    channelselect();
                    break;
                    
                case btnSELECT:
                    modechange(1);
                    break;
                    
                case btnPOLY:
                    modechange(2);
                    break;
            }
            break;
            
        case 2: // MONO / EDIT
            switch (lcd_key) {
                case btnRIGHT:
                    fmscreen++;
                    if(fmscreen == 14) fmscreen = 1;
                    fmparamdisplay();
                    break;
                    
                case btnLEFT:
                    fmscreen--;
                    if(fmscreen == 0) fmscreen = 13;
                    fmparamdisplay();
                    break;
                    
                case btnUP:
                    tfichannel++;
                    if(tfichannel > 6) {
                        tfichannel = 1;
                    }
                    fmparamdisplay();
                    break;
                    
                case btnDOWN:
                    tfichannel--;
                    if(tfichannel == 0) {
                        tfichannel = 6;
                    }
                    fmparamdisplay();
                    break;
                    
                case btnSELECT:
                    modechange(1);
                    break;
                    
                case btnPOLY:
                    modechange(2);
                    break;
            }
            operatorparamdisplay();
            break;
            
        case 3: // POLY / PRESET
            switch (lcd_key) {
                case btnRIGHT:
                    tfichannel = 1;
                    tfifilenumber[tfichannel-1]++;
                    if(tfifilenumber[tfichannel-1] >= n) {
                        tfifilenumber[tfichannel-1] = 0;
                    }
                    for (int i = 1; i <= 5; i++) {
                        tfifilenumber[i] = tfifilenumber[0];
                    }
                    // Start the delay timer:
                    tfi_select_time = millis();
                    tfi_pending_load = true;
                    pending_tfi_channel = 1; // Will apply to all channels when timer expires
                    showing_loading = false;
                    updateFileDisplay();
                    break;
                    
                case btnLEFT:
                    tfichannel = 1;
                    if(tfifilenumber[tfichannel-1] == 0) {
                        tfifilenumber[tfichannel-1] = n-1;
                    } else {
                        tfifilenumber[tfichannel-1]--;
                    }
                    for (int i = 1; i <= 5; i++) {
                        tfifilenumber[i] = tfifilenumber[0];
                    }
                    // Start the delay timer:
                    tfi_select_time = millis();
                    tfi_pending_load = true;
                    pending_tfi_channel = 1; // Will apply to all channels when timer expires
                    showing_loading = false;
                    updateFileDisplay();
                    break;
                    
                case btnUP:
                    saveprompt();
                    break;
                    
                case btnDOWN:
                    saveprompt();
                    break;
                    
                case btnSELECT:
                    modechange(1);
                    break;
                    
                case btnPOLY:
                    modechange(2);
                    break;
                    
                case btnBLANK:
                    deletefile();
                    break;
            }
            break;
            
        case 4: // POLY / EDIT
            switch (lcd_key) {
                case btnRIGHT:
                    fmscreen++;
                    if(fmscreen == 14) fmscreen = 1;
                    fmparamdisplay();
                    break;
                    
                case btnLEFT:
                    fmscreen--;
                    if(fmscreen == 0) fmscreen = 13;
                    fmparamdisplay();
                    break;
                    
                case btnUP:
                    saveprompt();
                    break;
                    
                case btnDOWN:
                    saveprompt();
                    break;
                    
                case btnSELECT:
                    modechange(1);
                    break;
                    
                case btnPOLY:
                    modechange(2);
                    break;
            }
            operatorparamdisplay();
            break;

        case 5: // MONO / VISUALIZER
        case 6: // POLY / VISUALIZER
            switch (lcd_key) {
                case btnSELECT:
                    modechange(1);
                    break;
                    
                case btnPOLY:
                    modechange(2);
                    break;
            }
            break;
    }
}

void setup_oled(void) {
    Wire.setSDA(OLED_SDA_PIN);
    Wire.setSCL(OLED_SCL_PIN);
    Wire.setClock(400000);
    Wire.begin();
    
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("SSD1306 allocation failed");
        while(1); // Halt if display fails to initialize
    }
    
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("JamaGEN START!");
    display.setCursor(0, 16);
    display.print("ver Pico 1.17");
    display.display();
    delay(1000);
}

void flash_write_settings(uint8_t region_val, uint8_t midi_ch) {
    EEPROM.write(0, region_val);
    EEPROM.write(1, midi_ch);
    EEPROM.commit();  // Save changes to flash memory
}

uint8_t flash_read_setting(uint8_t offset) {
    return EEPROM.read(offset);
}

void setup_hardware(void) {
    Serial.begin(115200);
    
    // Initialize button pins with pullup resistors
    pinMode(BTN_PRESET_PIN, INPUT_PULLUP);
    pinMode(BTN_LEFT_PIN, INPUT_PULLUP);
    pinMode(BTN_RIGHT_PIN, INPUT_PULLUP);
    pinMode(BTN_CH_UP_PIN, INPUT_PULLUP);
    pinMode(BTN_CH_DOWN_PIN, INPUT_PULLUP);
    pinMode(BTN_MONO_POLY_PIN, INPUT_PULLUP);
    pinMode(BTN_DELETE_PIN, INPUT_PULLUP);
    pinMode(BTN_PANIC_PIN, INPUT_PULLUP); 
    pinMode(BTN_FUTURE_PIN, INPUT_PULLUP);
    
    // Initialize multiplexer control pins
    pinMode(MUX_S0_PIN, OUTPUT);
    pinMode(MUX_S1_PIN, OUTPUT);
    pinMode(MUX_S2_PIN, OUTPUT);
    pinMode(MUX_S3_PIN, OUTPUT);
    
    // Read initial pot values - all through multiplexer now
    for (int i = 0; i < 4; i++) {
        selectMuxChannel(i);
        prevpotvalue[i] = analogRead(MUX_SIG_PIN) >> 3;
    }
}

void setup_sd(void) {
    // Initialize SPI pins for SD card (like your working test code)
    SPI.setSCK(SD_SCK_PIN);
    SPI.setTX(SD_MOSI_PIN);
    SPI.setRX(SD_MISO_PIN);
    SPI.begin();
    
    // Set CS pin as output and ensure it's high initially
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Checking SD...");
    display.display();
    delay(500);
    
    // Use slower speed initially for better reliability
    if (!SD.begin(SD_CS_PIN, SPI_HALF_SPEED, SPI)) {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.print("CANNOT FIND SD");
        display.display();
        delay(5000);
        return;
    }
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("SD Card OK");
    display.display();
    delay(1000);
    
    // Test if we can get past this point
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("SD setup done");
    display.display();
    delay(1000);
}

void setup_midi(void) {
    // Set UART pins first
    Serial1.setTX(MIDI_TX_PIN);
    Serial1.setRX(MIDI_RX_PIN);
    
    // Initialize Serial1 manually at MIDI baud rate BEFORE calling MIDI.begin()
    // This prevents the MIDI library from hanging during UART initialization
    Serial1.begin(MIDI_BAUD_RATE);
    delay(100);  // Give UART time to stabilize
    
    // Now initialize MIDI library - it will use the already-initialized Serial1
    MIDI.begin(MIDI_CHANNEL_OMNI);
    MIDI.turnThruOff();
    
    // Set handlers to call your existing functions directly
    MIDI.setHandleNoteOn([](byte channel, byte note, byte velocity) {
        handle_note_on(channel, note, velocity);
    });
    
    MIDI.setHandleNoteOff([](byte channel, byte note, byte velocity) {
        handle_note_off(channel, note, velocity);
    });
    
    MIDI.setHandleControlChange([](byte channel, byte cc, byte value) {
        handle_control_change(channel, cc, value);
    });
    
    MIDI.setHandlePitchBend([](byte channel, int bend) {
        handle_pitch_bend(channel, (int16_t)bend);
    });
    // Forward Program Change & Aftertouch for channels outside 1..6
    MIDI.setHandleProgramChange([](byte channel, byte number) {
        if (channel < 1 || channel > 6) MIDI.sendProgramChange(number, channel);
    });
    MIDI.setHandleAfterTouchChannel([](byte channel, byte pressure) {
        if (channel < 1 || channel > 6) MIDI.sendAfterTouch(pressure, channel);
    });
    MIDI.setHandleAfterTouchPoly([](byte channel, byte note, byte pressure) {
        if (channel < 1 || channel > 6) MIDI.sendAfterTouch(note, pressure, channel);
    });
}

void midi_send_cc(uint8_t channel, uint8_t cc, uint8_t value) {
    MIDI.sendControlChange(cc, value, channel);
}

void midi_send_note_on(uint8_t channel, uint8_t note, uint8_t velocity) {
    MIDI.sendNoteOn(note, velocity, channel);
}

void midi_send_note_off(uint8_t channel, uint8_t note, uint8_t velocity) {
    MIDI.sendNoteOff(note, velocity, channel);
}

void midi_send_pitch_bend(uint8_t channel, int16_t bend) {
    MIDI.sendPitchBend(bend, channel);
}

void showAccelerationFeedback(void) {
    if (!button_is_held) return;
    
    uint32_t hold_duration = millis() - button_hold_start_time;
    
    // Only show feedback in preset browsing modes
    if (mode != 1 && mode != 3) return;
    
    // Add acceleration indicator to display
    if (hold_duration > hold_threshold_2) {
        // Show turbo indicator (could be added to updateFileDisplay)
        oled_print(120, 0, ">>>");
    } else if (hold_duration > hold_threshold_1) {
        // Show fast indicator
        oled_print(120, 0, ">>");
    }
}

uint8_t read_buttons(void) {
    static uint64_t last_button_time = 0;
    uint64_t current_time = millis();
    
    // Add MIDI processing at start of button reading
    handle_midi_input();
    
    // Read current button states
    bool left_pressed = !digitalRead(BTN_LEFT_PIN);
    bool right_pressed = !digitalRead(BTN_RIGHT_PIN);
    bool up_pressed = !digitalRead(BTN_CH_UP_PIN);
    bool down_pressed = !digitalRead(BTN_CH_DOWN_PIN);
    bool select_pressed = !digitalRead(BTN_PRESET_PIN);
    bool poly_pressed = !digitalRead(BTN_MONO_POLY_PIN);
    bool delete_pressed = !digitalRead(BTN_DELETE_PIN);
    bool panic_pressed = !digitalRead(BTN_PANIC_PIN);
    bool future_pressed = !digitalRead(BTN_FUTURE_PIN);
    
    // Check for panic button first (highest priority)
    if (panic_pressed) {
        if ((current_time - last_button_time) >= initial_debounce) {
            last_button_time = current_time;
            midiPanic();  // Call panic immediately
            return btnNONE;  // Don't return a button code, just execute panic
        }
    }
    
    // Determine which button is currently pressed (existing logic)
    uint8_t current_button = btnNONE;
    if (left_pressed) current_button = btnLEFT;
    else if (right_pressed) current_button = btnRIGHT;
    else if (up_pressed) current_button = btnUP;
    else if (down_pressed) current_button = btnDOWN;
    else if (select_pressed) current_button = btnSELECT;
    else if (poly_pressed) current_button = btnPOLY;
    else if (delete_pressed) current_button = btnBLANK;
    

    if (current_button == btnLEFT || current_button == btnRIGHT) {
        if (!button_is_held || last_held_button != current_button) {
            button_hold_start_time = current_time;
            button_is_held = true;
            last_held_button = current_button;
        }
        
        uint32_t hold_duration = current_time - button_hold_start_time;
        uint16_t dynamic_debounce;
        
        if (hold_duration > hold_threshold_2) {
            dynamic_debounce = turbo_debounce;
        } else if (hold_duration > hold_threshold_1) {
            dynamic_debounce = fast_debounce;
        } else {
            dynamic_debounce = initial_debounce;
        }
        
        if ((current_time - last_button_time) >= dynamic_debounce) {
            handle_midi_input();
            last_button_time = current_time;
            return current_button;
        }
        
    } else if (current_button != btnNONE) {
        button_is_held = false;
        last_held_button = btnNONE;
        
        if ((current_time - last_button_time) >= initial_debounce) {
            handle_midi_input();
            last_button_time = current_time;
            return current_button;
        }
    } else {
        button_is_held = false;
        last_held_button = btnNONE;
    }
    
    return btnNONE;
}

void tfiLoadImmediate(void) {
    if (n == 0) return;

    uint16_t idx = tfifilenumber[tfichannel-1];
    if (idx >= n) return;

    dataFile = SD.open(fullnames[idx], FILE_READ);
    if (!dataFile) {
        return;
    }

    // Read TFI file
    int tfiarray[42];
    for (int i = 0; i < 42; i++) {
        if (dataFile.available()) {
            tfiarray[i] = dataFile.read();
        } else {
            tfiarray[i] = 0;
        }
    }
    dataFile.close();

    tfisend(tfiarray, tfichannel);
}

// Load and apply the currently selected TFI for a specific FM channel (1..6)
// without relying on the global tfichannel.
void tfiLoadImmediateOnChannel(uint8_t ch) {
    if (n == 0) return;
    if (ch < 1 || ch > 6) return;

    uint16_t idx = tfifilenumber[ch - 1];
    if (idx >= n) return;

    File f = SD.open(fullnames[idx], FILE_READ);
    if (!f) return;

    int tfiarray[42];
    for (int i = 0; i < 42; i++) {
        if (f.available()) {
            tfiarray[i] = f.read();
        } else {
            tfiarray[i] = 0;
        }
    }
    f.close();

    // Send parameters explicitly to the requested channel
    tfisend(tfiarray, ch);
}

// Apply the tfifilenumber-selected TFI to all six FM channels immediately.
void applyTFIToAllChannelsImmediate() {
    for (uint8_t ch = 1; ch <= 6; ch++) {
        tfiLoadImmediateOnChannel(ch);
        delay(5); // Small delay between channels to prevent UART overflow
        // Only add MIDI processing if we're past the initial setup
        if (booted == 1) {
            handle_midi_input();
        }
    }
}



void scandir(bool saved) {
    n = 0;
    scanDirectoryRecursive("/", saved);
    
    // Clamp selections if list shrank
    for (int i = 0; i < 6; ++i) {
        if (n == 0) { 
            tfifilenumber[i] = 0; 
        } else if (tfifilenumber[i] >= n) { 
            tfifilenumber[i] = n - 1; 
        }
    }
}

void scanDirectoryRecursive(const char* path, bool saved) {
    if (n >= nMax - 10) return; // Leave some buffer space
    
    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) {
        if (dir) dir.close();
        return;
    }
    
    // Show progress during scanning
    if (strcmp(path, "/") == 0) {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.print("Scanning files...");
        display.display();
    }
    
    while (n < nMax - 10) {
        File entry = dir.openNextFile();
        if (!entry) break;
        
        const char* name = entry.name();
        if (!name || strlen(name) == 0) {
            entry.close();
            continue;
        }
        
        // Skip system files/folders
        if (name[0] == '.' || 
            strstr(name, "System Volume") != NULL ||
            strstr(name, "$RECYCLE") != NULL) {
            entry.close();
            continue;
        }
        
        if (entry.isDirectory()) {
            // Recursively scan subdirectory
            char subPath[96];
            if (strcmp(path, "/") == 0) {
                snprintf(subPath, 96, "/%s", name);
            } else {
                snprintf(subPath, 96, "%s/%s", path, name);
            }
            entry.close();
            scanDirectoryRecursive(subPath, saved);
            
        } else {
            // Check for TFI files
            int nameLen = strlen(name);
            bool isTFI = (nameLen >= 4 &&
                         name[nameLen-4] == '.' &&
                         (name[nameLen-3] == 't' || name[nameLen-3] == 'T') &&
                         (name[nameLen-2] == 'f' || name[nameLen-2] == 'F') &&
                         (name[nameLen-1] == 'i' || name[nameLen-1] == 'I'));
                         
            if (isTFI) {
                // Create display name with folder prefix if not in root
                if (strcmp(path, "/") == 0) {
                    // Root file - just use filename without .tfi
                    int dispLen = nameLen - 4;
                    if (dispLen > MaxNumberOfChars) dispLen = MaxNumberOfChars;
                    strncpy(filenames[n], name, dispLen);
                    filenames[n][dispLen] = '\0';
                    
                    // Store full path
                    snprintf(fullnames[n], FullNameChars, "/%s", name);
                    
                } else {
                    // Subfolder file - just use filename without .tfi extension
                    int dispLen = nameLen - 4;
                    if (dispLen > MaxNumberOfChars) dispLen = MaxNumberOfChars;
                    strncpy(filenames[n], name, dispLen);
                    filenames[n][dispLen] = '\0';
                    
                    // Store full path for loading
                    snprintf(fullnames[n], FullNameChars, "%s/%s", path, name);
                }
                
                // Check if this is the saved file we're looking for
                char fullPath[FullNameChars];
                snprintf(fullPath, FullNameChars, "%s/%s", path, name);
                if (saved && strcmp(name, savefilefull) == 0) {
                    for (int i = 0; i < 6; i++) tfifilenumber[i] = n;
                }
                
                n++;
                
                // Update progress display occasionally
                if (n % 20 == 0) {
                    display.clearDisplay();
                    display.setCursor(0, 0);
                    display.print("Scanning files...");
                    display.setCursor(0, 16);
                    display.print("Found: ");
                    display.print(n);
                    display.display();
                }
            }
            entry.close();
        }
        
        // Yield periodically to prevent watchdog timeout
        if (n % 10 == 0) {
            delay(1);
            handle_midi_input();
        }
    }
    
    dir.close();
}


void oled_print(int x, int y, const char* text) {
    display.setCursor(x, y);
    display.print(text);
}

void oled_clear(void) {
    display.clearDisplay();
}

void oled_refresh(void) {
    display.display();
}

void modechange(int modetype) {
    bool quickswitch = false;
    
    if (mode == 1 && modetype == 1) { mode = 2; quickswitch = true; }
    else if (mode == 1 && modetype == 2) mode = 3;
    else if (mode == 2 && modetype == 1) { mode = 5; quickswitch = true; }  // MONO EDIT -> MONO VIZ
    else if (mode == 2 && modetype == 2) mode = 4;
    else if (mode == 3 && modetype == 1) { mode = 4; quickswitch = true; }
    else if (mode == 3 && modetype == 2) mode = 1;
    else if (mode == 4 && modetype == 1) { mode = 6; quickswitch = true; }  // POLY EDIT -> POLY VIZ
    else if (mode == 4 && modetype == 2) mode = 2;
    else if (mode == 5 && modetype == 1) { mode = 1; quickswitch = true; }  // MONO VIZ -> MONO PRESET
    else if (mode == 5 && modetype == 2) mode = 6;
    else if (mode == 6 && modetype == 1) { mode = 3; quickswitch = true; }  // POLY VIZ -> POLY PRESET
    else if (mode == 6 && modetype == 2) mode = 5;
    
    switch(mode) {
        case 1:
            messagestart = millis();
            oled_clear();
            if (!quickswitch) {
                oled_print(0, 0, "MONO | Preset");
                refreshscreen = 1;
            } else {
                channelselect();
            }
            break;
            
        case 2:
            // Read all pot values through multiplexer
            for (int i = 0; i < 4; i++) {
                selectMuxChannel(i);
                prevpotvalue[i] = analogRead(MUX_SIG_PIN) >> 3;
            }
            
            messagestart = millis();
            oled_clear();
            if (!quickswitch) {
                oled_print(0, 0, "MONO | FM Edit");
                refreshscreen = 1;
            } else {
                fmparamdisplay();
            }
            break;
            
        case 3:
            messagestart = millis();
            oled_clear();
            if (!quickswitch) {
                oled_print(0, 0, "POLY | Preset");
                refreshscreen = 1;
            } else {
                channelselect();
            }
            break;
            
        case 4:
            // Read all pot values through multiplexer
            for (int i = 0; i < 4; i++) {
                selectMuxChannel(i);
                prevpotvalue[i] = analogRead(MUX_SIG_PIN) >> 3;
            }
            
            messagestart = millis();
            oled_clear();
            if (!quickswitch) {
                oled_print(0, 0, "POLY | FM Edit");
                refreshscreen = 1;
            } else {
                fmparamdisplay();
            }
            break;
            
        case 5: // MONO VISUALIZER
            messagestart = millis();
            oled_clear();
            if (!quickswitch) {
                oled_print(0, 0, "MONO | Visualizer");
                refreshscreen = 1;
            } else {
                visualizerDisplay();
            }
            break;
            
        case 6: // POLY VISUALIZER
            messagestart = millis();
            oled_clear();
            if (!quickswitch) {
                oled_print(0, 0, "POLY | Visualizer");
                refreshscreen = 1;
            } else {
                visualizerDisplay();
            }
            break;
    }
    oled_refresh();
}

void modechangemessage(void) {
    if ((millis() - messagestart) > messagedelay && refreshscreen == 1) {
        switch(mode) {
            case 1:
                channelselect();
                refreshscreen = 0;
                break;
            case 2:
                fmparamdisplay();
                refreshscreen = 0;
                break;
            case 3:
                channelselect();
                refreshscreen = 0;
                break;
            case 4:
                fmparamdisplay();
                refreshscreen = 0;
                break;
            case 5: // MONO VISUALIZER
                visualizerDisplay();
                refreshscreen = 0;
                break;
            case 6: // POLY VISUALIZER
                visualizerDisplay();
                refreshscreen = 0;
                break;
        }
    }
}


// Update velocity visualization when notes are played:
void updateVelocityViz(uint8_t channel, uint8_t velocity) {
    if (channel >= 11) return;  // Support channels 0-10 (MIDI channels 1-11)
    
    voice_velocity[channel] = velocity;
    
    // Update peak hold if this velocity is higher
    if (velocity > voice_peak[channel]) {
        voice_peak[channel] = velocity;
        voice_peak_time[channel] = millis();
    }
}

void clearVelocityViz(uint8_t channel) {
    if (channel >= 11) return;
    voice_velocity[channel] = 0;
}

void updateVisualizerAnimation(void) {
    // Only run if in visualizer mode
    if (mode != 5 && mode != 6) return;
    
    static unsigned long last_update = 0;
    unsigned long current_time = millis();
    
    // Reduce update frequency to 15 FPS for better performance
    if (current_time - last_update < 67) return;
    last_update = current_time;
    
    bool needs_update = false;
    
    // Decay all 11 channels
    for (int channel = 0; channel < 11; channel++) {
        if (voice_velocity[channel] > 0) {
            if (voice_velocity[channel] > velocity_decay_rate) {
                voice_velocity[channel] -= velocity_decay_rate;
            } else {
                voice_velocity[channel] = 0;
            }
            needs_update = true;
        }
        
        // Clear peak hold after timeout
        if (voice_peak[channel] > 0 && 
            (current_time - voice_peak_time[channel]) > peak_hold_duration) {
            voice_peak[channel] = 0;
            needs_update = true;
        }
    }
    
    if (needs_update) {
        visualizerDisplay();
    }
}

// Optimized visualizer display function with faster rendering:
void visualizerDisplay(void) {
    display.clearDisplay();
    
    // Draw title
    display.setCursor(0, 0);
    display.setTextSize(1);
    if (mode == 5) {
        display.print("MONO VIZ - 11CH");
    } else {
        display.print("POLY VIZ - 11CH");
    }
    
    // Draw 11 channels across 128 pixels (11.6 pixels per channel)
    for (int channel = 0; channel < 11; channel++) {
        int x_start = channel * 11 + channel;  // 11 pixels + 1 spacing = 12 total per channel
        int bar_width = 10;                    // Narrow bars to fit 11 channels
        int bar_height = 16;                   // Shorter bars to fit labels
        int y_base = 31;                       // Bottom of screen
        
        // Calculate bar heights
        int current_bar = map(voice_velocity[channel], 0, 127, 0, bar_height);
        int peak_bar = map(voice_peak[channel], 0, 127, 0, bar_height);
        
        // Draw main velocity bar
        if (current_bar > 0) {
            display.fillRect(x_start, y_base - current_bar, bar_width, current_bar, SSD1306_WHITE);
        }
        
        // Draw peak hold line
        if (voice_peak[channel] > 0) {
            display.drawFastHLine(x_start, y_base - peak_bar, bar_width, SSD1306_WHITE);
        }
        
        // Draw channel labels at bottom (very small text)
        display.setCursor(x_start + 2, 10);
        display.setTextSize(1);
        
        // Compact channel labels
        if (channel < 6) {
            display.print(channel + 1);  // FM channels: 1, 2, 3, 4, 5, 6
        } else if (channel < 10) {
            display.print("P");           // PSG channels: P, P, P, P (channels 7-10)
            display.setCursor(x_start + 2, 18);
            display.print(channel - 6);  // P1, P2, P3, P4
        } else {
            display.print("N");           // PSG Noise channel: N (channel 11)
        }
    }
    
    display.display();
}


void switchVisualizerPage(void) {
    static uint8_t viz_page = 0;
    viz_page = !viz_page;
    visualizerDisplay();
}

void bootprompt(void) {
    uint8_t currentpotvalue[4];
    uint8_t lastDisplayedChannel = 255;
    uint8_t lastDisplayedRegion = 255;
    
    menuprompt = 0;
    
    oled_clear();
    oled_print(0, 0, "MIDI CH / REGION");
    oled_refresh();
    
    while (menuprompt == 0) {
        handle_midi_input();
        
        // Read OP1 (channel 0) for MIDI channel
        selectMuxChannel(0);
        currentpotvalue[0] = analogRead(MUX_SIG_PIN) >> 3;
        
        // Read OP3 (channel 2) for region
        selectMuxChannel(2);
        currentpotvalue[2] = analogRead(MUX_SIG_PIN) >> 3;
        
        midichannel = (currentpotvalue[0] / 8) + 1;
        uint8_t currentRegion = (currentpotvalue[2] < 64) ? 0 : 1;
        
        // Rest of function unchanged...
    }
}

void saveprompt(void) {
    menuprompt = 0;
    
    oled_clear();
    oled_print(0, 0, "UP: overwrite");
    oled_print(0, 16, "DOWN: save new");
    oled_refresh();
    
    while (menuprompt == 0) {
        handle_midi_input();
        lcd_key = read_buttons();
        switch (lcd_key) {
            case btnRIGHT:
            case btnLEFT:
            case btnSELECT:
                menuprompt = 1;
                messagestart = millis();
                oled_clear();
                oled_print(0, 0, "save cancelled");
                refreshscreen = 1;
                oled_refresh();
                break;
                
            case btnUP:
                menuprompt = 1;
                saveoverwrite();
                break;
                
            case btnDOWN:
                menuprompt = 1;
                savenew();
                break;
        }
    }
}

void savenew(void) {
    oled_clear();
    oled_print(0, 0, "SAVING...");
    oled_print(0, 16, "please wait");
    oled_refresh();
    
    // Find new patch filename
    while (savenumber < 1000) {
        sprintf(savefilefull, "newpatch%03d.tfi", savenumber);
        if (!SD.exists(savefilefull)) {
            break;  // File doesn't exist, we can use this name
        }
        savenumber++;
    }
    
    dataFile = SD.open(savefilefull, FILE_WRITE);
    if (!dataFile) {
        oled_clear();
        oled_print(0, 0, "FILE SAVE ERROR");
        oled_refresh();
        delay(5000);
        return;
    }
    
    // Convert fmsettings back into a tfi file
    int tfiarray[42];
    tfichannel = 1;
    
    tfiarray[0] = fmsettings[tfichannel-1][0] / 16;    // Algorithm
    tfiarray[1] = fmsettings[tfichannel-1][1] / 16;    // Feedback
    tfiarray[2] = fmsettings[tfichannel-1][2] / 8;     // OP1 Multiplier
    tfiarray[12] = fmsettings[tfichannel-1][12] / 8;   // OP3 Multiplier
    tfiarray[22] = fmsettings[tfichannel-1][22] / 8;   // OP2 Multiplier
    tfiarray[32] = fmsettings[tfichannel-1][32] / 8;   // OP4 Multiplier
    tfiarray[3] = fmsettings[tfichannel-1][3] / 32;    // OP1 Detune
    tfiarray[13] = fmsettings[tfichannel-1][13] / 32;  // OP3 Detune
    tfiarray[23] = fmsettings[tfichannel-1][23] / 32;  // OP2 Detune
    tfiarray[33] = fmsettings[tfichannel-1][33] / 32;  // OP4 Detune
    tfiarray[4] = 127 - fmsettings[tfichannel-1][4];   // OP1 Total Level
    tfiarray[14] = 127 - fmsettings[tfichannel-1][14]; // OP3 Total Level
    tfiarray[24] = 127 - fmsettings[tfichannel-1][24]; // OP2 Total Level
    tfiarray[34] = 127 - fmsettings[tfichannel-1][34]; // OP4 Total Level
    tfiarray[5] = fmsettings[tfichannel-1][5] / 32;    // OP1 Rate Scaling
    tfiarray[15] = fmsettings[tfichannel-1][15] / 32;  // OP3 Rate Scaling
    tfiarray[25] = fmsettings[tfichannel-1][25] / 32;  // OP2 Rate Scaling
    tfiarray[35] = fmsettings[tfichannel-1][35] / 32;  // OP4 Rate Scaling
    tfiarray[6] = fmsettings[tfichannel-1][6] / 4;     // OP1 Attack Rate
    tfiarray[16] = fmsettings[tfichannel-1][16] / 4;   // OP3 Attack Rate
    tfiarray[26] = fmsettings[tfichannel-1][26] / 4;   // OP2 Attack Rate
    tfiarray[36] = fmsettings[tfichannel-1][36] / 4;   // OP4 Attack Rate
    tfiarray[7] = fmsettings[tfichannel-1][7] / 4;     // OP1 1st Decay Rate
    tfiarray[17] = fmsettings[tfichannel-1][17] / 4;   // OP3 1st Decay Rate
    tfiarray[27] = fmsettings[tfichannel-1][27] / 4;   // OP2 1st Decay Rate
    tfiarray[37] = fmsettings[tfichannel-1][37] / 4;   // OP4 1st Decay Rate
    tfiarray[10] = (127 - fmsettings[tfichannel-1][10]) / 8; // OP1 2nd Total Level
    tfiarray[20] = (127 - fmsettings[tfichannel-1][20]) / 8; // OP3 2nd Total Level
    tfiarray[30] = (127 - fmsettings[tfichannel-1][30]) / 8; // OP2 2nd Total Level
    tfiarray[40] = (127 - fmsettings[tfichannel-1][40]) / 8; // OP4 2nd Total Level
    tfiarray[8] = fmsettings[tfichannel-1][8] / 8;     // OP1 2nd Decay Rate
    tfiarray[18] = fmsettings[tfichannel-1][18] / 8;   // OP3 2nd Decay Rate
    tfiarray[28] = fmsettings[tfichannel-1][28] / 8;   // OP2 2nd Decay Rate
    tfiarray[38] = fmsettings[tfichannel-1][38] / 8;   // OP4 2nd Decay Rate
    tfiarray[9] = fmsettings[tfichannel-1][9] / 8;     // OP1 Release Rate
    tfiarray[19] = fmsettings[tfichannel-1][19] / 8;   // OP3 Release Rate
    tfiarray[29] = fmsettings[tfichannel-1][29] / 8;   // OP2 Release Rate
    tfiarray[39] = fmsettings[tfichannel-1][39] / 8;   // OP4 Release Rate
    tfiarray[11] = fmsettings[tfichannel-1][11] / 8;   // OP1 SSG-EG
    tfiarray[21] = fmsettings[tfichannel-1][21] / 8;   // OP3 SSG-EG
    tfiarray[31] = fmsettings[tfichannel-1][31] / 8;   // OP2 SSG-EG
    tfiarray[41] = fmsettings[tfichannel-1][41] / 8;   // OP4 SSG-EG
    
    for (int i = 0; i < 42; i++) {
        dataFile.write((uint8_t)tfiarray[i]);
    }
    
    dataFile.close();
    
    oled_clear();
    oled_print(0, 0, "SAVED!");
    oled_print(0, 16, savefilefull);
    oled_refresh();
    delay(2000);
    
    messagestart = millis();
    refreshscreen = 1;
    
    scandir(true);  // Rescan directory and find the new file
    
    // Show filename on screen
    char display_buffer[32];
    sprintf(display_buffer, "P %03d/%03d", tfifilenumber[0] + 1, n);
    oled_clear();
    oled_print(0, 0, display_buffer);
    oled_print(0, 16, savefilefull);
    oled_refresh();
}

void saveoverwrite(void) {
    uint16_t idx = tfifilenumber[tfichannel-1];
    if (idx >= n) return;

    dataFile = SD.open(fullnames[idx], FILE_WRITE);
    if (!dataFile) {
        oled_clear(); oled_print(0, 0, "CANNOT WRITE TFI"); oled_refresh(); return;
    }

    // Convert fmsettings back into a tfi file
    int tfiarray[42];
    tfichannel = 1;

    tfiarray[0] = fmsettings[tfichannel-1][0] / 16;    // Algorithm
    tfiarray[1] = fmsettings[tfichannel-1][1] / 16;    // Feedback
    tfiarray[2] = fmsettings[tfichannel-1][2] / 8;     // OP1 Multiplier
    tfiarray[3] = fmsettings[tfichannel-1][3] / 32;    // OP1 Attack Rate
    tfiarray[4] = fmsettings[tfichannel-1][4];         // OP1 Decay Rate
    tfiarray[5] = fmsettings[tfichannel-1][5];         // OP1 Sustain Rate
    tfiarray[6] = fmsettings[tfichannel-1][6];         // OP1 Release Rate
    tfiarray[7] = fmsettings[tfichannel-1][7];         // OP1 Sustain Level
    tfiarray[8] = fmsettings[tfichannel-1][8];         // OP1 Total Level
    tfiarray[9] = fmsettings[tfichannel-1][9];         // OP1 Key Scaling
    tfiarray[10] = fmsettings[tfichannel-1][10];       // OP1 Detune
    tfiarray[11] = fmsettings[tfichannel-1][11];       // OP1 Amplitude Modulation
    tfiarray[12] = fmsettings[tfichannel-1][12] / 8;   // OP3 Multiplier
    tfiarray[13] = fmsettings[tfichannel-1][13] / 32;  // OP3 Attack Rate
    tfiarray[14] = fmsettings[tfichannel-1][14] / 16;  // OP3 Decay Rate
    tfiarray[15] = fmsettings[tfichannel-1][15] / 16;  // OP3 Sustain Rate
    tfiarray[16] = fmsettings[tfichannel-1][16];       // OP3 Release Rate
    tfiarray[17] = fmsettings[tfichannel-1][17];       // OP3 Sustain Level
    tfiarray[18] = fmsettings[tfichannel-1][18];       // OP3 Total Level
    tfiarray[19] = fmsettings[tfichannel-1][19];       // OP3 Key Scaling
    tfiarray[20] = fmsettings[tfichannel-1][20];       // OP3 Detune
    tfiarray[21] = fmsettings[tfichannel-1][21];       // OP3 Amplitude Modulation
    tfiarray[22] = fmsettings[tfichannel-1][22] / 8;   // OP2 Multiplier
    tfiarray[23] = fmsettings[tfichannel-1][23] / 32;  // OP2 Attack Rate
    tfiarray[24] = fmsettings[tfichannel-1][24] / 16;  // OP2 Decay Rate
    tfiarray[25] = fmsettings[tfichannel-1][25] / 16;  // OP2 Sustain Rate
    tfiarray[26] = fmsettings[tfichannel-1][26];       // OP2 Release Rate
    tfiarray[27] = fmsettings[tfichannel-1][27];       // OP2 Sustain Level
    tfiarray[28] = fmsettings[tfichannel-1][28];       // OP2 Total Level
    tfiarray[29] = fmsettings[tfichannel-1][29];       // OP2 Key Scaling
    tfiarray[30] = fmsettings[tfichannel-1][30];       // OP2 Detune
    tfiarray[31] = fmsettings[tfichannel-1][31];       // OP2 Amplitude Modulation
    tfiarray[32] = fmsettings[tfichannel-1][32] / 8;   // OP4 Multiplier
    tfiarray[33] = fmsettings[tfichannel-1][33] / 32;  // OP4 Attack Rate
    tfiarray[34] = fmsettings[tfichannel-1][34] / 16;  // OP4 Decay Rate
    tfiarray[35] = fmsettings[tfichannel-1][35];       // OP4 Sustain Rate
    tfiarray[36] = fmsettings[tfichannel-1][36];       // OP4 Release Rate
    tfiarray[37] = fmsettings[tfichannel-1][37];       // OP4 Sustain Level
    tfiarray[38] = fmsettings[tfichannel-1][38];       // OP4 Total Level
    tfiarray[39] = fmsettings[tfichannel-1][39];       // OP4 Key Scaling
    tfiarray[40] = fmsettings[tfichannel-1][40];       // OP4 Detune
    tfiarray[41] = fmsettings[tfichannel-1][41];       // OP4 Amplitude Modulation

    for (int i = 0; i < 42; i++) dataFile.write((uint8_t)tfiarray[i]);
    dataFile.close();

    oled_clear();
    oled_print(0, 0, "SAVED!");
    oled_print(0, 16, filenames[idx]);
    oled_refresh();
    delay(500);

    messagestart = millis();
    refreshscreen = 1;

    scandir(true); // rescan & rewrite index
}

void deletefile(void) {
    menuprompt = 0;

    oled_clear();
    oled_print(0, 0, "CONFIRM DELETE");
    oled_print(0, 16, "press again: y");
    oled_refresh();
    delay(1000);

    while (menuprompt == 0) {
        handle_midi_input();
        lcd_key = read_buttons();
        switch (lcd_key) {
            case btnBLANK: // confirm
            {
                uint16_t idx = tfifilenumber[tfichannel-1];
                if (idx >= n) { oled_clear(); oled_print(0, 0, "DELETE ERROR"); oled_refresh(); delay(2000); return; }

                if (!SD.remove(fullnames[idx])) {
                    oled_clear(); oled_print(0, 0, "DELETE ERROR"); oled_refresh(); delay(2000);
                    return;
                }

                oled_clear();
                oled_print(0, 0, "FILE DELETED!");
                oled_print(0, 16, filenames[idx]);
                oled_refresh();
                delay(1000);

                scandir(false);                // refresh list (also rewrites index)
                if (n == 0) { messagestart = millis(); refreshscreen = 1; return; }
                if (tfifilenumber[tfichannel-1] >= n) tfifilenumber[tfichannel-1] = n-1;

                messagestart = millis();
                refreshscreen = 1;
                menuprompt = 1;
                break;
            }
            case btnSELECT: // cancel
                oled_clear(); oled_print(0, 0, "cancelled"); oled_refresh(); delay(500);
                menuprompt = 1;
                break;
            default:
                break;
        }
    }
    refreshscreen = 1;
}

void tfiselect(void) {
    if (n == 0) return;  // No files available

    uint16_t idx = tfifilenumber[tfichannel-1];
    if (idx >= n) return;

    // Remember which channel this choice is for
    tfi_select_time = millis();
    tfi_pending_load = true;
    pending_tfi_channel = tfichannel;
    showing_loading = false;

    // In MONO modes, apply immediately so the channel is initialized without delay
    if (mode == 1 || mode == 2) {
        tfiLoadImmediateOnChannel(tfichannel); // applies to current tfichannel
        updateFileDisplay();
        tfi_pending_load = false;  
        showing_loading = false;
    } else {
        // In POLY modes, just update display now; delayed loader will show UI and apply
        if (booted == 1 && (mode == 1 || mode == 3)) {
            updateFileDisplay();
        }
    }
}

void midiNoteToString(uint8_t note, char* noteStr) {
    const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    
    uint8_t octave = (note / 12) - 1;  // MIDI note 60 = C4
    uint8_t noteIndex = note % 12;
    
    // Handle special cases for very low or high notes
    if (note < 12) {
        sprintf(noteStr, "%s%d", noteNames[noteIndex], octave + 1);
    } else if (note > 127) {
        strcpy(noteStr, "---");
    } else {
        sprintf(noteStr, "%s%d", noteNames[noteIndex], octave);
    }
}

void loadPendingTFI(void) {
    if (n == 0) return;

    uint16_t idx = tfifilenumber[pending_tfi_channel-1];
    if (idx >= n) return;

    dataFile = SD.open(fullnames[idx], FILE_READ);
    if (!dataFile) {
        oled_clear();
        oled_print(0, 0, "CANNOT READ TFI");
        oled_refresh();
        tfi_pending_load = false;
        showing_loading = false;
        return;
    }

    // Read TFI file
    int tfiarray[42];
    for (int i = 0; i < 42; i++) {
        if (dataFile.available()) {
            tfiarray[i] = dataFile.read();
        } else {
            tfiarray[i] = 0;
        }
    }
    dataFile.close();

    // In poly mode, apply to all 6 channels
    if (mode == 3 || mode == 4) {
        for (uint8_t ch = 1; ch <= 6; ch++) {
            tfisend(tfiarray, ch);
            delay(2); // Small delay between channels to prevent UART overrun
        }
    } else {
        // In mono mode, just apply to the specific channel
        tfisend(tfiarray, pending_tfi_channel);
    }

    // Clear the loading state
    tfi_pending_load = false;
    showing_loading = false;
    
    // Update display to remove "loading..." message
    if (booted == 1 && (mode == 1 || mode == 3)) {
        updateFileDisplay();
    }
}


void channelselect(void) {
    if (n == 0) return;  // No files available

    // When switching MONO channels, immediately initialize the newly selected channel
    // with the currently selected TFI so it has a valid sound/params.
    // Also send All Notes Off / Reset All Controllers for a clean slate.
    if (mode == 1 || mode == 2) { // MONO | Preset or MONO | Edit
        midi_send_cc(tfichannel, 123, 0); // All Notes Off (per-channel)
        midi_send_cc(tfichannel, 121, 0); // Reset All Controllers (if supported)
        tfiLoadImmediateOnChannel(tfichannel); // Apply the current TFI to this channel now
    }

    updateFileDisplay();
}

void updateMidiDisplay(uint8_t channel, uint8_t note) {
    last_midi_channel = channel;
    last_midi_note = note;
    last_midi_time = millis();
    
    // Refresh display if we're in a file browsing mode
    if (mode == 1 || mode == 3) {
        updateFileDisplay();
    }
}

void updateFileDisplay(void) {
    if (n == 0) return;
    
    // Only update if we're in a file browsing mode
    if (mode != 1 && mode != 3) return;
    
    // Rate limit display updates to reduce OLED overhead
    if (millis() - last_display_update < 50) {
        display_needs_refresh = true; // Mark for later update
        return;
    }
    
    // Show filename on screen
    char display_buffer[32];
    if (mode == 3) {
        sprintf(display_buffer, "P %03d/%03d", tfifilenumber[tfichannel-1] + 1, n);
    } else {
        sprintf(display_buffer, "C%d %03d/%03d", tfichannel, tfifilenumber[tfichannel-1] + 1, n);
    }
    
    oled_clear();
    oled_print(0, 0, display_buffer);
    
    // Add MIDI info only if recent and in correct modes
    if (last_midi_time > 0 && (millis() - last_midi_time) < midi_display_timeout && (mode == 1 || mode == 3)) {
        char midi_buffer[16];
        char note_name[8];
        
        midiNoteToString(last_midi_note, note_name);
        sprintf(midi_buffer, "C%d %s", last_midi_channel, note_name);
        oled_print(75, 0, midi_buffer);
    }
    
    oled_print(0, 16, filenames[tfifilenumber[tfichannel-1]]);
    
    // Show loading indicator if TFI is pending load
    if (showing_loading) {
        oled_print(0, 24, "loading tfi...");
    }
    
    showAccelerationFeedback();
    oled_refresh();
    
    last_display_update = millis();
    display_needs_refresh = false;
}
void midiNoteToStringShort(uint8_t note, char* noteStr) {
    const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    
    uint8_t octave = (note / 12) - 1;
    uint8_t noteIndex = note % 12;
    
    if (note < 12 || note > 127) {
        strcpy(noteStr, "---");
    } else {
        sprintf(noteStr, "%s%d", noteNames[noteIndex], octave);
    }
}

void midiNoteToStringNoOctave(uint8_t note, char* noteStr) {
    const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    
    if (note > 127) {
        strcpy(noteStr, "---");
    } else {
        strcpy(noteStr, noteNames[note % 12]);
    }
}


void tfisend(int opnarray[42], int sendchannel) {
    // Send all TFI data to appropriate CCs with periodic MIDI processing
    
    midi_send_cc(sendchannel, 14, opnarray[0] * 16);   // Algorithm
    midi_send_cc(sendchannel, 15, opnarray[1] * 16);   // Feedback
    
    midi_send_cc(sendchannel, 20, opnarray[2] * 8);    // OP1 Multiplier
    midi_send_cc(sendchannel, 21, opnarray[12] * 8);   // OP3 Multiplier
    midi_send_cc(sendchannel, 22, opnarray[22] * 8);   // OP2 Multiplier
    midi_send_cc(sendchannel, 23, opnarray[32] * 8);   // OP4 Multiplier
    
    midi_send_cc(sendchannel, 24, opnarray[3] * 32);   // OP1 Detune
    midi_send_cc(sendchannel, 25, opnarray[13] * 32);  // OP3 Detune
    midi_send_cc(sendchannel, 26, opnarray[23] * 32);  // OP2 Detune
    midi_send_cc(sendchannel, 27, opnarray[33] * 32);  // OP4 Detune
    delayMicroseconds(1500);
    handle_midi_input(); // Process MIDI after detune section
    
    midi_send_cc(sendchannel, 16, 127 - opnarray[4]);  // OP1 Total Level
    midi_send_cc(sendchannel, 17, 127 - opnarray[14]); // OP3 Total Level
    midi_send_cc(sendchannel, 18, 127 - opnarray[24]); // OP2 Total Level
    midi_send_cc(sendchannel, 19, 127 - opnarray[34]); // OP4 Total Level
    
    midi_send_cc(sendchannel, 39, opnarray[5] * 32);   // OP1 Rate Scaling
    midi_send_cc(sendchannel, 40, opnarray[15] * 32);  // OP3 Rate Scaling
    midi_send_cc(sendchannel, 41, opnarray[25] * 32);  // OP2 Rate Scaling
    midi_send_cc(sendchannel, 42, opnarray[35] * 32);  // OP4 Rate Scaling
    handle_midi_input(); // Process MIDI after rate scaling
    
    midi_send_cc(sendchannel, 43, opnarray[6] * 4);    // OP1 Attack Rate
    midi_send_cc(sendchannel, 44, opnarray[16] * 4);   // OP3 Attack Rate
    midi_send_cc(sendchannel, 45, opnarray[26] * 4);   // OP2 Attack Rate
    midi_send_cc(sendchannel, 46, opnarray[36] * 4);   // OP4 Attack Rate
    
    midi_send_cc(sendchannel, 47, opnarray[7] * 4);    // OP1 1st Decay Rate
    midi_send_cc(sendchannel, 48, opnarray[17] * 4);   // OP3 1st Decay Rate
    midi_send_cc(sendchannel, 49, opnarray[27] * 4);   // OP2 1st Decay Rate
    midi_send_cc(sendchannel, 50, opnarray[37] * 4);   // OP4 1st Decay Rate
    handle_midi_input(); // Process MIDI after envelope rates
    
    midi_send_cc(sendchannel, 55, opnarray[10] * 8);   // OP1 2nd Total Level
    midi_send_cc(sendchannel, 56, opnarray[20] * 8);   // OP3 2nd Total Level
    midi_send_cc(sendchannel, 57, opnarray[30] * 8);   // OP2 2nd Total Level
    midi_send_cc(sendchannel, 58, opnarray[40] * 8);   // OP4 2nd Total Level
    
    midi_send_cc(sendchannel, 51, opnarray[8] * 8);    // OP1 2nd Decay Rate
    midi_send_cc(sendchannel, 52, opnarray[18] * 8);   // OP3 2nd Decay Rate
    midi_send_cc(sendchannel, 53, opnarray[28] * 8);   // OP2 2nd Decay Rate
    midi_send_cc(sendchannel, 54, opnarray[38] * 8);   // OP4 2nd Decay Rate
    handle_midi_input(); // Process MIDI after sustain/decay
    
    midi_send_cc(sendchannel, 59, opnarray[9] * 8);    // OP1 Release Rate
    midi_send_cc(sendchannel, 60, opnarray[19] * 8);   // OP3 Release Rate
    midi_send_cc(sendchannel, 61, opnarray[29] * 8);   // OP2 Release Rate
    midi_send_cc(sendchannel, 62, opnarray[39] * 8);   // OP4 Release Rate
    
    midi_send_cc(sendchannel, 90, opnarray[11] * 8);   // OP1 SSG-EG
    midi_send_cc(sendchannel, 91, opnarray[21] * 8);   // OP3 SSG-EG
    midi_send_cc(sendchannel, 92, opnarray[31] * 8);   // OP2 SSG-EG
    midi_send_cc(sendchannel, 93, opnarray[41] * 8);   // OP4 SSG-EG
    handle_midi_input(); // Process MIDI after SSG-EG
    
    midi_send_cc(sendchannel, 75, 90);  // FM Level
    midi_send_cc(sendchannel, 76, 90);  // AM Level
    midi_send_cc(sendchannel, 77, 127); // Stereo (centered)
    
    midi_send_cc(sendchannel, 70, 0);   // OP1 Amplitude Modulation (off)
    midi_send_cc(sendchannel, 71, 0);   // OP3 Amplitude Modulation (off)
    midi_send_cc(sendchannel, 72, 0);   // OP2 Amplitude Modulation (off)
    midi_send_cc(sendchannel, 73, 0);   // OP4 Amplitude Modulation (off)
    
    // Store TFI settings in global array for editing (unchanged)
    fmsettings[sendchannel-1][0] = opnarray[0] * 16;    // Algorithm
    fmsettings[sendchannel-1][1] = opnarray[1] * 16;    // Feedback
    fmsettings[sendchannel-1][2] = opnarray[2] * 8;     // OP1 Multiplier
    fmsettings[sendchannel-1][12] = opnarray[12] * 8;   // OP3 Multiplier
    fmsettings[sendchannel-1][22] = opnarray[22] * 8;   // OP2 Multiplier
    fmsettings[sendchannel-1][32] = opnarray[32] * 8;   // OP4 Multiplier
    fmsettings[sendchannel-1][3] = opnarray[3] * 32;    // OP1 Detune
    fmsettings[sendchannel-1][13] = opnarray[13] * 32;  // OP3 Detune
    fmsettings[sendchannel-1][23] = opnarray[23] * 32;  // OP2 Detune
    fmsettings[sendchannel-1][33] = opnarray[33] * 32;  // OP4 Detune
    fmsettings[sendchannel-1][4] = 127 - opnarray[4];   // OP1 Total Level
    fmsettings[sendchannel-1][14] = 127 - opnarray[14]; // OP3 Total Level
    fmsettings[sendchannel-1][24] = 127 - opnarray[24]; // OP2 Total Level
    fmsettings[sendchannel-1][34] = 127 - opnarray[34]; // OP4 Total Level
    fmsettings[sendchannel-1][5] = opnarray[5] * 32;    // OP1 Rate Scaling
    fmsettings[sendchannel-1][15] = opnarray[15] * 32;  // OP3 Rate Scaling
    fmsettings[sendchannel-1][25] = opnarray[25] * 32;  // OP2 Rate Scaling
    fmsettings[sendchannel-1][35] = opnarray[35] * 32;  // OP4 Rate Scaling
    fmsettings[sendchannel-1][6] = opnarray[6] * 4;     // OP1 Attack Rate
    fmsettings[sendchannel-1][16] = opnarray[16] * 4;   // OP3 Attack Rate
    fmsettings[sendchannel-1][26] = opnarray[26] * 4;   // OP2 Attack Rate
    fmsettings[sendchannel-1][36] = opnarray[36] * 4;   // OP4 Attack Rate
    fmsettings[sendchannel-1][7] = opnarray[7] * 4;     // OP1 1st Decay Rate
    fmsettings[sendchannel-1][17] = opnarray[17] * 4;   // OP3 1st Decay Rate
    fmsettings[sendchannel-1][27] = opnarray[27] * 4;   // OP2 1st Decay Rate
    fmsettings[sendchannel-1][37] = opnarray[37] * 4;   // OP4 1st Decay Rate
    fmsettings[sendchannel-1][10] = 127 - (opnarray[10] * 8); // OP1 2nd Total Level
    fmsettings[sendchannel-1][20] = 127 - (opnarray[20] * 8); // OP3 2nd Total Level
    fmsettings[sendchannel-1][30] = 127 - (opnarray[30] * 8); // OP2 2nd Total Level
    fmsettings[sendchannel-1][40] = 127 - (opnarray[40] * 8); // OP4 2nd Total Level
    fmsettings[sendchannel-1][8] = opnarray[8] * 8;     // OP1 2nd Decay Rate
    fmsettings[sendchannel-1][18] = opnarray[18] * 8;   // OP3 2nd Decay Rate
    fmsettings[sendchannel-1][28] = opnarray[28] * 8;   // OP2 2nd Decay Rate
    fmsettings[sendchannel-1][38] = opnarray[38] * 8;   // OP4 2nd Decay Rate
    fmsettings[sendchannel-1][9] = opnarray[9] * 8;     // OP1 Release Rate
    fmsettings[sendchannel-1][19] = opnarray[19] * 8;   // OP3 Release Rate
    fmsettings[sendchannel-1][29] = opnarray[29] * 8;   // OP2 Release Rate
    fmsettings[sendchannel-1][39] = opnarray[39] * 8;   // OP4 Release Rate
    fmsettings[sendchannel-1][11] = opnarray[11] * 8;   // OP1 SSG-EG
    fmsettings[sendchannel-1][21] = opnarray[21] * 8;   // OP3 SSG-EG
    fmsettings[sendchannel-1][31] = opnarray[31] * 8;   // OP2 SSG-EG
    fmsettings[sendchannel-1][41] = opnarray[41] * 8;   // OP4 SSG-EG
    fmsettings[sendchannel-1][42] = 90;  // FM Level
    fmsettings[sendchannel-1][43] = 90;  // AM Level
    fmsettings[sendchannel-1][44] = 127; // Stereo (centered)
    fmsettings[sendchannel-1][45] = 0;   // OP1 Amplitude Modulation
    fmsettings[sendchannel-1][46] = 0;   // OP3 Amplitude Modulation
    fmsettings[sendchannel-1][47] = 0;   // OP2 Amplitude Modulation
    fmsettings[sendchannel-1][48] = 0;   // OP4 Amplitude Modulation
    fmsettings[sendchannel-1][49] = 0;   // Patch is unedited
}

void fmparamdisplay(void) {
    uint8_t i;
    char line1[32] = "";
    char line2[32] = "";
    char temp_str[16];
    
    oled_clear();
    
    if (mode == 2) {
        sprintf(line1, "C%d ", tfichannel);
    } else {
        strcpy(line1, "P  ");
    }
    
    switch(fmscreen) {
        case 1: // Algorithm, Feedback, Pan
            strcat(line1, "01:Alg FB Pan");
            
            if (polypan > 64) {
                strcpy(line2, "<> ON  ");
            } else {
                strcpy(line2, "<>OFF  ");
            }
            
            i = fmsettings[tfichannel-1][0];
            sprintf(temp_str, "%d ", i / 16);
            strcat(line2, temp_str);
            
            i = fmsettings[tfichannel-1][1];
            sprintf(temp_str, "%3d ", i);
            strcat(line2, temp_str);
            
            // Pan display
            i = fmsettings[tfichannel-1][44];
            if (i < 32) strcat(line2, "OFF");
            else if (i < 64) strcat(line2, " L ");
            else if (i < 96) strcat(line2, " R ");
            else strcat(line2, " C ");
            break;
            
        case 2: // Total Level (OP Volume)
            strcat(line1, "02:OP Volume");
            sprintf(line2, "%3d %3d %3d %3d", 
                fmsettings[tfichannel-1][4],
                fmsettings[tfichannel-1][24],
                fmsettings[tfichannel-1][14],
                fmsettings[tfichannel-1][34]);
            break;
            
        case 3: // Frequency Multiple
            strcat(line1, "03:Freq Multp");
            sprintf(line2, "%3d %3d %3d %3d", 
                fmsettings[tfichannel-1][2],
                fmsettings[tfichannel-1][22],
                fmsettings[tfichannel-1][12],
                fmsettings[tfichannel-1][32]);
            break;
            
        case 4: // Detune
            strcat(line1, "04:Detune-Mul");
            sprintf(line2, "%3d %3d %3d %3d", 
                fmsettings[tfichannel-1][3],
                fmsettings[tfichannel-1][23],
                fmsettings[tfichannel-1][13],
                fmsettings[tfichannel-1][33]);
            break;
            
        case 5: // Rate Scaling
            strcat(line1, "05:Rate Scale");
            sprintf(line2, "%3d %3d %3d %3d", 
                fmsettings[tfichannel-1][5],
                fmsettings[tfichannel-1][25],
                fmsettings[tfichannel-1][15],
                fmsettings[tfichannel-1][35]);
            break;
            
        case 6: // Attack Rate
            strcat(line1, "06:Attack");
            sprintf(line2, "%3d %3d %3d %3d", 
                fmsettings[tfichannel-1][6],
                fmsettings[tfichannel-1][26],
                fmsettings[tfichannel-1][16],
                fmsettings[tfichannel-1][36]);
            break;
            
        case 7: // Decay Rate 1
            strcat(line1, "07:Decay 1");
            sprintf(line2, "%3d %3d %3d %3d", 
                fmsettings[tfichannel-1][7],
                fmsettings[tfichannel-1][27],
                fmsettings[tfichannel-1][17],
                fmsettings[tfichannel-1][37]);
            break;
            
        case 8: // Sustain (Total Level 2)
            strcat(line1, "08:Sustain");
            sprintf(line2, "%3d %3d %3d %3d", 
                fmsettings[tfichannel-1][10],
                fmsettings[tfichannel-1][30],
                fmsettings[tfichannel-1][20],
                fmsettings[tfichannel-1][40]);
            break;
            
        case 9: // Decay Rate 2
            strcat(line1, "09:Decay 2");
            sprintf(line2, "%3d %3d %3d %3d", 
                fmsettings[tfichannel-1][8],
                fmsettings[tfichannel-1][28],
                fmsettings[tfichannel-1][18],
                fmsettings[tfichannel-1][38]);
            break;
            
        case 10: // Release Rate
            strcat(line1, "10:Release");
            sprintf(line2, "%3d %3d %3d %3d", 
                fmsettings[tfichannel-1][9],
                fmsettings[tfichannel-1][29],
                fmsettings[tfichannel-1][19],
                fmsettings[tfichannel-1][39]);
            break;
            
        case 11: // SSG-EG
            strcat(line1, "11:SSG-EG");
            sprintf(line2, "%3d %3d %3d %3d", 
                fmsettings[tfichannel-1][11],
                fmsettings[tfichannel-1][31],
                fmsettings[tfichannel-1][21],
                fmsettings[tfichannel-1][41]);
            break;
            
        case 12: // Amp Mod
            strcat(line1, "12:Amp Mod");
            strcpy(line2, "");
            for (int op = 0; op < 4; op++) {
                int setting_idx = (op == 0) ? 45 : (op == 1) ? 47 : (op == 2) ? 46 : 48;
                i = fmsettings[tfichannel-1][setting_idx];
                if (i < 64) {
                    strcat(line2, "OFF ");
                } else {
                    strcat(line2, " ON ");
                }
            }
            break;
            
        case 13: // LFO/FM/AM Level
            strcat(line1, "13:LFO/FM/AM");
            sprintf(line2, "    %3d %3d %3d", 
                lfospeed,
                fmsettings[tfichannel-1][42],
                fmsettings[tfichannel-1][43]);
            break;
    }
    
    oled_print(0, 0, line1);
    oled_print(0, 16, line2);
    oled_refresh();
}

void operatorparamdisplay(void) {
    handle_midi_input();
    
    uint8_t currentpotvalue[4];
    int8_t difference;
    
    // Read all 4 pot values through multiplexer
    for (int i = 0; i < 4; i++) {
        selectMuxChannel(i);
        currentpotvalue[i] = analogRead(MUX_SIG_PIN) >> 3;
    }
    
    bool displayNeedsUpdate = false;
    
    for (int i = 0; i <= 3; i++) {
        difference = prevpotvalue[i] - currentpotvalue[i];
        
        if (difference > 2 || difference < -2) {
            handle_midi_input();
            
            if (currentpotvalue[i] < 3) currentpotvalue[i] = 0;
            if (currentpotvalue[i] > 124) currentpotvalue[i] = 127;
            prevpotvalue[i] = currentpotvalue[i];
            
            if (mode == 2) {
                fmccsend(i, currentpotvalue[i]);
            } else {
                for (int c = 6; c >= 1; c--) {
                    tfichannel = c;
                    fmccsend(i, currentpotvalue[i]);
                }
            }
            
            displayNeedsUpdate = true;
        }
    }
    
    if (displayNeedsUpdate) {
        fmparamdisplay();
    }
}

void fmccsend(uint8_t potnumber, uint8_t potvalue) {
    // This matches the original 1.10 code exactly
    // NOTE: OP2 and OP3 are swapped in the comments to match original ordering
    
    switch(fmscreen) {
        
    // Algorithm, Feedback, Pan
    case 1:
    {
        // Handle pan mode display (same as original)
        oled_clear();
        if (polypan > 64) { // stereo pan on
            oled_print(1, 16, "L R ON");
        } else { // stereo pan off
            oled_print(1, 16, "L R OFF");
            fmsettings[tfichannel-1][44] = 127; 
            midi_send_cc(tfichannel, 77, 127); // reset panning to center
        }
        
        if (potnumber == 0) { polypan = potvalue; } // enter pan mode 
        if (potnumber == 1) { 
            fmsettings[tfichannel-1][0] = potvalue; 
            midi_send_cc(tfichannel, 14, potvalue); 
        } // Algorithm
        if (potnumber == 2) { 
            fmsettings[tfichannel-1][1] = potvalue; 
            midi_send_cc(tfichannel, 15, potvalue); 
        } // Feedback
        if (potnumber == 3) { 
            fmsettings[tfichannel-1][44] = potvalue; 
            midi_send_cc(tfichannel, 77, potvalue); 
        } // Pan
        break;
    }

    // Total Level (OP Volume) - THESE WORK CORRECTLY, NO SCALING
    case 2:
    {
        if (potnumber == 0) { 
            fmsettings[tfichannel-1][4] = potvalue; 
            midi_send_cc(tfichannel, 16, potvalue); 
        } // OP1 Total Level
        if (potnumber == 1) { 
            fmsettings[tfichannel-1][24] = potvalue; 
            midi_send_cc(tfichannel, 18, potvalue); 
        } // OP2 Total Level (note: original calls this OP3)
        if (potnumber == 2) { 
            fmsettings[tfichannel-1][14] = potvalue; 
            midi_send_cc(tfichannel, 17, potvalue); 
        } // OP3 Total Level (note: original calls this OP2)
        if (potnumber == 3) { 
            fmsettings[tfichannel-1][34] = potvalue; 
            midi_send_cc(tfichannel, 19, potvalue); 
        } // OP4 Total Level
        break;
    }

    // Multiplier - THESE WORK CORRECTLY, NO SCALING
    case 3:
    {
        if (potnumber == 0) { 
            fmsettings[tfichannel-1][2] = potvalue; 
            midi_send_cc(tfichannel, 20, potvalue); 
        } // OP1 Multiplier
        if (potnumber == 1) { 
            fmsettings[tfichannel-1][22] = potvalue; 
            midi_send_cc(tfichannel, 22, potvalue); 
        } // OP2 Multiplier (original calls this OP3)
        if (potnumber == 2) { 
            fmsettings[tfichannel-1][12] = potvalue; 
            midi_send_cc(tfichannel, 21, potvalue); 
        } // OP3 Multiplier (original calls this OP2)
        if (potnumber == 3) { 
            fmsettings[tfichannel-1][32] = potvalue; 
            midi_send_cc(tfichannel, 23, potvalue); 
        } // OP4 Multiplier
        break;
    }

    // Detune - THESE WORK CORRECTLY, NO SCALING
    case 4:
    {
        if (potnumber == 0) { 
            fmsettings[tfichannel-1][3] = potvalue; 
            midi_send_cc(tfichannel, 24, potvalue); 
        } // OP1 Detune
        if (potnumber == 1) { 
            fmsettings[tfichannel-1][23] = potvalue; 
            midi_send_cc(tfichannel, 26, potvalue); 
        } // OP2 Detune (original calls this OP3)
        if (potnumber == 2) { 
            fmsettings[tfichannel-1][13] = potvalue; 
            midi_send_cc(tfichannel, 25, potvalue); 
        } // OP3 Detune (original calls this OP2)
        if (potnumber == 3) { 
            fmsettings[tfichannel-1][33] = potvalue; 
            midi_send_cc(tfichannel, 27, potvalue); 
        } // OP4 Detune
        break;
    }

    // Rate Scaling - THESE WORK CORRECTLY, NO SCALING
    case 5:
    {
        if (potnumber == 0) { 
            fmsettings[tfichannel-1][5] = potvalue; 
            midi_send_cc(tfichannel, 39, potvalue); 
        } // OP1 Rate Scaling
        if (potnumber == 1) { 
            fmsettings[tfichannel-1][25] = potvalue; 
            midi_send_cc(tfichannel, 41, potvalue); 
        } // OP2 Rate Scaling (original calls this OP3)
        if (potnumber == 2) { 
            fmsettings[tfichannel-1][15] = potvalue; 
            midi_send_cc(tfichannel, 40, potvalue); 
        } // OP3 Rate Scaling (original calls this OP2)
        if (potnumber == 3) { 
            fmsettings[tfichannel-1][35] = potvalue; 
            midi_send_cc(tfichannel, 42, potvalue); 
        } // OP4 Rate Scaling
        break;
    }

    // Attack Rate - THESE WORK CORRECTLY, NO SCALING
    case 6:
    {
        if (potnumber == 0) { 
            fmsettings[tfichannel-1][6] = potvalue; 
            midi_send_cc(tfichannel, 43, potvalue); 
        } // OP1 Attack Rate
        if (potnumber == 1) { 
            fmsettings[tfichannel-1][26] = potvalue; 
            midi_send_cc(tfichannel, 45, potvalue); 
        } // OP2 Attack Rate (original calls this OP3)
        if (potnumber == 2) { 
            fmsettings[tfichannel-1][16] = potvalue; 
            midi_send_cc(tfichannel, 44, potvalue); 
        } // OP3 Attack Rate (original calls this OP2)
        if (potnumber == 3) { 
            fmsettings[tfichannel-1][36] = potvalue; 
            midi_send_cc(tfichannel, 46, potvalue); 
        } // OP4 Attack Rate
        break;
    }

    // Decay Rate 1 - THESE WORK CORRECTLY, NO SCALING
    case 7:
    {
        if (potnumber == 0) { 
            fmsettings[tfichannel-1][7] = potvalue; 
            midi_send_cc(tfichannel, 47, potvalue); 
        } // OP1 1st Decay Rate
        if (potnumber == 1) { 
            fmsettings[tfichannel-1][27] = potvalue; 
            midi_send_cc(tfichannel, 49, potvalue); 
        } // OP2 1st Decay Rate (original calls this OP3)
        if (potnumber == 2) { 
            fmsettings[tfichannel-1][17] = potvalue; 
            midi_send_cc(tfichannel, 48, potvalue); 
        } // OP3 1st Decay Rate (original calls this OP2)
        if (potnumber == 3) { 
            fmsettings[tfichannel-1][37] = potvalue; 
            midi_send_cc(tfichannel, 50, potvalue); 
        } // OP4 1st Decay Rate
        break;
    }

    // Sustain (2nd Total Level) - INVERTED CORRECTLY
    case 8:
    {
        if (potnumber == 0) { 
            fmsettings[tfichannel-1][10] = 127 - potvalue; 
            midi_send_cc(tfichannel, 55, 127 - potvalue); 
        } // OP1 2nd Total Level
        if (potnumber == 1) { 
            fmsettings[tfichannel-1][30] = 127 - potvalue; 
            midi_send_cc(tfichannel, 57, 127 - potvalue); 
        } // OP2 2nd Total Level (original calls this OP3)
        if (potnumber == 2) { 
            fmsettings[tfichannel-1][20] = 127 - potvalue; 
            midi_send_cc(tfichannel, 56, 127 - potvalue); 
        } // OP3 2nd Total Level (original calls this OP2)
        if (potnumber == 3) { 
            fmsettings[tfichannel-1][40] = 127 - potvalue; 
            midi_send_cc(tfichannel, 58, 127 - potvalue); 
        } // OP4 2nd Total Level
        break;
    }

    // Decay Rate 2 - THESE WORK CORRECTLY, NO SCALING
    case 9:
    {
        if (potnumber == 0) { 
            fmsettings[tfichannel-1][8] = potvalue; 
            midi_send_cc(tfichannel, 51, potvalue); 
        } // OP1 2nd Decay Rate
        if (potnumber == 1) { 
            fmsettings[tfichannel-1][28] = potvalue; 
            midi_send_cc(tfichannel, 53, potvalue); 
        } // OP2 2nd Decay Rate (original calls this OP3)
        if (potnumber == 2) { 
            fmsettings[tfichannel-1][18] = potvalue; 
            midi_send_cc(tfichannel, 52, potvalue); 
        } // OP3 2nd Decay Rate (original calls this OP2)
        if (potnumber == 3) { 
            fmsettings[tfichannel-1][38] = potvalue; 
            midi_send_cc(tfichannel, 54, potvalue); 
        } // OP4 2nd Decay Rate
        break;
    }

    // Release Rate - THESE WORK CORRECTLY, NO SCALING
    case 10:
    {
        if (potnumber == 0) { 
            fmsettings[tfichannel-1][9] = potvalue; 
            midi_send_cc(tfichannel, 59, potvalue); 
        } // OP1 Release Rate
        if (potnumber == 1) { 
            fmsettings[tfichannel-1][29] = potvalue; 
            midi_send_cc(tfichannel, 61, potvalue); 
        } // OP2 Release Rate (original calls this OP3)
        if (potnumber == 2) { 
            fmsettings[tfichannel-1][19] = potvalue; 
            midi_send_cc(tfichannel, 60, potvalue); 
        } // OP3 Release Rate (original calls this OP2)
        if (potnumber == 3) { 
            fmsettings[tfichannel-1][39] = potvalue; 
            midi_send_cc(tfichannel, 62, potvalue); 
        } // OP4 Release Rate
        break;
    }

    // SSG-EG - THESE WORK CORRECTLY, NO SCALING
    case 11:
    {
        if (potnumber == 0) { 
            fmsettings[tfichannel-1][11] = potvalue; 
            midi_send_cc(tfichannel, 90, potvalue); 
        } // OP1 SSG-EG
        if (potnumber == 1) { 
            fmsettings[tfichannel-1][31] = potvalue; 
            midi_send_cc(tfichannel, 92, potvalue); 
        } // OP2 SSG-EG (original calls this OP3)
        if (potnumber == 2) { 
            fmsettings[tfichannel-1][21] = potvalue; 
            midi_send_cc(tfichannel, 91, potvalue); 
        } // OP3 SSG-EG (original calls this OP2)
        if (potnumber == 3) { 
            fmsettings[tfichannel-1][41] = potvalue; 
            midi_send_cc(tfichannel, 93, potvalue); 
        } // OP4 SSG-EG
        break;
    }

    // Amp Mod - THESE WORK CORRECTLY, NO SCALING
    case 12:
    {
        if (potnumber == 0) { 
            fmsettings[tfichannel-1][45] = potvalue; 
            midi_send_cc(tfichannel, 70, potvalue); 
        } // OP1 Amplitude Modulation
        if (potnumber == 1) { 
            fmsettings[tfichannel-1][47] = potvalue; 
            midi_send_cc(tfichannel, 72, potvalue); 
        } // OP2 Amplitude Modulation (original calls this OP3)
        if (potnumber == 2) { 
            fmsettings[tfichannel-1][46] = potvalue; 
            midi_send_cc(tfichannel, 71, potvalue); 
        } // OP3 Amplitude Modulation (original calls this OP2)
        if (potnumber == 3) { 
            fmsettings[tfichannel-1][48] = potvalue; 
            midi_send_cc(tfichannel, 73, potvalue); 
        } // OP4 Amplitude Modulation
        break;
    }

    // LFO/FM/AM Level
    case 13:
    {
        // Blank out unused pot display (like original)
        oled_clear();
        oled_print(1, 16, "   "); // blank out the first pot display
        
        if (potnumber == 1) { 
            lfospeed = potvalue; 
            midi_send_cc(1, 1, potvalue); 
        } // LFO Speed (GLOBAL)
        if (potnumber == 2) { 
            fmsettings[tfichannel-1][42] = potvalue; 
            midi_send_cc(tfichannel, 75, potvalue); 
        } // FM Level
        if (potnumber == 3) { 
            fmsettings[tfichannel-1][43] = potvalue; 
            midi_send_cc(tfichannel, 76, potvalue); 
        } // AM Level
        break;
    }   
     
  } // end switch  
}

void handle_midi_input(void) {
    MIDI.read();
}

// Corrected handle_note_off function:

void handle_note_off(uint8_t channel, uint8_t note, uint8_t velocity) {
    // Only clear visualizer when in visualizer modes
    if ((mode == 5 || mode == 6) && channel >= 1 && channel <= 11) {
        clearVelocityViz(channel - 1);
    }
    
    // Rest of existing note-off logic unchanged...
    if (mode == 3 || mode == 4 || mode == 6) {
        if (channel == midichannel) {
            
            for (int i = 0; i <= 5; i++) {
                handle_midi_input();
                if (note == polynote[i]) {
                    
                    if (sustain) {
                        sustainon[i] = true;
                        noteheld[i] = false;
                        break;
                    } else {
                        midi_send_note_off(i + 1, note, velocity);
                        polyon[i] = false;
                        polynote[i] = 0;
                        noteheld[i] = false;
                        break;
                    }
                }
            }
            
            notecounter = 0;
            for (int i = 0; i <= 5; i++) {
                if (noteheld[i]) notecounter++;
            }
            
        }
    } else {
        if (channel >= 1 && channel <= 6) {
            if (sustain && sustainon[channel-1]) {
                noteheld[channel-1] = false;
                return;
            }
            polyon[channel-1] = false;
            noteheld[channel-1] = false;
            midi_send_note_off(channel, note, velocity);
        } else {
            MIDI.sendNoteOff(note, velocity, channel);
        }
    }
}


void handle_pitch_bend(uint8_t channel, int16_t bend) {
    if (mode == 3 || mode == 4) { // if we're in poly mode
        // Send pitch bend to all 6 FM channels (like the original)
        for (int i = 1; i <= 6; i++) {
            midi_send_pitch_bend(i, bend);
            handle_midi_input(); // Keep MIDI responsive during loops
        }
    } else { // mono mode
        if (channel >= 1 && channel <= 6) {
            midi_send_pitch_bend(channel, bend);
        } else {
            // Pass other channels straight through
            MIDI.sendPitchBend(bend, channel);
        }
    }
}

void showScanResults() {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Scan complete!");
    display.setCursor(0, 16);
    display.print("Files found: ");
    display.print(n);
    display.display();
    delay(1500);
}

void midiPanic(void) {
    // Send All Notes Off and Reset All Controllers to all 6 FM channels
    for (uint8_t ch = 1; ch <= 6; ch++) {
        midi_send_cc(ch, 123, 0);  // All Notes Off
        midi_send_cc(ch, 121, 0);  // Reset All Controllers
        midi_send_cc(ch, 120, 0);  // All Sound Off (immediate silence)
        
        // Clear internal voice tracking
        polyon[ch-1] = false;
        polynote[ch-1] = 0;
        sustainon[ch-1] = false;
        noteheld[ch-1] = false;
    }
    
    // Reset global state
    sustain = false;
    notecounter = 0;
    lowestnote = 0;
    
    // Show panic message briefly
    oled_clear();
    oled_print(0, 0, "MIDI PANIC!");
    oled_print(0, 16, "All notes off");
    oled_refresh();
    delay(500);
    
    // Refresh normal display based on current mode
    if (mode == 1 || mode == 3) {
        updateFileDisplay();
    } else if (mode == 2 || mode == 4) {
        fmparamdisplay();
    }
}
   
