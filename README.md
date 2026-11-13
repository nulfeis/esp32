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

Esta prueba convive con el dashboard en el mismo firmware pero no está integrada a ninguna pantalla.

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
