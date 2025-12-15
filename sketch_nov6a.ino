#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// Two PCA9685 boards at addresses 0x40 and 0x41
Adafruit_PWMServoDriver pca1 = Adafruit_PWMServoDriver(0x40);
Adafruit_PWMServoDriver pca2 = Adafruit_PWMServoDriver(0x41);

void setup() {
  Serial.begin(115200);
  Serial.println("Setting all PCA9685 channels to 0...");

  Wire.begin();
  pca1.begin();
  pca2.begin();

  // Set PWM frequency (use same as rest of project)
  pca1.setPWMFreq(1526);
  pca2.setPWMFreq(1526);

  delay(10);  // let chips stabilize

  // --- Turn everything OFF ---
  for (int i = 0; i < 16; i++) {
    pca1.setPWM(i, 0, 0);
    pca2.setPWM(i, 0, 0);
  }

  Serial.println("✅ All 32 channels set to 0 duty (0 V).");
}

void loop() {
  // Nothing else — keep it at 0.
}
