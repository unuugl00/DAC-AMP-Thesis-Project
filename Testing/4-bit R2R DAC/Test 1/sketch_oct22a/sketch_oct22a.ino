#include <U8g2lib.h>
#include <Wire.h>

// --- OLED Display Setup (From your working code) ---
// We will use your exact, confirmed constructor
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// --- 4-Bit R-2R DAC & LED Pin Definitions ---
// (MSB) Most Significant Bit
#define DAC_BIT_3 19
// (LSB) Least Significant Bit
#define DAC_BIT_2 18
#define DAC_BIT_1 5
#define DAC_BIT_0 17

// --- Oscilloscope (ADC) Pin Definitions ---
#define ADC_WAVE_PIN 34   // Channel 1: Reads the DAC's analog output
#define ADC_POT_PIN  35   // Channel 2: Reads the Potentiometer

// --- Oscilloscope Buffer ---
int readings[SCREEN_WIDTH];
int currentReadingIndex = 0; // Current position in the buffer

// --- DAC Wave Generator ---
byte dacValue = 0; // A 4-bit value (0-15)

void setup() {
  Serial.begin(115200);

  // Initialize DAC & LED pins as outputs
  pinMode(DAC_BIT_3, OUTPUT);
  pinMode(DAC_BIT_2, OUTPUT);
  pinMode(DAC_BIT_1, OUTPUT);
  pinMode(DAC_BIT_0, OUTPUT);

  // Initialize ADC pins as input
  // (Not strictly necessary for ADC-only pins like 34 & 35)

  // Initialize OLED display (using your working setup)
  Wire.begin(21, 22); // Good practice to specify pins
  u8g2.begin();

  // Show a startup message using U8g2 commands
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr); // Use the font you tested
  u8g2.drawStr(10, 20, "ESP32 DAC Scope");
  u8g2.drawStr(18, 40, "All systems GO!");
  u8g2.sendBuffer();
  delay(1500);
}

void loop() {
  // 1. READ CONTROLS (Potentiometer)
  // This logic is unchanged
  int potValue = analogRead(ADC_POT_PIN);
  int waveDelay = map(potValue, 0, 4095, 0, 20);
  int potY = 63 - map(potValue, 0, 4095, 0, 63);

  // 2. GENERATE DAC SIGNAL & LIGHT LEDs
  // This logic is unchanged
  digitalWrite(DAC_BIT_3, (dacValue >> 3) & 1);
  digitalWrite(DAC_BIT_2, (dacValue >> 2) & 1);
  digitalWrite(DAC_BIT_1, (dacValue >> 1) & 1);
  digitalWrite(DAC_BIT_0, (dacValue >> 0) & 1);
  dacValue = (dacValue + 1) % 16;

  // 3. READ FOR OSCILLOSCOPE (Channel 1)
  // This logic is unchanged
  int adcWaveValue = analogRead(ADC_WAVE_PIN);
  int waveY = 63 - map(adcWaveValue, 0, 4095, 0, 63);
  readings[currentReadingIndex] = waveY;
  currentReadingIndex = (currentReadingIndex + 1) % SCREEN_WIDTH;

  // 4. DRAW TO OLED
  // This function call is unchanged
  drawWaveform(potY);

  // 5. APPLY FREQUENCY DELAY
  // This logic is unchanged
  delay(1 + waveDelay);
}

/**
 * @brief Clears the display and draws both waveforms using U8g2
 * @param potLevel The Y-coordinate (0-63) for the Potentiometer line.
 */
void drawWaveform(int potLevel) {
  // U8g2 uses a buffer. You clear it, draw everything, then send it.
  u8g2.clearBuffer();

  // --- Draw Channel 1 (The DAC Wave) ---
  for (int x = 0; x < SCREEN_WIDTH - 1; x++) {
    int readIdx = (currentReadingIndex + x) % SCREEN_WIDTH;
    int nextReadIdx = (currentReadingIndex + x + 1) % SCREEN_WIDTH;

    // u8g2.drawLine has the same syntax: (x1, y1, x2, y2)
    u8g2.drawLine(
      x,
      readings[readIdx],
      x + 1,
      readings[nextReadIdx]
    );
  }

  // --- Draw Channel 2 (The Pot Level) ---
  // Use u8g2.drawHLine: (x, y, width)
  for (int x = 0; x < SCREEN_WIDTH; x += 4) {
    u8g2.drawHLine(x, potLevel, 2);
  }

  // Send the completed buffer to the display
  u8g2.sendBuffer();
}