/*
  esp32_autoscan_seed.ino
  ------------------------
  Auto-scan 32x32 crossbar matrix using PCA9685 drivers for rows.
  Computes measured vs ideal values and outputs a SHA256-based seed for later
  expansion via a Python parser.

  Hardware assumptions:
   - 2x PCA9685 (I2C addr 0x40 and 0x41) drive 32 row signals.
   - Each column output feeds op-amp TIAs into MAC_ACC_ADC_PIN (GPIO2).
   - TS5A3159A control pins: SELECT=11, A_MCU=12 (set HIGH to connect).
   - No separate column-select hardware: all reads use same ADC.

  Author: ChatGPT (for Aneesh)
*/

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <Arduino.h>
#include "SHA256.h"   // from ArduinoCrypto library

// ================= USER CONFIG ==================
#define SDA_PIN 4
#define SCL_PIN 5

Adafruit_PWMServoDriver pca1(0x40);
Adafruit_PWMServoDriver pca2(0x41);

#define TS5_SELECT_PIN 11
#define TS5_AMCU_PIN   12
#define MAC_ACC_ADC_PIN 2

const int ROW_COUNT = 32;
const int COL_COUNT = 32;   // dummy placeholder, analog front-end handles columns

// analog/system constants
const float Vref = 3.3f;    // ADC reference
const float Vmid = 1.65f;   // virtual ground
const float Rf = 22000.0f;  // feedback resistor (ohm)
const float V_unit = 0.02f; // 20 mV test amplitude
const int   ADC_AVG = 32;
const unsigned long SETTLE_MS = 30;  // settle delay per test
// =================================================

SHA256 sha;  // from ArduinoCrypto

float G_meas[ROW_COUNT][COL_COUNT];
float last_Vout[COL_COUNT];
float last_Icol_uA[COL_COUNT];

// ---- helpers ----
void zeroAllRows();
void setRowPWM(int rowIndex1toN, int duty);
uint16_t adcReadAvg(int pin, int samples);
float readVoutV();
float Vout_to_IuA(float Vout);
int voltToDuty(float V);
void setupPins();

// -------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ESP32 Auto-scan & Seed Generator ===");
  Wire.begin(SDA_PIN, SCL_PIN);

  pca1.begin();
  pca2.begin();
  pca1.setPWMFreq(1600);
  pca2.setPWMFreq(1600);

  setupPins();
  delay(200);

  Serial.printf("Rows=%d Cols=%d V_unit=%.4f Rf=%.1f\n", ROW_COUNT, COL_COUNT, V_unit, Rf);

  zeroAllRows();
  delay(50);

  // --------------- UNIT VECTOR SWEEP ---------------
  Serial.println("Starting unit-vector sweep to build G_meas...");

  unsigned long start_t = millis();

  for (int r = 1; r <= ROW_COUNT; ++r) {
    zeroAllRows();
    int duty = voltToDuty(V_unit);
    setRowPWM(r, duty);
    delay(SETTLE_MS);

    for (int c = 0; c < COL_COUNT; ++c) {
      float Vout = readVoutV();
      float IuA = Vout_to_IuA(Vout);
      float G = (IuA * 1e-6f) / V_unit;  // Siemens
      G_meas[r - 1][c] = G;
      last_Vout[c] = Vout;
      last_Icol_uA[c] = IuA;

      // fold data into digest
      int16_t packr = r, packc = c;
      sha.update((uint8_t*)&packr, sizeof(packr));
      sha.update((uint8_t*)&packc, sizeof(packc));
      sha.update((uint8_t*)&IuA, sizeof(IuA));

      Serial.printf("UNIT,row=%02d,col=%02d,Vout=%.4fV,IuA=%.3f\n", r, c, Vout, IuA);
    }
  }

  unsigned long unit_time = millis() - start_t;
  Serial.printf("Unit sweep complete in %lu ms\n", unit_time);

  // --------------- FUNCTIONAL TEST VECTORS ---------------
  Serial.println("Running functional test vectors...");

  float vec_all[ROW_COUNT];
  float vec_sparse[ROW_COUNT];
  for (int i = 0; i < ROW_COUNT; i++) {
    vec_all[i] = 0.001f;   // 1 mV all active
    vec_sparse[i] = 0.0f;
  }
  vec_sparse[0] = 0.02f; vec_sparse[5] = 0.015f; vec_sparse[13] = 0.02f;

  auto apply_and_read = [&](float *Vrows, const char *name) {
    zeroAllRows();
    for (int r = 1; r <= ROW_COUNT; r++) {
      int duty = voltToDuty(Vrows[r - 1]);
      setRowPWM(r, duty);
    }
    delay(SETTLE_MS);

    Serial.printf("\nVector %s applied. Reading ADC for all columns...\n", name);
    for (int c = 0; c < COL_COUNT; c++) {
      float Vout = readVoutV();
      float IuA = Vout_to_IuA(Vout);
      float Ipred = 0.0f;
      for (int i = 0; i < ROW_COUNT; i++) {
        Ipred += G_meas[i][c] * Vrows[i];
      }
      float Vpred = Vmid - Rf * Ipred;
      float resid = Vout - Vpred;

      sha.update((uint8_t*)&resid, sizeof(resid));
      Serial.printf("COL%02d,Vout=%.4fV,IuA=%.3f,Vpred=%.4f,resid=%.4f\n",
                    c, Vout, IuA, Vpred, resid);
    }
  };

  apply_and_read(vec_all, "ALL-1mV");
  apply_and_read(vec_sparse, "SPARSE");

  // --------------- SHA FINALIZATION ---------------
  uint32_t hw = esp_random();
  sha.update((uint8_t*)&hw, sizeof(hw));

  uint8_t digest[32];
  sha.finalize(digest, 32);  // ✅ Correct for ArduinoCrypto

  const char hexchars[] = "0123456789abcdef";
  char hexout[65];
  for (int i = 0; i < 32; i++) {
    hexout[i * 2] = hexchars[(digest[i] >> 4) & 0xF];
    hexout[i * 2 + 1] = hexchars[digest[i] & 0xF];
  }
  hexout[64] = '\0';

  Serial.println("\n=== FINAL SEED (copy for Python parser) ===");
  Serial.println(hexout);
  Serial.println("=== END SEED ===");

  Serial.println("All tasks complete.");
}

void loop() {
  // nothing after run
}

// -------------------------------------------------
// Helper implementations
// -------------------------------------------------

void setupPins() {
  pinMode(TS5_SELECT_PIN, OUTPUT);
  pinMode(TS5_AMCU_PIN, OUTPUT);
  digitalWrite(TS5_SELECT_PIN, LOW);
  digitalWrite(TS5_AMCU_PIN, HIGH);  // enable path
}

void zeroAllRows() {
  for (int ch = 0; ch < 16; ch++) {
    pca1.setPWM(ch, 0, 0);
    pca2.setPWM(ch, 0, 0);
  }
}

void setRowPWM(int rowIndex1toN, int duty) {
  if (rowIndex1toN < 1 || rowIndex1toN > ROW_COUNT) return;
  duty = constrain(duty, 0, 4095);
  int idx = rowIndex1toN - 1;
  if (idx < 16) pca1.setPWM(idx, 0, duty);
  else pca2.setPWM(idx - 16, 0, duty);
}

int voltToDuty(float V) {
  int d = (int)round((V / Vref) * 4095.0f);
  d = constrain(d, 0, 4095);
  return d;
}

uint16_t adcReadAvg(int pin, int samples) {
  uint32_t sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delayMicroseconds(200);
  }
  return (uint16_t)(sum / samples);
}

float readVoutV() {
  uint16_t raw = adcReadAvg(MAC_ACC_ADC_PIN, ADC_AVG);
  float V = ((float)raw / 4095.0f) * Vref;
  return V;
}

float Vout_to_IuA(float Vout) {
  float dV = Vout - Vmid;
  float I = -dV / Rf;  // A
  return I * 1e6f;     // µA
}
