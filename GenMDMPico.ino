#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>

// ------------ USB-MIDI ------------
Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

// ------------ Pin map (Pico GPIO → open-collector buffer → Genesis) ------------
static const uint8_t PIN_D0 = 2;   // -> Genesis pin 4 (Right)
static const uint8_t PIN_D1 = 3;   // -> Genesis pin 3 (Left)
static const uint8_t PIN_D2 = 4;   // -> Genesis pin 2 (Down)
static const uint8_t PIN_D3 = 5;   // -> Genesis pin 1 (Up)
static const uint8_t PIN_TL = 6;   // -> Genesis pin 9 (C/TL)
static const uint8_t PIN_TH = 7;   // <- Genesis pin 7 (TH, divider to Pico)

// Optional debug LED on Pico (built-in = 25)
static const uint8_t LED_GPIO = LED_BUILTIN;
#define DEBUG_LED 0  // set to 1 to flash briefly on TH edges

// ------------ Open-collector helpers ------------
inline void ocDriveLow(uint8_t pin) { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
inline void ocRelease(uint8_t pin)  { pinMode(pin, INPUT); }

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

// TL mirrors TH once nibble is valid
inline void setTLByTH(bool thLevel) {
  writeOC(PIN_TL, thLevel ? true : false);
}

// ------------ Byte→Nibble FIFO ------------
static const uint16_t NIB_FIFO_SZ = 1024;
volatile uint8_t  nibFifo[NIB_FIFO_SZ];
volatile uint16_t nibHead = 0, nibTail = 0;

inline bool fifoEmpty() { return nibHead == nibTail; }

void fifoPushNib(uint8_t n) {
  uint16_t next = (uint16_t)((nibHead + 1) % NIB_FIFO_SZ);
  if (next == nibTail) return; // drop if full
  nibFifo[nibHead] = (n & 0x0F);
  nibHead = next;
}

uint8_t fifoPopNib() {
  if (fifoEmpty()) return 0x0;
  uint8_t v = nibFifo[nibTail];
  nibTail = (uint16_t)((nibTail + 1) % NIB_FIFO_SZ);
  return v;
}

// Push a raw MIDI byte → two nibbles
inline void fifoPushByte(uint8_t b) {
  fifoPushNib((uint8_t)((b >> 4) & 0x0F));
  fifoPushNib((uint8_t)(b & 0x0F));
}

// ------------ GenMDM v1.02 framing = raw MIDI bytes ------------
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

// SysEx passthrough (MIDI lib v5.x = 2-arg callback)
void hSysEx(byte *data, unsigned length) {
  fifoPushByte(0xF0);
  for (unsigned i = 0; i < length; ++i) fifoPushByte(data[i]);
  fifoPushByte(0xF7);
}

// ------------ TH edge ISR ------------
volatile bool lastTH = true;

void thISR() {
#if DEBUG_LED
  digitalWrite(LED_GPIO, HIGH);
#endif
  bool th = digitalRead(PIN_TH);

  // Pop next nibble or idle as 0xF (all released/high)
  uint8_t n = fifoEmpty() ? 0xF : fifoPopNib();
  outputNibble(n);

  // Let data lines settle before acknowledging
  delayMicroseconds(2);

  // Mirror TH on TL
  setTLByTH(th);
  lastTH = th;

#if DEBUG_LED
  digitalWrite(LED_GPIO, LOW);
#endif
}

// ------------ MIDI Handlers ------------
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
#if DEBUG_LED
  pinMode(LED_GPIO, OUTPUT);
  digitalWrite(LED_GPIO, LOW);
#endif

  // Release outputs (float-high)
  ocRelease(PIN_D0); ocRelease(PIN_D1);
  ocRelease(PIN_D2); ocRelease(PIN_D3);
  ocRelease(PIN_TL);

  pinMode(PIN_TH, INPUT); // TH through divider

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

  // Interrupt on both TH edges
  attachInterrupt(digitalPinToInterrupt(PIN_TH), thISR, CHANGE);

  // Prime bus to "idle ones"
  outputNibble(0xF);
  setTLByTH(digitalRead(PIN_TH));
}

void loop() {
  MIDI.read();  // USB-MIDI service
}
