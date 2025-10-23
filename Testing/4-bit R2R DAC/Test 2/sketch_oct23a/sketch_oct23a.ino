#include <U8g2lib.h>
#include <Wire.h>

// --- OLED Display Setup ---
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// --- 4-Bit R-2R DAC & LED Pin Definitions ---
#define DAC_BIT_3 19 // MSB (1st LED)
#define DAC_BIT_2 18 // 2nd LED
#define DAC_BIT_1 5  // 3rd LED
#define DAC_BIT_0 17 // LSB (4th LED)

// --- ADC Pin Definitions ---
#define ADC_WAVE_PIN 34   // Channel 1: Reads the DAC's analog output
#define ADC_POT_PIN  35   // Channel 2: Reads the Potentiometer

// --- Oscilloscope Buffer ---
int readings[SCREEN_WIDTH]; // Still needed for drawing the wave
int currentReadingIndex = 0;

// --- DAC Wave Generator ---
byte dacValue = 0; // A 4-bit value (0-15)

// --- Helper array for the LED test ---
const int dacPins[] = {DAC_BIT_3, DAC_BIT_2, DAC_BIT_1, DAC_BIT_0};
const char* bitNames[] = {"BIT 3 (MSB)", "BIT 2", "BIT 1", "BIT 0 (LSB)"};

// --- Variables for Real-Time Display ---
float currentVoltage = 0.0;
unsigned long loopStartTime = 0;
unsigned long loopEndTime = 0;
float loopTimeMillis = 0.0;
// Averaging filter for time display
const int numTimeSamples = 10;
float timeSamples[numTimeSamples];
int timeSampleIndex = 0;
float avgLoopTimeMillis = 0.0;

void setup() {
  Serial.begin(115200);

  // Initialize DAC & LED pins as outputs
  pinMode(DAC_BIT_3, OUTPUT);
  pinMode(DAC_BIT_2, OUTPUT);
  pinMode(DAC_BIT_1, OUTPUT);
  pinMode(DAC_BIT_0, OUTPUT);

  // Set ADC settings
  analogSetWidth(12);
  analogSetAttenuation(ADC_11db);

  // Initialize OLED display
  Wire.begin(21, 22);
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr); // A small readable font

  // --- LED DIAGNOSTIC TEST (Unchanged) ---
  Serial.println("Starting LED Diagnostic Test...");
  for (int i = 0; i < 4; i++) {
    u8g2.clearBuffer();
    u8g2.drawStr(10, 30, "LED TEST:");
    u8g2.drawStr(10, 45, bitNames[i]);
    u8g2.sendBuffer();
    Serial.print("Testing LED "); Serial.println(bitNames[i]);
    digitalWrite(dacPins[i], HIGH);
    delay(1000); // Shorter delay for faster startup
    digitalWrite(dacPins[i], LOW);
    delay(100);
  }
  u8g2.clearBuffer();
  u8g2.drawStr(10, 30, "Test complete!");
  u8g2.drawStr(10, 45, "Starting scope...");
  u8g2.sendBuffer();
  Serial.println("LED test complete. Starting main loop.");
  delay(500); // Shorter delay
}

void loop() {
  // --- Record start time ---
  loopStartTime = micros(); // Get time in microseconds

  // 1. READ CONTROLS (Potentiometer)
  int potValue = analogRead(ADC_POT_PIN);
  int waveDelay = map(potValue, 0, 4095, 0, 200); // Max 200ms delay

  // 2. GENERATE DAC SIGNAL & LIGHT LEDs
  digitalWrite(DAC_BIT_3, (dacValue >> 3) & 1);
  digitalWrite(DAC_BIT_2, (dacValue >> 2) & 1);
  digitalWrite(DAC_BIT_1, (dacValue >> 1) & 1);
  digitalWrite(DAC_BIT_0, (dacValue >> 0) & 1);
  dacValue = (dacValue + 1) % 16; // Counts 0-15

  // 3. READ FOR OSCILLOSCOPE (Channel 1)
  int adcWaveValue = analogRead(ADC_WAVE_PIN);

  // --- Calculate INSTANTANEOUS Voltage ---
  currentVoltage = (float)adcWaveValue / 4095.0 * 3.3; // Convert ADC value to voltage

  readings[currentReadingIndex] = adcWaveValue; // Store raw value in buffer
  currentReadingIndex = (currentReadingIndex + 1) % SCREEN_WIDTH;

  // 4. DRAW TO OLED (This function now also draws the text)
  drawWaveform();

  // 5. APPLY FREQUENCY DELAY
  delay(1 + waveDelay); // Add 1ms base delay

  // --- Record end time and calculate duration ---
  loopEndTime = micros();
  loopTimeMillis = (float)(loopEndTime - loopStartTime) / 1000.0; // Convert to milliseconds

  // --- Update rolling average for display stability ---
  timeSamples[timeSampleIndex] = loopTimeMillis;
  timeSampleIndex = (timeSampleIndex + 1) % numTimeSamples;
  avgLoopTimeMillis = 0;
  for(int i=0; i<numTimeSamples; i++) {
    avgLoopTimeMillis += timeSamples[i];
  }
  avgLoopTimeMillis /= numTimeSamples;
}

/**
 * @brief Clears the display, draws waveforms, and adds text readings
 */
void drawWaveform() {
  u8g2.clearBuffer();

  // --- 1. AUTOSCALING LOGIC (Needed for drawing wave correctly) ---
  int dataMin = 4095;
  int dataMax = 0;
  for (int i = 0; i < SCREEN_WIDTH; i++) {
    if (readings[i] < dataMin) dataMin = readings[i];
    if (readings[i] > dataMax) dataMax = readings[i];
  }
  if (dataMin == dataMax) dataMax = dataMin + 1;

  // --- 2. DRAW CHANNEL 1 (The DAC Wave) ---
  const int yOffset = 10; // Space for text
  const int graphHeight = SCREEN_HEIGHT - yOffset;

  for (int x = 0; x < SCREEN_WIDTH - 1; x++) {
    int readIdx = (currentReadingIndex + x) % SCREEN_WIDTH;
    int nextReadIdx = (currentReadingIndex + x + 1) % SCREEN_WIDTH;

    int y1 = yOffset + (graphHeight - 1) - map(readings[readIdx], dataMin, dataMax, 0, graphHeight - 1);
    int y2 = yOffset + (graphHeight - 1) - map(readings[nextReadIdx], dataMin, dataMax, 0, graphHeight - 1);

    u8g2.drawLine(x, y1, x + 1, y2);
  }

  // --- 3. DRAW CHANNEL 2 (The Pot Level) ---
  int potRaw = analogRead(ADC_POT_PIN);
  int potY = yOffset + (graphHeight - 1) - map(potRaw, 0, 4095, 0, graphHeight - 1);

  for (int x = 0; x < SCREEN_WIDTH; x += 4) {
    u8g2.drawHLine(x, potY, 2);
  }

  // --- 4. Draw Real-Time Voltage and Time Text ---
  char buffer[20];
  u8g2.setFont(u8g2_font_t0_11_tf); // Small font

  // Display Real-Time Voltage
  sprintf(buffer, "V: %.2fV", currentVoltage);
  u8g2.setCursor(2, 8); // Top-left
  u8g2.print(buffer);

  // Display Loop Time
  sprintf(buffer, "T: %.1fms", avgLoopTimeMillis);
  // Adjusted position to top-right
  u8g2.setCursor(SCREEN_WIDTH - 65, 8);
  u8g2.print(buffer);


  // Send the completed buffer to the display
  u8g2.sendBuffer();
}