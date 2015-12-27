// 
// TODO: 
// - Remove delay glitching when delay time is changed
// - Add delay feedback path and feedback button
// - Make filter envelope pot work on both release and attack
//
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
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

// Musical settings
const int SCALE[] = { 46,49,51,54,61,63,66,68 }; // Low with 2 note split
//const int BLACK_KEYS[] = {22,25,27,30,32,34,37,39,42,44,46,49,51,54,56,58,61,63,66,68,73,75,78,80};
const int SAMPLERATE_STEPS[] = {
  1109,2489,4435,44100
}; // These steps are at 'musical' values - they coincide with midi note frequencies 466,831,2960,7549,
const int DETUNE_OFFSET_SEMITONES[] = { 3,4,5,7,9 };


// Variable declarations
int detune_amount = 0;
int osc1_frequency = 0;
byte osc1_midi_note = 0;
static int resonance;
static int filter_env_pot_value;
static int amp_env_release;

void setup() {
  Serial.begin(115200);
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

  mixer2.gain(0, 0.5); // output from the bitcrusher
  mixer2.gain(1, 0.2); // output from the delay

  mixer3.gain(0, 1.0); // delay feed
  mixer3.gain(1, 0.3); // delay feedback

  delay1.delay(0,300);

  for (int i = 1; i < 8; i++) {
    delay1.disable(i); // disable unused delay outputs
  }

  #if USB_MIDI
    usbMIDI.setHandleNoteOff(midi_note_off);
    usbMIDI.setHandleNoteOn(midi_note_on) ;
  #else
    MIDI.setHandleNoteOn(midi_note_on);
    MIDI.setHandleNoteOff(midi_note_off);
    MIDI.begin();
  #endif

  pinMode(BITCRUSH_PIN_1, INPUT_PULLUP);
  pinMode(BITCRUSH_PIN_0, INPUT_PULLUP);
  pinMode(FEEDBACK_PIN, INPUT_PULLUP);

  pinMode(LED_PIN, OUTPUT);
  delay(1000);

}

void loop() {
  wait(millis() + 1000);
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
  filter_env_pot_value = analogRead(FILTER_ENV_POT);
  envelope2.release(amp_env_release * 0.5);

  delay1.delay(0,map(analogRead(DELAY_TIME_POT),0,1023,50,300));
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

  envelope1.noteOn();
  envelope2.noteOn();
}

void note_off() {
  // if (!note_is_playing) {
  envelope1.noteOff();
  envelope2.noteOff();
  // }
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
    read_pots();
  }
}