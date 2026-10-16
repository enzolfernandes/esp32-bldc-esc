# ESP32 BLDC Electronic Speed Controller

An Electrical Engineering undergraduate thesis by **Enzo Luiz Fernandes - UNESP/FEIS**, combining power electronics, PCB design, simulation and embedded firmware for a three-phase BLDC controller prototype.

[Published thesis - UNESP](https://hdl.handle.net/11449/327581) · [Firmware guide](Firmware/DOCUMENTACAO_PROGRAMACAO.md) · [Schematics and PCB](Hardware/PCB_Project) · [Results and limitations](Docs/Thesis/capitulos/5_conclusao.tex)

> **Status:** hardware and firmware artifacts are available, and static physical bench tests are documented in the thesis. **Full dynamic validation with the motor rotating was not completed** because of limitations identified in the physical power stage. This is an academic prototype, with no validated power, torque, efficiency or reliability rating claimed here.

![Existing photograph of the physical ESC prototype](Docs/Thesis/imagens/prototipo_top.png)

*Prototype photograph from the thesis. The image documents the assembly; it is not a demonstration of successful motor operation.*

## My contribution and project scope

I developed and analyzed the prototype as my undergraduate thesis: power-stage and PCB design, LTspice simulations, C/C++ firmware integration, static bench testing and fault investigation. The thesis records the methods, observations and remaining limitations.

The firmware uses third-party frameworks and libraries. Their authorship remains with their maintainers; writing this project does not imply authorship of Arduino-ESP32, ESP-IDF, FreeRTOS, Bluepad32 or networking dependencies.

## What is implemented and what is verified

| Area | Available evidence | Boundary |
| --- | --- | --- |
| Hardware | Altium schematics, PCB layout, fabrication outputs and prototype photographs | Design targets are not measured operating ratings |
| Simulation | LTspice circuits and exported data in [Simulation/](Simulation/) | Simulated waveforms are not physical measurements |
| MATLAB | [ScriptPlots.m](Simulation/ScriptPlots.m) imports LTspice data and exports plots | No Simulink model was identified in the inspected repository tree |
| Firmware | C control/drivers/HAL; C++ application and interfaces; six-step commutation; PI routines; FSM; ADC acquisition and protection logic | Source implementation does not prove full closed-loop motor validation |
| Physical tests | Static bench tests and investigation documented in the [published thesis](https://hdl.handle.net/11449/327581) | Full dynamic validation remains incomplete |
| Future work | Power-stage revision, further sensorless validation, FOC and CAN | FOC and CAN are not presented as implemented |

No compilation, flashing, simulation rerun or hardware test was performed as part of the **16 October 2026 documentation review**. No release artifact is provided as a verified build.

## Architecture and reading guide

| Layer | Responsibility | Entry point |
| --- | --- | --- |
| Application | Startup, operating states, command routing and telemetry | [main.cpp](Firmware/src/main.cpp), [fsm_system.c](Firmware/src/fsm_system.c) |
| Input | Serial HMI and optional PS4/Bluetooth input | [input modules](Firmware/lib/input/) |
| Control | Six-step commutation and PI routines, with FreeRTOS task infrastructure | [motor_control.c](Firmware/lib/control/motor_control.c) |
| Drivers | Phase-current sensing, battery monitoring and protection interfaces | [drivers](Firmware/lib/drivers/) |
| Hardware abstraction | MCPWM, ADC1, GPIO and DAC access through ESP-IDF APIs | [HAL](Firmware/lib/hal/) |

The project uses **Arduino within PlatformIO**, together with native ESP-IDF APIs and FreeRTOS. It is not described as a standalone, fully migrated ESP-IDF application. The current configuration and pin mapping are in [platformio.ini](Firmware/platformio.ini), [board_config.h](Firmware/include/board_config.h) and [ESP32_PINMAP.md](Hardware/PCB_Project/ESP32_PINMAP.md).

Command interfaces and telemetry have separate roles: serial HMI / optional PS4 input provide commands; serial and optional Wi-Fi provide telemetry. Consult configuration flags before interpreting which interface is active.

## Inspect and attempt a local build

Install Git and PlatformIO Core, or VS Code with the PlatformIO extension. From a terminal:

```sh
git clone https://github.com/enzolfernandes/esp32-bldc-esc.git
cd esp32-bldc-esc/Firmware
pio run -e esp32doit-devkit-v1
```

The checked-in configuration specifies:

- Board: `esp32doit-devkit-v1`; framework: `arduino`.
- Platform: `espressif32@6.10.0`.
- Arduino/Bluepad32 framework package pinned to commit `e010351b2cafe3a872f18878cb2d3e08f167c650`.
- ESPAsyncWebServer-esphome and AsyncTCP-esphome dependency ranges in `lib_deps`.

The command above is a **build procedure to attempt**, not a claim of a reproduced build. Dependency ranges, tool versions and network availability can affect reproduction. Preserve the resolved versions and build log when validating locally. Building does not require energizing the power stage. Hardware commissioning requires the checks and limits documented in the thesis and firmware guide; no upload or power-stage test is automated by this README.

For the stored simulation data, open MATLAB in `Simulation/` and run `ScriptPlots.m`; it reads `trifasico.txt` and writes figures to `plot_simulacao/`. This processes existing simulation output, rather than rerunning LTspice or producing bench measurements.

## Repository map

| Directory | Contents |
| --- | --- |
| [Docs/Thesis/](Docs/Thesis/) | LaTeX thesis source and figures |
| [Hardware/PCB_Project/](Hardware/PCB_Project/) | Altium design files and fabrication outputs |
| [Simulation/](Simulation/) | LTspice circuits, exported data and MATLAB plotting script |
| [Firmware/](Firmware/) | ESP32 source, configuration, documentation and terminology |

## Evidence and attribution

- [Institutional thesis record](https://repositorio.unesp.br/entities/publication/047e3b2b-8462-4287-8a9b-e9088d2abdaf): academic authorship and publication.
- [Technical baseline before this documentation revision](https://github.com/enzolfernandes/esp32-bldc-esc/tree/1c9bcb96cc0133639c62de5d59716d5ba15e6c02).
- [Six-step revision in project history](https://github.com/enzolfernandes/esp32-bldc-esc/commit/47ea7e2c07447b11022af4c07b100056b5089129).

No repository-level LICENSE file was identified in the inspected tree. No new license is assigned by this README; preserve applicable third-party notices and obtain permission where required.

*Presentation revised on 16 October 2026. This revision changes documentation only and does not add firmware functionality or test results.*
