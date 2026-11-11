#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <TFT_eSPI.h>
#include "certs.h"
#include "wings_image.h"
#include "btc_image.h"
#include "eth_image.h"
#include "quotes_es.h"
#include "secrets.h"

// Naranjo de Bitcoin #F7931A y morado de Ethereum #627EEA, en RGB565
const uint16_t BTC_ORANGE = 0xF483;
const uint16_t ETH_PURPLE = 0x63FD;
const uint16_t COLOR_UP = TFT_GREEN;
const uint16_t COLOR_DOWN = TFT_RED;

// Paleta "Terminal Violet": morado + plata, para titulos/datos destacados
const uint16_t PALETTE_PRIMARY = 0xB25D; // #B14AED
const uint16_t PALETTE_ACCENT = 0xCE5A;  // #C9C9D6

// Chile: UTC-3 en horario de verano (vigente nov-ene)
const long GMT_OFFSET_SEC = -3 * 3600;
const int DAYLIGHT_OFFSET_SEC = 0;

const char *WEATHER_URL =
    "https://api.open-meteo.com/v1/forecast?latitude=" WEATHER_LAT "&longitude=" WEATHER_LON
    "&current_weather=true&timezone=America%2FSantiago";
const char *CRYPTO_URL =
    "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin,ethereum&vs_currencies=usd&include_24hr_change=true";

const int BTN_LEFT = 0;
const int BTN_RIGHT = 35;
const unsigned long DEBOUNCE_MS = 200;
const unsigned long AUTO_ROTATE_MS = 8000;

const int LED_PIN = 32;
const int LED_BTN_PIN = 21;
unsigned long g_lastLedButton = 0;
bool g_ledState = false;

// GPIO36 es solo-entrada, sin pull resistors internos: ideal para el ADC del sensor.
const int MOISTURE_PIN = 36;
const unsigned long MOISTURE_READ_MS = 2000;
// Valores crudos de referencia; hay que calibrarlos con el sensor en aire seco y en agua.
const int MOISTURE_DRY_RAW = 3000;
const int MOISTURE_WET_RAW = 1200;

enum Screen { SCR_CLOCK = 0, SCR_WEATHER, SCR_BTC, SCR_ETH, SCR_QUOTE, SCR_IMAGE, SCR_MOISTURE, SCREEN_COUNT };

TFT_eSPI tft = TFT_eSPI();

int g_screen = SCR_CLOCK;
unsigned long g_lastButton = 0;
unsigned long g_lastAutoRotate = 0;
bool g_dirty = true;
int g_lastClockSecond = -1;

float g_temp = NAN, g_wind = NAN;
int g_weatherCode = -1;
unsigned long g_lastWeatherFetch = 0;
bool g_weatherOk = false;

float g_btcUsd = NAN, g_btcChange = NAN;
float g_ethUsd = NAN, g_ethChange = NAN;
unsigned long g_lastCryptoFetch = 0;
bool g_cryptoOk = false;

const unsigned long FETCH_INTERVAL_MS = 10UL * 60UL * 1000UL; // 10 min
const unsigned long RETRY_MS = 30UL * 1000UL;                 // 30 seg si falla

int g_moistureRaw = 0;
int g_moisturePercent = 0;
unsigned long g_lastMoistureRead = 0;

void readMoisture() {
    long sum = 0;
    const int samples = 8;
    for (int i = 0; i < samples; i++) {
        sum += analogRead(MOISTURE_PIN);
        delay(2);
    }
    g_moistureRaw = sum / samples;
    int pct = map(g_moistureRaw, MOISTURE_DRY_RAW, MOISTURE_WET_RAW, 0, 100);
    g_moisturePercent = constrain(pct, 0, 100);
    Serial.printf("Humedad: raw=%d pct=%d\n", g_moistureRaw, g_moisturePercent);
}

const char *weatherCodeToText(int code) {
    if (code == -1) return "Cargando...";
    if (code == 0) return "Despejado";
    if (code == 1 || code == 2) return "Parc. nublado";
    if (code == 3) return "Nublado";
    if (code == 45 || code == 48) return "Niebla";
    if (code >= 51 && code <= 57) return "Llovizna";
    if (code >= 61 && code <= 67) return "Lluvia";
    if (code >= 71 && code <= 77) return "Nieve";
    if (code >= 80 && code <= 82) return "Chubascos";
    if (code >= 95) return "Tormenta";
    return "Desconocido";
}

bool httpGetJson(const char *url, const char *rootCA, JsonDocument &doc) {
    WiFiClientSecure client;
    client.setCACert(rootCA);
    client.setHandshakeTimeout(15);

    HTTPClient http;
    http.setConnectTimeout(10000);
    http.setTimeout(10000);

    if (!http.begin(client, url)) {
        Serial.println("No se pudo iniciar request");
        return false;
    }

    int httpCode = http.GET();
    bool ok = false;
    if (httpCode == 200) {
        String payload = http.getString();
        DeserializationError err = deserializeJson(doc, payload);
        if (!err) {
            ok = true;
        } else {
            Serial.print("Error parseando JSON: ");
            Serial.println(err.c_str());
        }
    } else {
        Serial.printf("HTTP error: %d (%s)\n", httpCode, url);
    }
    http.end();
    return ok;
}

bool fetchWeather() {
    JsonDocument doc;
    if (!httpGetJson(WEATHER_URL, ISRG_ROOT_X1, doc)) return false;
    g_temp = doc["current_weather"]["temperature"] | NAN;
    g_wind = doc["current_weather"]["windspeed"] | NAN;
    g_weatherCode = doc["current_weather"]["weathercode"] | -1;
    Serial.printf("Clima: %.1fC, viento %.1f km/h, code %d\n", g_temp, g_wind, g_weatherCode);
    return true;
}

bool fetchCrypto() {
    JsonDocument doc;
    if (!httpGetJson(CRYPTO_URL, GTS_ROOT_R4, doc)) return false;
    g_btcUsd = doc["bitcoin"]["usd"] | NAN;
    g_btcChange = doc["bitcoin"]["usd_24h_change"] | NAN;
    g_ethUsd = doc["ethereum"]["usd"] | NAN;
    g_ethChange = doc["ethereum"]["usd_24h_change"] | NAN;
    Serial.printf("BTC: $%.0f (%.2f%%)  ETH: $%.0f (%.2f%%)\n", g_btcUsd, g_btcChange, g_ethUsd, g_ethChange);
    return true;
}

void drawWrapped(const String &text, int x, int y, int maxWidth, int lineHeight) {
    String line = "";
    int cursorY = y;
    int start = 0;
    while (start < (int)text.length()) {
        int spaceIdx = text.indexOf(' ', start);
        String word = (spaceIdx == -1) ? text.substring(start) : text.substring(start, spaceIdx);
        String tentative = line.length() ? (line + " " + word) : word;
        if (tft.textWidth(tentative) > maxWidth && line.length()) {
            tft.setCursor(x, cursorY);
            tft.println(line);
            cursorY += lineHeight;
            line = word;
        } else {
            line = tentative;
        }
        if (spaceIdx == -1) break;
        start = spaceIdx + 1;
    }
    if (line.length()) {
        tft.setCursor(x, cursorY);
        tft.println(line);
    }
}

void drawCryptoScreen(const uint8_t *bitmap, int bw, int bh, uint16_t color,
                       const char *label, float price, float change, int logoX = 5) {
    tft.drawBitmap(logoX, 2, bitmap, bw, bh, color);

    tft.setTextSize(2);
    tft.setTextColor(color, TFT_BLACK);
    tft.setCursor(130, 15);
    tft.println(label);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(130, 45);
    if (!isnan(price)) tft.printf("$%.0f", price);
    else tft.println("Cargando...");

    if (!isnan(change)) {
        uint16_t changeColor = (change >= 0) ? COLOR_UP : COLOR_DOWN;
        tft.setTextColor(changeColor, TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(130, 75);
        tft.printf("%s%.1f%%", (change >= 0 ? "+" : ""), change);
    }
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
}

void render() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    switch (g_screen) {
        case SCR_CLOCK: {
            struct tm timeinfo;
            char timeStr[16], dateStr[16];
            if (getLocalTime(&timeinfo)) {
                strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
                strftime(dateStr, sizeof(dateStr), "%d/%m/%Y", &timeinfo);
            } else {
                strcpy(timeStr, "--:--:--");
                strcpy(dateStr, "--/--/----");
            }
            tft.setTextColor(PALETTE_ACCENT, TFT_BLACK);
            tft.setTextSize(4);
            tft.setCursor(10, 40);
            tft.println(timeStr);
            tft.setTextColor(PALETTE_PRIMARY, TFT_BLACK);
            tft.setTextSize(2);
            tft.setCursor(10, 90);
            tft.println(dateStr);
            break;
        }
        case SCR_WEATHER: {
            tft.setTextColor(PALETTE_PRIMARY, TFT_BLACK);
            tft.setTextSize(2);
            tft.setCursor(10, 10);
            tft.println("Clima " WEATHER_CITY_LABEL);

            tft.setTextColor(PALETTE_ACCENT, TFT_BLACK);
            tft.setTextSize(3);
            tft.setCursor(10, 50);
            if (!isnan(g_temp)) tft.printf("%.1f C\n", g_temp);
            else tft.println("-- C");

            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setTextSize(2);
            tft.setCursor(10, 85);
            tft.println(weatherCodeToText(g_weatherCode));

            tft.setTextColor(PALETTE_PRIMARY, TFT_BLACK);
            tft.setCursor(10, 110);
            if (!isnan(g_wind)) tft.printf("Viento: %.1f km/h", g_wind);
            break;
        }
        case SCR_BTC:
            drawCryptoScreen(BTC_LOGO_BITMAP, BTC_LOGO_W, BTC_LOGO_H, BTC_ORANGE, "BTC", g_btcUsd, g_btcChange);
            break;
        case SCR_ETH:
            // El glifo de ETH deja mas margen vacio a los lados dentro de su propio
            // bitmap que el de BTC, asi que se dibuja mas a la derecha (+17px) para
            // que el centro visual de ambos simbolos quede alineado en pantalla.
            drawCryptoScreen(ETH_LOGO_BITMAP, ETH_LOGO_W, ETH_LOGO_H, ETH_PURPLE, "ETH", g_ethUsd, g_ethChange, 22);
            break;
        case SCR_QUOTE: {
            struct tm timeinfo;
            int dayIndex = 0;
            if (getLocalTime(&timeinfo)) {
                dayIndex = timeinfo.tm_yday % QUOTES_ES_COUNT;
            }
            const Quote &q = QUOTES_ES[dayIndex];

            tft.setTextColor(PALETTE_PRIMARY, TFT_BLACK);
            tft.setTextSize(2);
            tft.setCursor(10, 10);
            tft.println("Frase del dia");

            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setTextSize(1);
            drawWrapped(String(q.text), 10, 40, 220, 12);

            tft.setTextColor(PALETTE_ACCENT, TFT_BLACK);
            tft.setCursor(10, 115);
            tft.print("- ");
            tft.println(q.author);
            break;
        }
        case SCR_IMAGE: {
            tft.drawBitmap(0, 0, WINGS_BITMAP, WINGS_W, WINGS_H, TFT_WHITE);
            break;
        }
        case SCR_MOISTURE: {
            tft.setTextColor(PALETTE_PRIMARY, TFT_BLACK);
            tft.setTextSize(2);
            tft.setCursor(10, 10);
            tft.println("Humedad suelo");

            uint16_t pctColor;
            if (g_moisturePercent < 30) pctColor = COLOR_DOWN;
            else if (g_moisturePercent < 60) pctColor = TFT_ORANGE;
            else pctColor = COLOR_UP;

            tft.setTextColor(pctColor, TFT_BLACK);
            tft.setTextSize(4);
            tft.setCursor(10, 45);
            tft.printf("%d%%", g_moisturePercent);

            int barX = 10, barY = 95, barW = 200, barH = 16;
            tft.drawRect(barX, barY, barW, barH, TFT_WHITE);
            int fillW = (barW - 2) * g_moisturePercent / 100;
            tft.fillRect(barX + 1, barY + 1, fillW, barH - 2, pctColor);

            tft.setTextColor(PALETTE_ACCENT, TFT_BLACK);
            tft.setTextSize(1);
            tft.setCursor(10, 120);
            tft.printf("raw: %d", g_moistureRaw);
            break;
        }
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(BTN_LEFT, INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    pinMode(LED_BTN_PIN, INPUT_PULLUP);

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(0, 0);
    tft.println("Conectando WiFi...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) delay(300);
    Serial.print("WiFi OK, IP: ");
    Serial.println(WiFi.localIP());

    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo)) delay(300);
    Serial.println("Hora sincronizada");

    g_weatherOk = fetchWeather();
    g_lastWeatherFetch = millis();
    g_cryptoOk = fetchCrypto();
    g_lastCryptoFetch = millis();

    readMoisture();
    g_lastMoistureRead = millis();

    g_lastAutoRotate = millis();
    render();
}

void loop() {
    unsigned long now = millis();

    if (now - g_lastLedButton > DEBOUNCE_MS && digitalRead(LED_BTN_PIN) == LOW) {
        g_ledState = !g_ledState;
        digitalWrite(LED_PIN, g_ledState);
        g_lastLedButton = now;
    }

    // Botones: cambio manual de pantalla
    if (now - g_lastButton > DEBOUNCE_MS) {
        if (digitalRead(BTN_LEFT) == LOW) {
            g_screen = (g_screen - 1 + SCREEN_COUNT) % SCREEN_COUNT;
            g_dirty = true;
            g_lastButton = now;
            g_lastAutoRotate = now;
        } else if (digitalRead(BTN_RIGHT) == LOW) {
            g_screen = (g_screen + 1) % SCREEN_COUNT;
            g_dirty = true;
            g_lastButton = now;
            g_lastAutoRotate = now;
        }
    }

    // Rotacion automatica
    if (now - g_lastAutoRotate >= AUTO_ROTATE_MS) {
        g_screen = (g_screen + 1) % SCREEN_COUNT;
        g_dirty = true;
        g_lastAutoRotate = now;
    }

    // Refrescos periodicos de datos
    unsigned long wInterval = g_weatherOk ? FETCH_INTERVAL_MS : RETRY_MS;
    if (now - g_lastWeatherFetch >= wInterval) {
        g_weatherOk = fetchWeather();
        g_lastWeatherFetch = now;
        if (g_screen == SCR_WEATHER) g_dirty = true;
    }

    unsigned long cInterval = g_cryptoOk ? FETCH_INTERVAL_MS : RETRY_MS;
    if (now - g_lastCryptoFetch >= cInterval) {
        g_cryptoOk = fetchCrypto();
        g_lastCryptoFetch = now;
        if (g_screen == SCR_BTC || g_screen == SCR_ETH) g_dirty = true;
    }

    if (now - g_lastMoistureRead >= MOISTURE_READ_MS) {
        readMoisture();
        g_lastMoistureRead = now;
        if (g_screen == SCR_MOISTURE) g_dirty = true;
    }

    // El reloj se redibuja cada segundo mientras esta visible
    if (g_screen == SCR_CLOCK) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo) && timeinfo.tm_sec != g_lastClockSecond) {
            g_lastClockSecond = timeinfo.tm_sec;
            g_dirty = true;
        }
    }

    if (g_dirty) {
        render();
        g_dirty = false;
    }

    delay(50);
}
