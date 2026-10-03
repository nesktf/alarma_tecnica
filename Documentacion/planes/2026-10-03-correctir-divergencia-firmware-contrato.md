# Plan: Corregir divergencia firmware/contrato

## Contexto

El simulador es el reflejo del comportamiento del NodeMCU real. La clave `alarm_system` debe ser una clave base que ambos emitan (no `preview_only`). El firmware actualmente no emite la clave `server` que el schema marca como requerida.

## Estado actual

- Schema: `alarm_system` marcado como `preview_only` (solo simulador)
- Firmware: emite `alarm_system` hardcodeado con estado inicial, pero no emite `server`
- Simulador: emite correctamente ambas claves
- Tests: 24/24 pasan

## Pasos de implementación

### 1. Schema: promover `alarm_system` a clave base

Archivo: `shared/api_state.schema.json`

- Remover `"preview_only": true` de `alarm_system`
- Agregar `"alarm_system"` a la lista `required` del objeto raíz
- Mantener `controls` y `logs` como `preview_only` (solo simulador)

### 2. Firmware: agregar `server` al JSON

Archivo: `firmware/sensor.cpp` (función `format_sensor_state`)

Agregar al final del JSON:
```json
"server":{"status":"running","host":"<IP>","port":80,"base_url":"http://<IP>","last_request":"/api/state"}
```

La IP es **estática**, definida en `.env` como `SERVER_IP="192.168.0.53"` y generada en
`firmware/build/generated/credentials.h` como `IPAddress(192, 168, 0, 53)`.

Para emitirla como string en el formatter sin asignación dinámica, se agrega una macro
`SERVER_IP_STR` en el script de generación de `credentials.h`:

```cpp
#define SERVER_IP_STR "192.168.0.53"
```

`format_sensor_state` la usa directamente con `snprintf` en el buffer estático existente.
No se llama a `WiFi.localIP()` — la IP es conocida en tiempo de compilación.

### 3. Firmware: ampliar buffer de respuesta

Archivo: `firmware/server.cpp`

Cambiar:
```cpp
char response[1024];
```
a:
```cpp
char response[1536];
```

### 4. Tests: ajustar verificación `preview_only`

Archivo: `tests/test_contract.py`

- Remover `alarm_system` del conjunto `preview_only` en `test_preview_only_keys_are_simulator_specific`
- Agregar verificación de que `alarm_system` es clave base

### 5. Documentación: actualizar referencias

**`firmware/README.md`**: `src/server.cpp` → `server.cpp`, `src/sensor.cpp` → `sensor.cpp`, `src/sensor.hpp` → `sensor.hpp`

**`AGENTS.md`**: misma corrección en arquitectura

**`redes-ii-vault/funcionalidades-pendientes.md`**:
- Marcar `[x]` clave `server` en firmware
- Aclarar `alarm_system` en firmware = estado inicial (sin sensores PIR reales aún)
- Corregir referencias a `src/`

## Orden de ejecución

```
schema → sensor.cpp (server key) → server.cpp (buffer) → tests → docs
```

Verificación después de cada paso: `python3 -m unittest discover -s tests -v` (24/24 OK)

Final: `xmake` en `firmware/` para confirmar build cruzado.

## Fecha

2026-10-03
