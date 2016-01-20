// 
// TODO: 
// - Remove delay glitching when delay time is changed
//   1. Switch the delay tap to use and set the delay time to the pot's value
//   2. Use a mixer to fade between the two taps
//   X. create a new clocked delay effect
//
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <FastLED.h>
#include <Keypad.h>
#include "midinotes.h"

// #define USB_MIDI
#if USB_MIDI
#else
  #include <MIDI.h>
  MIDI_CREATE_DEFAULT_INSTANCE();
#endif

// GUItool: begin automatically generated code
AudioSynthWaveform       waveform2;      //xy=87,292
AudioSynthWaveform       waveform1;      //xy=88,239
AudioSynthWaveformDc     dc1;            //xy=97,344
AudioEffectEnvelope      envelope2;      //xy=236,344
AudioMixer4              mixer1;         //xy=264,278
AudioFilterStateVariable filter1;        //xy=412,286
AudioEffectEnvelope      envelope1;      //xy=567,273
AudioMixer4              mixer3;         //xy=752,366
AudioEffectBitcrusher    bitcrusher1;    //xy=818,274
AudioEffectDelay         delay1;         //xy=908,365
AudioMixer4              mixer2;         //xy=1070,293
AudioOutputAnalog        dac1;           //xy=1213,293
AudioConnection          patchCord1(waveform2, 0, mixer1, 1);
AudioConnection          patchCord2(waveform1, 0, mixer1, 0);
AudioConnection          patchCord3(dc1, envelope2);
AudioConnection          patchCord4(envelope2, 0, filter1, 1);
AudioConnection          patchCord5(mixer1, 0, filter1, 0);
AudioConnection          patchCord6(filter1, 0, envelope1, 0);
AudioConnection          patchCord7(envelope1, bitcrusher1);
AudioConnection          patchCord8(envelope1, 0, mixer3, 0);
AudioConnection          patchCord9(mixer3, delay1);
AudioConnection          patchCord10(bitcrusher1, 0, mixer2, 0);
AudioConnection          patchCord11(delay1, 0, mixer2, 1);
AudioConnection          patchCord12(delay1, 0, mixer3, 1);
AudioConnection          patchCord13(mixer2, dac1);
// GUItool: end automatically generated code

// LED strip
#define LED_DT 11          // Data pin to connect to the strip. Would like this to be pin 17
#define COLOR_ORDER GRB          // It's GRB for WS2812B
#define LED_TYPE WS2812          // (WS2801, WS2812B or APA102)
#define NUM_LEDS  8          // Number of LED's.
struct CRGB leds[NUM_LEDS];      // Initialize our LED array.
const int LED_BRIGHTNESS = 128;

// Pin assignment
const int DELAY_TIME_POT = A0;
const int AMP_ENV_POT = A3;
const int FILTER_RES_POT = A2;
const int DELAY_MIX_POT = A1;
const int FILTER_FREQ_POT = A4;
const int OSC_DETUNE_POT = A5;
const int TEMPO_POT = A9; 
const int LED_PIN = 13;

// Key matrix
const byte ROWS = 7;
const byte COLS = 4; 
byte row_pins[ROWS] = {9,4,10,12,2,21,22}; //connect to the row pinouts of the keypad
byte col_pins[COLS] = {5,6,7,8}; //connect to the column pinouts of the keypad

// Key matrix char definitions
const char DUMMY_KEY = 126;
const char KEYB_0 = 65;
const char KEYB_1 = 66;
const char KEYB_2 = 67;
const char KEYB_3 = 68;
const char KEYB_4 = 69;
const char KEYB_5 = 70;
const char KEYB_6 = 71;
const char KEYB_7 = 72;
const char KEYB_8 = 73;
const char KEYB_9 = 74;

const char STEP_0 = 97;
const char STEP_1 = 98;
const char STEP_2 = 99;
const char STEP_3 = 100;
const char STEP_4 = 101;
const char STEP_5 = 102;
const char STEP_6 = 103;
const char STEP_7 = 104;

const char OCT_DOWN = 30;
const char OCT_UP = 31;
const char DBL_SPEED = 32;
const char SEQ_RANDOM = 33;

const char BITC_0 = 34;
const char BITC_1 = 35;
const char FX_0   = 36;
const char OSC1_PULSE = 37;
const char MIC_2 = 38;
const char OSC1_SAW = 39;
const char OSC2_PULSE = 40;
const char MIC_1 = 41;

// Key matrix hookup
char keys[ROWS][COLS] = {
  { KEYB_0, KEYB_1, KEYB_2, KEYB_3 },
  { KEYB_4, KEYB_5, KEYB_6, KEYB_7 },
  { STEP_0, STEP_1, STEP_2, STEP_3 },
  { STEP_4, STEP_5, STEP_6, STEP_7 },
  { OCT_DOWN, DBL_SPEED, SEQ_RANDOM, OCT_UP },
  { BITC_0, BITC_1, FX_0,  OSC1_PULSE },
  { MIC_2, OSC1_SAW, OSC2_PULSE, MIC_1 }
};
Keypad keypad = Keypad( makeKeymap(keys), row_pins, col_pins, ROWS, COLS );

const int MIDI_CHANNEL = 1;
const int GATE_LENGTH_MSEC = 40;
const int SYNC_LENGTH_MSEC = 1;
const int MIN_TEMPO_MSEC = 300; // Tempo is actually an interval in ms

// Sequencer settings
const int NUM_STEPS = 8;
const int VELOCITY_STEP = 1;
const int INITIAL_VELOCITY = 100;
const int VELOCITY_THRESHOLD = 50;
unsigned char current_step = 0;
unsigned char target_step = 0;
unsigned int tempo = 0;
unsigned long next_step_time = 0;
unsigned long gate_off_time = 0;
unsigned long sync_off_time = 0;

// Note to colour mapping
const CRGB COLOURS[] = {
  0x993366,
  0x990066,
  CRGB::Blue,
  0x008060,
  CRGB::Green,
  0x999900,
  0xCC6300,
  0xCC0000
};

// Initial sequencer values
byte step_note[] = { 0,1,2,3,4,5,6,7 };
byte step_enable[] = { 1,1,1,1,1,1,1,1 };
int step_velocity[] = { 127,127,127,127,127,127,127,127 };
char set_key = 9;
static boolean sequencer_is_running = true;

// Musical settings
//const int BLACK_KEYS[] = {22,25,27,30,32,34,37,39,42,44,46,49,51,54,56,58,61,63,66,68,73,75,78,80};
const int SCALE[] = { 46,49,51,54,61,63,66,68 }; // Low with 2 note split
const float SAMPLERATE_STEPS[] = { 44100,4435,2489,1109 }; 
const char DETUNE_OFFSET_SEMITONES[] = { 3,4,5,7,9 };
const int MAX_DELAY_TIME_MSEC = 200;

// Variable declarations
int detune_amount = 0;
int osc1_frequency = 0;
byte osc1_midi_note = 0;
int resonance;
int filter_env_pot_value;
int amp_env_release;
boolean note_is_playing = false;
boolean double_speed = false;
int transpose = 0;
char bitcrusher_button0_pressed = 0;
char bitcrusher_button1_pressed = 0;
boolean next_step_is_random = false;

void setup() {

  AudioMemory(180); // 260 bytes per block, 2.9ms per block

  dc1.amplitude(1.0); // Filter env - this is going to be related to velocity

  // Initialize the audio components
  waveform1.begin(0.3, 220, WAVEFORM_SAWTOOTH);
  waveform2.begin(0.2, 110, WAVEFORM_SQUARE);
  
  mixer1.gain(0, 0.5);
  mixer1.gain(1, 0.5);

  filter1.resonance(2.8); // range 0.7-5.0
  filter1.frequency(400);
  filter1.octaveControl(4);

  bitcrusher1.bits(16);
  bitcrusher1.sampleRate(44100);

  envelope1.delay(0);
  envelope1.attack(1);
  envelope1.hold(0);
  envelope1.decay(0);
  envelope1.sustain(1.0);
  envelope1.release(400);

  envelope2.delay(0);
  envelope2.attack(20);
  envelope2.hold(0);
  envelope2.decay(0);
  envelope2.sustain(1.0);
  envelope2.release(300);

  mixer2.gain(0, 1.0); // output from the bitcrusher
  mixer2.gain(1, 0.5); // output from the delay

  mixer3.gain(0, 1.0); // delay feed
  mixer3.gain(1, 0.3); // delay feedback

  delay1.delay(0,MAX_DELAY_TIME_MSEC);

  for (int i = 1; i < 8; i++) {
    delay1.disable(i); // disable unused delay outputs
  }

  FastLED.addLeds<LED_TYPE, LED_DT, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHTNESS); 
  FastLED.clear();
  leds[7] = CRGB::Blue;
  FastLED.show();
  
  keypad.setDebounceTime(5);

  #if USB_MIDI
    usbMIDI.setHandleNoteOff(midi_note_off);
    usbMIDI.setHandleNoteOn(midi_note_on) ;
  #else
    MIDI.setHandleNoteOn(midi_note_on);
    MIDI.setHandleNoteOff(midi_note_off);
    MIDI.begin(MIDI_CHANNEL);
  #endif

  pinMode(LED_PIN, OUTPUT);
}

void loop() {

  // Decrement the velocity of the current note. If minimum velocity is reached leave it there
  if (step_velocity[current_step] <= VELOCITY_STEP) { /*step_velocity[current_step] = 0; step_enable[current_step] = 0;*/ }
  else { step_velocity[current_step] -= VELOCITY_STEP; }

  // digitalWrite(SYNC_PIN, HIGH); // Volca sync on

  if (step_enable[current_step]) {
    // MIDI.sendNoteOn(SCALE[step_note[current_step]], step_velocity[current_step], MIDI_CHANNEL);
    note_on(SCALE[step_note[current_step]]+transpose, step_velocity[current_step]);
  } 
  
  handle_input_until(sync_off_time);

  // digitalWrite(SYNC_PIN, LOW); // Volca sync off

  handle_input_until(gate_off_time);

  // MIDI.sendNoteOff(SCALE[step_note[current_step]], 64, MIDI_CHANNEL);
  note_off();

  // Very crude sequencer off implementation but works surprisingly well
  while (tempo_interval_msec() >= MIN_TEMPO_MSEC) {
    sequencer_is_running = false;
    handle_input_until(millis() + 20);
    tempo = tempo_interval_msec();
  }
  sequencer_is_running = true;

  if(!double_speed) {
    next_step_time = millis() + tempo_interval_msec();
  } else {
    next_step_time = millis() + (tempo_interval_msec()/2);
  }
  
  sync_off_time = next_step_time + SYNC_LENGTH_MSEC;
  gate_off_time = next_step_time + GATE_LENGTH_MSEC;

  target_step++;
  if (target_step >= NUM_STEPS) target_step = 0;
 
  handle_input_until(next_step_time);

  if (!next_step_is_random) {
    current_step++;
    if (current_step >= NUM_STEPS) current_step = 0;
  } else {
    current_step = random(NUM_STEPS);
  }

}

void read_pots() {
  AudioNoInterrupts();

  detune_amount = analogRead(OSC_DETUNE_POT);
  waveform2.frequency(detune(osc1_midi_note,detune_amount));
  filter1.frequency(map(analogRead(FILTER_FREQ_POT),0,1023,60,300));

  resonance = analogRead(FILTER_RES_POT);
  filter1.resonance(map(resonance,0,1023,70,500)/100.0);

  amp_env_release = map(analogRead(AMP_ENV_POT),0,1023,10,300);
  envelope1.release(amp_env_release);

  // Filter envelope. 
  // TODO: Left of center the pot influences the attack. Right of center it influences the release
  filter_env_pot_value = 512;//analogRead(DELAY_MIX_POT);
  envelope2.release(amp_env_release * 0.5);

  delay1.delay(0, map(analogRead(DELAY_TIME_POT)/16,0,64,50,MAX_DELAY_TIME_MSEC));
  mixer2.gain(1, analogRead(DELAY_TIME_POT) * 0.0002);
  // TODO: smooth out delay time changes to prevent glitches

  bitcrusher1.sampleRate(SAMPLERATE_STEPS[(bitcrusher_button1_pressed << 1) + bitcrusher_button0_pressed]);

  // if(digitalRead(FEEDBACK_PIN)) {
  mixer3.gain(1, 0.4); // delay feedback
  // } else {
  //   mixer3.gain(1, 0.9);
  // }

  AudioInterrupts(); 
}

void midi_note_on(byte channel, byte note, byte velocity) {
  note_on(note, velocity);
}

void midi_note_off(byte channel, byte note, byte velocity) {
  note_off();
}

void note_on(byte midi_note, byte velocity) {

  digitalWrite(LED_PIN, HIGH);

  AudioNoInterrupts();

  dc1.amplitude(velocity / 127.); // DC amplitude controls filter env amount
  osc1_midi_note = midi_note;
  osc1_frequency = (int)midi_note_to_frequency(midi_note);
  waveform1.frequency(osc1_frequency);
  // Detune OSC2
  waveform2.frequency(detune(osc1_midi_note,detune_amount));

  AudioInterrupts(); 

  note_is_playing = true;
  envelope1.noteOn();
  envelope2.noteOn();
}

void note_off() {
  if (note_is_playing) {
    envelope1.noteOff();
    envelope2.noteOff();
    note_is_playing = false;
  }
  digitalWrite(LED_PIN, LOW);
}

float midi_note_to_frequency(int x) {
  return MIDI_NOTE_FREQUENCY[x];
}

int detune(int note, int amount) { // amount goes from 0-1023
  if (amount > 800) {
    return midi_note_to_frequency(note) * (amount+9000)/10000.;
  } else if (amount < 100) {
    return midi_note_to_frequency(note - 12) * ( 20000 - amount )/20000.;
  } else {
    int offset = map(amount,200,900,4,0);
    return midi_note_to_frequency(note - DETUNE_OFFSET_SEMITONES[offset]);
  }
}

void handle_input_until(unsigned long until) {
  while (millis() < until) {

    update_leds();

    #if USB_MIDI
      usbMIDI.read(); // All Channels
    #else
      MIDI.read();
    #endif

    handle_keys();
    read_pots();
  }
}


// Updates the LED colour and brightness to match the stored sequence
void update_leds() {
  // TODO: check if updating the leds is necessary
  FastLED.clear();

  if (!sequencer_is_running) {
    FastLED.show();
    return;
  } 

  for (int l = 0; l < 8; l++) {
    if (step_enable[l]) {

      leds[l] = COLOURS[step_note[l]];

      if (step_velocity[l] < VELOCITY_THRESHOLD) {
        leds[l].nscale8_video(step_velocity[l]+20);
      }
    }
    else leds[l] = CRGB::Black;
    leds[current_step] = CRGB::White;
  }

  FastLED.show();
}

// Scans the keypad and handles step enable and keys
void handle_keys() {
   // Fills keypad.key[ ] array with up-to 10 active keys.
  // Returns true if there are ANY active keys.
  if (keypad.getKeys())
  {
    for (int i=0; i<LIST_MAX; i++)   // Scan the whole key list.
    {
      if ( keypad.key[i].stateChanged )   // Only find keys that have changed state.
      {
        char k = keypad.key[i].kchar;
        switch (keypad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
            case PRESSED:    
                if (k <= KEYB_9 && k >= KEYB_0) {
                  if(sequencer_is_running) {
                    step_note[target_step] = k - KEYB_0;
                    step_enable[target_step] = 1;
                    step_velocity[target_step] = INITIAL_VELOCITY; 
                  } else {
                    // MIDI.sendNoteOn(SCALE[k-KEYB_0], INITIAL_VELOCITY, MIDI_CHANNEL);
                    note_on(SCALE[k-KEYB_0]+transpose, INITIAL_VELOCITY);
                  }
                } else if (k <= STEP_7 && k >= STEP_0) {
                  step_enable[k-STEP_0] = 1-step_enable[k-STEP_0];
                  step_velocity[k-STEP_0] = INITIAL_VELOCITY;
                } else if (k == DBL_SPEED) {
                  double_speed = true;
                } else if (k == OCT_DOWN) {
                  transpose = -12;
                } else if (k == OCT_UP) {
                  transpose = 12;
                } else if (k == SEQ_RANDOM) {
                  next_step_is_random = true;
                } else if (k == BITC_0) {
                  bitcrusher_button0_pressed = 1;
                } else if (k == BITC_1) {
                  bitcrusher_button1_pressed = 1;
                } else if (k == OSC1_SAW) {
                  waveform1.begin(WAVEFORM_SAWTOOTH);
                } else if (k == OSC1_PULSE) {
                  waveform1.begin(WAVEFORM_SQUARE);
                } else if (k == MIC_1) {
                  waveform1.begin(WAVEFORM_SINE);
                } else if (k == MIC_2) {
                  waveform2.begin(WAVEFORM_SINE);
                } else if (k == OSC2_PULSE) {
                  waveform2.begin(WAVEFORM_SQUARE);
                }
                break;
            case HOLD:
                break;
            case RELEASED:
                if (k <= KEYB_9 && k >= KEYB_0) {
                  note_off();
                } else if (k == DBL_SPEED) {
                  double_speed = false;
                } else if (k == OCT_DOWN) {
                  transpose = 0;
                } else if (k == OCT_UP) {
                  transpose = 0;
                } else if (k == SEQ_RANDOM) {
                  next_step_is_random = false;
                } else if (k == BITC_0) {
                  bitcrusher_button0_pressed = 0;
                } else if (k == BITC_1) {
                  bitcrusher_button1_pressed = 0;
                }
                break;
            case IDLE:
                break;
        }
      }
    }
  }
}

int tempo_interval_msec() {
  int interval = map(analogRead(TEMPO_POT),100,1023,MIN_TEMPO_MSEC,GATE_LENGTH_MSEC);
  return interval;
}