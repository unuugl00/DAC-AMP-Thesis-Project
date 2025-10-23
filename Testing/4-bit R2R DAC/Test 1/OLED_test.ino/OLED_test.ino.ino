#include <U8g2lib.h>
#include <Wire.h>

// --- OLED Display Setup (From your working code) ---
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// --- 4-Bit R-2R DAC & LED Pin Definitions ---
#define DAC_BIT_3 19 // MSB (1st LED)
#define DAC_BIT_2 18 // 2nd LED
#define DAC_BIT_1 5  // 3rd LED (Your "dim" one)
#define DAC_BIT_0 17 // LSB (Your "off" one)

// --- ADC Pin Definitions ---
#define ADC_WAVE_PIN 34   // Channel 1: Reads the DAC's analog output
#define ADC_POT_PIN  35   // Channel 2: Reads the Potentiometer

// --- Oscilloscope Buffer ---
// This buffer will now store the RAW 0-4095 values from the ADC
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

  // Explicitly set ADC settings (to address "voltage issue")
  analogSetWidth(12); // Set to 12-bit resolution (0-4095)
  analogSetAttenuation(ADC_11db); // Set for 0-3.3V range (this is the default)

  // Initialize OLED display
  Wire.begin(21, 22); 
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr); // Use the font you know works

  // --- NEW: LED DIAGNOSTIC TEST ---
  Serial.println("Starting LED Diagnostic Test...");
  for (int i = 0; i < 4; i++) {
    // Print to OLED
    u8g2.clearBuffer();
    u8g2.drawStr(10, 30, "LED TEST:");
    u8g2.drawStr(10, 45, bitNames[i]);
    u8g2.sendBuffer();
    
    // Print to Serial Monitor
    Serial.print("Testing LED "); Serial.println(bitNames[i]);

    // Turn on ONLY this one LED
    digitalWrite(dacPins[i], HIGH);
    delay(1500); // Hold for 1.5 seconds so you can see it
    digitalWrite(dacPins[i], LOW);
    delay(200);
  }
  
  // Test all on
  u8g2.clearBuffer();
  u8g2.drawStr(10, 30, "TEST: ALL ON");
  u8g2.sendBuffer();
  Serial.println("Testing ALL LEDs ON");
  digitalWrite(DAC_BIT_3, HIGH);
  digitalWrite(DAC_BIT_2, HIGH);
  digitalWrite(DAC_BIT_1, HIGH);
  digitalWrite(DAC_BIT_0, HIGH);
  delay(1500);
  digitalWrite(DAC_BIT_3, LOW);
  digitalWrite(DAC_BIT_2, LOW);
  digitalWrite(DAC_BIT_1, LOW);
  digitalWrite(DAC_BIT_0, LOW);
  
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
  // Map pot to a delay (frequency control)
  int waveDelay = map(potValue, 0, 4095, 0, 20);

  // 2. GENERATE DAC SIGNAL & LIGHT LEDs
  digitalWrite(DAC_BIT_3, (dacValue >> 3) & 1);
  digitalWrite(DAC_BIT_2, (dacValue >> 2) & 1);
  digitalWrite(DAC_BIT_1, (dacValue >> 1) & 1);
  digitalWrite(DAC_BIT_0, (dacValue >> 0) & 1);
  dacValue = (dacValue + 1) % 16;

  // 3. READ FOR OSCILLOSCOPE (Channel 1)
  int adcWaveValue = analogRead(ADC_WAVE_PIN);
  
  // --- REFINED ---
  // Store the RAW 0-4095 value. We will scale it later.
  readings[currentReadingIndex] = adcWaveValue;
  currentReadingIndex = (currentReadingIndex + 1) % SCREEN_WIDTH;

  // 4. DRAW TO OLED
  drawWaveform(); // We don't need to pass the pot value anymore

  // 5. APPLY FREQUENCY DELAY
  delay(1 + waveDelay);
}

/**
 * @brief Clears the display and draws both waveforms using U8g2
 * NEW: This function now AUTOSCALES the waveform to fill the screen.
 */
void drawWaveform() {
  u8g2.clearBuffer();

  // --- 1. AUTOSCALING LOGIC ---
  int dataMin = 4095; // Start high
  int dataMax = 0;    // Start low

  // Find the min and max ADC values currently in our buffer
  for (int i = 0; i < SCREEN_WIDTH; i++) {
    if (readings[i] < dataMin) dataMin = readings[i];
    if (readings[i] > dataMax) dataMax = readings[i];
  }

  // Safety check to prevent dividing by zero if the wave is flat
  if (dataMin == dataMax) {
    dataMax = dataMin + 1;
  }

  // --- 2. DRAW CHANNEL 1 (The DAC Wave) ---
  for (int x = 0; x < SCREEN_WIDTH - 1; x++) {
    int readIdx = (currentReadingIndex + x) % SCREEN_WIDTH;
    int nextReadIdx = (currentReadingIndex + x + 1) % SCREEN_WIDTH;

    // Map the RAW value to the 0-63 screen height using our new min/max
    int y1 = 63 - map(readings[readIdx], dataMin, dataMax, 0, 63);
    int y2 = 63 - map(readings[nextReadIdx], dataMin, dataMax, 0, 63);

    u8g2.drawLine(x, y1, x + 1, y2);
  }

  // --- 3. DRAW CHANNEL 2 (The Pot Level) ---
  // We read the pot here directly
  int potRaw = analogRead(ADC_POT_PIN);
  int potY = 63 - map(potRaw, 0, 4095, 0, 63); // Map full range
  
  for (int x = 0; x < SCREEN_WIDTH; x += 4) {
    u8g2.drawHLine(x, potY, 2);
  }

  // Send the completed buffer to the display
  u8g2.sendBuffer();
}