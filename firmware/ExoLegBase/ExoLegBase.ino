
/*
 * Prosthetic Exo-Leg | V5
 * ESP32-WROOM-DA | Arduino-ESP32 3.x
 * Dual MPU6050, A3144 foot switch, knee/ankle PWM outputs
 * Gait references are logged; PWM is manually commanded.
 */

// 1. Libraries and pin definitions
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

constexpr uint8_t SDA_PIN = 21, SCL_PIN = 22;
constexpr uint8_t HALL_PIN = 34, PERMIT_PIN = 27;
constexpr uint8_t KNEE_PIN = 18, ANKLE_PIN = 19;

// 2. Timing, motion thresholds, and bench PWM limits
constexpr uint32_t SAMPLE_MS = 10, PRINT_MS = 50;
constexpr uint32_t DEBOUNCE_MS = 30, MOTION_CONFIRM_MS = 120;
constexpr uint32_t STILL_CONFIRM_MS = 600, COMMAND_TIMEOUT_MS = 250;
constexpr float MOTION_START_DPS = 15.0f, MOTION_STOP_DPS = 5.0f;
constexpr uint32_t PWM_HZ = 50;
constexpr uint8_t PWM_BITS = 16;
constexpr int MIN_US = 1400, MAX_US = 1600, CENTER_US = 1500;

// 3. IMU data and gait states
struct Imu {
  uint8_t address;
  bool ready = false;
  float ax = 0, ay = 0, az = 0;
  float gx = 0, gy = 0, gz = 0;
  float pitch = 0;
};

Imu imuA{0x68}, imuB{0x69};

enum GaitState { UNKNOWN, STANDING, INITIATING, STANCE, SWING, FAULT };
GaitState gait = UNKNOWN;
uint32_t stateSince = 0, phaseSince = 0;

const char* gaitName(GaitState s) {
  switch (s) {
    case STANDING: return "STANDING";
    case INITIATING: return "INITIATING";
    case STANCE: return "STANCE";
    case SWING: return "SWING";
    case FAULT: return "FAULT";
    default: return "UNKNOWN";
  }
}

// 4. Reference joint trajectories (anatomical degrees, not PWM angles)
struct GaitPoint { float percent, knee, ankle; };

const GaitPoint walking[] = {
  {0, 5, 0},
  {10, 15, -5},
  {30, 5, 5},
  {50, 5, 10},
  {60, 40, -15},
  {73, 60, -5},
  {87, 30, 0},
  {100, 5, 0}
};

constexpr size_t POINTS = sizeof(walking) / sizeof(walking[0]);

struct JointTargets { float knee, ankle; };
JointTargets virtualTarget{NAN, NAN};
float gaitPercent = NAN;

// 5. MPU6050 register access
bool writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool readRegs(uint8_t addr, uint8_t reg, uint8_t* b, size_t count) {
  Wire.beginTransmission(addr);
  Wire.write(reg);

  if (Wire.endTransmission(false) != 0) return false;

  if (Wire.requestFrom(addr, count, true) != count) {
    while (Wire.available()) Wire.read();
    return false;
  }

  for (size_t i = 0; i < count; i++) b[i] = Wire.read();
  return true;
}

int16_t signed16(const uint8_t* b) {
  return static_cast<int16_t>((uint16_t(b[0]) << 8) | b[1]);
}

// 6. MPU6050 initialization and sampling
bool initImu(Imu& imu) {
  uint8_t id = 0;

  if (!readRegs(imu.address, 0x75, &id, 1) || id != 0x68)
    return false;

  if (!writeReg(imu.address, 0x6B, 0x01))
    return false;

  delay(20);

  return writeReg(imu.address, 0x1A, 3) &&
         writeReg(imu.address, 0x19, 9) &&
         writeReg(imu.address, 0x1B, 0) &&
         writeReg(imu.address, 0x1C, 0);
}

bool sampleImu(Imu& imu) {
  uint8_t b[14];

  if (!imu.ready || !readRegs(imu.address, 0x3B, b, 14))
    return false;

  imu.ax = signed16(b) / 16384.0f;
  imu.ay = signed16(b + 2) / 16384.0f;
  imu.az = signed16(b + 4) / 16384.0f;

  imu.gx = signed16(b + 8) / 131.0f;
  imu.gy = signed16(b + 10) / 131.0f;
  imu.gz = signed16(b + 12) / 131.0f;

  // Accelerometer-only tilt estimate; mounting axes are not calibrated.
  imu.pitch = atan2f(
    -imu.ax,
    sqrtf(imu.ay * imu.ay + imu.az * imu.az)
  ) * 180.0f / PI;

  return isfinite(imu.pitch);
}

float angularSpeed(const Imu& imu) {
  return sqrtf(
    imu.gx * imu.gx +
    imu.gy * imu.gy +
    imu.gz * imu.gz
  );
}

// 7. Foot-contact switch filtering
bool rawContact = false, footContact = false;
uint32_t contactChanged = 0;

void updateContact(uint32_t now) {
  // A3144 output is active-low; GPIO34 requires an external pull-up.
  bool current = digitalRead(HALL_PIN) == LOW;

  if (current != rawContact) {
    rawContact = current;
    contactChanged = now;
  }

  if (now - contactChanged >= DEBOUNCE_MS)
    footContact = rawContact;
}

// 8. IMU motion detection
bool moving = false;
uint32_t motionChanged = 0;

void updateMotion(uint32_t now) {
  float rate = fmaxf(
    angularSpeed(imuA),
    angularSpeed(imuB)
  );

  bool previous = moving;

  if (rate > MOTION_START_DPS)
    moving = true;
  else if (rate < MOTION_STOP_DPS)
    moving = false;

  if (moving != previous)
    motionChanged = now;
}

bool motionConfirmed(uint32_t now) {
  return moving &&
         now - motionChanged >= MOTION_CONFIRM_MS;
}

bool stillConfirmed(uint32_t now) {
  return !moving &&
         now - motionChanged >= STILL_CONFIRM_MS;
}

// 9. Gait-state estimation
void changeState(GaitState next, uint32_t now) {
  if (next == gait) return;

  gait = next;
  stateSince = now;

  if (next == STANCE || next == SWING)
    phaseSince = now;

  Serial.printf("# GAIT: %s\n", gaitName(gait));
}

void updateGait(uint32_t now) {
  bool motion = motionConfirmed(now);
  bool still = stillConfirmed(now);

  switch (gait) {
    case UNKNOWN:
      if (footContact && still)
        changeState(STANDING, now);
      else if (!footContact && motion)
        changeState(SWING, now);
      break;

    case STANDING:
      if (footContact && motion)
        changeState(INITIATING, now);
      else if (!footContact && motion)
        changeState(SWING, now);
      else if (!footContact && still)
        changeState(UNKNOWN, now);
      break;

    case INITIATING:
      if (!footContact && motion)
        changeState(SWING, now);
      else if (footContact && still)
        changeState(STANDING, now);
      break;

    case STANCE:
      if (!footContact)
        changeState(SWING, now);
      else if (still)
        changeState(STANDING, now);
      break;

    case SWING:
      if (footContact)
        changeState(STANCE, now);
      else if (still)
        changeState(UNKNOWN, now);
      break;

    case FAULT:
      break;
  }
}

// 10. Gait-phase timing and joint reference interpolation
void estimateGaitPercent(uint32_t now) {
  float elapsed = float(now - phaseSince);

  switch (gait) {
    case STANDING:
      gaitPercent = 0;
      break;

    case STANCE:
      gaitPercent = constrain(
        elapsed / 650.0f * 60.0f, 0.0f, 59.9f
      );
      break;

    case SWING:
      gaitPercent = 60.0f + constrain(
        elapsed / 450.0f * 40.0f, 0.0f, 39.9f
      );
      break;

    default:
      gaitPercent = NAN;
      break;
  }
}

float smoothstep(float t) {
  t = constrain(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

void generateTrajectory() {
  virtualTarget = {NAN, NAN};

  if (!isfinite(gaitPercent)) return;

  if (gait == STANDING) {
    virtualTarget = {5.0f, 0.0f};
    return;
  }

  for (size_t i = 0; i + 1 < POINTS; i++) {
    const GaitPoint& a = walking[i];
    const GaitPoint& b = walking[i + 1];

    if (gaitPercent >= a.percent &&
        gaitPercent <= b.percent) {

      float t = smoothstep(
        (gaitPercent - a.percent) /
        (b.percent - a.percent)
      );

      virtualTarget.knee =
        a.knee + (b.knee - a.knee) * t;

      virtualTarget.ankle =
        a.ankle + (b.ankle - a.ankle) * t;

      return;
    }
  }
}

// 11. Manual PWM bench-control state
bool armed = false;
bool kneeAttached = false;
bool ankleAttached = false;

uint32_t lastCommand = 0;
int kneePulse = 0, anklePulse = 0;

bool permit() {
  return digitalRead(PERMIT_PIN) == HIGH;
}

uint32_t pulseDuty(int us) {
  return uint32_t(
    uint64_t(us) * PWM_HZ * 65535ULL / 1000000ULL
  );
}

void disarm() {
  if (kneeAttached)
    ledcDetach(KNEE_PIN);

  if (ankleAttached)
    ledcDetach(ANKLE_PIN);

  kneeAttached = false;
  ankleAttached = false;
  armed = false;

  kneePulse = 0;
  anklePulse = 0;

  pinMode(KNEE_PIN, OUTPUT);
  pinMode(ANKLE_PIN, OUTPUT);

  digitalWrite(KNEE_PIN, LOW);
  digitalWrite(ANKLE_PIN, LOW);
}

// 12. Manual pulse commands
bool moveBenchServos(int kneeUs, int ankleUs) {
  if (!armed || !permit() || gait == FAULT)
    return false;

  if (kneeUs < MIN_US || kneeUs > MAX_US ||
      ankleUs < MIN_US || ankleUs > MAX_US)
    return false;

  bool a = ledcWrite(KNEE_PIN, pulseDuty(kneeUs));
  bool b = ledcWrite(ANKLE_PIN, pulseDuty(ankleUs));

  if (!a || !b) {
    disarm();
    return false;
  }

  kneePulse = kneeUs;
  anklePulse = ankleUs;
  lastCommand = millis();

  return true;
}

bool armBench() {
  if (armed || !permit() ||
      !imuA.ready || !imuB.ready ||
      gait == FAULT)
    return false;

  kneeAttached = ledcAttach(KNEE_PIN, PWM_HZ, PWM_BITS);
  ankleAttached = ledcAttach(ANKLE_PIN, PWM_HZ, PWM_BITS);

  if (!kneeAttached || !ankleAttached) {
    disarm();
    return false;
  }

  armed = true;
  lastCommand = millis();

  if (!moveBenchServos(CENTER_US, CENTER_US)) {
    disarm();
    return false;
  }

  return true;
}

// 13. Serial command parser
char command[64];
size_t commandLength = 0;
bool discardLine = false;

void processCommand(const char* cmd) {
  if (strcmp(cmd, "DISARM") == 0) {
    disarm();
    Serial.println("# DISARMED");
    return;
  }

  if (strcmp(cmd, "ARM") == 0) {
    Serial.println(
      armBench() ? "# ARMED" : "# ARM REJECTED"
    );
    return;
  }

  if (strcmp(cmd, "KEEPALIVE") == 0) {
    if (armed && permit() && gait != FAULT)
      lastCommand = millis();
    return;
  }

  int kneeUs, ankleUs;
  char extra;

  if (sscanf(cmd, "MOVE %d %d %c",
             &kneeUs, &ankleUs, &extra) == 2) {

    bool ok = moveBenchServos(kneeUs, ankleUs);

    if (!ok)
      disarm();

    Serial.println(
      ok ? "# MOVE ACCEPTED" : "# MOVE REJECTED"
    );
    return;
  }

  disarm();
  Serial.println("# INVALID COMMAND");
}

void readSerial() {
  for (int n = 0; n < 64 && Serial.available(); n++) {
    char c = Serial.read();

    if (c == '\r') continue;

    if (c == '\n') {
      if (!discardLine && commandLength > 0) {
        command[commandLength] = '\0';
        processCommand(command);
      }

      commandLength = 0;
      discardLine = false;
      continue;
    }

    if (discardLine) continue;

    if (c < 32 || c > 126 ||
        commandLength >= sizeof(command) - 1) {

      disarm();
      commandLength = 0;
      discardLine = true;
      continue;
    }

    command[commandLength++] = c;
  }
}

// 14. Telemetry
void printData(uint32_t now) {
  Serial.printf(
    "%lu,%d,%d,%d,%s,%.1f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%d,%d,%d\n",
    (unsigned long)now,
    footContact ? 1 : 0,
    imuA.ready ? 1 : 0,
    imuB.ready ? 1 : 0,
    gaitName(gait),
    gaitPercent,
    imuA.pitch,
    imuB.pitch,
    angularSpeed(imuA),
    angularSpeed(imuB),
    virtualTarget.knee,
    virtualTarget.ankle,
    permit() ? 1 : 0,
    armed ? 1 : 0,
    kneePulse,
    anklePulse
  );
}

// 15. Hardware initialization
uint32_t lastSample = 0, lastPrint = 0;

void setup() {
  pinMode(PERMIT_PIN, INPUT); // External pulldown
  pinMode(HALL_PIN, INPUT);   // External 3.3V pull-up

  disarm();

  Serial.begin(115200);

  Wire.begin(SDA_PIN, SCL_PIN, 100000);
  Wire.setTimeOut(10);

  imuA.ready = initImu(imuA);
  imuB.ready = initImu(imuB);

  uint32_t now = millis();

  rawContact = footContact =
    (digitalRead(HALL_PIN) == LOW);

  contactChanged = now;
  motionChanged = now;
  stateSince = now;
  phaseSince = now;

  if (!imuA.ready || !imuB.ready)
    changeState(FAULT, now);

  Serial.println("# Exo-Leg V5");

  Serial.println(
    "# Manual commands: ARM, MOVE 1500 1500, KEEPALIVE, DISARM"
  );

  Serial.println(
    "ms,contact,imuA_ok,imuB_ok,state,gait_pct,"
    "pitchA,pitchB,gyroA,gyroB,"
    "virtual_knee_deg,virtual_ankle_deg,"
    "permit,armed,knee_pwm_us,ankle_pwm_us"
  );
}

// 16. Main loop
void loop() {
  uint32_t now = millis();

  if (armed &&
      (!permit() ||
       now - lastCommand > COMMAND_TIMEOUT_MS ||
       gait == FAULT)) {
    disarm();
  }

  readSerial();

  if (now - lastSample >= SAMPLE_MS) {
    lastSample = now;

    updateContact(now);

    bool a = sampleImu(imuA);
    bool b = sampleImu(imuB);

    if (!a || !b) {
      if (!a) imuA.ready = false;
      if (!b) imuB.ready = false;

      changeState(FAULT, now);
      virtualTarget = {NAN, NAN};
      gaitPercent = NAN;
      disarm();

    } else if (gait != FAULT) {
      updateMotion(now);
      updateGait(now);
      estimateGaitPercent(now);
      generateTrajectory();
    }
  }

  if (now - lastPrint >= PRINT_MS) {
    lastPrint = now;
    printData(now);
  }

  delay(1);
}
