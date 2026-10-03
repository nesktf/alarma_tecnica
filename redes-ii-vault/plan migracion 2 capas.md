# Plan de migración a 2 capas — estado al 2026-10-03

## Objetivo
Estructurar el proyecto en dos capas principales: firmware (único que se flashea) y simulador (previsualización), con un contrato compartido que permita crecerlos en paralelo.

## Decisiones tomadas
1. Monorepo: capas `firmware/` + `simulator/` + `shared/`.
2. `lib/esp8266` permanece en la raíz; el build references con `../`.
3. Lógica de alarma portable: diferida hasta definir Prioridad 0 (umbral/histéresis).
4. Campos de demostración (`alarm_system`, `controls`, `logs`) marcados `preview_only` en el esquema (opción B, menos churn).

## Cambios aplicados
- `git mv src firmware/src`, `git mv xmake.lua firmware/xmake.lua`.
- Rutas en `firmware/xmake.lua` reenrutadas a `../lib`, `../static`, `../.env`; toolchain con rutas absolutas.
- `set_defaultplat("cross")` -> `set_defaultplat("linux")` (xmake 3.1.1 no resuelve toolchain con cross).
- Generador de `credentials.h` emite `WIFI_PSWD` + alias `WIFI_PASS`.
- Creados `shared/api_state.schema.json` y `tests/test_contract.py`.
- Docs actualizadas: `README.md`, `AGENTS.md`, `firmware/README.md`, vault.

## Verificación
- `python3 -m unittest discover -s tests`: 24 tests OK.
- `xmake f -p linux --toolchain=xtensa-lx106-elf && xmake`: build ok, RAM 37%, IRAM 91%, flash 28%.
- Simulador responde en `127.0.0.1:8080` con el nuevo esquema.

## Próximos pasos (pendientes de confirmación)
- Protocolo serie central->NodeMCU (baud rate, formato trama, campos por sector).
- Pines relay/timbre en Arduino central.
- Teclado matrix 4x3 pines.
- ¿Firmware central ya tiene lógica ALARMA implementada?
- ¿NodeMCU mantiene sensor ultrasónico HC-SR04 en D6/D5?

## Arquitectura propuesta (nueva idea del usuario)
- Arduino Nano central = maestro RS485 + lógica de disparo + relay/timbre.
- 5 sectores con Arduino Nano + PIR en caja estanca.
- Teclado 0-9 + bomberos/emergencia/policia -> central.
- NodeMCU = gateway/visor puro (UART <- central, WiFi -> cliente web).
- Central envía datos periódicamente; solo registra eventos cuando hay cambio.
- Lógica de disparo: ALARMA = ARM·(OR M_a + OR T_a) + OR E_a + (V_bus < V_disp).
