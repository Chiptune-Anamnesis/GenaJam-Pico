// RP2040 "GenMDM v1.02" USB-MIDI -> Genesis Controller Port Transport
// Board: Raspberry Pi Pico / RP2040-Zero (Earle Philhower core)
// Tools -> USB Stack: Adafruit TinyUSB
// Tools -> USB Type: MIDI
//
// *** Genesis wiring (through level-shift / open-collector) ***
//   D3..D0 = Genesis pins 1 (Up), 2 (Down), 3 (Left), 4 (Right)
//   TL     = Genesis pin 9  (C / TL)  [we *drive* this (open-collector)]
//   TH     = Genesis pin 7           [we *read* this via divider]
//   GND    = Genesis pin 8
//
// Protocol summary:
//   - Console toggles TH (both edges).
//   - On each edge: we put the next nibble on D3..D0, then set TL to *mirror* TH.
//   - The stream itself is *raw MIDI bytes* (GenMDM v1.02 framing), which our ISR
//     splits into nibbles (high, then low) as the console clocks them in.

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>

// ------------ USB-MIDI ------------
Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

// ------------ Pin map (change if you like) ------------
static const uint8_t PIN_D0 = 2;   // -> Genesis pin 4 (Right) via open-collector
static const uint8_t PIN_D1 = 3;   // -> Genesis pin 3 (Left)
static const uint8_t PIN_D2 = 4;   // -> Genesis pin 2 (Down)
static const uint8_t PIN_D3 = 5;   // -> Genesis pin 1 (Up)
static const uint8_t PIN_TL = 6;   // -> Genesis pin 9 (C/TL) open-collector
static const uint8_t PIN_TH = 7;   // <- Genesis pin 7 (TH) via divider to 3.3V

// ------------ Open-collector helpers ------------
inline void ocDriveLow(uint8_t pin) { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
inline void ocRelease(uint8_t pin)  { pinMode(pin, INPUT); /* float; console pulls up */ }

inline void writeOC(uint8_t pin, bool highRelease) {
  if (highRelease) ocRelease(pin); else ocDriveLow(pin);
}

// D3..D0 correspond to Up,Down,Left,Right lines respectively.
inline void outputNibble(uint8_t n) {
  writeOC(PIN_D0, (n >> 0) & 1);
  writeOC(PIN_D1, (n >> 1) & 1);
  writeOC(PIN_D2, (n >> 2) & 1);
  writeOC(PIN_D3, (n >> 3) & 1);
}

// TL mirrors TH once nibble is valid (TH=1 -> TL released; TH=0 -> TL low)
inline void setTLByTH(bool thLevel) {
  writeOC(PIN_TL, thLevel ? true : false);
}

// ------------ Byte->Nibble FIFO ------------
static const uint16_t NIB_FIFO_SZ = 1024;
volatile uint8_t  nibFifo[NIB_FIFO_SZ];
volatile uint16_t nibHead = 0, nibTail = 0;

inline bool fifoEmpty() { return nibHead == nibTail; }
inline bool fifoFull()  { return (uint16_t)((nibHead + 1) % NIB_FIFO_SZ) == nibTail; }

void fifoPushNib(uint8_t n) {
  uint16_t next = (uint16_t)((nibHead + 1) % NIB_FIFO_SZ);
  if (next == nibTail) return; // drop on overflow
  nibFifo[nibHead] = (n & 0x0F);
  nibHead = next;
}

uint8_t fifoPopNib() {
  if (fifoEmpty()) return 0x0;
  uint8_t v = nibFifo[nibTail];
  nibTail = (uint16_t)((nibTail + 1) % NIB_FIFO_SZ);
  return v;
}

// Push a raw MIDI byte -> two nibbles (high, low)
inline void fifoPushByte(uint8_t b) {
  fifoPushNib((uint8_t)((b >> 4) & 0x0F));
  fifoPushNib((uint8_t)(b & 0x0F));
}

// ------------ GenMDM v1.02 exact framing ------------
// Just push literal MIDI bytes. No preambles or padding.
void encodeRawMidi3(uint8_t status, uint8_t d1, uint8_t d2) {
  fifoPushByte(status);
  fifoPushByte(d1);
  fifoPushByte(d2);
}
void encodeRawMidi2(uint8_t status, uint8_t d1) {
  fifoPushByte(status);
  fifoPushByte(d1);
}
void encodeRawMidi1(uint8_t status) {
  fifoPushByte(status);
}

// SysEx passthrough (F0 ... F7) as-is
void hSysEx(byte *data, unsigned length, bool /*complete*/) {
  fifoPushByte(0xF0);
  for (unsigned i = 0; i < length; ++i) fifoPushByte(data[i]);
  fifoPushByte(0xF7);
}

// ------------ TH edge ISR ------------
volatile bool lastTH = true;

void thISR() {
  bool th = digitalRead(PIN_TH);
  // On each TH edge, present next nibble and then mirror TL to TH.
  uint8_t n = fifoEmpty() ? 0x0 : fifoPopNib();
  outputNibble(n);
  setTLByTH(th);
  lastTH = th;
}

// ------------ MIDI Handlers (channel numbers are 1..16) ------------
void hNoteOn(byte ch, byte note, byte vel) {
  if (vel == 0) {
    encodeRawMidi3(0x80 | ((ch - 1) & 0x0F), note, 0);
  } else {
    encodeRawMidi3(0x90 | ((ch - 1) & 0x0F), note, vel);
  }
}
void hNoteOff(byte ch, byte note, byte /*vel*/) {
  encodeRawMidi3(0x80 | ((ch - 1) & 0x0F), note, 0);
}
void hCC(byte ch, byte cc, byte val) {
  encodeRawMidi3(0xB0 | ((ch - 1) & 0x0F), cc, val);
}
void hProgramChange(byte ch, byte pgm) {
  encodeRawMidi2(0xC0 | ((ch - 1) & 0x0F), pgm);
}
void hPitchBend(byte ch, int bend) {
  uint16_t v = (uint16_t)(bend + 8192);
  uint8_t lsb = v & 0x7F, msb = (v >> 7) & 0x7F;
  encodeRawMidi3(0xE0 | ((ch - 1) & 0x0F), lsb, msb);
}
void hAfterTouchCh(byte ch, byte val) {
  encodeRawMidi2(0xD0 | ((ch - 1) & 0x0F), val);
}
void hAfterTouchNote(byte ch, byte note, byte val) {
  encodeRawMidi3(0xA0 | ((ch - 1) & 0x0F), note, val);
}

// ------------ Setup / Loop ------------
void setup() {
  // Release all open-collector outputs (float-high via console pull-ups)
  ocRelease(PIN_D0); ocRelease(PIN_D1);
  ocRelease(PIN_D2); ocRelease(PIN_D3);
  ocRelease(PIN_TL);

  pinMode(PIN_TH, INPUT); // feed through resistor divider

  usb_midi.setStringDescriptor("Pico GenMDM v1.02 Transport");

  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.setHandleNoteOn(hNoteOn);
  MIDI.setHandleNoteOff(hNoteOff);
  MIDI.setHandleControlChange(hCC);
  MIDI.setHandleProgramChange(hProgramChange);
  MIDI.setHandlePitchBend(hPitchBend);
  MIDI.setHandleAfterTouchChannel(hAfterTouchCh);
  MIDI.setHandleAfterTouchPoly(hAfterTouchNote);
  MIDI.setHandleSystemExclusive(hSysEx);

  // Interrupt on both edges of TH
  attachInterrupt(digitalPinToInterrupt(PIN_TH), thISR, CHANGE);

  // Prime lines to a known state
  outputNibble(0x0);
  setTLByTH(digitalRead(PIN_TH));
}

void loop() {
  // Service USB-MIDI
  MIDI.read();
}
