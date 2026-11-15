# esp32

Firmware para un **TTGO T-Display** (ESP32 + pantalla LCD ST7789 de 1.14", 240x135) que convierte la placa en un mini dashboard de escritorio: reloj, clima, precios de cripto, una frase del día y una galería de imágenes, navegable con los dos botones físicos de la placa.

## Hardware

- **TTGO T-Display** — ESP32 con pantalla LCD ST7789 integrada de 1.14" (240x135px) y 2 botones físicos (GPIO0 y GPIO35).
- **Kit de electrónica genérico** (protoboard 830 puntos, módulo de alimentación, resistencias, LEDs, buzzers, pulsadores, cables jumper) — usado para los experimentos de hardware externo descritos abajo.

## Qué hace

El dashboard tiene **6 pantallas**, que rotan solas cada 8 segundos o se navegan manual con los botones (izquierda = anterior, derecha = siguiente):

1. **Reloj** — hora y fecha sincronizadas por NTP.
2. **Clima** — temperatura, descripción y viento (API de [Open-Meteo](https://open-meteo.com/)).
3. **Bitcoin** — precio en USD y variación de 24h (verde/rojo), con el símbolo ₿ dibujado como grilla de puntos.
4. **Ethereum** — igual que BTC, con el logo de ETH en el mismo estilo.
5. **Frase del día** — una frase en español de una lista curada local, que cambia una vez por día.
6. **Imagen** — una imagen convertida a bitmap monocromático (técnica halftone/dot-grid).

Precios de cripto vía [CoinGecko](https://www.coingecko.com/en/api). Todas las conexiones HTTPS verifican el certificado real del servidor (sin `setInsecure()`).

## Experimentos con protoboard

Además del dashboard, el firmware incluye una prueba de hardware externo conectado por protoboard (sin soldar, solo con pines de header insertados a presión y cables macho-macho enrollados a mano):

- **Buzzer activo (GPIO33)** — toggle automático cada 1s, sin botón. El cableado todavía no se confirmó funcionando.
- El sensor de humedad (Grove, por GPIO36) se probó pero se sacó del firmware — el pipeline de lectura funcionaba, pero el método de conexión sin soldar daba contacto intermitente. Queda pendiente para cuando se solucione el cableado (soldando o con cables dupont hembra-macho reales).
- **LED en 3V3/GND directo** — sin pasar por ningún GPIO ni código: dos cables macho-macho llevan el 3V3 (fila 52) y el GND (fila 62) de la columna J hacia otra zona libre del protoboard, y ahí el LED queda permanentemente encendido. Confirmado funcionando.
- Se intentó controlar ese LED con un botón externo de 4 patas (vía GPIO, con lógica en el firmware) para encenderlo solo al presionar — **no se logró hacer funcionar** y se descartó por ahora. El botón en sí nunca se confirmó cerrando el circuito (puede ser el componente, el cableado, o la combinación de patas usada).

Esta prueba convive con el dashboard en el mismo firmware pero no está integrada a ninguna pantalla ni a ningún GPIO — es solo cableado.

### Pinout físico en la protoboard (este TTGO T-Display)

La placa va montada sobre 2 rieles de pines macho (10 posiciones c/u) clavados en las columnas **B** e **I**, filas 52-63. Los nombres impresos en la placa, de la fila 52 a la 63:

| Fila | Columna I | Columna B |
|------|-----------|-----------|
| 52   | 3V3       | GND       |
| 53   | 36        | 8         |
| 54   | 37        | 21        |
| 55   | 38        | 22        |
| 56   | 39        | 17        |
| 57   | 32        | 2         |
| 58   | 33 (BUZZER_PIN) | 15  |
| 59   | 25        | 13        |
| 60   | 26        | 12        |
| 61   | 27        | GND       |
| 62   | GND       | GND       |
| 63   | 5V        | 3V3       |

Los botones físicos de la placa (GPIO0 y GPIO35) no aparecen en esta lista porque no están en los headers — vienen integrados en el PCB. GPIO12 y GPIO2 (fila 60 y 57) son pines de "strapping" (afectan el modo de arranque si algo externo los fuerza a un nivel durante el reset) — evitar colgarles algo que imponga un nivel fijo al encender la placa.

**Importante:** el cuerpo del ESP32 queda montado encima de las columnas intermedias (C, D, E de un lado; F, G, H del otro), tapándolas físicamente. Los únicos huecos libres en cada fila para cablear algo (LED, jumper, etc.) son la **columna A** (lado de la columna B) y la **columna J** (lado de la columna I).

**Lección aprendida:** varias sesiones de debugging con contactos intermitentes (a veces prendía, a veces no, en los mismos puntos) resultaron ser porque el ESP32 no estaba bien apretado contra el protoboard, no un problema de cableado ni de columnas tapadas. Si algo que debería funcionar da resultados inconsistentes, lo primero a revisar es presionar la placa firme contra el protoboard.

## Cómo correrlo

El proyecto usa [PlatformIO](https://platformio.org/) dentro de un entorno virtual de Python manejado con [`uv`](https://github.com/astral-sh/uv), para no instalar nada a nivel de sistema.

```bash
uv venv .venv
uv pip install --python .venv/bin/python platformio
```

Copiá la plantilla de configuración y completala con tus datos reales:

```bash
cp src/secrets.h.example src/secrets.h
# editar src/secrets.h: SSID/password de WiFi y coordenadas de tu ciudad
```

Compilar y subir (con la placa conectada por USB):

```bash
.venv/bin/pio run -t upload --upload-port /dev/ttyACM0
```

> **Nota (Linux):** si no tenés permisos sobre el puerto serie, agregá tu usuario al grupo correspondiente (`uucp` en Arch, `dialout` en Debian/Ubuntu) y reiniciá sesión: `sudo usermod -aG uucp $USER`.

## Estructura

```
platformio.ini        # config de PlatformIO: placa, pines de la pantalla, librerias
src/
  main.cpp             # firmware principal: pantallas, fetch de datos, botones
  secrets.h            # WiFi + ubicacion reales (gitignored, no se sube)
  secrets.h.example     # plantilla de secrets.h
  certs.h               # certificados raiz TLS embebidos (Let's Encrypt, Google Trust Services)
  quotes_es.h            # frases curadas en espanol para la pantalla de "Frase del dia"
  wings_image.h           # imagen de la galeria, como bitmap 1bpp
  btc_image.h             # logo de Bitcoin, generado proceduralmente como grilla de puntos
  eth_image.h             # logo de Ethereum, generado desde una imagen por el mismo proceso
```

## Notas técnicas

- **Logos e imágenes en puntos**: en vez de guardar una imagen a color o en escala de grises completa, los logos de BTC/ETH y la imagen de la galería se convierten a un patrón de puntos (halftone) de 1 bit por pixel — mucho más liviano en flash, y con una estética consistente entre todas las imágenes de la pantalla.
- **Frases sin tildes**: la fuente por defecto de la librería de la pantalla (TFT_eSPI) no soporta tildes ni "ñ", así que el texto de las frases se guarda sin acentos. La versión correcta de cada frase queda documentada en un comentario en `quotes_es.h`.
- **Certificados TLS embebidos**: en vez de desactivar la verificación de certificados (inseguro), el firmware trae embebidos los certificados raíz reales de las autoridades que usan las APIs consultadas, para validar la conexión correctamente.
