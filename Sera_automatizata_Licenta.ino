#include <WiFi.h>
#include <Wire.h>
#include <BH1750.h>
#include "time.h"
#include "DHT.h"
#include <ESP32Servo.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <WebServer.h>

// --- Configurare pinii și module ---
#define DHTTYPE DHT22
#define DHT_PIN_INAUNTRU 26
#define DHT_PIN_AFARA    27
#define SERVO_PIN        13
#define RELAY_PIN        33
#define SENSOR_PIN       34

#define TEMP_DIFF_THRESHOLD 5.0
#define HUMIDITY_RAIN_THRESHOLD 85.0
#define PRAG_USCAT 1500
#define LIGHT_THRESHOLD 200

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define DATA_PIN   23
#define CS_PIN     5
#define CLK_PIN    18

// --- Obiecte senzori și stări ---
DHT dhtIn(DHT_PIN_INAUNTRU, DHTTYPE);
DHT dhtOut(DHT_PIN_AFARA, DHTTYPE);
Servo acoperis;
BH1750 lightMeter;
MD_MAX72XX matrix = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN);

const char* ssid = "";
const char* password = "";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

WebServer server(80);

bool acoperisDeschis = false;
bool matriceAprinsa = false;
bool controlManual = false;

float tempIn = 0, humIn = 0, tempOut = 0, humOut = 0;
int umidSol = 0, ora = 0;
float lux = 0.0;
String statusPompa = "Oprită";
String statusAcoperis = "Închis";
String modControl = "Automat";

unsigned long previousMillis = 0;
const long interval = 5000;

// === SETUP ===
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("🔧 ESP32 pornește...");

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  matrix.begin();
  matrix.clear();
  matrix.control(MD_MAX72XX::INTENSITY, 8);

  Wire.begin();
  lightMeter.begin();
  Serial.println("📟 Senzor lumină inițializat.");

  dhtIn.begin();
  dhtOut.begin();

  acoperis.setPeriodHertz(50);
  acoperis.attach(SERVO_PIN, 500, 2400);
  acoperis.write(0);

  Serial.print("📶 Conectare la WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("✅ Conectat la WiFi!");
  Serial.print("📡 IP Local: ");
  Serial.println(WiFi.localIP());

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  server.on("/", handleRoot);
  server.on("/toggleRoof", []() {
    acoperisDeschis = !acoperisDeschis;
    acoperis.write(acoperisDeschis ? 90 : 0);
    statusAcoperis = acoperisDeschis ? "Deschis" : "Închis";
    controlManual = true;
    modControl = "Manual";
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/autoRoof", []() {
    controlManual = false;
    modControl = "Automat";
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.begin();
  Serial.println("🚀 Server web pornit!");
}

// === LOOP ===
void loop() {
  server.handleClient();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    tempIn = dhtIn.readTemperature();
    humIn = dhtIn.readHumidity();
    tempOut = dhtOut.readTemperature();
    humOut = dhtOut.readHumidity();

    if (!controlManual) {
      float diferenta = abs(tempOut - tempIn);
      if (diferenta >= TEMP_DIFF_THRESHOLD && !acoperisDeschis && humOut < HUMIDITY_RAIN_THRESHOLD) {
        acoperis.write(90);
        acoperisDeschis = true;
        statusAcoperis = "Deschis";
      } else if (diferenta < TEMP_DIFF_THRESHOLD && acoperisDeschis) {
        acoperis.write(0);
        acoperisDeschis = false;
        statusAcoperis = "Închis";
      }
    }

    umidSol = analogRead(SENSOR_PIN);
    if (umidSol > PRAG_USCAT) {
      digitalWrite(RELAY_PIN, HIGH);
      statusPompa = "Pornită";
    } else {
      digitalWrite(RELAY_PIN, LOW);
      statusPompa = "Oprită";
    }

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      ora = timeinfo.tm_hour;
    }

    lux = lightMeter.readLightLevel();
    if (ora >= 8 && ora <= 20 && lux < LIGHT_THRESHOLD) {
      aprindeMatriceaComplet();
      matriceAprinsa = true;
    } else {
      matrix.clear();
      matrix.update();
      matriceAprinsa = false;
    }
  }
}

// === INTERFAȚĂ WEB ===
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta http-equiv='refresh' content='5'/>";
  html += "<style>body{font-family:sans-serif;background:#f2f2f2;padding:2rem;}div{background:#fff;padding:1.5rem;margin:1rem 0;border-radius:10px;font-size:1.5em;}button{padding:1rem 2rem;font-size:1.5em;border:none;border-radius:10px;background:#007bff;color:white;cursor:pointer;}button:hover{background:#0056b3;}</style></head><body>";

  html += "<div><strong>📍 Interior:</strong> " + String(tempIn, 2) + "°C, " + String(humIn, 1) + "%</div>";
  html += "<div><strong>📍 Exterior:</strong> " + String(tempOut, 2) + "°C, " + String(humOut, 1) + "%</div>";
  html += "<div><strong>💡 Lumină:</strong> " + String(lux, 2) + " lx</div>";
  html += "<div><strong>🕒 Ora:</strong> " + String(ora) + "</div>";
  html += "<div><strong>🌱 Umiditate Sol:</strong> " + String(umidSol) + "</div>";
  html += "<div><strong>🪟 Acoperiș:</strong> " + statusAcoperis + "</div>";
  html += "<div><strong>💧 Pompa:</strong> " + statusPompa + "</div>";
  html += "<div><strong>🔲 Matrice LED:</strong> " + String(matriceAprinsa ? "Pornită" : "Oprită") + "</div>";

  html += "<div><strong>⚙️ Mod Control:</strong> " + modControl + "</div>";

  html += "<form action='/toggleRoof' method='get'><button type='submit'>";
  html += acoperisDeschis ? "Închide Acoperiș" : "Deschide Acoperiș";
  html += "</button></form>";

  html += "<form action='/autoRoof' method='get'><button type='submit'>Activează Control Automat</button></form>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

// === FUNCȚIE APRINDERE LED MATRICE ===
void aprindeMatriceaComplet() {
  matrix.clear();
  for (int col = 0; col < MAX_DEVICES * 8; col++) {
    for (int row = 0; row < 8; row++) {
      matrix.setPoint(row, col, true);
    }
  }
  matrix.update();
}
