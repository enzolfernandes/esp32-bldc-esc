# Electronic Speed Controller (ESC) for BLDC Motors

## 📌 Project Overview
This repository contains the full Research and Development (R&D) cycle of a high-performance **Electronic Speed Controller (ESC)** designed for Three-Phase Brushless DC (BLDC) motors. Developed as a Senior Undergraduate Thesis in Electrical Engineering, this project bridges mathematical modeling, power electronics circuit design, and wireless embedded systems architecture.

> ⚠️ **Project Status: Work in Progress (WIP)** > The hardware design, power simulations, and mathematical modeling stages are complete. The project is currently transitioning into the embedded firmware development phase.

---

## 🛠️ Tech Stack & Engineering Tools
* **Hardware & EDA:** Altium Designer, EasyEDA
* **Simulation & Modeling:** LTspice, MATLAB/Simulink
* **Target Architecture (Planned):** C/C++, ESP32 Microcontroller, Bluetooth Core
* **Documentation:** LaTeX

---

## 📁 Repository Structure
```text
├── Docs/               # LaTeX source files for the thesis monograph and academic documentation
├── Hardware/           # Altium Designer schematics, PCB layouts, Gerber files, and BOM
├── Simulation/         # MATLAB/Simulink plant models and LTspice power stage simulations
└── Firmware/           # (Planned) Embedded C++ source code for the ESP32 microcontroller
```
---

## 📈 Engineering Stages & Implementation

### 1. Mathematical Modeling & Plant Simulation (MATLAB/Simulink)
Before physical layout development, the BLDC motor dynamics and switching characteristics were modeled mathematically. 
* **Simulink Models:** Used to evaluate back-EMF behavior, rotor position estimation, and control loop stability.
* **Control Strategy:** Theoretical foundations for vector control and electronic commutation routines were validated to ensure optimal torque and speed efficiency.

### 2. Power Stage & Signal Integrity Simulation (LTspice)
To prevent hardware failures during high-power switching events, critical sections of the circuit were simulated using LTspice.
* Transient analysis of power MOSFET switching characteristics.
* Validation of gate driver circuit performance and bootstrap capacitor sizing.
* Thermal and overcurrent protection threshold evaluation to ensure circuit robustness under power loading.

### 3. Hardware & PCB Design (Altium Designer)
The physical hardware was developed with a focus on power density, thermal dissipation, and signal integrity.
* **Schematic Capture:** Rigorous component specification targeting high efficiency and low noise figures.
* **PCB Layout:** Advanced routing using Altium Designer, featuring optimized power paths to minimize parasitic inductance and separation between high-current power stages and low-voltage digital control signals.
* **Fabrication:** Initial prototyping layouts generated via CNC routing for rapid laboratory bench testing.

---

## 🚀 Development Roadmap (Next Steps)

The next phase focuses on the execution of embedded firmware development and wireless hardware validation:

- [ ] **Firmware Architecture Setup:** Structuring the core C++ application using ESP-IDF/PlatformIO.
- [ ] **Commutation Lobbies:** Implementing hardware interrupts for precise phase switching based on sensor/sensorless feedback.
- [ ] **IoT & Wireless Connectivity:** Developing a **Bluetooth** communication layer for wireless remote control and real-time telemetry monitoring (speed, current, and temperature indexes).
- [ ] **Laboratory Bench Testing:** System debugging using oscilloscopes, logic analyzers, and signal generators to validate torque response and telemetry accuracy under real load conditions.

---

## 📬 Contact & Contributions
If you have questions regarding the hardware architecture, LTspice simulation models, or embedded IoT design, feel free to reach out.
