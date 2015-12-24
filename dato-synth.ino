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

const float MIDI_NOTE_FREQUENCY[127] = { 
  8.1757989156, 8.6619572180, 9.1770239974, 9.7227182413, 10.3008611535, 10.9133822323, 11.5623257097, 12.2498573744, 12.9782717994, 13.7500000000, 14.5676175474, 15.4338531643, 16.3515978313,
 17.3239144361, 18.3540479948, 19.4454364826, 20.6017223071, 21.8267644646, 23.1246514195, 24.4997147489, 25.9565435987, 27.5000000000, 29.1352350949, 30.8677063285,  32.7031956626,
 34.6478288721, 36.7080959897, 38.8908729653, 41.2034446141, 43.6535289291, 46.2493028390, 48.9994294977, 51.9130871975, 55.0000000000, 58.2704701898, 61.7354126570, 65.4063913251,
 69.2956577442, 73.4161919794, 77.7817459305, 82.4068892282, 87.3070578583, 92.4986056779, 97.9988589954, 103.8261743950, 110.0000000000, 116.5409403795, 123.4708253140, 130.8127826503,
138.5913154884, 146.8323839587, 155.5634918610, 164.8137784564, 174.6141157165, 184.9972113558, 195.9977179909, 207.6523487900, 220.0000000000, 233.0818807590, 246.9416506281,  261.6255653006,
 277.1826309769, 293.6647679174, 311.1269837221, 329.6275569129, 349.2282314330, 369.9944227116, 391.9954359817, 415.3046975799, 440.0000000000, 466.1637615181, 493.8833012561, 523.2511306012,
 554.3652619537, 587.3295358348, 622.2539674442, 659.2551138257, 698.4564628660, 739.9888454233, 783.9908719635, 830.6093951599, 880.0000000000, 932.3275230362, 987.7666025122,1046.5022612024,
1108.7305239075,1174.6590716696,1244.5079348883,1318.5102276515,1396.9129257320,1479.9776908465,1567.9817439270,1661.2187903198,1760.0000000000,1864.6550460724,1975.5332050245,2093.0045224048,
2217.4610478150,2349.3181433393,2489.0158697766,2637.0204553030,2793.8258514640,2959.9553816931,3135.9634878540,3322.4375806396,3520.0000000000,3729.3100921447,3951.0664100490,4186.0090448096,
4434.9220956300,4698.6362866785,4978.0317395533,5274.0409106059,5587.6517029281,5919.9107633862,6271.9269757080,6644.8751612791,7040.0000000000,7458.6201842894,7902.1328200980, 8372.0180896192,
 8869.8441912599, 9397.2725733570, 9956.0634791066,10548.0818212118,11175.3034058561,11839.8215267723
};
const int SCALE[] = { 46,49,51,54,61,63,66,68 }; // Low with 2 note split
//const int BLACK_KEYS[] = {22,25,27,30,32,34,37,39,42,44,46,49,51,54,56,58,61,63,66,68,73,75,78,80};
const int SAMPLERATE_STEPS[] = {
  1109,2489,4435,44100
}; // These steps are at 'musical' values - they coincide with midi note frequencies 466,831,2960,7549,
const int DETUNE_OFFSET_SEMITONES[] = { 3,4,5,7,9 };


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
  mixer3.gain(1, 0.2); // delay feedback

  delay1.delay(0,300);

  for (int i = 1; i < 8; i++) {
    delay1.disable(i); // disable unused delay outputs
  }

  usbMIDI.setHandleNoteOff(usb_note_off);
  usbMIDI.setHandleNoteOn(usb_note_on) ;

  pinMode(BITCRUSH_PIN_1, INPUT_PULLUP);
  pinMode(BITCRUSH_PIN_0, INPUT_PULLUP);
  pinMode(FEEDBACK_PIN, INPUT_PULLUP);

  pinMode(LED_PIN, OUTPUT);
  delay(1000);

}

void loop() {
  wait(millis() + 1000);
  Serial.println(filter_env_pot_value,DEC);
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

  // Filter envelope. Left of center the pot influences the attack. Right of center it influences the release
  filter_env_pot_value = analogRead(FILTER_ENV_POT);
  
  if(filter_env_pot_value < 512) {
    envelope2.attack(filter_env_pot_value);
    envelope2.release(amp_env_release);
  } else {
    envelope2.attack(20);
    envelope2.release(filter_env_pot_value - 512);
  }

  delay1.delay(0,map(analogRead(DELAY_TIME_POT),0,1023,50,300));
  mixer2.gain(1, analogRead(DELAY_TIME_POT) * 0.0002);
  // TODO: smooth out delay time changes to prevent glitches

  bitcrusher1.sampleRate(SAMPLERATE_STEPS[(digitalRead(BITCRUSH_PIN_1) << 1) +  digitalRead(BITCRUSH_PIN_0)]);

  if(digitalRead(FEEDBACK_PIN)) {
    mixer3.gain(1, 0.2); // delay feedback
  } else {
    mixer3.gain(1, 1.0);
  }

  AudioInterrupts(); 
}

void usb_note_on(byte channel, byte note, byte velocity) {
  note_on(note, velocity);
}

void usb_note_off(byte channel, byte note, byte velocity) {
  note_off();
}

void note_on(byte midi_note, byte velocity) {

  AudioNoInterrupts();

  dc1.amplitude(velocity / 127.); // DC amplitude 
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
    usbMIDI.read(); // All Channels
    digitalWrite(LED_PIN, LOW);
    read_pots();
  }
}