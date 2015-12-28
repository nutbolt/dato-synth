// 
// TODO: 
// - Sequencer pin definitions need to be changed
// - Remove delay glitching when delay time is changed
// - Add delay feedback path and feedback button
// - Make filter envelope pot work on both release and attack
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
const int LED_DT = 4;            // Data pin to connect to the strip.
#define COLOR_ORDER GRB          // It's GRB for WS2812B
#define LED_TYPE WS2812          // (WS2801, WS2812B or APA102)
const int NUM_LEDS = 8;          // Number of LED's.
struct CRGB leds[NUM_LEDS];      // Initialize our LED array.
const int LED_BRIGHTNESS = 128;

// Pin assignment
const int DELAY_TIME_POT = A0;
const int AMP_ENV_POT = A1;
const int FILTER_ENV_POT = A3;
const int FILTER_RES_POT = A2;
const int FILTER_FREQ_POT = A4;
const int OSC_DETUNE_POT = A5;
const int BITCRUSH_PIN_0 = 12;
const int BITCRUSH_PIN_1 = 11;
const int FEEDBACK_PIN = 10;
const int LED_PIN = 13;
// Pins
const int TEMPO_PIN = A3; // Actually A9 but now doubled up with the filter env pot
const int SYNC_PIN = 5; // NC


// Key matrix
const byte ROWS = 5;
const byte COLS = 4; 
byte row_pins[ROWS] = {20,3,22,21,2}; //connect to the row pinouts of the keypad
byte col_pins[COLS] = {6,7,8,9}; //connect to the column pinouts of the keypad

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

// Key matrix hookup
char keys[ROWS][COLS] = {
  { KEYB_0, KEYB_1, KEYB_2, KEYB_3 },
  { KEYB_4, KEYB_5, KEYB_6, KEYB_7 },
  { STEP_0, STEP_1, STEP_2, STEP_3 },
  { STEP_4, STEP_5, STEP_6, STEP_7 },
  { OCT_UP, DBL_SPEED, DUMMY_KEY, OCT_DOWN }
};
Keypad keypad = Keypad( makeKeymap(keys), row_pins, col_pins, ROWS, COLS );

const int MIDI_CHANNEL = 1;
const int GATE_LENGTH_MSEC = 40;
const int SYNC_LENGTH_MSEC = 1;
const int MIN_TEMPO_MSEC = 300; // Tempo is actually an interval in ms

// Sequencer settings
const int NUM_STEPS = 8;
const int VELOCITY_STEP = 5;
const int INITIAL_VELOCITY = 100;
const int VELOCITY_THRESHOLD = 50;
unsigned char current_step = 0;
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
const float SAMPLERATE_STEPS[] = { 1109,2489,4435,44100 }; 
const char DETUNE_OFFSET_SEMITONES[] = { 3,4,5,7,9 };
const int MAX_DELAY_TIME_MSEC = 300;


// Variable declarations
int detune_amount = 0;
int osc1_frequency = 0;
byte osc1_midi_note = 0;
int resonance;
int filter_env_pot_value;
int amp_env_release;
boolean note_is_playing = false;

void setup() {
  Serial.begin(115200);
  AudioMemory(100); // 260 bytes per block, 2.9ms per block

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

  mixer2.gain(0, 0.5); // output from the bitcrusher
  mixer2.gain(1, 0.2); // output from the delay

  mixer3.gain(0, 1.0); // delay feed
  mixer3.gain(1, 0.3); // delay feedback

  delay1.delay(0,MAX_DELAY_TIME_MSEC);

  for (int i = 1; i < 8; i++) {
    delay1.disable(i); // disable unused delay outputs
  }

  #if USB_MIDI
    usbMIDI.setHandleNoteOff(midi_note_off);
    usbMIDI.setHandleNoteOn(midi_note_on) ;
  #else
    MIDI.setHandleNoteOn(midi_note_on);
    MIDI.setHandleNoteOff(midi_note_off);
    MIDI.begin(MIDI_CHANNEL);
  #endif

  pinMode(BITCRUSH_PIN_1, INPUT_PULLUP);
  pinMode(BITCRUSH_PIN_0, INPUT_PULLUP);
  pinMode(FEEDBACK_PIN, INPUT_PULLUP);

  pinMode(LED_PIN, OUTPUT);
  delay(1000);

}

void loop() {
  // Loop through the steps of the sequencer
  for (int s = 0; s < 8; s++) {
    
    current_step = s;

    // Decrement the velocity of the current note. If minimum velocity is reached leave it there
    if (step_velocity[current_step] <= VELOCITY_STEP) { /*step_velocity[current_step] = 0; step_enable[current_step] = 0;*/ }
    else{step_velocity[current_step] -= VELOCITY_STEP;}

    handle_leds();

    digitalWrite(SYNC_PIN, HIGH); // Volca sync on

    sync_off_time = millis() + SYNC_LENGTH_MSEC;
    
    if (step_enable[current_step]) {
      // MIDI.sendNoteOn(SCALE[step_note[current_step]], step_velocity[current_step], MIDI_CHANNEL);
      note_on(SCALE[step_note[current_step]], step_velocity[current_step]);
    }
    gate_off_time = millis() + GATE_LENGTH_MSEC;

    wait(sync_off_time);

    digitalWrite(SYNC_PIN, LOW); // Volca sync off

    wait(gate_off_time);

    // MIDI.sendNoteOff(SCALE[step_note[current_step]], 64, MIDI_CHANNEL);
    note_off();

    next_step_time = millis() + get_tempo_msec();

    // Very crude sequencer off implementation
    while (get_tempo_msec() >= MIN_TEMPO_MSEC) {
      sequencer_is_running = false;
      wait(millis() + 20);
      tempo = get_tempo_msec();
    }
    sequencer_is_running = true;

    // Advance the step number already, so any pressed keys end up in the next step
    if (s == 7) current_step = 0;
    else current_step++;

    wait(next_step_time);
  }
}

void read_pots(){
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
  filter_env_pot_value = 512;//analogRead(FILTER_ENV_POT);
  envelope2.release(amp_env_release * 0.5);

  delay1.delay(0,map(analogRead(DELAY_TIME_POT),0,1023,50,MAX_DELAY_TIME_MSEC));
  mixer2.gain(1, analogRead(DELAY_TIME_POT) * 0.0002);
  // TODO: smooth out delay time changes to prevent glitches

  bitcrusher1.sampleRate(SAMPLERATE_STEPS[(digitalRead(BITCRUSH_PIN_1) << 1) +  digitalRead(BITCRUSH_PIN_0)]);

  if(digitalRead(FEEDBACK_PIN)) {
    mixer3.gain(1, 0.4); // delay feedback
  } else {
    mixer3.gain(1, 0.9);
  }

  AudioInterrupts(); 
}

void midi_note_on(byte channel, byte note, byte velocity) {
  note_on(note, velocity);
}

void midi_note_off(byte channel, byte note, byte velocity) {
  note_off();
}

void note_on(byte midi_note, byte velocity) {

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
  //if (!note_is_playing) {
    envelope1.noteOff();
    envelope2.noteOff();
    note_is_playing = false;
  //}
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

void wait(unsigned long until) {
  while (millis() < until) {
    digitalWrite(LED_PIN, HIGH);

    #if USB_MIDI
      usbMIDI.read(); // All Channels
    #else
      MIDI.read();
    #endif

    digitalWrite(LED_PIN, LOW);

    handle_keys();
    read_pots();
  }
}


// Updates the LED colour and brightness to match the stored sequence
void handle_leds() {
  FastLED.clear();

  for (int l = 0; l < 8; l++) 
  {
    if (step_enable[l])
    {
      leds[l] = COLOURS[step_note[l]];

      if (step_velocity[l] < VELOCITY_THRESHOLD) 
      {
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
                    step_note[current_step] = k - KEYB_0;
                    step_enable[current_step] = 1;
                    step_velocity[current_step] = INITIAL_VELOCITY; 
                  } else {
                    // MIDI.sendNoteOn(SCALE[k-KEYB_0], INITIAL_VELOCITY, MIDI_CHANNEL);
                    note_on(SCALE[k-KEYB_0], INITIAL_VELOCITY);
                  }
                } else if (k <= STEP_7 && k >= STEP_0) {
                  step_enable[k-STEP_0] = 1-step_enable[k-STEP_0];
                  step_velocity[k-STEP_0] = INITIAL_VELOCITY;
                }
                break;
            /*case HOLD:
                break;*/
            case RELEASED:
                if (k <= KEYB_9 && k >= KEYB_0) {
                  // MIDI.sendNoteOff(SCALE[k-KEYB_0], 0, MIDI_CHANNEL);
                  note_off();
                }
                break;
            /*case IDLE:
                break;
            */
        }
      }
    }
  }
}

int get_tempo_msec() {
  return map(analogRead(TEMPO_PIN),30,1023,MIN_TEMPO_MSEC,GATE_LENGTH_MSEC);
}