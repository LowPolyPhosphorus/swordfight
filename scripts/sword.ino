// sword_led_mpu.ino
// MPU6050 (Adafruit library) + FastLED, combined and non-blocking.
//
// Behavior:
//  - Idle/swing: LEDs cycle through a hue gradient. Cycle SPEED is driven
//    by rotation rate (gyro magnitude) — slow motion = slow color drift,
//    fast swing = fast color cycling.
//  - Clash/impact: sudden accel spike (checked against baseline AND
//    sample-to-sample jerk, to reject a merely-fast-but-smooth swing)
//    triggers a bright white flash that holds briefly then smooths back
//    down into the current gradient color — a "flashbang" for a hit.
//
// Libraries needed (Tools -> Manage Libraries):
//   FastLED
//   Adafruit MPU6050
//   Adafruit Unified Sensor

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <FastLED.h>

#define LED_PIN     5
#define NUM_LEDS    52   // match your actual strip length
#define BRIGHTNESS  120  // keep moderate given the 6V power situation

CRGB leds[NUM_LEDS];
Adafruit_MPU6050 mpu;

// ---------------- Tuning ----------------

// Filled in by calibrate() at boot — sword must be held still during that.
float restAccelMag = 9.8;   // m/s^2, ~1g at rest
float restGyroMag   = 0.0;  // rad/s, should be near 0

// Rotation -> hue cycling speed
float gyroDegDivisor = 20.0;  // higher = gentler color speed response
float maxHueSpeed    = 25.0;  // clamp so it can't strobe uncomfortably

// Clash / impact detection
float impactAccelThresh = 25.0;  // m/s^2 ABOVE resting baseline
float impactJerkThresh  = 40.0;  // m/s^2 change between consecutive samples
unsigned long impactCooldownMs = 400; // don't re-trigger within this window

// Flash shape: brief solid hold, then smooth decay back into gradient color
const unsigned long flashHoldMs  = 40;
const unsigned long flashDecayMs = 350;

// ---------------- State ----------------

float lastAccelMag = 9.8;
float hue = 0;

bool flashing = false;
unsigned long flashStartTime = 0;
unsigned long lastImpactTime = 0;

// ---------------- Calibration ----------------

void calibrate() {
  float sumA = 0, sumG = 0;
  const int N = 200;
  for (int i = 0; i < N; i++) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    sumA += sqrt(a.acceleration.x * a.acceleration.x +
                 a.acceleration.y * a.acceleration.y +
                 a.acceleration.z * a.acceleration.z);
    sumG += sqrt(g.gyro.x * g.gyro.x +
                 g.gyro.y * g.gyro.y +
                 g.gyro.z * g.gyro.z);
    delay(5);
  }
  restAccelMag = sumA / N;
  restGyroMag  = sumG / N;
  lastAccelMag = restAccelMag;

  Serial.print("Calibrated rest accel: "); Serial.println(restAccelMag);
  Serial.print("Calibrated rest gyro:  "); Serial.println(restGyroMag);
}

// ---------------- Setup ----------------

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  if (!mpu.begin()) {
    Serial.println("MPU6050 not found!");
    while (1) delay(10);
  }

  // Wide ranges so a hard swing/clash doesn't clip
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_1000_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("Hold the sword still for calibration...");
  delay(1000);
  calibrate();
  Serial.println("Ready.");
}

// ---------------- Loop ----------------

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float accelMag = sqrt(a.acceleration.x * a.acceleration.x +
                         a.acceleration.y * a.acceleration.y +
                         a.acceleration.z * a.acceleration.z);
  float gyroMag = sqrt(g.gyro.x * g.gyro.x +
                        g.gyro.y * g.gyro.y +
                        g.gyro.z * g.gyro.z); // rad/s

  float accelDelta = fabs(accelMag - lastAccelMag);
  lastAccelMag = accelMag;

  // ---- Clash / impact detection ----
  bool impactCondition =
      (accelMag - restAccelMag > impactAccelThresh) &&
      (accelDelta > impactJerkThresh) &&
      (millis() - lastImpactTime > impactCooldownMs);

  if (impactCondition && !flashing) {
    flashing = true;
    flashStartTime = millis();
    lastImpactTime = millis();
    Serial.print("CLASH! accel=");
    Serial.print(accelMag);
    Serial.print(" jerk=");
    Serial.println(accelDelta);
  }

  // ---- Rotation-driven hue cycling ----
  float gyroDeg = gyroMag * (180.0 / PI); // rad/s -> deg/s
  float hueSpeed = constrain(gyroDeg / gyroDegDivisor, 0.0, maxHueSpeed);
  hue += hueSpeed;
  if (hue >= 255.0) hue -= 255.0;

  CRGB baseColor = CHSV((uint8_t)hue, 255, 255);

  // ---- Render ----
  if (flashing) {
    unsigned long elapsed = millis() - flashStartTime;
    if (elapsed < flashHoldMs) {
      fill_solid(leds, NUM_LEDS, CRGB::White);
    } else if (elapsed < flashHoldMs + flashDecayMs) {
      float t = (float)(elapsed - flashHoldMs) / (float)flashDecayMs; // 0..1
      CRGB c = blend(CRGB::White, baseColor, (uint8_t)(t * 255));
      fill_solid(leds, NUM_LEDS, c);
    } else {
      flashing = false;
    }
  }

  if (!flashing) {
    fill_solid(leds, NUM_LEDS, baseColor);
  }

  FastLED.show();
  delay(5); // ~200Hz loop
}
