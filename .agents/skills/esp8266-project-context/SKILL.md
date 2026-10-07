---
name: esp8266-project-context
description: Apply this repository's architecture, build, safety, and validation rules when changing ESP8266 firmware, NodeMCU wiring, or embedded web features.
---

# ESP8266 project context

Use this skill for firmware or hardware work in this repository. Read the root
`AGENTS.md` first; it defines the architecture, contract, wiring, build commands, and
workflow that take precedence over upstream examples.

## Workflow

1. Confirm the requested change against `redes-ii-vault/funcionalidades-pendientes.md`
   when it affects alarm behavior or simulation.
2. Read the relevant project sources and `shared/api_state.schema.json` before designing
   changes. Keep firmware and simulator behavior aligned only where the shared contract
   requires it.
3. For physical NodeMCU pin or electrical questions, consult
   `.agents/skills/esp8266-nodemcu-30pin/SKILL.md` and its `reference/` files. Confirm
   the actual board revision and wiring when they affect the answer.
4. Verify core-specific APIs and settings against the version vendored under
   `firmware/lib/esp8266/` and the root `xmake.lua`. The imported board skill's code and
   build examples target PlatformIO and Arduino core 3.1.2; they are not this project's
   build configuration.
5. Keep firmware buffers and dependencies small, avoid dynamic allocation, and never
   invent alarm policy. Protect credentials and do not run a hardware flash as an
   automated test.
6. Run the relevant native tests and, for firmware changes, the root-level Xtensa
   cross-build described in `AGENTS.md`. A native test does not validate physical
   hardware.

## External references

Use these as research sources, not as a replacement architecture or a reason to copy
code wholesale. Check each project's current version, compatibility, and license before
reusing code.

- [good-code](https://github.com/lmtreser/good-code): Arduino practices, style guides,
  and Arduino/ESP8266/ESP32 templates. Consult for maintainability; do not replace this
  repository's xmake workflow with its templates.
- [ESP8266-MyWidget-Demo](https://github.com/onewithhammer/ESP8266-MyWidget-Demo):
  an example of async web services, MQTT, LittleFS, and OTA. Its PlatformIO framework,
  dependencies, endpoints, and features are not project requirements.
- [awesome-esp](https://github.com/agucova/awesome-esp): a curated project and tooling
  directory. Treat entries as leads; verify their maintenance, target, and license.
- [esp-dev-skill](https://github.com/JasonYANG170/esp-dev-skill): a broad Espressif
  skill and recipe collection. Select the exact framework/SoC module before using it.
  Its `ESP8266_RTOS_SDK` material is not interchangeable with this repository's
  Arduino ESP8266 core.
- [mcu-skills](https://github.com/alexex1993/mcu-skills): the project-local
  `esp8266-nodemcu-30pin` skill was copied from this repository at commit
  `3937b231e1119828879dd55f43fc1c338ba9c587`. Its MIT license is included beside the
  skill. Re-check the upstream board revision and wiring before applying pin advice.
