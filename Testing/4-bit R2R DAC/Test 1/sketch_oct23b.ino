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
int readings[SCREEN_WIDTH];
int currentReadingIndex = 0; 

// --- DAC Wave Generator ---
byte dacValue = 0; // A 4-bit value (0-15)

// --- Helper array for the LED test ---
const int dacPins[] = {DAC_BIT_3, DAC_BIT_2, DAC_BIT_1, DAC_BIT_0};
const char* bitNames[] = {"BIT 3 (MSB)", "BIT 2", "BIT 1", "BIT 0 (LSB)"};

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
  u8g2.setFont(u8g2_font_ncenB08_tr); 

  // --- LED DIAGNOSTIC TEST ---
  Serial.println("Starting LED Diagnostic Test...");
  for (int i = 0; i < 4; i++) {
    u8g2.clearBuffer();
    u8g2.drawStr(10, 30, "LED TEST:");
    u8g2.drawStr(10, 45, bitNames[i]);
    u8g2.sendBuffer();
    
    Serial.print("Testing LED "); Serial.println(bitNames[i]);
    digitalWrite(dacPins[i], HIGH);
    delay(1000); // Hold for 1 second
    digitalWrite(dacPins[i], LOW);
    delay(200);
  }
  
  // --- END OF TEST ---
  u8g2.clearBuffer();
  u8g2.drawStr(10, 30, "Test complete!");
  u8g2.drawStr(10, 45, "Starting scope...");
  u8g2.sendBuffer();
  Serial.println("LED test complete. Starting main loop.");
  delay(1000);
}

void loop() {
  // 1. READ CONTROLS (Potentiometer)
  int potValue = analogRead(ADC_POT_PIN);
  
  // --- UPDATED LINE ---
  // Map pot to a delay of 0-200ms for a much slower wave
  int waveDelay = map(potValue, 0, 4095, 0, 200);

  // 2. GENERATE DAC SIGNAL & LIGHT LEDs
  digitalWrite(DAC_BIT_3, (dacValue >> 3) & 1);
  digitalWrite(DAC_BIT_2, (dacValue >> 2) & 1);
  digitalWrite(DAC_BIT_1, (dacValue >> 1) & 1);
  digitalWrite(DAC_BIT_0, (dacValue >> 0) & 1);
  dacValue = (dacValue + 1) % 16; // Counts 0-15

  // 3. READ FOR OSCILLOSCOPE (Channel 1)
  int adcWaveValue = analogRead(ADC_WAVE_PIN);
  readings[currentReadingIndex] = adcWaveValue;
  currentReadingIndex = (currentReadingIndex + 1) % SCREEN_WIDTH;

  // 4. DRAW TO OLED
  drawWaveform(); 

  // 5. APPLY FREQUENCY DELAY
  delay(1 + waveDelay);
}

/**
 * @brief Clears the display and draws both waveforms using U8g2
 * This function autoscales the waveform to fill the screen.
 */
void drawWaveform() {
  u8g2.clearBuffer();

  // --- 1. AUTOSCALING LOGIC ---
  int dataMin = 4095;
  int dataMax = 0;   
  for (int i = 0; i < SCREEN_WIDTH; i++) {
    if (readings[i] < dataMin) dataMin = readings[i];
    if (readings[i] > dataMax) dataMax = readings[i];
  }
  if (dataMin == dataMax) dataMax = dataMin + 1;

  // --- 2. DRAW CHANNEL 1 (The DAC Wave) ---
  for (int x = 0; x < SCREEN_WIDTH - 1; x++) {
    int readIdx = (currentReadingIndex + x) % SCREEN_WIDTH;
    int nextReadIdx = (currentReadingIndex + x + 1) % SCREEN_WIDTH;

    int y1 = 63 - map(readings[readIdx], dataMin, dataMax, 0, 63);
    int y2 = 63 - map(readings[nextReadIdx], dataMin, dataMax, 0, 63);

    u8g2.drawLine(x, y1, x + 1, y2);
  }

  // --- 3. DRAW CHANNEL 2 (The Pot Level) ---
  int potRaw = analogRead(ADC_POT_PIN);
  int potY = 63 - map(potRaw, 0, 4095, 0, 63); 
  
  for (int x = 0; x < SCREEN_WIDTH; x += 4) {
    u8g2.drawHLine(x, potY, 2);
  }

  // Send the completed buffer to the display
  u8g2.sendBuffer();
}