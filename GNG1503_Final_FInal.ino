#define BLYNK_PRINT Serial

#define BLYNK_TEMPLATE_ID "TMPL2toP5x4WO"
#define BLYNK_TEMPLATE_NAME "Tape o matique"
#define BLYNK_AUTH_TOKEN "tz4WIRGE1_rn5ZcgeYk79sOD_eav-kP3"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_MPR121.h>
#include <DFRobot_LIS.h>
#include <math.h>

#ifndef _BV
#define _BV(bit) (1 << (bit)) 
#endif

// ===== WiFi =====
char ssid[] = "iPhone (7)";
char pass[] = "tms8-Ah83";

// ===== Blynk =====
WidgetTerminal terminal(V0);

// ===== Sensors =====
Adafruit_MPR121 cap = Adafruit_MPR121();
uint16_t lasttouched = 0;
uint16_t currtouched = 0;

DFRobot_H3LIS200DL_I2C acce;

// ===== Punch detection =====
float peakG = 0;
const float PUNCH_THRESHOLD_G = 5.0;

// ===== Touch lock (prevents spam) =====
bool touchLocked = false;
unsigned long touchLockTime = 0;
const int TOUCH_LOCK_MS = 200;

// ===== LOG =====
void logLine(String msg) {
  Serial.println(msg);
  terminal.println(msg);
  terminal.flush();
}

// ===== Zone mapping (FRENCH) =====
String zoneName(uint8_t i) {
  switch (i) {
    case 0: return "centre";
    case 1: return "droite";
    case 2: return "bas";
    case 3: return "haut";
    case 5: return "gauche";
    default: return "zone " + String(i);
  }
}

// ===== Power level =====
int getPowerLevel(float g) {
  if (g < 5) return 0;
  else if (g < 10) return 1;
  else if (g < 20) return 2;
  else if (g < 35) return 3;
  else if (g < 50) return 4;
  else return 5;
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(2000);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  logLine("Systeme demarrage...");

  Wire.begin();

  // MPR121
  if (!cap.begin(0x5A, &Wire)) {
    logLine("MPR121 non detecte!");
    while (1);
  }
  cap.setAutoconfig(true);
  logLine("MPR121 pret");

  // Accelerometer
  while (!acce.begin()) {
    logLine("Accelerometre echec");
    delay(1000);
  }

  // ===== FIXED SETTINGS =====
  acce.setRange(DFRobot_LIS::eH3lis200dl_100g);
  acce.setAcquireRate(DFRobot_LIS::eNormal_1000HZ);

  logLine("Accelerometre pret (1000Hz, ±100g)");
}

// ===== LOOP =====
void loop() {
  Blynk.run();

  // ===== READ ACCEL (already in g) =====
  float ax = acce.readAccX();
  float ay = acce.readAccY();
  float az = acce.readAccZ();

  float magnitude = sqrt(ax * ax + ay * ay + az * az);

  if (magnitude > peakG) {
    peakG = magnitude;
  }

  // ===== TOUCH LOCK TIMER =====
  if (touchLocked && millis() - touchLockTime > TOUCH_LOCK_MS) {
    touchLocked = false;
  }

  currtouched = cap.touched();

  // ===== collect zones =====
  String zones = "";
  int count = 0;

  if (!touchLocked) {

    for (uint8_t i = 0; i < 12; i++) {
      if (currtouched & _BV(i)) {
        zones += zoneName(i) + " ";
        count++;
      }
    }

    if (count > 0) {

      touchLocked = true;
      touchLockTime = millis();

      bool strongEnough = peakG > PUNCH_THRESHOLD_G;
      bool zDominant = abs(az) > abs(ax) && abs(az) > abs(ay);

      if (strongEnough && zDominant) {

        int level = getPowerLevel(peakG);

        logLine("COUP | Zones: " + zones +
                "| Nombre: " + String(count) +
                "| " + String(peakG, 2) +
                " g | Niveau: " + String(level));

      } else {

        logLine("Ignore | Zones: " + zones +
                "| Nombre: " + String(count) +
                "| " + String(peakG, 2));
      }

      peakG = 0;
    }
  }

  lasttouched = currtouched;
}
