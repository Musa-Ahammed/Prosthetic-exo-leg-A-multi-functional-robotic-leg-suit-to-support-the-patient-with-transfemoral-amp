# ESP32 starter code

[Back to the project overview](../README.md)

[ExoLegBase.ino](ExoLegBase.ino) contains starter code for testing sensors and optional servo signals with an ESP32-WROOM-DA. It does not implement the paper's walking controller or Kalman filter.

## Current setup status

The sketch targets the Arduino-ESP32  core and the **ESP32-WROOM-DA Module** board setting.



## What the code does

- Reads two MPU6050 sensors at I²C addresses `0x68` and `0x69` to monitor leg orientation and movement.

- Reads an A3144 Hall-effect sensor connected to GPIO34 to detect magnet proximity in the foot-contact mechanism.

- Samples sensor readings approximately every **10 ms (100 Hz)** and logs data every **50 ms (20 Hz)**.

- Measures acceleration, angular velocity, and estimated tilt angles using the MPU6050 sensors.

- Identifies possible gait states, including **standing, walking initiation, stance, and swing**, based on Hall sensor and MPU6050 readings.

- Estimates the current walking-cycle percentage using predefined stance and swing durations.

- Generates smooth, predefined **knee and ankle angle trajectories** that approximate natural human walking motion.

- Uses Servo A (GPIO18) for the knee and Servo B (GPIO19) for the ankle.

- Supports manual servo movement through Serial Monitor commands: `ARM`, `MOVE`, `KEEPALIVE`, and `DISARM`.

- Restricts bench servo-control pulses to **1400–1600 µs** and disables PWM when the hardware permit is removed, a sensor fault occurs, or the command timeout exceeds **250 ms**.

- Sends timestamps, sensor readings, gait states, estimated joint angles, servo pulse widths, and permit status over Serial at **115200 baud**.

- Keeps automatic gait trajectories separate from physical servo control. **Sensor-detected movement does not automatically move the servos.**

## Pin assignments in the code

| Function                   | ESP32 GPIO |
| -------------------------- | ---------- |
| I2C SDA / SCL              | 21 / 22    |
| Hall inputs A / B          | 34 / 35    |
| Servo signal outputs A / B | 18 / 19    |
| Permit input               | 27         |

The code expects external pull-ups for the Hall inputs and an external pull-down for the permit input. Its comments reserve GPIO2 and GPIO25 for antenna switching. The [hardware images](../hardware/README.md) are separate design references; do not assume their wiring matches this sketch.

## Optional bench servo signals



| Serial command    | Behavior                                                                                                                               |
| ----------------- | -------------------------------------------------------------------------------------------------------------------------------------- |
| `ARM`             | Starts both outputs at 1500 µs if both sensors have fresh readings and the permit input is HIGH. Sending it again while armed disarms. |
| `PULSE 1450 1550` | Sets the two pulse widths within the allowed 1400–1600 µs range.                                                                       |
| `KEEPALIVE`       | Refreshes the command timeout while armed.                                                                                             |
| `DISARM`          | Stops the servo signals.                                                                                                               |

Commands end with a newline. The code uses 50 Hz PWM and disarms after a command timeout of more than 250 ms, a sensor failure, a LOW permit input, or an invalid command. Arming starts servo pulses and can cause movement.

This mode is for bench testing without a wearer. The pulse range is not calibrated to prosthetic joint limits. Stopping the signal does not remove servo power; actuator tests need a separate power disconnect.
