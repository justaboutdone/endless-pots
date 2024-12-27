/* 

Pins:
A0  -  pot1 wiper 1
A1  -  pot1 wiper 2
A2  -  pot2 wiper 1
A3  -  pot2 wiper 2

*/

#define ENABLE_MIDI  true  // Turns on or off all use of MIDIUSB

#define DEBUG  0  // Set to 1 to enable Serial.print messages

#define MIDI_CHANNEL  0  // "Channel 1" 

#if ENABLE_MIDI
  #include "MIDIUSB.h"
#endif

#include <ResponsiveAnalogRead.h>


///// Definitions //////

const int NPots = 2;                    // Total number of potentiometers
const int wiper1Pin[NPots] = {A0, A2};  // Wiper 1 pins, separated by commas
const int wiper2Pin[NPots] = {A1, A3};  // Wiper 2 pins, separated by commas

byte potCC = 0;                         // MIDI CC for first pot. Subsequent pots will increase from this point (pot1 = CC 0 , pot2 = CC 1, etc.)

int wiper1Reading[NPots] = {0};
int wiper2Reading[NPots] = {0};

float snapMultiplier = 0.01;
ResponsiveAnalogRead responsiveWiper1[NPots] = {};
ResponsiveAnalogRead responsiveWiper2[NPots] = {};

double prevAngle[NPots] = {};
double angle[NPots] = {};
double angleChange[NPots] = {};
int midiAngleChange[NPots] = {};
int wiper1[NPots] = {};
int wiper2[NPots] = {};
double fx[NPots] = {};
double fy[NPots] = {};
int prevDialPosition[NPots] = {};
int dialPosition[NPots] = {};
int internalDialValue[NPots] = {};
int prevInternalDialValue[NPots] = {};

uint8_t incomingControl = 0;
uint8_t incomingControlValue = 0;


void setup() {
  
// Initialize serial communication
#if DEBUG
  Serial.begin(115200); 
#endif

  // Initialize responsive analog reads
  for (int i = 0; i < NPots; i++) {
    responsiveWiper1[i] = ResponsiveAnalogRead(0, true, snapMultiplier);
    responsiveWiper1[i].setAnalogResolution(1023); 
    responsiveWiper2[i] = ResponsiveAnalogRead(0, true, snapMultiplier);
    responsiveWiper2[i].setAnalogResolution(1023); 

    // Mark as uninitialized
    prevDialPosition[i] = -1; // A value outside the 0-127 range to indicate uninitialized
    prevInternalDialValue[i] = -1; // Same as above for internal MIDI value
    internalDialValue[i] = -1;
  }
}



void loop() {
  potentiometers();
  delay(10); // Adjust this to change potentiometer sensitivity. 10ms seems to work well.
}



int computeDelta(int prevPos, int currentPos) {
  int delta = currentPos - prevPos;
  if (delta > 64) {
    delta -= 128;
  } else if (delta < -64) {
    delta += 128;
  }
  return delta;
}



void potentiometers() {
  // Process all incoming MIDI messages
  midiEventPacket_t event;
  while ((event = MidiUSB.read()).header != 0) {
    if ((event.byte1 & 0x0F) == MIDI_CHANNEL) {
      incomingControl = event.byte2;
      incomingControlValue = event.byte3;

      for (int i = 0; i < NPots; i++) {
        if (incomingControl == potCC + i) {
          internalDialValue[i] = incomingControlValue;
          prevInternalDialValue[i] = incomingControlValue; // Sync previous value
#if DEBUG
          Serial.print("Pot ");
          Serial.print(i);
          Serial.print(" Initialized via MIDI: ");
          Serial.println(internalDialValue[i]);
#endif
        }
      }
    }
  }

  for (int i = 0; i < NPots; i++) {
    // Smooth analogRead values
    wiper1Reading[i] = analogRead(wiper1Pin[i]);
    wiper2Reading[i] = analogRead(wiper2Pin[i]);
    responsiveWiper1[i].update(wiper1Reading[i]); 
    responsiveWiper2[i].update(wiper2Reading[i]);
    wiper1[i] = responsiveWiper1[i].getValue();
    wiper2[i] = responsiveWiper2[i].getValue();  

    // Compute current angle and dial position
    fx[i] = ((double)wiper1[i] / 511.5) - 1;   // range -1 to +1
    fy[i] = ((double)wiper2[i] / 511.5) - 1;  
    angle[i] = atan2(fy[i], fx[i]);            // range -pi to +pi
    dialPosition[i] = map(angle[i], -PI, PI, 0, 128);

    // Skip uninitialized knobs
    if (prevDialPosition[i] == -1) {
      prevDialPosition[i] = dialPosition[i];
      continue; // Wait for the first movement
    }

    int deltaDialPosition = computeDelta(prevDialPosition[i], dialPosition[i]);
    if (deltaDialPosition != 0) { // Only update if there is a change
      if (internalDialValue[i] == -1) {
        // Initialize internal value on first movement
        internalDialValue[i] = dialPosition[i];
        prevInternalDialValue[i] = internalDialValue[i];
      } else {
        // Update internal value based on delta
        internalDialValue[i] += deltaDialPosition;

        // Wrap around if necessary
        if (internalDialValue[i] < 0) internalDialValue[i] += 128;
        else if (internalDialValue[i] > 127) internalDialValue[i] -= 128;
      }

      // Send MIDI if the value has changed
      if (internalDialValue[i] != prevInternalDialValue[i]) {
        controlChange(MIDI_CHANNEL, potCC + i, internalDialValue[i]);
        prevInternalDialValue[i] = internalDialValue[i];

#if DEBUG
        Serial.print("Pot ");
        Serial.print(i);
        Serial.print(" Internal Value: ");
        Serial.println(internalDialValue[i]);
#endif
      }
    }

    prevDialPosition[i] = dialPosition[i];
  }
}



/////// MIDI Functions //////

void controlChange(uint8_t channel, uint8_t control, uint8_t value) {
  #if ENABLE_MIDI
    midiEventPacket_t event = {0x0B, (uint8_t)(0xB0 | channel), control, value};
    MidiUSB.sendMIDI(event);
    MidiUSB.flush();
  #endif
}

// noteOn and noteOff are not necessary for this code, but I'll leave them in for anyone who might need them: 

void noteOn(uint8_t channel, uint8_t pitch, uint8_t velocity) {
  #if ENABLE_MIDI
    midiEventPacket_t noteOn = {0x09, (uint8_t)(0x90 | channel), pitch, velocity};
    MidiUSB.sendMIDI(noteOn);
    MidiUSB.flush();
  #endif
}

void noteOff(uint8_t channel, uint8_t pitch) {
  #if ENABLE_MIDI
    midiEventPacket_t noteOff = {0x08, (uint8_t)(0x80 | channel), pitch, 0};
    MidiUSB.sendMIDI(noteOff);
    MidiUSB.flush();
  #endif
}

