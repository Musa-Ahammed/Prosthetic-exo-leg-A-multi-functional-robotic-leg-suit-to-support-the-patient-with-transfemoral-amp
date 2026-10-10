# Prosthetic Exo-Leg

### A Functional Robotic Leg Suit to Assist Patients with Transfemoral Amputation

An undergraduate thesis project at BRAC University exploring an affordable powered prosthetic leg for people with above-knee amputation. The project combines mechanical design, ESP32 control, motion sensors, and servo motors to assist knee and ankle movement.

The work was published at **IEEE TENCON 2023**. This repository brings together project notes, hardware images, measurement files, and ESP32 starter code.

[Publication details](docs/references.md) · [Hardware](hardware/README.md) · [Measurements](experiments/README.md) · [Firmware](firmware/README.md)

![Prosthetic exo-leg mechanical design study](media/design-study.png)

*Mechanical design study from the team report. This is a CAD view of the design.*

<img width="1240" height="671" alt="leg" src="https://github.com/user-attachments/assets/6180f328-d558-4075-b1ab-b7cf8bc8ba8c" />

<img width="1172" height="606" alt="leg2" src="https://github.com/user-attachments/assets/434c16d3-1ecf-4eef-817e-ca5e9bc97562" />


## How the project works

The published system uses motion and foot-loading information to guide knee and ankle movement.

| Part | Purpose |
| --- | --- |
| ESP32 | Processes sensor readings and controls actuation. |
| MPU6050 sensors | Measure acceleration and rotation. |
| Hall-effect sensor and flexible foot pad | Detect loading through foot-pad deformation. |
| Servo motors | Move the knee and ankle. |
| Kalman filtering | Reduces noise and drift in the sensor signals described in the paper. |
| Mechanical structure | Supports the socket, joints, actuators, and foot. |

Reference walking patterns help guide prototype movement. The team refined the design through sensor testing and repeated adjustments. See the [methodology notes](docs/methodology.md) for more detail.

## My contribution

As a coauthor, I contributed to:

- Circuit and PCB design.
- ESP32 programming.
- Sensor testing and prototype adjustments.
- Writing the conference paper.

The project and its reported results reflect the work of the full author team.

## Reported results

The paper compares walking measurements from a reference participant and one prosthesis user, using an existing prosthesis and the proposed exo-leg.

| Measure | Result reported in the paper |
| --- | --- |
| Prototype mass | 2.4 kg. |
| Observed runtime | 8 hours 50 minutes during operation without a wearer. |
| Final prototype budget | US$814.84 at the time of the study. |
| Walking comparison | Similarity in the Y-axis pattern, with remaining X-axis differences. |
| Sensor processing | Kalman-filtered signals are shown; no numerical error-reduction percentage is reported. |

These are preliminary results. The runtime is not a walking-endurance measurement, and the small evaluation does not establish long-term performance. The prototype was not water-resistant. Further work includes broader testing, movement refinement, and user training.

## Repository materials

| Material | Location |
| --- | --- |
| Publication and references | [References](docs/references.md) |
| System approach and design notes | [Methodology](docs/methodology.md) |
| Schematic, PCB images, and early budget | [Hardware](hardware/README.md) |
| Original measurement workbooks and saved results | [Experiments](experiments/README.md) |
| Measurement summary and both plots | [Results report](experiments/results/report.md) |
| ESP32 starter code and setup notes | [Firmware](firmware/README.md) · [ExoLegBase.ino](firmware/ExoLegBase.ino) |
| Mechanical design image | [Design study](media/design-study.png) |
| Available sources and remaining questions | [Source notes](docs/source-notes.md) |

### Current folder structure

```text
.
├── README.md
├── docs/
│   ├── methodology.md
│   ├── references.md
│   └── source-notes.md
├── experiments/
│   ├── README.md
│   ├── data/
│   │   ├── data-dictionary.md
│   │   ├── raw/
│   │   │   ├── Position1.xlsx
│   │   │   └── Position2.xlsx
│   │   └── processed/
│   │       └── measurements.csv
│   └── results/
│       ├── channel-summary.csv
│       ├── Position1.png
│       ├── Position2.png
│       └── report.md
├── firmware/
│   ├── README.md
│   └── ExoLegBase.ino
├── hardware/
│   ├── README.md
│   ├── BOM-early-design.csv
│   ├── Sch-PEL.jpg
│   ├── PCB-PEL.jpg
│   └── 3D-PEL.jpg
└── media/
    └── design-study.png
```

## Measurement preview

![Position 1 measurements for the three recorded conditions](experiments/results/Position1.png)

[Open Position 1 at full size](experiments/results/Position1.png) · [View Position 2](experiments/results/Position2.png)

The workbooks relate to knee and ankle measurements, but the file-to-joint mapping, units, and sampling interval still need confirmation. The plots show recorded values by sample index. The saved CSV files and plots can be viewed directly; the analysis script is not included in this folder.

The firmware folder contains starter code for electronics testing. It requires a missing header before it can be built. The hardware folder contains reference images and an early budget; editable circuit, PCB, and CAD files are not included.

## Publication

F. Y. Nipa, F. U. Enam, M. T. A. Abir, M. A. Mahin, A. H. M. A. Rahim, M. M. H. Shawon, and M. R. Hasan, “Prosthetic Exo-Leg: A Functional Robotic Leg Suit to Assist Patients with Transfemoral Amputation,” *TENCON 2023 – 2023 IEEE Region 10 Conference (TENCON)*, 2023.

[Read the publication on IEEE Xplore](https://ieeexplore.ieee.org/document/10322482/)
