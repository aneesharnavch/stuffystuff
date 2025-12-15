// ===== PWM Writer for ESP32 =====
// Set the PWM channel, pin, and duty manually.
// 12-bit resolution (0–4095)

const int pwmPin = 25;     // <-- change to your row drive pin
const int pwmChannel = 1;
const int pwmFreq = 5000;  // 5 kHz is typical
const int pwmResolution = 12;

// === MANUAL DUTY ENTRY ===
int duty = 2048; // <--- Change this value [0, 512, 1024, 2048, 3072, 4095]

void setup() {
  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(pwmPin, pwmChannel);
  Serial.begin(115200);
  Serial.println("PWM Writer Ready");
  ledcWrite(pwmChannel, duty);
  Serial.print("Set Duty: ");
  Serial.println(duty);
}

void loop() {
  // Keeps outputting the chosen duty
  delay(1000);
}
