#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

#define SDA_PIN 4
#define SCL_PIN 5
#define TS5_SELECT 11   // selects which mux path
#define TS5_AMCU   12   // analog connect control
#define MAC_ACC_ADC 2   // analog output from op-amp chain (GPIO2)

Adafruit_PWMServoDriver pca1 = Adafruit_PWMServoDriver(0x40);
Adafruit_PWMServoDriver pca2 = Adafruit_PWMServoDriver(0x41);

const float Vref = 3.3;
const float Vmid = 1.65;   // virtual ground reference
const float Rf   = 22000.0;
const float Rcell = 1000.0;

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  pca1.begin();  pca2.begin();
  pca1.setPWMFreq(1600);
  pca2.setPWMFreq(1600);

  pinMode(TS5_SELECT, OUTPUT);
  pinMode(TS5_AMCU, OUTPUT);
  digitalWrite(TS5_SELECT, LOW);
  digitalWrite(TS5_AMCU, LOW);

  delay(200);
  Serial.println("=== Crossbar Analog MVM Test Ready ===");

  // Example: activate one row at mid duty
  setRow(1, 2048);
  delay(100);
  readMACOutput();
}

void loop() {
  // Example automated sweep for 4 rows
  for (int r = 1; r <= 4; r++) {
    int pwmVal = 512 * r;  // stepped DC levels 0.41–2.05 V
    setRow(r, pwmVal);
    delay(50);
    readMACOutput();
    delay(500);
  }
}

// -------------------- utilities --------------------

void setRow(int rowIndex, int duty) {
  // rowIndex 1–32
  int idx = rowIndex - 1;  // internal zero-based

  duty = constrain(duty, 0, 4095);
  for (int ch = 0; ch < 16; ch++) { pca1.setPWM(ch,0,0); pca2.setPWM(ch,0,0); }

  if (idx < 16) pca1.setPWM(idx, 0, duty);
  else          pca2.setPWM(idx - 16, 0, duty);

  float Vrow = (float)duty / 4095.0 * Vref;
  Serial.printf("Row %d set to duty=%d (%.3f V)\n", rowIndex, duty, Vrow);
}

void readMACOutput() {
  // Configure TS5A3159 to connect MAC_ACC_ADC line
  digitalWrite(TS5_AMCU, HIGH);   // close switch
  digitalWrite(TS5_SELECT, LOW);  // select path 0 (adjust later if needed)
  delay(10);

  uint16_t raw = analogRead(MAC_ACC_ADC);
  float Vout = raw / 4095.0 * Vref;
  float Vout_mV = (Vout - Vmid) * 1000.0;  // relative to virtual ground
  float Icol_mA = Vout_mV / Rf;            // approximate current

  Serial.printf("ADC raw=%d  Vout=%.3f V (Δ%.2f mV wrt Vmid)  I≈%.3f mA\n",
                raw, Vout, Vout_mV, Icol_mA);
}
