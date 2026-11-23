#include <SPI.h>
#include <MFRC522.h>
#include <FastLED.h>
#include <math.h>

// =====================================================
// RFID / NFC CONFIGURATION
// =====================================================

#define SS_PIN 10
#define RST_PIN 9

MFRC522 rfid(SS_PIN, RST_PIN);

String lastUid = "";
unsigned long lastReadAt = 0;

const unsigned long CARD_DEBOUNCE_MS = 3000;

// =====================================================
// LED CONFIGURATION
// =====================================================

#define LED_PIN 6
#define NUM_LEDS 21
#define BRIGHTNESS 80
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];

const unsigned long LED_FRAME_INTERVAL = 10;

// =====================================================
// AMBIENT BUBBLES
// =====================================================

struct Bubble {
  float pos;
  float speed;
  uint8_t brightness;
  int8_t direction;
};

Bubble bubbles[3];

uint8_t breath = 0;
bool breathUp = true;

unsigned long lastFrameTime = 0;
unsigned long lastBreathStep = 0;
unsigned long lastFlipCheck = 0;
unsigned long lastLedUpdate = 0;

// =====================================================
// EXPERIENCE / LIGHTNING
// =====================================================

bool experienceActive = false;

unsigned long experienceStart = 0;

const unsigned long EXPERIENCE_DURATION = 2200;

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(9600);

  // Initialize RFID reader
  SPI.begin();
  rfid.PCD_Init();

  // Initialize LEDs
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(
    leds,
    NUM_LEDS
  );

  FastLED.setBrightness(BRIGHTNESS);

  FastLED.clear();
  FastLED.show();

  // Initialize random generator
  randomSeed(analogRead(A0));

  // Initialize bubbles
  for (int i = 0; i < 3; i++) {

    bubbles[i].pos =
      random(NUM_LEDS * 10) / 10.0;

    bubbles[i].speed =
      random(150, 400) / 100.0;

    bubbles[i].brightness =
      random(60, 120);

    bubbles[i].direction =
      random(2) ? 1 : -1;
  }

  lastFrameTime = millis();

  // No extra Serial messages.
  // Website receives only CARD:UID.
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  handleRfid();

  updateLeds();

}

// =====================================================
// RFID / NFC DETECTION
// =====================================================

void handleRfid() {

  // Check if a new card is present
  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }

  // Read card UID
  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }

  // Build UID string
  String uidString = "";

  for (byte i = 0; i < rfid.uid.size; i++) {

    if (rfid.uid.uidByte[i] < 0x10) {
      uidString += "0";
    }

    uidString += String(
      rfid.uid.uidByte[i],
      HEX
    );
  }

  uidString.toUpperCase();

  // =================================================
  // DUPLICATE CARD PROTECTION
  // =================================================

  unsigned long now = millis();

  bool differentCard = (uidString != lastUid);

  bool debounceExpired =
    (now - lastReadAt >= CARD_DEBOUNCE_MS);

  if (differentCard || debounceExpired) {

    // Only valid card messages go to website
    Serial.print("CARD:");
    Serial.println(uidString);

    // Save last detected card
    lastUid = uidString;
    lastReadAt = now;

    // Start lightning experience
    experienceActive = true;
    experienceStart = now;
  }

  // End RFID communication
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// =====================================================
// LED CONTROLLER
// =====================================================

void updateLeds() {

  unsigned long now = millis();

  if (now - lastLedUpdate < LED_FRAME_INTERVAL) {
    return;
  }

  float dt = (now - lastFrameTime) / 1000.0;

  lastFrameTime = now;
  lastLedUpdate = now;

  // =================================================
  // EXPERIENCE MODE
  // =================================================

  if (experienceActive) {

    unsigned long elapsed =
      now - experienceStart;

    // PHASE 1: Electric charging
    if (elapsed < 250) {

      lightningCharge(elapsed);

    }

    // PHASE 2: Lightning strikes
    else if (elapsed < 700) {

      lightningStrike(elapsed);

    }

    // PHASE 3: Warm energy pulse
    else if (elapsed < 1400) {

      energyPulse(elapsed - 700);

    }

    // PHASE 4: Fade back to ambient
    else if (elapsed < EXPERIENCE_DURATION) {

      experienceFade(elapsed - 1400);

    }

    // Experience finished
    else {

      experienceActive = false;

    }

    return;
  }

  // Normal ambient animation
  updateAmbient(dt, now);
}

// =====================================================
// AMBIENT ANIMATION
// =====================================================

void updateAmbient(float dt, unsigned long now) {

  // Navy blue breathing effect
  if (now - lastBreathStep >= 30) {

    lastBreathStep = now;

    if (breathUp) {

      breath++;

      if (breath >= 90) {
        breathUp = false;
      }

    } else {

      breath--;

      if (breath <= 15) {
        breathUp = true;
      }
    }
  }

  CRGB navy = CRGB(0, 0, breath);

  fill_solid(leds, NUM_LEDS, navy);

  // Bubble direction flip
  bool doFlipCheck =
    (now - lastFlipCheck >= 100);

  // =================================================
  // MOVING BUBBLES
  // =================================================

  for (int i = 0; i < 3; i++) {

    bubbles[i].pos +=
      bubbles[i].speed *
      bubbles[i].direction *
      dt;

    // Left boundary
    if (bubbles[i].pos < 0) {

      bubbles[i].pos = 0;
      bubbles[i].direction = 1;
    }

    // Right boundary
    if (bubbles[i].pos > NUM_LEDS - 1) {

      bubbles[i].pos = NUM_LEDS - 1;
      bubbles[i].direction = -1;
    }

    // Random direction change
    if (doFlipCheck && random(100) < 4) {

      bubbles[i].direction *= -1;
    }

    // Slight random brightness variation
    int variation = random(-3, 4);

    int newBrightness =
      bubbles[i].brightness + variation;

    bubbles[i].brightness =
      constrain(newBrightness, 40, 140);

    // Main LED positions
    int i0 = (int)floor(bubbles[i].pos);
    int i1 = i0 + 1;

    float frac = bubbles[i].pos - i0;

    uint8_t bMain = bubbles[i].brightness;

    uint8_t b0 = bMain * (1.0 - frac);
    uint8_t b1 = bMain * frac;

    // Main glow - first LED
    if (i0 >= 0 && i0 < NUM_LEDS) {

      leds[i0] += CRGB(
        b0,
        b0 * 0.55,
        b0 * 0.12
      );
    }

    // Main glow - next LED
    if (i1 >= 0 && i1 < NUM_LEDS) {

      leds[i1] += CRGB(
        b1,
        b1 * 0.55,
        b1 * 0.12
      );
    }

    // Secondary glow positions
    int gLeft = i0 - 1;
    int gRight = i1 + 1;

    uint8_t glow0 = 25 * (1.0 - frac);
    uint8_t glow1 = 25 * frac;

    if (gLeft >= 0) {

      leds[gLeft] += CRGB(
        glow0,
        glow0 * 0.6,
        glow0 * 0.12
      );
    }

    if (gRight < NUM_LEDS) {

      leds[gRight] += CRGB(
        glow1,
        glow1 * 0.6,
        glow1 * 0.12
      );
    }
  }

  if (doFlipCheck) {
    lastFlipCheck = now;
  }

  FastLED.show();
}

// =====================================================
// PHASE 1: ELECTRIC CHARGING
// Duration: 0 - 250 ms
// =====================================================

void lightningCharge(unsigned long elapsed) {

  fill_solid(
    leds,
    NUM_LEDS,
    CRGB(0, 0, 0)
  );

  uint8_t intensity = map(
    elapsed,
    0,
    250,
    10,
    150
  );

  // Random electric sparks
  for (int i = 0; i < NUM_LEDS; i++) {

    if (random(100) < 25) {

      leds[i] = CRGB(
        intensity * 0.15,
        intensity * 0.45,
        intensity
      );
    }
  }

  FastLED.show();
}

// =====================================================
// PHASE 2: LIGHTNING STRIKES
// Duration: 250 - 700 ms
// =====================================================

void lightningStrike(unsigned long elapsed) {

  int flashChance = random(100);

  // White electric flash
  if (flashChance < 35) {

    fill_solid(
      leds,
      NUM_LEDS,
      CRGB(255, 255, 255)
    );

  }

  // Blue electric flash
  else if (flashChance < 55) {

    fill_solid(
      leds,
      NUM_LEDS,
      CRGB(40, 130, 255)
    );

  }

  // Darkness between strikes
  else {

    fill_solid(
      leds,
      NUM_LEDS,
      CRGB(0, 0, random(10, 35))
    );
  }

  FastLED.show();
}

// =====================================================
// PHASE 3: WARM ENERGY PULSE
// Duration: 700 - 1400 ms
// =====================================================

void energyPulse(unsigned long elapsed) {

  fill_solid(
    leds,
    NUM_LEDS,
    CRGB(0, 0, 10)
  );

  // Pulse starts from center
  float center = (NUM_LEDS - 1) / 2.0;

  float progress = elapsed / 700.0;

  float radius =
    progress * (NUM_LEDS / 2.0);

  for (int i = 0; i < NUM_LEDS; i++) {

    float distance = fabs(i - center);

    if (distance < radius) {

      float intensity =
        1.0 - (distance / radius);

      intensity = constrain(
        intensity,
        0.0,
        1.0
      );

      uint8_t r = 255 * intensity;
      uint8_t g = 100 * intensity;
      uint8_t b = 10 * intensity;

      leds[i] += CRGB(r, g, b);
    }
  }

  FastLED.show();
}

// =====================================================
// PHASE 4: FADE BACK TO AMBIENT
// Duration: 1400 - 2200 ms
// =====================================================

void experienceFade(unsigned long elapsed) {

  float progress = elapsed / 800.0;

  progress = constrain(
    progress,
    0.0,
    1.0
  );

  uint8_t blue = 20 + 50 * progress;

  uint8_t red =
    120 * (1.0 - progress);

  uint8_t green =
    45 * (1.0 - progress);

  fill_solid(
    leds,
    NUM_LEDS,
    CRGB(red, green, blue)
  );

  // Final glowing embers
  for (int i = 0; i < 3; i++) {

    int p = random(NUM_LEDS);

    leds[p] += CRGB(
      80,
      30,
      5
    );
  }

  FastLED.show();
}

// =====================================================
// END OF CODE
// =====================================================
