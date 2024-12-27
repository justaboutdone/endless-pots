// standard Arduino libraries
#include <SPI.h>
// Arduino libraries to be installed beforehand:
#include <Mcp320x.h>
#include <ResponsiveAnalogRead.h>

// Constants and Macros
#define ADC1_CS D0       // SPI slave select
#define ADC2_CS D1       // SPI slave select
#define ADC_VREF 3300    // 3.3V Vref
#define ADC_CLK 1600000  // SPI clock 1.6MHz
const int NPots = 8;     // Total number of pots
float snapMultiplier = 0.01;

// MCP3208 Instances
MCP3208 adc1(ADC_VREF, ADC1_CS);
MCP3208 adc2(ADC_VREF, ADC2_CS);

// Responsive Analog Read Instances
ResponsiveAnalogRead responsiveWiper1[NPots] = {};
ResponsiveAnalogRead responsiveWiper2[NPots] = {};

// Variables for Dial Positions and Angles
double prevAngle[NPots] = {};
int prevDialPosition[NPots] = {};
int prevInternalDialValue[NPots] = {};

// Previous Values
int previous_wiper1[NPots] = { 0 };
int previous_wiper2[NPots] = { 0 };
float previous_angle[NPots] = { 0.0 };
float angle_change[NPots] = { 0.0 };

// Button States and Fader Value
bool buttonStates[16] = { false };
int faderValue = 0;

void setup() {
  configurePins();
  initializeSPI();
  initializeResponsiveAnalogReads();
}

void loop() {
  readPotentiometers();
  delay(10);  // Adjust this to change potentiometer sensitivity
}

void configurePins() {
  pinMode(ADC1_CS, OUTPUT);
  pinMode(ADC2_CS, OUTPUT);
  digitalWrite(ADC1_CS, LOW);
  digitalWrite(ADC2_CS, LOW);
}

void initializeSPI() {
  Serial.begin(115200);
  SPISettings settings(ADC_CLK, MSBFIRST, SPI_MODE0);
  SPI.begin();
  SPI.beginTransaction(settings);
}

void initializeResponsiveAnalogReads() {
  for (int i = 0; i < NPots; i++) {
    responsiveWiper1[i] = ResponsiveAnalogRead(0, true, snapMultiplier);
    responsiveWiper1[i].setAnalogResolution(4095);
    responsiveWiper2[i] = ResponsiveAnalogRead(0, true, snapMultiplier);
    responsiveWiper2[i].setAnalogResolution(4095);
    prevDialPosition[i] = -1;
    prevInternalDialValue[i] = -1;
    internalDialValue[i] = -1;
  }
}

void readPotentiometers() {
  bool changes = false;
  for (int i = 0; i < NPots; i++) {
    MCP3208::Channel channel1, channel2;
    getChannels(i, channel1, channel2);
    uint16_t wiper1Reading = (i < 4) ? adc1.read(channel1) : adc2.read(channel1);
    uint16_t wiper2Reading = (i < 4) ? adc1.read(channel2) : adc2.read(channel2);

    if (wiper1Reading != previous_wiper1[i] || wiper2Reading != previous_wiper2[i]) {
      updateReadings(i, wiper1Reading, wiper2Reading);
      changes = true;
    }
  }

  if (changes) {
    sendSerialData();
  }
}

void getChannels(int i, MCP3208::Channel &channel1, MCP3208::Channel &channel2) {
  if (i < 4) {
    channel1 = static_cast<MCP3208::Channel>(MCP3208::Channel::SINGLE_0 + i * 2);
    channel2 = static_cast<MCP3208::Channel>(MCP3208::Channel::SINGLE_0 + i * 2 + 1);
  } else {
    channel1 = static_cast<MCP3208::Channel>(MCP3208::Channel::SINGLE_0 + (i - 4) * 2);
    channel2 = static_cast<MCP3208::Channel>(MCP3208::Channel::SINGLE_0 + (i - 4) * 2 + 1);
  }
}

void updateReadings(int i, uint16_t wiper1Reading, uint16_t wiper2Reading) {
  previous_wiper1[i] = wiper1Reading;
  previous_wiper2[i] = wiper2Reading;
  responsiveWiper1[i].update(float(wiper1Reading));
  responsiveWiper2[i].update(float(wiper2Reading));
  double wiper1 = responsiveWiper1[i].getValue() - 2048;
  double wiper2 = responsiveWiper2[i].getValue() - 2048;
  // Here the magic happens: calculate the angle of the potentiometer by using the arctangent function
  float thisangle = atan2(wiper2, wiper1) * 180 / PI;
  if (thisangle < 0) {
    thisangle += 360;
  }
  float delta_angle = thisangle - previous_angle[i];
  if (delta_angle > 180) {
    delta_angle -= 360;
  } else if (delta_angle < -180) {
    delta_angle += 360;
  }
  if (abs(delta_angle) > 0.5) {
    angle_change[i] = delta_angle;
    previous_angle[i] = thisangle;
  }
}

void sendSerialData() {
  uint8_t buffer[sizeof(angle_change) + sizeof(buttonStates) + sizeof(faderValue)];
  memcpy(buffer, angle_change, sizeof(angle_change));
  memcpy(buffer + sizeof(angle_change), buttonStates, sizeof(buttonStates));
  memcpy(buffer + sizeof(angle_change) + sizeof(buttonStates), &faderValue, sizeof(faderValue));
  Serial.write(buffer, sizeof(buffer));
}