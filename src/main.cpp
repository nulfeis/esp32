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
#include "secrets.h"

// Naranjo de Bitcoin #F7931A en RGB565
const uint16_t BTC_ORANGE = 0xF483;

// Chile: UTC-3 en horario de verano (vigente nov-ene)
const long GMT_OFFSET_SEC = -3 * 3600;
const int DAYLIGHT_OFFSET_SEC = 0;

const char *WEATHER_URL =
    "https://api.open-meteo.com/v1/forecast?latitude=0.0000&longitude=0.0000&current_weather=true&timezone=America%2FSantiago";
const char *BTC_URL = "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd";
const char *QUOTE_URL = "https://zenquotes.io/api/today";

const int BTN_LEFT = 0;
const int BTN_RIGHT = 35;
const unsigned long DEBOUNCE_MS = 200;
const unsigned long AUTO_ROTATE_MS = 8000;

enum Screen { SCR_CLOCK = 0, SCR_WEATHER, SCR_BTC, SCR_QUOTE, SCR_IMAGE, SCREEN_COUNT };

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

float g_btcUsd = NAN;
unsigned long g_lastBtcFetch = 0;
bool g_btcOk = false;

String g_quote = "";
String g_author = "";
unsigned long g_lastQuoteFetch = 0;
bool g_quoteOk = false;

const unsigned long FETCH_INTERVAL_MS = 10UL * 60UL * 1000UL; // 10 min
const unsigned long RETRY_MS = 30UL * 1000UL;                 // 30 seg si falla

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

bool fetchBtc() {
    JsonDocument doc;
    if (!httpGetJson(BTC_URL, GTS_ROOT_R4, doc)) return false;
    g_btcUsd = doc["bitcoin"]["usd"] | NAN;
    Serial.printf("BTC: %.0f USD\n", g_btcUsd);
    return true;
}

bool fetchQuote() {
    JsonDocument doc;
    if (!httpGetJson(QUOTE_URL, ISRG_ROOT_X1, doc)) return false;
    g_quote = doc[0]["q"] | "";
    g_author = doc[0]["a"] | "";
    Serial.printf("Frase: %s - %s\n", g_quote.c_str(), g_author.c_str());
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
            tft.setTextSize(4);
            tft.setCursor(10, 40);
            tft.println(timeStr);
            tft.setTextSize(2);
            tft.setCursor(10, 90);
            tft.println(dateStr);
            break;
        }
        case SCR_WEATHER: {
            tft.setTextSize(2);
            tft.setCursor(10, 10);
            tft.println("Clima Tu Ciudad");
            tft.setTextSize(3);
            tft.setCursor(10, 50);
            if (!isnan(g_temp)) tft.printf("%.1f C\n", g_temp);
            else tft.println("-- C");
            tft.setTextSize(2);
            tft.setCursor(10, 85);
            tft.println(weatherCodeToText(g_weatherCode));
            tft.setCursor(10, 110);
            if (!isnan(g_wind)) tft.printf("Viento: %.1f km/h", g_wind);
            break;
        }
        case SCR_BTC: {
            tft.drawBitmap(5, 10, BTC_LOGO_BITMAP, BTC_LOGO_W, BTC_LOGO_H, BTC_ORANGE);

            tft.setTextSize(2);
            tft.setCursor(130, 30);
            tft.println("BTC");
            tft.setTextSize(2);
            tft.setCursor(130, 60);
            tft.setTextColor(BTC_ORANGE, TFT_BLACK);
            if (!isnan(g_btcUsd)) tft.printf("$%.0f", g_btcUsd);
            else tft.println("Cargando...");
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            break;
        }
        case SCR_QUOTE: {
            tft.setTextSize(2);
            tft.setCursor(10, 10);
            tft.println("Frase del dia");
            tft.setTextSize(1);
            if (g_quote.length()) {
                drawWrapped(g_quote, 10, 40, 220, 12);
                tft.setCursor(10, 115);
                tft.print("- ");
                tft.println(g_author);
            } else {
                tft.setCursor(10, 40);
                tft.println("Cargando...");
            }
            break;
        }
        case SCR_IMAGE: {
            tft.drawBitmap(0, 0, WINGS_BITMAP, WINGS_W, WINGS_H, TFT_WHITE);
            break;
        }
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(BTN_LEFT, INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);

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
    g_btcOk = fetchBtc();
    g_lastBtcFetch = millis();
    g_quoteOk = fetchQuote();
    g_lastQuoteFetch = millis();

    g_lastAutoRotate = millis();
    render();
}

void loop() {
    unsigned long now = millis();

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

    unsigned long bInterval = g_btcOk ? FETCH_INTERVAL_MS : RETRY_MS;
    if (now - g_lastBtcFetch >= bInterval) {
        g_btcOk = fetchBtc();
        g_lastBtcFetch = now;
        if (g_screen == SCR_BTC) g_dirty = true;
    }

    unsigned long qInterval = g_quoteOk ? FETCH_INTERVAL_MS : RETRY_MS;
    if (now - g_lastQuoteFetch >= qInterval) {
        g_quoteOk = fetchQuote();
        g_lastQuoteFetch = now;
        if (g_screen == SCR_QUOTE) g_dirty = true;
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
