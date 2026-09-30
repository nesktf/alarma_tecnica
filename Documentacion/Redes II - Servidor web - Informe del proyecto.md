# **Sistema de Alarma Distribuido para Colegio**

---

**Informe de proyecto · Cátedra Redes II**  
*Versión simplificada: sin base de datos. Los eventos se guardan como logs compactos y codificados en la memoria flash del NodeMCU, y los más antiguos se sobrescriben para dar lugar a los nuevos. Todos los valores numéricos son supuestos de diseño a validar con mediciones (ver 4.11).* 

## Estado actual del repositorio

Este repositorio ya incorpora la base funcional para la etapa de visualización y validación del firmware:

- El firmware del NodeMCU sirve archivos estáticos desde LittleFS y expone `GET /api/state`.
- El simulador local reusa la misma interfaz y el mismo contrato JSON para prueba del dashboard y validación HTTP.
- La API de controles `POST /api/controls` existe exclusivamente en el simulador y no es parte del firmware.
- El pinout actual documentado para NodeMCU es TRIG `D6/GPIO12` y ECHO `D5/GPIO14`.
- La coordinación de documentación se mantiene en `README.md`, `AGENTS.md`, `redes-ii-vault/Arquitectura/...` y `redes-ii-vault/funcionalidades-pendientes.md`.
- La detección de alarma sigue pendiente de especificación formal: el sensor HC-SR04 mide distancia, no movimiento, por lo que la política de activación debe definirse antes de implementar lógica de alarma.

Este documento conserva la visión de proyecto y la fundamentación académica, pero el código actual del repositorio deja en claro que el estado operativo relevante es la medición, la telemetría y la validación del contrato web, no la detección automática de intrusos.

## ---

**Contenido**

> 1. Presentación general del proyecto, alcance y objetivos  
> 2. Bases matemáticas del funcionamiento y la estructura del sistema  
> 3. Hardware seleccionado y sus características  
> 4. Diseño de la arquitectura del sistema

## ---

**1\. Presentación general del proyecto, alcance y objetivos**

### ---

### **1.1 Descripción general**

El proyecto consiste en instalar en un colegio un sistema de alarma diseñado por la cátedra de Redes II. Lo desarrolla un equipo dividido en subgrupos, cada uno con responsabilidades asignadas, y se organiza en tres niveles:

| Nivel | Equipo | Función   |
| :---- | :---- | :---- |
| **Nodos de área** | Arduino Nano \+ módulo RS485 \+ sensor PIR (y, según el área, pulsadores y LED) en caja estanca | Detectan movimiento y emergencias en puntos clave del colegio y responden a las consultas del nodo central |
| **Nodo central** | Arduino Nano | Recibe el ingreso por teclado, consulta cíclicamente a los nodos de área por RS485, acciona timbre y semáforo y reporta al servidor web |
| **Nodo de visualización** | NodeMCU (ESP8266) | Servidor web de solo visualización; guarda los eventos en su memoria flash y enciende el panel LED de áreas |

Cada área tiene un código numérico, de modo que ante una activación se sabe de qué área proviene la señal.  
**Lógica de funcionamiento.** El sistema opera con señales supervisadas en nivel alto (lógica a prueba de fallas). La alarma se dispara cuando:

> * baja la tensión de alimentación;  
> * se consulta un área con el sistema armado y esta informa movimiento; o  
> * se presiona un pulsador de emergencia.

Al dispararse, la interfaz web muestra de inmediato el área de activación.  
**Secuencia de estados (semáforo del nodo central):**

| Color | Estado | Descripción   |
| :---- | :---- | :---- |
| Verde | Alarma desactivada | Sistema en reposo |
| Amarillo | Armado | Parpadeo y sonido durante 1 minuto (temporización de armado) |
| Rojo | Alarma activada | Timbre activo y área(s) de activación resaltada(s) en la interfaz |

### **1.2 Objetivo general**

Diseñar e implementar un sistema de alarma de arquitectura distribuida para un colegio, que detecte intrusiones y emergencias en múltiples áreas, las notifique en tiempo real en una interfaz web de solo visualización y conserve un registro compacto de los eventos sin depender de una PC ni de servicios externos.

### **1.3 Objetivos específicos**

> 1. **Detección.** Detectar intrusiones y emergencias en múltiples áreas mediante sensores PIR y pulsadores, con consulta cíclica por RS485 y un código por área.  
> 2. **Visualización en tiempo real.** Notificar los eventos a una interfaz web de solo visualización con forma de mapa simplificado del colegio.  
> 3. **Registro persistente y protegido.** Guardar los eventos de forma persistente y cifrada, sin depender de una PC encendida 24/7, como logs compactos codificados en la memoria flash del NodeMCU. La lectura y escritura se planifican para cuidar la vida útil de las celdas de la flash a largo plazo.  
> 4. **Visualización de logs.** Consultar los logs desde un panel con filtros.  
> 5. **Seguridad.** Red WiFi con SSID protegido y acceso cifrado al servidor mediante contraseña, con protección contra ataques que puedan dejar caído el servicio u obligar a reiniciarlo (cooldown ante peticiones abusivas).  
> 6. **Continuidad.** Garantizar la operación ante fallos de energía, red o hardware.

### **1.4 Objetivos secundarios**

> * **Mapa de referencia.** Reconstruir el mapa original de la escuela para que sirva de base de la interfaz de áreas.  
> * **Sistema de usuarios.** Permitir saber qué eventos ocurrieron durante la vigilancia de cada responsable. No estaba planificado en el alcance original, por lo que queda en segundo plano. Requiere definir una ventana de tiempo razonable por jornada de vigilancia, para que los eventos registrados pasado un tiempo X no se sigan atribuyendo a la misma persona (ver 2.8).  
> * **Iluminación de áreas.** Iluminar el o los espacios donde se detectó una activación, tanto en el mapa web como en el panel LED físico.

### **1.5 Alcance**

**Incluye**

> * Nodos de área en cajas estancas, nodo central y bus RS485 con consulta cíclica.  
> * NodeMCU como servidor web de solo visualización (HTTP y Server-Sent Events) dentro de la red local del colegio.  
> * Registro compacto y circular de eventos en la flash del NodeMCU (LittleFS), con diccionario de códigos, CRC16 y cifrado.  
> * Dos interfaces web a construir: **mapa de áreas** y **visor de logs**.  
> * Panel LED de áreas manejado por un decodificador.  
> * Seguridad de red y de acceso, watchdog y recuperación ante fallos.

**Fuera de alcance en esta etapa (simplificación del proyecto)**

> * Cualquier base de datos: SQLite, PostgreSQL/Supabase o SQLCipher. Tampoco hay script de persistencia en PC ni sincronización con la nube.  
> * Caché de datos en el navegador (IndexedDB) y modo offline.  
> * Acceso remoto desde fuera de la red del colegio.  
> * Notificaciones externas (push, Telegram, WhatsApp), llamadas a servicios de emergencia y tarjeta SD.  
> * Comandos desde la web: la interfaz es de solo lectura y no puede armar, desarmar ni silenciar la alarma.

**Áreas contempladas**

| Código | Área | Nodo | Estado   |
| :---- | :---- | :---- | :---- |
| 0 | Sistema (eventos generales) | — | — |
| 1 | Área Operativa | Nano central (local) | Actual |
| 2 | Automotores | Nano remoto (RS485) | Actual |
| 3 | Jefatura de Taller | Nano remoto (RS485) | Actual |
| 4 | Vicedirección | Nano remoto (RS485) | Actual |
| 5 | Informática | Nano remoto (RS485) | Actual |
| 6 | Biblioteca | Nano remoto (RS485) | Proyectada |
| 7 | Preceptoría 1 | Nano remoto (RS485) | Proyectada |
| 8 | Preceptoría 2 | Nano remoto (RS485) | Proyectada |
| 9 | Preceptoría 3 | Nano remoto (RS485) | Proyectada |

Los códigos de área son una propuesta de codificación. La caja estanca de Jefatura de Taller, que figura también entre las cajas proyectadas, incluye LED de emergencia, LED de portero, LED de llamada a Dirección, pulsador de emergencia y pulsador de luz de emergencia.

### **1.6 Interfaces a construir**

| Interfaz | Propósito | Descripción resumida   |
| :---- | :---- | :---- |
| Mapa de áreas | Ver el estado de cada área en tiempo real | Mapa simplificado del colegio con las áreas resaltadas según su estado (ver 4.8) |
| Visor de logs | Consultar el historial guardado en el NodeMCU | Tabla con filtros por severidad, categoría, área y fecha (ver 4.8) |

### 

### 

### 

### 

### 

### 

### 

### **1.7 Entregables**

| Componente |
| :---- |
| Firmware del Arduino Nano central |
| Firmware del Arduino Nano de cada área |
| Cajas estancas con sensores |
| Firmware del NodeMCU (servidor web, recepción serie, log compacto, panel LED) |
| Interfaz de mapa de áreas |
| Interfaz de visor de logs y decodificador binario en JavaScript |
| Seguridad (WPA2, autenticación, cooldown) |

### **1.8 Etapas de implementación (duración estimada)**

| Etapa | Entregable |
| :---- | :---- |
| 1 | Prototipo de comunicación Nano ↔ NodeMCU (trama serie con CRC16) |
| 2 | Servidor web básico en red segura (WPA2, IP fija) |
| 3 | Eventos en tiempo real (SSE) y mapa de áreas |
| 4 | Formato binario y anillos de log en LittleFS |
| 5 | Visor de logs, decodificador JavaScript y filtros |
| 6 | Seguridad: autenticación, cooldown y límites |
| 7 | Panel LED, usuarios/turnos y mapa fiel al plano |
| 8 | Pruebas de estrés y ajustes |

### 

### **1.9 Supuestos y puntos a confirmar**

> 1. **Placa.** Se asume NodeMCU con ESP8266, según el informe de objetivos. El diagrama original muestra una placa tipo ESP32; el diseño usa LittleFS y ESPAsyncWebServer, disponibles en ambas.  
> 2. **Cifrado.** Se cumple el objetivo "persistente y cifrada" con AES-128 en modo CTR sobre los registros (ver 4.7). La clave queda en la misma flash: protege contra la lectura casual, no contra acceso físico al módulo. Confirmar si alcanza con la codificación compacta.  
> 3. **Protocolos y códigos.** Las tramas RS485 y serie, la velocidad del bus (9600 baud) y los códigos de área y de evento son propuestas a acordar con el subgrupo de hardware.  
> 4. **Temporización de 1 minuto.** El parpadeo con sonido del estado amarillo se modela como demora de armado. Confirmar si también rige como demora de entrada antes de disparar el timbre.  
> 5. **Área sin respuesta.** Con el sistema armado, un timeout sostenido se trata como sabotaje y dispara la alarma. Confirmar.  
> 6. **Panel LED.** El rótulo original del panel omite Vicedirección; se asume un LED por área actual (5).  
> 7. **Parámetros numéricos.** Los valores de la sección 2 (ciclos de borrado, tasas de eventos, umbrales, tamaños de anillo, jornada máxima) son supuestos de diseño a validar con mediciones.

## ---

**2\. Bases matemáticas del funcionamiento y la estructura del sistema**

---

Esta sección justifica con cálculos los parámetros que se usan en el resto del informe: la lógica de disparo, la medición de tensión, los tiempos del bus RS485, la integridad de las tramas, el tamaño del registro, la vida útil de la flash, la atribución de eventos a responsables, los recursos del NodeMCU y la autonomía eléctrica.

### **2.1 Supervisión en nivel alto y función de disparo**

Cada línea supervisada x\_i vale 1 en reposo y sano, y 0 ante una anomalía (pulsación, corte de cable, cortocircuito a masa o pérdida de alimentación). Con esta convención una falla del cableado produce el mismo efecto que una activación: por eso la lógica es "a prueba de fallas".  
Sean, para cada área a:

> * M\_a: movimiento confirmado en el área a.  
> * E\_a: pulsador de emergencia presionado en el área a.  
> * T\_a: área a sin respuesta de forma sostenida (ver 2.4).  
> * ARM: sistema en estado armado (ver 4.4).  
> * V\_bus: tensión de alimentación, con umbral V\_disp (ver 2.2).

La función de disparo es:

`ALARMA = ARM · ( OR_a M_a  +  OR_a T_a )  +  OR_a E_a  +  ( V_bus < V_disp )`

Es decir:

> * El movimiento y el sabotaje por falta de respuesta solo disparan con el sistema armado.  
> * Los pulsadores de emergencia y la baja tensión disparan siempre, esté armado o no.  
> * El disparo es "latcheado": las áreas activadas permanecen resaltadas hasta que se desarma el sistema.

**Pulsadores.** Se usa contacto normalmente cerrado (NC) a \+5 V con resistencia de pull-down a masa en la entrada. En reposo la entrada está en alto; al presionar o cortarse el cable, cae a bajo.  
**PIR.** El módulo entrega nivel alto al detectar movimiento, por lo que su salida no es a prueba de fallas por sí sola. Si el sensor perdiera su alimentación, quedaría en bajo (sin detección) sin generar alarma. La supervisión se resuelve a nivel de sistema: si el Nano de la caja pierde alimentación, deja de responder y se dispara la condición T\_a (ver 2.4). La falla del sensor PIR con el Nano funcionando es un riesgo residual (ver 4.11).

### **2.2 Medición de tensión y umbrales**

La tensión de alimentación (nominal 12 V) se mide en el Nano central con un divisor resistivo hacia la entrada A0, con referencia ADC de 5 V y 10 bits.

`R1 = 10 kΩ (al bus)      R2 = 4,7 kΩ (a masa)`  
`k  = R2 / (R1 + R2) = 0,3197`  
`V_pin = k · V_bus`  
`N_adc = V_pin / 5 V · 1023`  
`Resolución sobre el bus = (5 V / 1023) / k ≈ 15,3 mV por cuenta`

| Magnitud | V\_bus | V\_pin | Cuentas ADC   |
| :---- | :---- | :---- | :---- |
| Máxima (cargador en flote) | 14,4 V | 4,60 V | 942 |
| Umbral de **aviso** (caída) | 11,8 V | 3,77 V | 772 |
| Restablecimiento de aviso | 12,3 V | 3,93 V | 805 |
| Umbral de **disparo** V\_disp | 11,0 V | 3,52 V | 720 |
| Restablecimiento de disparo | 11,5 V | 3,68 V | 752 |

Criterios de la medición:

> * **Filtrado.** Se toma una muestra cada 10 ms y se promedian 16 muestras (160 ms).  
> * **Persistencia.** La condición debe mantenerse t\_p \= 3 s bajo el umbral para descartar transitorios, por ejemplo el pico de arranque del timbre.  
> * **Histéresis.** Los umbrales de restablecimiento están 0,5 V por encima de los de disparo para evitar oscilación.  
> * **Incertidumbre.** Como el ADC usa Vcc como referencia, una tolerancia de ±3 % en Vcc produce ±0,33 V de error a 11 V. Por eso se calibra una vez contra un multímetro y se guarda el factor en la EEPROM. Además, aviso y disparo están separados 0,8 V.  
> * **Corriente del divisor.** 12 V / 14,7 kΩ \= 0,82 mA. Se agrega un capacitor de 100 nF en A0; la impedancia de fuente es 3,2 kΩ, menor que los 10 kΩ recomendados para el ADC del ATmega328P.

### **2.3 Ciclo de consulta del bus RS485**

Parámetros: 9600 baud, formato 8N1 (10 bits por byte).

`t_byte  = 10 / 9600 = 1,042 ms`  
`Consulta   L_q = 5 bytes  →  5,21 ms`  
`Respuesta  L_r = 6 bytes  →  6,25 ms`  
`t_ta (inversión de sentido, central + nodo) = 3 ms`  
`t_guard (silencio entre tramas) = 2 ms`

`T_nodo = (L_q + L_r)·t_byte + t_ta + t_guard = 11,46 + 3 + 2 ≈ 16,5 ms`  
`T_c    = N · T_nodo            (tiempo de barrido de N nodos)`

Se planifica un período de consulta fijo P\_c \= 250 ms, que deja tiempo libre para el teclado, el ADC y el reporte al NodeMCU.

| Nodos remotos N | T\_c | Ocupación del bus (T\_c / P\_c)   |
| :---- | :---- | :---- |
| 4 (situación actual) | 66 ms | 26 % |
| 8 (proyectada, áreas 2 a 9\) | 132 ms | 53 % |

**Latencia de detección.** Para evitar falsos eventos, un evento se confirma cuando aparece en dos respuestas consecutivas (ver 2.5). En el peor caso:

`t_det,max = 2 · P_c + T_nodo ≈ 516 ms`  
`t_det,prom ≈ 1,5 · P_c ≈ 375 ms`

A esto se suman el reporte serie (trama de 11 bytes ≈ 11,5 ms), unos 50 ms de procesamiento y envío SSE en el NodeMCU y unos 100 ms de red y navegador. El total es de unos 0,7 s en el peor caso, dentro del objetivo de **menos de 1 s** desde el evento hasta la pantalla.  
**Retención en el nodo de área (latch).** Un pulsador se mantiene presionado unos 100 ms y el tiempo de espera del PIR es ajustable, ambos comparables o menores que P\_c. Por eso cada nodo captura el evento por interrupción y lo retiene (bit "latcheado") hasta que el central lo reconoce en la consulta siguiente. Sin este latch se podrían perder activaciones cortas.  
**Capacidad del bus.** El transceptor MAX485 admite 32 cargas unitarias, lo que cubre las 8 áreas remotas con amplio margen. A 9600 baud el alcance teórico ronda los 1200 m, muy por encima de las distancias esperables en un colegio (supuesto: menos de 150 m).

### **2.4 Detección de área sin respuesta**

Cada intercambio tiene un tiempo de espera t\_to \= 20 ms (la respuesta normal llega unos 9 ms después del fin de la consulta) y un reintento inmediato. Un ciclo se considera fallido si ambos intentos fallan. Un área se declara sin respuesta tras k \= 5 ciclos fallidos consecutivos:

`t_sab = k · P_c = 5 · 250 ms = 1,25 s`

**Probabilidad de falso sabotaje.** Sea p la probabilidad de pérdida por intento, con errores independientes:

`P(ciclo fallido)            = p²`  
`P(k ciclos consecutivos)    = p^(2k)`  
`Ciclos por día              = 86 400 / 0,25 = 345 600`

Con p \= 10⁻³ (entorno con ruido, por ejemplo el taller de automotores) el valor es despreciable. Incluso con p \= 0,1 (canal muy degradado): (0,01)^5 \= 10⁻¹⁰ por ciclo, es decir unas 3,5 · 10⁻⁵ falsas alarmas por día por nodo. Con 8 nodos equivale a un falso sabotaje cada 10 años aproximadamente. El ruido real es a ráfagas y no independiente, por lo que k queda como parámetro configurable a ajustar en pruebas.  
Con el sistema desarmado, un área sin respuesta genera solo un aviso de comunicación (ver 4.7); armado, se trata como sabotaje.

### **2.5 Integridad de tramas y registros (CRC16)**

Se usa CRC-16/CCITT-FALSE (polinomio 0x1021, valor inicial 0xFFFF) en las tramas RS485, en las tramas serie hacia el NodeMCU y en cada registro de log. Propiedades:

> * Detecta todos los errores de 1 y 2 bits en tramas de menos de 32 767 bits.  
> * Detecta todo número impar de bits erróneos, porque el polinomio es divisible por x \+ 1\.  
> * Detecta todas las ráfagas de hasta 16 bits.  
> * Ante corrupción aleatoria, la probabilidad de no detección es 2⁻¹⁶ \= 1,5 · 10⁻⁵.

**Falsos eventos por corrupción no detectada.** Con N \= 8 hay 16 tramas por ciclo, es decir 345 600 · 16 ≈ 5,5 · 10⁶ tramas por día. Si se supone una probabilidad de trama corrupta de 10⁻⁴, resultan unas 553 tramas corruptas por día, de las cuales 553 / 65 536 ≈ 0,008 pasarían el CRC (una cada 119 días). Con la confirmación por dos respuestas consecutivas, la segunda trama también tendría que corromperse sin ser detectada y con el mismo contenido, lo que baja la probabilidad a un orden de 10⁻¹¹ por día.

### **2.6 Formato y tamaño del registro**

Cada evento se guarda como un registro binario de **16 bytes** (todos los campos multibyte en big-endian). El detalle campo por campo está en 4.7.

`Tamaño del registro         b = 16 B`  
`Contador absoluto idx       32 bits → 2³² registros`  
`A 320 registros/día         2³² / 320 = 1,3 · 10⁷ días (no se agota)`  
`Marca de tiempo             32 bits, segundos Unix UTC (válida hasta 2106)`

Frente a una línea de texto tipo 2026-09-29 14:03:11;ALARMA;Movimiento;Informática (unos 55 a 60 bytes), el registro binario ocupa entre 3,5 y 4 veces menos. Esa relación es la que permite retener meses de historial en pocos cientos de KB.  
Tasas de eventos supuestas (a validar):

| Anillo | Contenido | Tasa R   |
| :---- | :---- | :---- |
| Crítico | Alarmas, armado/desarmado, accesos por teclado | 20 registros/día |
| General | Comunicación, energía, red, diagnóstico y resto | 300 registros/día |
| **Total** |  | **320 registros/día ≈ 5,1 KB/día** |

### **2.7 Anillos de log, retención y vida útil de la flash**

Se usan dos anillos independientes, de modo que el ruido del anillo general nunca sobrescribe el historial de alarmas. Cada anillo es una cola circular de archivos de segmento en LittleFS (ver 4.7). Un segmento contiene una cabecera de 16 B y 496 registros, es decir 7 952 B, lo que cabe en un bloque lógico.

| Anillo | Segmentos | Registros S | Tamaño   |
| :---- | :---- | :---- | :---- |
| Crítico | 16 | 7 936 | ≈ 124 KB |
| General | 48 | 23 808 | ≈ 372 KB |
| **Total** | 64 | 31 744 | **≈ 497 KB** |

Se reserva una partición LittleFS de 2 MB en los 4 MB de flash del NodeMCU. Los anillos ocupan un cuarto de esa partición, y el resto queda para la interfaz web (unos 100 KB comprimidos) y el margen que LittleFS necesita para compactar metadatos.  
**Retención** (t \= S / R):

`Crítico:  7 936 / 20  = 397 días  (≈ 13 meses)`  
`General: 23 808 / 300 =  79 días  (≈ 2,6 meses)`

**Vida útil por ciclos de borrado.** Se supone una flash NOR de 4 KB por sector y C\_max \= 10⁵ ciclos por sector. Se toma el peor caso: cada escritura a disco ("flush") provoca el borrado de un sector. LittleFS reparte el desgaste entre los sectores libres; se estima un conjunto rotativo de P \= 450 sectores:

`Vida útil (días) = C_max · P / F        con F = flushes por día`

| Escenario | F (flushes/día) | Vida útil   |
| :---- | :---- | :---- |
| **A.** Puntero de escritura en EEPROM emulada (un sector fijo, un borrado por evento) | 320 | 100 000 / 320 \= 312 días (**0,86 años**) |
| **B.** LittleFS, un flush por evento, sin lote | 320 | ≈ 140 600 días (≈ 385 años) |
| **C.** LittleFS con lote de 8 registros (diseño adoptado): 20 críticos inmediatos \+ 300/8 | ≈ 58 | ≈ 776 000 días (≈ 2 100 años) |
| **D.** Fuente defectuosa sin limitador (1 evento por segundo) | 10 800 \+ 58 | ≈ 4 100 días (≈ 11 años) |
| **E.** Fuente defectuosa con agregación (1 registro por minuto por origen) | 180 \+ 58 | ≈ 189 000 días (≈ 518 años) |

Conclusiones:

> * El escenario A muestra el error a evitar: guardar el puntero de escritura en un sector fijo agotaría la flash en menos de un año. Por eso el diseño **no guarda punteros**. La posición de escritura se reconstruye al arrancar leyendo el último segmento (ver 4.7).  
> * En el diseño adoptado la vida útil no la limitan los ciclos de borrado sino la retención de datos de la flash (del orden de 10 a 20 años según la hoja de datos). La planificación de escritura sirve sobre todo para **reducir los borrados** y con ellos los bloqueos de CPU. Un borrado de sector tarda típicamente 45 ms y hasta 400 ms, tiempo en que el ESP8266 no puede atender la red.  
> * El escenario E muestra el efecto de la agregación de repeticiones: 1 440 registros por día por origen defectuoso. Aun así, el anillo general retendría 23 808 / (300 \+ 1 440\) ≈ 14 días, y el anillo crítico no se ve afectado.

**Costo de la ventana de pérdida.** Con el lote, un corte de energía puede perder como máximo los eventos INFO/AVISO de los últimos 60 s. Los eventos de severidad ALARMA se escriben de inmediato.

### **2.8 Jornada de vigilancia y atribución de eventos**

Sea {(t\_j, u\_j)} la secuencia ordenada de ingresos válidos por teclado (armado o desarmado, eventos 0x10 a 0x12), donde u\_j es el identificador de usuario. La atribución se calcula al consultar los logs, sin guardar el responsable en cada registro. Para un evento e en el instante t\_e, se busca el último ingreso j con t\_j ≤ t\_e y se aplican dos reglas según el estado en que quedó el sistema:

`Estado ARMADO tras el ingreso j:`  
    `responsable(e) = u_j       hasta el siguiente desarmado (sin límite de tiempo)`

`Estado DESARMADO tras el ingreso j (persona presente):`  
    `responsable(e) = u_j       si t_e < min( t_{j+1}, t_j + T_max )`  
    `responsable(e) = ninguno   en caso contrario`

> * **Jornada máxima.** T\_max \= 12 h (supuesto). Pasado ese tiempo con el sistema desarmado, los eventos dejan de atribuirse a la misma persona, que es lo que pide el objetivo secundario.  
> * **Sistema armado sin límite.** Mientras el sistema está armado no hay nadie presente, y una noche o un fin de semana (más de 60 h) superan cualquier T\_max razonable. Atribuir esos eventos a quien armó es lo correcto: es la persona que dejó el sistema en vigilancia.  
> * **Datos necesarios.** Los eventos de armado/desarmado llevan el usuario en dato1 y se guardan en el anillo crítico, que tiene retención de 13 meses (ver 2.7). Así los eventos de alarma siempre encuentran su ingreso de referencia.  
> * **Identificación.** Cada usuario tiene un PIN en la EEPROM del Nano central (hasta 8 usuarios, ids 1 a 8). El id 0 significa "sin usuario".

### **2.9 Recursos del NodeMCU y protección ante peticiones**

**Memoria (heap).** Se estima un heap libre de unos 35 KB con Wi-Fi y servidor activos. Presupuesto de conexiones simultáneas:

| Uso | Cantidad | Costo unitario | Subtotal   |
| :---- | :---- | :---- | :---- |
| Clientes SSE | 3 | 4 KB | 12 KB |
| Descarga de logs (un bloque en curso) | 1 | 6 KB | 6 KB |
| Peticiones cortas | 2 | 2 KB | 4 KB |
| Reserva mínima libre | — | — | 10 KB |
| **Total** |  |  | **32 KB de 35 KB** |

Se limita a 6 conexiones TCP totales. Si el heap libre cae de 12 KB, se rechazan conexiones nuevas con HTTP 503\.  
**Cooldown por IP (token bucket).**

`Capacidad C = 10 tokens        Recarga r = 2 tokens/s`  
`Costo: petición corta = 1 token; bloque de log = 5 tokens`  
`Sin tokens → HTTP 429 con Retry-After`  
`Tabla de 16 IP (LRU): 16 × 15 B = 240 B de RAM`

**Bloqueo por autenticación fallida.** Tras n \= 5 fallos consecutivos desde una IP:

`T_bloqueo = min( 60 · 2^(n − 5), 900 ) s`

Además hay un límite global de 30 autenticaciones fallidas por hora, que activa un bloqueo general de 15 minutos.  
**Fuerza bruta.** Con bloqueo de 15 minutos tras cada 5 intentos, un atacante logra como máximo 480 intentos por día por IP:

> * Una clave de 4 dígitos (10⁴ combinaciones) cedería en unos 21 días, por lo que no es aceptable.  
> * Una clave de 10 caracteres alfanuméricos (62¹⁰ ≈ 8,4 · 10¹⁷) es inviable de atacar por este medio.

Por eso se exige una clave web de al menos 10 caracteres.

### **2.10 Balance energético y autonomía**

Consumos supuestos (lado 5 V, en reposo):

| Carga | Corriente a 5 V | Potencia   |
| :---- | :---- | :---- |
| Nano central \+ MAX485 \+ PIR \+ teclado | 45 mA | 0,23 W |
| Semáforo (un LED encendido) | 15 mA | 0,08 W |
| 4 nodos de área actuales (45 mA c/u) | 180 mA | 0,90 W |
| NodeMCU (promedio; picos de 250 mA al transmitir) | 80 mA | 0,40 W |
| RTC \+ decodificador \+ panel LED | 20 mA | 0,10 W |
| **Total lado 5 V** | **340 mA** | **1,70 W** |

Con convertidores reductores de eficiencia media 85 %, la potencia de entrada es 1,70 / 0,85 \= 2,0 W, es decir unos **170 mA a 12 V**. Con las 8 áreas remotas proyectadas se llega a unos 255 mA. El timbre (150 mA a 12 V) suma 1,8 W solo mientras suena.  
**Autonomía con batería de 12 V y 7 Ah** (capacidad utilizable del 70 %, es decir 4,9 Ah):

`Situación actual:   4,9 Ah / 0,17 A  ≈ 29 h`  
`Situación proyectada: 4,9 Ah / 0,255 A ≈ 19 h`

**Caída de tensión en el cableado.** Se usa cable UTP Cat5e, con unos 0,084 Ω/m por conductor. Se emplean dos conductores en paralelo para \+12 V y dos para masa, es decir 0,042 Ω/m por rama. Para un tendido de 100 m:

`R_lazo = 2 · 0,042 · 100 = 8,4 Ω`  
`Cada caja consume ≈ 21 mA a 12 V (con reductor local de 90 %)`  
`4 cajas al final del tendido: I = 84 mA → caída = 8,4 · 0,084 = 0,71 V`  
`8 cajas al final del tendido: I = 168 mA → caída = 1,4 V (V_caja ≈ 10,6 V)`

En todos los casos la tensión en la caja queda muy por encima del mínimo del reductor local (unos 4,5 V), por lo que se distribuye 12 V y se reduce localmente a 5 V.

## ---

**3\. Hardware seleccionado y sus características**

### ---

**3.1 Criterios de selección**

> * **Disponibilidad y costo:** placas de uso común en la cátedra y fáciles de reponer.  
> * **Suficiencia:** los cálculos de la sección 2 muestran que el procesamiento, la memoria y la velocidad de bus necesarios son modestos.  
> * **Robustez del cableado:** bus diferencial RS485 en lugar de líneas de nivel lógico largas.  
> * **Independencia:** el nodo central funciona sin el NodeMCU ni la red (ver 4.10).

### 

### 

### **3.2 Equipamiento por nivel**

| Nivel | Equipamiento   |
| :---- | :---- |
| **Nodo de área (caja estanca IP65)** | Arduino Nano, módulo MAX485, sensor PIR HC-SR501, reductor 12 → 5 V, resistencia de terminación de 120 Ω (solo en el último nodo), llave DIP de 3 posiciones y, según el área, pulsador de emergencia y LED |
| **Nodo central** | Arduino Nano, módulo MAX485, sensor PIR, pulsador de emergencia, teclado matricial 4×4, semáforo LED, timbre con driver MOSFET, divisor de medición de tensión |
| **Nodo de visualización** | NodeMCU (ESP8266), conversor de nivel lógico, RTC DS3231, decodificador 74HC138 y panel de 5 LED |
| **Alimentación** | Fuente de 12 V / 3 A, batería de plomo-ácido de 12 V / 7 Ah con cargador en flote, reductor LM2596, fusible de 2 A |

### 

### **3.3 Especificaciones de los componentes**

| Componente | Características principales | Justificación de uso   |
| :---- | :---- | :---- |
| **Arduino Nano** | ATmega328P a 16 MHz, 5 V, 32 KB de flash, 2 KB de SRAM, 1 KB de EEPROM, ADC de 10 bits, 1 UART por hardware, entrada VIN recomendada de 7 a 12 V | Lee sensores por interrupción, atiende el bus y la máquina de estados con holgura (ocupación del bus del 26 al 53 %) |
| **NodeMCU (ESP8266, ESP-12E)** | 80 MHz (hasta 160), 4 MB de flash SPI, unos 80 KB de RAM de datos, Wi-Fi 802.11 b/g/n de 2,4 GHz con WPA2, lógica de 3,3 V (no tolera 5 V), picos de 250 mA al transmitir | Servidor web, SSE y logs cifrados en LittleFS. El diagrama original muestra una placa ESP32; el diseño usa ESP8266 y funciona igual en ambas (ver 4.11) |
| **Módulo MAX485** | Transceptor RS485 half-duplex, 5 V, hasta 32 cargas unitarias, pines RO, DI y DE/RE | Bus diferencial para tramos largos con ruido, como el taller |
| **Sensor PIR HC-SR501** | 4,5 a 20 V, consumo en reposo menor a 50 µA, salida de 3,3 V, alcance de 3 a 7 m, ángulo menor a 120°, tiempo de retardo ajustable de 0,3 a 200 s, calentamiento de aproximadamente 1 min | Detección de movimiento en cada área |
| **Pulsador de emergencia** | Tipo hongo de 22 mm, contacto NC | Contacto NC para supervisión a prueba de fallas (ver 2.1) |
| **Teclado matricial 4×4** | Membrana, 8 líneas (4 filas y 4 columnas) | Ingreso de PIN para armar y desarmar |
| **Semáforo LED** | Módulo de 3 LED (rojo, amarillo, verde) para 5 V con resistencias serie | Indicación de estado del sistema (ver 1.1) |
| **Timbre** | Timbre o sirena de 12 V, unos 150 mA, con MOSFET de nivel lógico (tipo IRLZ44N), resistencia de gate de 220 Ω, pull-down de 10 kΩ y diodo de rueda libre | El pull-down evita que el timbre suene durante el arranque del Nano |
| **Conversor de nivel** | Bidireccional de 4 canales (BSS138), 3,3 V / 5 V | Enlace serie entre el Nano (5 V) y el NodeMCU (3,3 V); se usan 2 canales |
| **Decodificador 74HC138** | 3 a 8 líneas, Vcc de 2 a 6 V (se alimenta con 3,3 V), salidas activas en bajo | Maneja el panel LED con 3 líneas de dirección y una de habilitación |
| **Panel LED** | 5 LED de alta eficiencia (rojo, ámbar o verde, Vf ≤ 2,2 V), resistencias de 120 Ω | Indicación física del área activada (ver 4.6) |
| **RTC DS3231** | I²C, deriva de ±2 ppm (aproximadamente ±1 min por año), batería CR2032 | El NodeMCU no tiene reloj propio y los logs necesitan fecha y hora (ver 4.11) |
| **Fuente y batería** | 12 V / 3 A, batería 12 V / 7 Ah, reductor LM2596 de 3 A para el nodo central y minirreductor MP1584 de 1 A en cada caja | Ver el balance de 2.10 |
| **Cableado** | UTP Cat5e: par 1 para A y B del RS485, par 2 para \+12 V, par 3 para masa, par 4 de reserva | Conductores en paralelo para reducir la caída de tensión (ver 2.10) |
| **Caja estanca** | IP65, prensacables PG9 | Protección contra polvo y humedad, con ventana para el PIR |

### **3.4 Consideraciones de instalación del bus RS485**

> * **Terminación.** 120 Ω entre A y B en el nodo central y en el último nodo del bus.  
> * **Polarización (bias).** Resistencias de 680 Ω en el nodo central, A hacia \+5 V y B hacia masa, para que el bus en reposo tenga un nivel definido. Coherente con la lógica en nivel alto, un cable cortado no genera ruido aleatorio sino un estado predecible.  
> * **Topología.** Bus lineal con derivaciones cortas (menos de 30 cm) hacia cada caja.  
> * **Direcciones.** La llave DIP de 3 posiciones de cada nodo define su dirección: dirección \= 2 \+ valor\_DIP, con valores de 0 a 7, que cubren las áreas 2 a 9\.

### **3.5 Lista de materiales (situación actual)**

| Ítem | Cantidad   |
| :---- | :---- |
| Arduino Nano | 5 (1 central y 4 de área) |
| Módulo MAX485 | 5 |
| Sensor PIR HC-SR501 | 5 |
| Pulsador de emergencia | 3 (central, Jefatura de Taller y Vicedirección) |
| Cajas estancas con minirreductor | 4 |
| NodeMCU con conversor de nivel, RTC, 74HC138 y 5 LED | 1 |
| Teclado 4×4, semáforo, timbre con driver | 1 de cada uno |
| Fuente, batería, cargador y reductor LM2596 | 1 de cada uno |

Cada área proyectada (6 a 9\) agrega un kit de nodo de área: Nano, MAX485, PIR, minirreductor, caja estanca y llave DIP.

## ---

**4\. Diseño de la arquitectura del sistema**

### ---

**4.1 Visión general**

  `+-------------------------------------------------------------+`  
  `|                   Nodos de área (RS485)                     |`  
  `|  Automotores (Nano + PIR)                                   |`  
  `|  Jefatura de Taller (Nano + PIR + pulsador)                |`  
  `|  Vicedirección (Nano + PIR + pulsador)                     |`  
  `|  Informática (Nano + PIR)                                  |`  
  `+------------------------------+------------------------------+`  
                                 `|`  
                          `Bus RS485 (9600 baud)`  
                                 `|`  
  `+------------------------------v------------------------------+`  
  `|                        Nodo Central                         |`  
  `|  Arduino Nano + PIR + pulsador + teclado + semáforo + timbre|`  
  `+------------------------------+------------------------------+`  
                                 `|`  
                     `Serie 9600 + CRC16`  
                                 `|`  
  `+------------------------------v------------------------------+`  
  `|                   NodeMCU (ESP8266)                         |`  
  `|             Servidor web + logs en LittleFS                 |`  
  `+--------------+-------------------------------+--------------+`  
                 `|                               |`  
          `Decodificador 74HC138             Wi-Fi WPA2`  
                 `|                               |`  
         `Panel LED (5 áreas)              Router Colegio`  
                                                 `|`  
                                            `HTTP + SSE`  
                                                 `|`  
                                           `Navegadores`  
                                      `(Mapa y Visor de logs)`

Principios de diseño:

> * **El nodo central decide.** La alarma (timbre, semáforo, armado) funciona por completo en el Nano central, sin depender del NodeMCU ni de la red.  
> * **El NodeMCU solo observa.** Recibe los eventos, los registra y los muestra. No puede armar, desarmar ni silenciar la alarma.  
> * **Consulta cíclica con confirmación.** Los nodos de área son pasivos y solo responden cuando se los consulta.

### **4.2 Conexionado propuesto**

**Nano central**

| Pin | Función   |
| :---- | :---- |
| D0 / D1 | UART por hardware hacia el NodeMCU, a través del conversor de nivel |
| D2 | Pulsador de emergencia (INT0) |
| D3 | PIR del Área Operativa (INT1) |
| D4 | DE/RE del MAX485 |
| D8 / D9 | Bus RS485 (AltSoftSerial: RX en D8 conectado a RO, TX en D9 conectado a DI) |
| D5, D6, D7, D13 | Filas del teclado |
| D11, D12, A1, A2 | Columnas del teclado |
| D10 | Timbre (gate del MOSFET) |
| A0 | Medición de V\_bus |
| A3, A4, A5 | Semáforo (verde, amarillo, rojo) |

El bus RS485 usa AltSoftSerial (basada en Timer1) para no competir con la UART de hardware, que atiende el enlace al NodeMCU. Los pines D0 y D1 son compartidos con el puerto USB: para programar el Nano se desconecta el enlace hacia el NodeMCU.  
**Nano de área**

| Pin | Función   |
| :---- | :---- |
| D2 | PIR (INT0) |
| D3 | Pulsador de emergencia, si el área lo tiene (INT1) |
| D4 | DE/RE del MAX485 |
| D8 / D9 | Bus RS485 (AltSoftSerial) |
| D5, D6, D7 | LED de estado y LED opcionales según el área |
| D10, D11, D12 | Llave DIP de dirección (INPUT\_PULLUP) |

En Jefatura de Taller, los LED de emergencia, portero y llamada a Dirección, y el pulsador de luz de emergencia, se gestionan localmente en el Nano de la caja. Solo el pulsador de emergencia se reporta por el bus (a confirmar).  
**NodeMCU**

| Pin | Función   |
| :---- | :---- |
| GPIO3 (RX) / GPIO1 (TX) | UART0 hacia el Nano central, a través del conversor de nivel |
| GPIO4 (D2) / GPIO5 (D1) | I²C hacia el RTC DS3231 |
| GPIO14 (D5), GPIO12 (D6), GPIO13 (D7) | Entradas A, B y C del 74HC138 |
| GPIO15 (D8) | Habilitación del decodificador (G1) |
| GPIO2 (D4) | TX de Serial1, salida de depuración |
| GPIO16 (D0) | LED de latido del NodeMCU |

GPIO15 debe estar en bajo durante el arranque del ESP8266. Con la habilitación del decodificador conectada a ese pin, los LED quedan apagados al arrancar, lo cual es coherente con esa restricción. Al usar la UART0 sin remapear, los mensajes de arranque a 74880 baud llegan al Nano y se descartan por CRC. Para programar por USB se desconecta el enlace con el Nano.

### **4.3 Protocolo del bus RS485**

El central es el único maestro. Se define una trama de consulta de 5 bytes y una de respuesta de 6 bytes (los valores usados en 2.3).  
**Consulta (central → nodo)**

| Byte | Campo | Descripción   |
| :---- | :---- | :---- |
| 0 | SOF | 0x7E |
| 1 | ADDR | Dirección del nodo (2 a 9\) |
| 2 | CTRL | bit0: sistema armado; bit1: reconocer (limpiar latches); bit2: encender LED de emergencia |
| 3–4 | CRC16 | Sobre los bytes 0 a 2 |

**Respuesta (nodo → central)**

| Byte | Campo | Descripción   |
| :---- | :---- | :---- |
| 0 | SOF | 0x7E |
| 1 | ADDR | Dirección del nodo |
| 2 | ESTADO | bit0: movimiento latcheado; bit1: emergencia latcheada; bit2: movimiento instantáneo; bit3: emergencia instantánea; bit7: el nodo se reinició |
| 3 | SEQ | Contador de eventos desde el arranque del nodo |
| 4–5 | CRC16 | Sobre los bytes 0 a 3 |

**Reglas del central**

> 1. Consulta a los nodos configurados uno por uno cada P\_c \= 250 ms, con un reintento inmediato si no responde en t\_to \= 20 ms.  
> 2. Confirma un evento cuando lo ve en dos respuestas consecutivas (ver 2.5). El cambio del contador SEQ permite detectar un evento repetido aunque se pierda un reconocimiento.  
> 3. Tras confirmar, envía CTRL.bit1 \= 1 en la consulta siguiente para que el nodo limpie sus latches.  
> 4. Tras k \= 5 ciclos fallidos consecutivos, declara el área sin respuesta (ver 2.4).  
> 5. Si un nodo responde con el bit7 activo, lo registra como reinicio de nodo (evento 0x44).

### **4.4 Máquina de estados del nodo central**

| Estado | Semáforo | Salidas | Transiciones   |
| :---- | :---- | :---- | :---- |
| DESARMADO | Verde fijo | Timbre apagado | PIN válido \+ \# → ARMANDO. Pulsador de emergencia o baja tensión → ALARMA |
| ARMANDO | Amarillo intermitente con pitido | Temporización de 60 s. Se ignora el PIR para permitir la salida; siguen activos el pulsador y la tensión | 60 s → ARMADO. PIN válido → DESARMADO. Pulsador o baja tensión → ALARMA |
| ARMADO | Amarillo fijo | Vigilancia completa | Movimiento confirmado, sabotaje, pulsador o baja tensión → ALARMA. PIN válido → DESARMADO |
| ALARMA | Rojo fijo | Timbre activo (máximo T\_timbre \= 5 min) y áreas activadas latcheadas | PIN válido → DESARMADO |

Reglas adicionales:

> * **Demora de entrada.** El parámetro T\_entrada (demora antes de disparar el timbre al detectar movimiento estando armado) queda en 0 s por defecto, a confirmar (ver 4.11).  
> * **Teclado.** El PIN se ingresa seguido de \#; \* borra lo ingresado. Tras 3 códigos incorrectos seguidos el teclado se bloquea 60 s y se registra el evento.  
> * **Persistencia de estado.** El estado armado se guarda en la EEPROM, en un anillo de 8 posiciones que rota en cada cambio, para que un reinicio del Nano no deje el sistema desarmado sin aviso. Con unos 20 cambios por día, el anillo de 8 posiciones supera holgadamente los 100 000 ciclos por celda.

### **4.5 Enlace serie Nano central ↔ NodeMCU**

Enlace a 9600 baud, 8N1, con CRC16 (ver 2.5).  
**Formato de trama**

| Byte | Campo   |
| :---- | :---- |
| 0 | SOF 0xA5 |
| 1 | LEN (longitud del payload) |
| 2 | TIPO |
| 3 | SEQ (número de trama del emisor) |
| 4… | PAYLOAD |
| último 2 | CRC16 sobre todos los bytes anteriores |

**Tipos de trama**

| Tipo | Sentido | Payload | Uso   |
| :---- | :---- | :---- | :---- |
| 0x01 | EVENTO (Nano → NodeMCU) | área (1 B), código (1 B), dato1 (1 B), dato2 (2 B) | Cada evento; la trama completa mide 11 B (11,5 ms) |
| 0x02 | ESTADO (Nano → NodeMCU) | estado central (1 B), máscara de áreas activas (2 B), máscara de áreas sin respuesta (2 B), tensión en décimas de V (1 B), usuario (1 B) | Instantánea cada 1 s y ante cada cambio; mide 13 B (ocupa el 1,4 % del enlace) |
| 0x81 | ACK (NodeMCU → Nano) | — (el SEQ confirma la trama recibida) | Reconocimiento; mide 6 B |

**Confiabilidad**

> * El Nano mantiene una cola de 8 eventos (40 B de RAM). Reenvía un evento hasta 3 veces si no recibe el ACK en 200 ms.  
> * Si el NodeMCU no está disponible, la cola descarta los eventos más antiguos. Al restablecerse el enlace, el Nano envía un evento 0x43 con la cantidad de eventos perdidos en dato2.  
> * La instantánea de estado cada 1 s permite al NodeMCU corregir su visión aunque se pierda algún evento, de modo que el mapa siempre converge al estado real.

### **4.6 Firmware del NodeMCU**

Ejecución cooperativa, sin bloqueos largos y con el watchdog alimentado en cada vuelta del bucle principal.

| Módulo | Función | Disparo   |
| :---- | :---- | :---- |
| Recepción serie | Ensambla tramas, valida CRC16, envía ACK | Cada vuelta del bucle |
| Bus de eventos | Descarta duplicados, agrega repeticiones (ventana de 60 s por área y código), decide el anillo destino y la marca de tiempo | Por evento recibido |
| Registro (LogStore) | Acumula el lote y lo escribe en el segmento actual | Ante evento de severidad ALARMA; lote lleno (8); o 60 s desde el primer registro pendiente |
| Servidor web | HTTP y SSE con ESPAsyncWebServer | Por petición |
| Difusión SSE | Envía el estado a los clientes conectados | Ante cambio; latido cada 15 s |
| Panel LED | Enciende el o los LED de las áreas activas | Temporizador de 2 ms |
| Salud | Controla heap, Wi-Fi y RTC | Cada 1 s |

**Panel LED con múltiples áreas.** Un decodificador 74HC138 activa una sola salida a la vez, pero pueden estar activas varias áreas. Por eso se recorren las áreas activas por multiplexado con un temporizador de 2 ms por posición:

`N_act = cantidad de áreas activas (máximo 5)`  
`Actualización de cada LED = 1 / (N_act · 2 ms)  →  100 Hz con 5 activas (sin parpadeo visible)`  
`Ciclo de trabajo por LED = 1 / N_act            →  20 % con 5 activas`  
`Corriente pico = (3,3 V − 2,0 V) / 120 Ω ≈ 10 mA  →  promedio de 2 mA con 5 activas`

Con una sola área activa el LED queda fijo (100 %), sin multiplexado. La corriente pico de 10 mA debe validarse contra los límites del 74HC138 (ver 4.11).  
**Hora.** La hora se lee del RTC DS3231. Si el RTC no está sincronizado, los registros se marcan con el bit "hora no confiable" (ver 4.7). La sincronización se realiza por NTP cuando la red lo permite, o por comando serie durante mantenimiento, ya que la interfaz web es de solo lectura.

### **4.7 Registro compacto, diccionario de códigos y cifrado**

**Estructura del registro (16 bytes, big-endian)**

| Bytes | Campo | Cifrado | Descripción   |
| :---- | :---- | :---- | :---- |
| 0–3 | idx | No | Contador absoluto de registro (también es el contador de CTR) |
| 4–7 | ts | Sí | Segundos Unix UTC |
| 8 | sev \+ cat | Sí | bits 7–5: severidad; bits 4–0: categoría |
| 9 | area \+ flags | Sí | bits 7–4: área (0 a 9); bits 3–0: flags |
| 10 | code | Sí | Código de evento |
| 11 | dato1 | Sí | Dato de 1 byte |
| 12–13 | dato2 | Sí | Dato de 2 bytes |
| 14–15 | crc | No | CRC16 sobre los bytes 0 a 13 tal como se guardan |

**Severidad:** 0 \= INFO, 1 \= AVISO, 2 \= ALARMA.  
**Categorías:** 0 \= SISTEMA, 1 \= ALARMA, 2 \= ACCESO, 3 \= COMUNICACIÓN, 4 \= ENERGÍA, 5 \= RED, 6 \= DIAGNÓSTICO.  
**Flags:** bit0 \= registro agregado (dato2 cuenta repeticiones); bit1 \= generado por el NodeMCU (no recibido del Nano); bit2 \= hora no confiable; bit3 \= reservado.  
**Diccionario de eventos (propuesta)**

| Código | Evento | Cat. | Sev. | Área | dato1 | dato2   |
| :---- | :---- | :---- | :---- | :---- | :---- | :---- |
| 0x01 | Arranque del NodeMCU | SISTEMA | INFO | 0 | causa del reinicio | — |
| 0x02 | Arranque del nodo central | SISTEMA | AVISO | 1 | causa del reinicio | — |
| 0x03 | Reinicio por watchdog | DIAGNÓSTICO | AVISO | 0 o 1 | origen (0 NodeMCU, 1 Nano) | — |
| 0x10 | Armado iniciado | ACCESO | INFO | 1 | usuario | — |
| 0x11 | Sistema armado | SISTEMA | INFO | 1 | usuario | — |
| 0x12 | Sistema desarmado | ACCESO | INFO | 1 | usuario | — |
| 0x20 | Movimiento con sistema armado | ALARMA | ALARMA | área | — | — |
| 0x21 | Pulsador de emergencia | ALARMA | ALARMA | área | — | — |
| 0x22 | Tensión baja (disparo) | ENERGÍA | ALARMA | 0 | — | tensión (cV) |
| 0x23 | Área sin respuesta con sistema armado (sabotaje) | ALARMA | ALARMA | área | — | — |
| 0x24 | Alarma finalizada | ALARMA | INFO | 1 | usuario | duración (s) |
| 0x30 | Código incorrecto en teclado | ACCESO | AVISO | 1 | n.º de intento | — |
| 0x31 | Teclado bloqueado | ACCESO | AVISO | 1 | — | segundos de bloqueo |
| 0x40 | Área sin respuesta con sistema desarmado | COMUNICACIÓN | AVISO | área | — | — |
| 0x41 | Área recuperada | COMUNICACIÓN | INFO | área | — | — |
| 0x42 | Errores CRC en el bus | COMUNICACIÓN | INFO | área | — | cantidad |
| 0x43 | Resincronización Nano ↔ NodeMCU | COMUNICACIÓN | AVISO | 0 | — | eventos perdidos |
| 0x44 | Reinicio de nodo de área | COMUNICACIÓN | AVISO | área | — | — |
| 0x50 | Tensión baja (aviso) | ENERGÍA | AVISO | 0 | — | tensión (cV) |
| 0x51 | Tensión restablecida | ENERGÍA | INFO | 0 | — | tensión (cV) |
| 0x60 | Autenticación web fallida | RED | AVISO | 0 | último octeto de la IP | — |
| 0x61 | Bloqueo por cooldown | RED | AVISO | 0 | último octeto de la IP | segundos |
| 0x62 | Acceso web correcto | RED | INFO | 0 | último octeto de la IP | — |
| 0x70 | Wi-Fi perdido | RED | AVISO | 0 | — | — |
| 0x71 | Wi-Fi recuperado | RED | INFO | 0 | — | duración de la caída (s) |
| 0x72 | Memoria libre baja | DIAGNÓSTICO | AVISO | 0 | — | heap libre (KB) |
| 0x73 | Segmento de log corrupto descartado | DIAGNÓSTICO | AVISO | 0 | anillo | n.º de segmento |
| 0x74 | Repeticiones agregadas | DIAGNÓSTICO | INFO | área | código repetido | cantidad |
| 0x75 | Hora ajustada | SISTEMA | INFO | 0 | — | — |

**Anillo de destino.** Va al anillo crítico todo evento de severidad ALARMA o de categoría ALARMA o ACCESO. Esto incluye armado y desarmado, necesarios para la atribución de 2.8. El resto va al anillo general.  
**Organización en LittleFS**

> * Cada anillo es un conjunto de archivos de segmento /l/C\_\<idx\>.bin (crítico) y /l/G\_\<idx\>.bin (general), donde \<idx\> es el índice del primer registro en hexadecimal.  
> * Cada segmento tiene una cabecera de 16 B (marca AL, versión, id de anillo, generación, índice inicial y CRC16) seguida de hasta 496 registros.  
> * Los segmentos solo se escriben al final del archivo (append). Al completarse un segmento se crea el siguiente; si el anillo supera su cantidad de segmentos, se borra el archivo más antiguo.  
> * **No se guardan punteros.** Al arrancar se lista el directorio, se toma el segmento con mayor idx, se trunca a un múltiplo de 16 B y se valida el último registro con su CRC. Así se evita el escenario A de 2.7.

**Cifrado AES-128 en modo CTR**

`bloque_contador = nonce (12 B) || idx (4 B, big-endian)`  
`keystream       = AES-128_K( bloque_contador )`  
`bytes 4 a 13    = texto_plano XOR keystream[0..9]`  
`nonce           = id del chip (8 B) || generación del anillo (4 B)`

> * Cada registro consume un solo bloque AES. La generación se incrementa cada vez que se reformatea un anillo, para no reutilizar nunca el mismo keystream con el mismo idx.  
> * La clave de 128 bits se guarda en la misma flash. Protege contra la lectura casual del contenido, no contra el acceso físico al módulo (ver 4.11).  
> * El descifrado se realiza **en el NodeMCU** al servir los logs, después de la autenticación. El navegador recibe registros en texto plano dentro de la sesión autenticada. Esto se debe a que el acceso local por HTTP no ofrece la API criptográfica del navegador y a que no se quiere exponer la clave al cliente.  
> * Al servir, el NodeMCU recalcula el CRC16 sobre el texto plano de cada registro, para que el navegador pueda verificar la integridad de la transmisión. Los registros con CRC almacenado inválido se envían con un flag de corrupto y se registran (evento 0x73 si un segmento completo resulta inválido).

### **4.8 Interfaces web**

**Servicios expuestos** (todos requieren autenticación; solo se admite el método GET)

| Ruta | Contenido   |
| :---- | :---- |
| / | Mapa de áreas (HTML comprimido) |
| /logs | Visor de logs (HTML comprimido) |
| /dict.json | Diccionario de códigos, categorías y áreas |
| /api/state | Instantánea del estado actual en JSON |
| /events | Flujo de Server-Sent Events con estado y alarmas |
| /api/log?ring=C|G\&from=\<idx\>\&n=\<cantidad\> | Bloque de registros descifrados en binario |
| /api/health | Tiempo de actividad, heap libre y señal Wi-Fi |

**Mapa de áreas.** Un SVG simplificado del colegio con un polígono por área (id="area-N").

> * **Estados por color:** reposo (verde tenue), movimiento detectado (amarillo), alarma (rojo parpadeante), sin respuesta (gris rayado).  
> * **Encabezado con semáforo:** estado del sistema (desarmado, armando, armado, alarma) y tensión de alimentación.  
> * **Lista de los últimos eventos:** se alimenta por SSE.  
> * **Reconexión:** al reconectarse, el navegador recibe primero la instantánea completa. El servidor envía un latido cada 15 s para detectar clientes caídos.

Ejemplo de mensaje SSE:

`event: state`  
`data: {"e":3,"act":[3],"nr":[],"v":12.4,"u":2,"t":1790720000}`

**Visor de logs.** Tabla con las columnas fecha y hora (en zona America/Argentina/Salta), severidad, categoría, área, evento, datos y responsable (calculado según 2.8).

> * Filtros por severidad, categoría, área y rango de fechas, aplicados en el navegador sobre los bloques descargados.  
> * Carga por páginas de 256 registros (4 KB por bloque) desde el más reciente, para no exceder la memoria del NodeMCU (ver 2.9).

Decodificador JavaScript de un registro:

`function decodeRecord(dv, off) {`  
  `const sc = dv.getUint8(off + 8);`  
  `const af = dv.getUint8(off + 9);`  
  `return {`  
    `idx:  dv.getUint32(off, false),`  
    `ts:   dv.getUint32(off + 4, false),`  
    `sev:  sc >> 5,  cat: sc & 0x1F,`  
    `area: af >> 4,  flags: af & 0x0F,`  
    `code: dv.getUint8(off + 10),`  
    `d1:   dv.getUint8(off + 11),`  
    `d2:   dv.getUint16(off + 12, false),`  
    `crcOk: crc16(dv, off, 14) === dv.getUint16(off + 14, false)`  
  `};`  
`}`

### **4.9 Seguridad**

| Capa | Medida   |
| :---- | :---- |
| Red inalámbrica | El NodeMCU se conecta como estación al Wi-Fi del colegio con WPA2-PSK (CCMP) y clave de al menos 16 caracteres. Dirección IP fija o reservada por DHCP. No se confía en ocultar el SSID como medida de seguridad |
| Acceso al servidor | Autenticación HTTP Digest, de modo que la clave no viaja en claro por la red. En flash se guarda el hash HA1 y no la clave. La clave exige al menos 10 caracteres (ver 2.9) |
| Solo lectura | El servidor responde solo a GET. No existe ninguna ruta que pueda armar, desarmar o silenciar la alarma |
| Protección contra abuso | Cooldown por IP con token bucket, bloqueo progresivo por autenticaciones fallidas, límite global de fallos, tope de 6 conexiones y rechazo con 503 si el heap libre cae de 12 KB (ver 2.9) |
| Límites de entrada | URL de hasta 128 B, cabeceras de hasta 512 B, tiempo de espera de 10 s en conexiones inactivas |
| Datos en reposo | Registros cifrados con AES-128 CTR (ver 4.7) |
| Trazabilidad | Autenticaciones fallidas, bloqueos y accesos correctos quedan registrados (0x60, 0x61, 0x62) |

Limitaciones aceptadas:

> * No se usa HTTPS: el consumo de RAM y CPU del ESP8266 lo hace impracticable junto con el servidor asíncrono. La confidencialidad en el aire depende de WPA2.  
> * Las tramas RS485 y serie llevan CRC16 pero no autenticación, por lo que con acceso físico a los cables un atacante podría inyectar tramas.

### **4.10 Continuidad y recuperación ante fallos**

| Falla | Detección | Respuesta | Registro   |
| :---- | :---- | :---- | :---- |
| Baja o corte de tensión de alimentación | V\_bus bajo el umbral durante 3 s (ver 2.2) | Aviso a 11,8 V y disparo de la alarma a 11,0 V. El sistema sigue operando con batería (unas 29 h, ver 2.10) | 0x50, 0x22, 0x51 |
| Caída del router o del Wi-Fi | El NodeMCU pierde la asociación | Reconexión con espera creciente (1, 2, 4… hasta 60 s). La alarma sigue funcionando (depende solo del Nano central) y el NodeMCU sigue registrando localmente | 0x70, 0x71 |
| Cuelgue del NodeMCU | Watchdog de hardware | Reinicio automático. El Nano sigue operando, reenvía su cola y envía una resincronización | 0x01, 0x03, 0x43 |
| Cuelgue del Nano central | Watchdog de 2 s | Reinicio y recuperación del estado desde la EEPROM (ver 4.4) | 0x02, 0x03 |
| Nodo de área sin respuesta o con cable cortado | k \= 5 ciclos fallidos (ver 2.4) | Aviso si está desarmado, alarma por sabotaje si está armado | 0x40 o 0x23, y 0x41 al recuperarse |
| Bus RS485 completo caído | Todas las áreas sin respuesta a la vez | Igual que el caso anterior, para todas las áreas | 0x40 o 0x23 por área |
| Corte de energía durante una escritura de log | Registro final incompleto o CRC inválido al arrancar | Se trunca al último múltiplo de 16 B válido. Pérdida máxima de 60 s de eventos INFO/AVISO (ver 2.7) | — |
| Segmento de flash corrupto | CRC inválido en registros o cabecera | Se descarta el segmento y el anillo continúa | 0x73 |
| Heap bajo | Menos de 12 KB libres | Se rechazan conexiones nuevas hasta recuperar margen | 0x72 |
| RTC sin sincronizar | Pérdida de batería del RTC o primer arranque | Se sigue registrando con el bit "hora no confiable" | flag bit2 |

### **4.11 Supuestos y puntos a confirmar (secciones 2 a 4\)**

> 1. **RTC.** Se agrega un módulo DS3231 al NodeMCU porque este no tiene reloj propio y los logs requieren fecha y hora. Confirmar si se acepta el componente o si se prefiere sincronizar por NTP desde el router.  
> 2. **Descifrado en el servidor.** Los registros se descifran en el NodeMCU y viajan en texto plano dentro de la sesión autenticada, por la falta de HTTPS y de la API criptográfica del navegador en HTTP. Confirmar si es aceptable.  
> 3. **Bloques de la flash.** Los cálculos suponen sectores de 4 KB, 100 000 ciclos por sector y un bloque lógico de LittleFS de unos 8 KB. Validar con el núcleo y la flash reales.  
> 4. **Tasas de eventos.** Los 20 registros críticos y 300 generales por día son estimaciones. Deben ajustarse con datos de operación.  
> 5. **Consumos y batería.** Los consumos por módulo, la eficiencia del 85 % y la batería de 12 V / 7 Ah son supuestos. Medir el consumo real para validar la autonomía.  
> 6. **Umbrales de tensión.** 11,8 V y 11,0 V deben contrastarse con la curva real de la batería y con la caída de tensión observada al sonar el timbre.  
> 7. **Ruido en el bus.** El valor k \= 5 para declarar una falta de respuesta y la probabilidad de pérdida p deben validarse en el entorno real, en particular en el taller de automotores.  
> 8. **PIR sin supervisión propia.** Un PIR averiado con el Nano funcionando no generaría alarma. Evaluar un autotest periódico o el cambio por un sensor con salida supervisada.  
> 9. **Demora de entrada.** T\_entrada \= 0 s por defecto y T\_timbre \= 5 min son supuestos. Confirmar los valores con el colegio.  
> 10. **Jornada máxima.** T\_max \= 12 h y la regla de atribución en estado armado (sin límite) son propuestas a acordar (ver 2.8).  
> 11. **Corriente del panel LED.** El pico de 10 mA por salida del 74HC138 supera la corriente nominal de sus salidas. Validar con una medición o agregar transistores.  
> 12. **Códigos y protocolos.** Las tramas, los códigos de evento y de área y el formato del registro son propuestas a acordar con el subgrupo de hardware.