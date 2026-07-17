#include <Arduino.h>
#include "secrets.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Wire.h>
#include <RTClib.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <LiquidCrystal_I2C.h>

WiFiManager wm;
#include <HTTPClient.h>
#include <time.h>

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600; // GMT+1
const int   daylightOffset_sec = 0;
bool sntpConfigured = false;

// --- Configuration ---
// WiFi credentials are now managed dynamically by WiFiManager!

const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_topic_data = "iot-hub/redphoenix25-v1-x8f9a2/data";
const char* mqtt_topic_cmd  = "iot-hub/redphoenix25-v1-x8f9a2/cmd";

const char* firebase_url = "https://iot-energy-hub-eea5f-default-rtdb.firebaseio.com";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// RTC Module (DS3231 / DS1307)
RTC_DS3231 rtc3231;
RTC_DS1307 rtc1307;
bool rtcFound = false;
bool useDS1307 = false;

// --- Pins ---
// 4-Channel Relay Module (Digital Outputs)
const int RELAY_SOCKET_1 = 26;
const int RELAY_SOCKET_2 = 27;
const int RELAY_SOCKET_3 = 14;
const int RELAY_SOCKET_4 = 13;

// Environment Sensors
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

const int PIR_PIN = 5;       // Digital In
const int LDR_PIN = 15;      // Digital In (DO pin)

// Energy Sensors (Analog ADC1 Pins ONLY!)
// Note: ADC2 pins fail when Wi-Fi is active. We must use ADC1 (32, 33, 34, 35, 36, 39).
const int ZMPT101B_PIN = 36; // VP
const int ACS712_MAIN  = 39; // VN (Main Line Total Current)
const int ACS712_PIN1  = 34;
const int ACS712_PIN2  = 35;
const int ACS712_PIN3  = 32;
const int ACS712_PIN4  = 33;

// LCD Setup (I2C uses default SDA: 21, SCL: 22)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- State Variables ---
bool socket1State = false;
bool socket2State = false;
bool socket3State = false;
bool socket4State = false;

// Energy Tracking
double hourlyEnergyTotal = 0.0;
double hourlyEnergyS1 = 0.0;
double hourlyEnergyS2 = 0.0;
double hourlyEnergyS3 = 0.0;
double hourlyEnergyS4 = 0.0;

double dailyEnergyTotal = 0.0;
double dailyEnergyS1 = 0.0;
double dailyEnergyS2 = 0.0;
double dailyEnergyS3 = 0.0;
double dailyEnergyS4 = 0.0;

// Voltage Analytics
float hourlyVoltageSum = 0.0;
int hourlyVoltageCount = 0;
float hourlyVoltageMin = 999.0;
float hourlyVoltageMax = 0.0;

float dailyVoltageSum = 0.0;
int dailyVoltageCount = 0;
float dailyVoltageMin = 999.0;
float dailyVoltageMax = 0.0;

int currentHour = -1;
int currentDay = -1;

// Live Telemetry
float currentVoltage = 0.0;
float currentAmperage = 0.0;
float currentPower = 0.0;

// Individual Socket Telemetry
float currentS1 = 0.0;
float currentS2 = 0.0;
float currentS3 = 0.0;
float currentS4 = 0.0;

// Calibration factors
const float CAL_VOLTAGE = 0.146f;
const float CAL_CURRENT_MAIN = 0.017f;
const float CAL_CURRENT_S1 = 0.017f;
const float CAL_CURRENT_S2 = 0.017f;
const float CAL_CURRENT_S3 = 0.017f;
const float CAL_CURRENT_S4 = 0.017f;

// Raw RMS noise offsets (in ADC counts) when relays are ON
const float NOISE_RMS_MAIN = 4.0f;
const float NOISE_RMS_S1   = 5.5f;
const float NOISE_RMS_S2   = 5.5f;
const float NOISE_RMS_S3   = 6.0f;
const float NOISE_RMS_S4   = 6.0f;

// Motion detection latch and bulb stay-on timer
bool motionLatch = false;          // Set in loop() on any PIR HIGH, consumed by sendUpdate()
unsigned long bulbOnStartTime = 0;
bool automatedBulbTrigger = false;
const unsigned long BULB_TIMEOUT_MS = 15000; // Bulb stays on for 15s

// Safety Cutoff
bool voltageFault = false;
unsigned long voltageSafeStartTime = 0;

unsigned long lastEnergyCalc = 0;
unsigned long lastUpdate = 0;
unsigned long lastFirebaseLog = 0;

Preferences prefs;
float dailyEnergyLimitKwh = 0.0;
bool hasShedLoadsToday = false;

void updateRelays() {
  if (voltageFault) {
    digitalWrite(26, HIGH);
    digitalWrite(27, HIGH);
    digitalWrite(14, HIGH);
    digitalWrite(13, HIGH);
  } else {
    digitalWrite(26, socket1State ? LOW : HIGH);
    digitalWrite(27, socket2State ? LOW : HIGH);
    digitalWrite(14, socket3State ? LOW : HIGH);
    digitalWrite(13, socket4State ? LOW : HIGH);
  }
}

void runAutomation(float temp, bool motionDetected, bool ldrLight) {
  // --- Rule 1: Temperature -> Fan (Socket 2) ---
  if (!isnan(temp)) {
    if (temp >= 30.0) {
      if (!socket2State) {
        socket2State = true;
        updateRelays();
        Serial.println("Rule: Temp >= 30.0C -> Socket 2 (Fan) ON.");
      }
    } else if (temp < 26.0) {
      if (socket2State) {
        socket2State = false;
        updateRelays();
        Serial.println("Rule: Temp < 26.0C -> Socket 2 (Fan) OFF.");
      }
    }
  }

  // --- Rule 2: LDR Smart Bulb (Socket 1) ---
  if (!ldrLight) { // Dark (covered)
    if (!socket1State) {
      socket1State = true;
      Serial.println("Rule: LDR Covered (Dark) -> Socket 1 (Bulb) ON.");
      updateRelays();
    }
    bulbOnStartTime = millis(); // Constantly reset timer while covered
    automatedBulbTrigger = true;
  } else { // Light (uncovered)
    // If it was triggered automatically, wait 15 seconds to turn off
    if (automatedBulbTrigger && socket1State) {
      if (millis() - bulbOnStartTime > BULB_TIMEOUT_MS) {
        socket1State = false;
        automatedBulbTrigger = false;
        Serial.println("Rule: LDR Uncovered for 15s -> Socket 1 (Bulb) OFF.");
        updateRelays();
      }
    }
  }
}

void calculateEnergy(float totalPowerW, float p1, float p2, float p3, float p4, float voltage) {
  DateTime now = rtcFound ? (useDS1307 ? rtc1307.now() : rtc3231.now()) : DateTime((uint32_t)0);
  int thisHour = now.hour();
  int thisDay = now.day();

  if (currentHour == -1) {
    currentHour = thisHour;
  }
  if (currentDay == -1) {
    currentDay = thisDay;
  }

  if (thisHour != currentHour) {
    hourlyEnergyTotal = 0.0;
    hourlyEnergyS1 = 0.0;
    hourlyEnergyS2 = 0.0;
    hourlyEnergyS3 = 0.0;
    hourlyEnergyS4 = 0.0;
    hourlyVoltageSum = 0.0;
    hourlyVoltageCount = 0;
    hourlyVoltageMin = 999.0;
    hourlyVoltageMax = 0.0;
    currentHour = thisHour;
  }

  if (thisDay != currentDay) {
    dailyEnergyTotal = 0.0;
    dailyEnergyS1 = 0.0;
    dailyEnergyS2 = 0.0;
    dailyEnergyS3 = 0.0;
    dailyEnergyS4 = 0.0;
    dailyVoltageSum = 0.0;
    dailyVoltageCount = 0;
    dailyVoltageMin = 999.0;
    dailyVoltageMax = 0.0;
    hasShedLoadsToday = false; // Reset the manual override protection flag
    currentDay = thisDay;
  }

  unsigned long currentTime = millis();
  float elapsedHours = (currentTime - lastEnergyCalc) / 3600000.0; 
  lastEnergyCalc = currentTime;

  float eTotal = totalPowerW * elapsedHours;
  float e1 = p1 * elapsedHours;
  float e2 = p2 * elapsedHours;
  float e3 = p3 * elapsedHours;
  float e4 = p4 * elapsedHours;

  hourlyEnergyTotal += eTotal;
  hourlyEnergyS1 += e1;
  hourlyEnergyS2 += e2;
  hourlyEnergyS3 += e3;
  hourlyEnergyS4 += e4;

  dailyEnergyTotal += eTotal;
  dailyEnergyS1 += e1;
  dailyEnergyS2 += e2;
  dailyEnergyS3 += e3;
  dailyEnergyS4 += e4;

  if (voltage > 10.0) { // filter bad zero readings
    hourlyVoltageSum += voltage;
    hourlyVoltageCount++;
    if (voltage < hourlyVoltageMin) hourlyVoltageMin = voltage;
    if (voltage > hourlyVoltageMax) hourlyVoltageMax = voltage;

    dailyVoltageSum += voltage;
    dailyVoltageCount++;
    if (voltage < dailyVoltageMin) dailyVoltageMin = voltage;
    if (voltage > dailyVoltageMax) dailyVoltageMax = voltage;
  }
}

void logEnergyToFirebase() {
  if (WiFi.status() != WL_CONNECTED) return;

  int year, month, day, hour;

  // Prefer SNTP (network time) — most accurate. Fall back to RTC.
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 100) && timeinfo.tm_year > 120) {
    year  = timeinfo.tm_year + 1900;
    month = timeinfo.tm_mon + 1;
    day   = timeinfo.tm_mday;
    hour  = timeinfo.tm_hour;
    Serial.printf("Firebase log using SNTP time: %d/%d/%d %02d:00\n", year, month, day, hour);
  } else if (rtcFound) {
    DateTime now = useDS1307 ? rtc1307.now() : rtc3231.now();
    year  = now.year();
    month = now.month();
    day   = now.day();
    hour  = now.hour();
    Serial.printf("Firebase log using RTC time: %d/%d/%d %02d:00\n", year, month, day, hour);
  } else {
    Serial.println("Firebase log SKIPPED: no time source available.");
    return;
  }

  // --- Log Hourly Data ---
  String urlHourly = String(firebase_url) + "/energy_logs/" + String(year) + "/" + String(month) + "/" + String(day) + "/" + String(hour) + ".json?auth=" + FIREBASE_DATABASE_SECRET;
  
  HTTPClient http;
  http.begin(urlHourly);
  http.addHeader("Content-Type", "application/json");
  
  float vAvg = hourlyVoltageCount > 0 ? (hourlyVoltageSum / hourlyVoltageCount) : 0.0;
  float vMin = hourlyVoltageMin == 999.0 ? 0.0 : hourlyVoltageMin;
  
  String payload = "{";
  payload += "\"total_wh\":" + String(hourlyEnergyTotal, 4) + ",";
  payload += "\"s1_wh\":" + String(hourlyEnergyS1, 4) + ",";
  payload += "\"s2_wh\":" + String(hourlyEnergyS2, 4) + ",";
  payload += "\"s3_wh\":" + String(hourlyEnergyS3, 4) + ",";
  payload += "\"s4_wh\":" + String(hourlyEnergyS4, 4) + ",";
  payload += "\"v_avg\":" + String(vAvg, 2) + ",";
  payload += "\"v_min\":" + String(vMin, 2) + ",";
  payload += "\"v_max\":" + String(hourlyVoltageMax, 2);
  payload += "}";
  
  int httpResponseCode = http.PATCH(payload);
  if (httpResponseCode > 0) {
    Serial.print("Firebase Hourly Logged: ");
    Serial.println(payload);
  } else {
    Serial.print("Firebase Hourly Error: ");
    Serial.println(httpResponseCode);
  }
  http.end();

  // --- Log Daily Summary ---
  String urlDaily = String(firebase_url) + "/energy_logs/" + String(year) + "/" + String(month) + "/" + String(day) + "/daily_summary.json?auth=" + FIREBASE_DATABASE_SECRET;
  
  http.begin(urlDaily);
  http.addHeader("Content-Type", "application/json");
  
  float dAvg = dailyVoltageCount > 0 ? (dailyVoltageSum / dailyVoltageCount) : 0.0;
  float dMin = dailyVoltageMin == 999.0 ? 0.0 : dailyVoltageMin;
  
  String dailyPayload = "{";
  dailyPayload += "\"total_wh\":" + String(dailyEnergyTotal, 4) + ",";
  dailyPayload += "\"s1_wh\":" + String(dailyEnergyS1, 4) + ",";
  dailyPayload += "\"s2_wh\":" + String(dailyEnergyS2, 4) + ",";
  dailyPayload += "\"s3_wh\":" + String(dailyEnergyS3, 4) + ",";
  dailyPayload += "\"s4_wh\":" + String(dailyEnergyS4, 4) + ",";
  dailyPayload += "\"v_avg\":" + String(dAvg, 2) + ",";
  dailyPayload += "\"v_min\":" + String(dMin, 2) + ",";
  dailyPayload += "\"v_max\":" + String(dailyVoltageMax, 2);
  dailyPayload += "}";
  
  httpResponseCode = http.PATCH(dailyPayload);
  if (httpResponseCode > 0) {
    Serial.print("Firebase Daily Logged: ");
    Serial.println(dailyPayload);
  } else {
    Serial.print("Firebase Daily Error: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

void sendUpdate() {
  // Read Sensors with retry logic to harden against NaN failures
  float t = NAN;
  float h = NAN;
  for (int i = 0; i < 3; i++) {
    t = dht.readTemperature();
    h = dht.readHumidity();
    Serial.printf("DHT22 READ TRY %d: t=%.2f, h=%.2f\n", i+1, t, h);
    if (!isnan(t) && !isnan(h)) {
      break;
    }
    delay(200);
  }
  int pirState = digitalRead(PIR_PIN);
  int ldrState = digitalRead(LDR_PIN);
  
  float mainVoltage = currentVoltage;
  float mainCurrent = currentAmperage;
  
  float c1 = currentS1;
  float c2 = currentS2;
  float c3 = currentS3;
  float c4 = currentS4;
  
  float power1 = c1 * mainVoltage;
  float power2 = c2 * mainVoltage;
  float power3 = c3 * mainVoltage;
  float power4 = c4 * mainVoltage;
  float totalPower = mainCurrent * mainVoltage;
  
  // Safety check
  if (mainVoltage < 180.0 || mainVoltage > 240.0) {
    if (!voltageFault) {
      voltageFault = true;
      Serial.println("VOLTAGE FAULT! Cutting power to all sockets.");
      updateRelays();
    }
    voltageSafeStartTime = 0; // reset safe timer
  } else {
    if (voltageFault) {
      if (voltageSafeStartTime == 0) {
        voltageSafeStartTime = millis();
      } else if (millis() - voltageSafeStartTime > 10000) {
        voltageFault = false;
        Serial.println("VOLTAGE STABLE! Restoring power to sockets.");
        updateRelays();
      }
    }
  }

  int lightLevel = ldrState == LOW ? 100 : 0; // LM393 modules usually pull LOW when bright
  bool ldrLight = (ldrState == LOW);           // true = light detected

  // LCD update has been moved to loop() to prevent flickering

  // Consume the motion latch — captures any PIR HIGH since last sendUpdate()
  bool motionDetected = motionLatch;
  motionLatch = false;
  Serial.printf("PIR latch: %s, PIR pin now: %s\n",
                motionDetected ? "TRIGGERED" : "clear",
                digitalRead(PIR_PIN) == HIGH ? "HIGH" : "LOW");

  // Apply automations BEFORE sending the update
  runAutomation(t, motionDetected, ldrLight);

  // Build JSON
  JsonDocument doc;
  
  // Environment
  JsonObject env = doc["env"].to<JsonObject>();
  
  if (isnan(t)) {
    env["temperature"] = nullptr;
  } else {
    env["temperature"] = t;
  }
  
  if (isnan(h)) {
    env["humidity"] = nullptr;
  } else {
    env["humidity"] = h;
  }
  
  env["motion"] = motionDetected;
  env["lightLevel"] = lightLevel; 
  
  env["voltage"] = mainVoltage;
  env["mainCurrent"] = mainCurrent;
  env["dailyEnergy"] = dailyEnergyTotal;
  
  // Provide a fault flag to UI so it knows why everything is OFF
  env["voltageFault"] = voltageFault;

  // Outlets
  JsonArray outlets = doc["outlets"].to<JsonArray>();
  
  JsonObject o1 = outlets.add<JsonObject>();
  o1["id"] = "socket1";
  o1["name"] = "Socket 1 (Bulb)";
  o1["state"] = socket1State;
  o1["power"] = power1;
  o1["current"] = c1;

  JsonObject o2 = outlets.add<JsonObject>();
  o2["id"] = "socket2";
  o2["name"] = "Socket 2 (Fan)";
  o2["state"] = socket2State;
  o2["power"] = power2;
  o2["current"] = c2;

  JsonObject o3 = outlets.add<JsonObject>();
  o3["id"] = "socket3";
  o3["name"] = "Socket 3";
  o3["state"] = socket3State;
  o3["power"] = power3;
  o3["current"] = c3;

  JsonObject o4 = outlets.add<JsonObject>();
  o4["id"] = "socket4";
  o4["name"] = "Socket 4";
  o4["state"] = socket4State;
  o4["power"] = power4;
  o4["current"] = c4;

  String jsonString;
  serializeJson(doc, jsonString);
  
  if (mqttClient.connected()) {
    mqttClient.publish(mqtt_topic_data, jsonString.c_str());
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, mqtt_topic_cmd) != 0) return;

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.print("MQTT JSON Error: ");
    Serial.println(error.c_str());
    return;
  }
  
  const char* command = doc["action"];
  const char* target = doc["id"];
  
  if (command && target && strcmp(command, "toggle") == 0) {
    if (strcmp(target, "socket1") == 0) {
      socket1State = !socket1State;
      automatedBulbTrigger = false; // Reset automated flag on manual override
    }
    else if (strcmp(target, "socket2") == 0) socket2State = !socket2State;
    else if (strcmp(target, "socket3") == 0) socket3State = !socket3State;
    else if (strcmp(target, "socket4") == 0) socket4State = !socket4State;
    
    updateRelays();
    sendUpdate();
  }
  else if (command && strcmp(command, "set_limit") == 0) {
    if (doc.containsKey("value")) {
      dailyEnergyLimitKwh = doc["value"].as<float>();
      prefs.putFloat("limit", dailyEnergyLimitKwh);
      Serial.printf("Daily Energy Limit set to: %.2f kWh\n", dailyEnergyLimitKwh);
    }
  }
}

void reconnectMQTT() {
  while (!mqttClient.connected() && WiFi.status() == WL_CONNECTED) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str(), NULL, NULL, "iot-hub/redphoenix25-v1-x8f9a2/status", 0, true, "offline")) {
      Serial.println("connected to MQTT broker!");
      mqttClient.publish("iot-hub/redphoenix25-v1-x8f9a2/status", "online", true);
      mqttClient.subscribe(mqtt_topic_cmd);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // Load Preferences
  prefs.begin("iothub", false);
  dailyEnergyLimitKwh = prefs.getFloat("limit", 0.0);
  Serial.printf("Loaded Daily Energy Limit: %.2f kWh\n", dailyEnergyLimitKwh);

  // Setup Pins
  pinMode(RELAY_SOCKET_1, OUTPUT);
  pinMode(RELAY_SOCKET_2, OUTPUT);
  pinMode(RELAY_SOCKET_3, OUTPUT);
  pinMode(RELAY_SOCKET_4, OUTPUT);
  
  digitalWrite(RELAY_SOCKET_1, LOW);
  digitalWrite(RELAY_SOCKET_2, LOW);
  digitalWrite(RELAY_SOCKET_3, LOW);
  digitalWrite(RELAY_SOCKET_4, LOW);
  
  pinMode(PIR_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  dht.begin();

  // Initialize I2C Bus
  Wire.begin(21, 22); // Explicitly start I2C on default pins
  delay(100); // Give devices a moment to power up

  // Initialize RTC (Try DS3231 first, then DS1307)
  // NOTE: We do NOT call rtc.adjust() here with compile-time F(__DATE__) anymore.
  // That would overwrite a correct RTC time every reboot. We only adjust on lost power.
  if (rtc3231.begin()) {
    rtcFound = true;
    useDS1307 = false;
    Serial.println("Found DS3231 RTC");
    if (rtc3231.lostPower()) {
      Serial.println("DS3231 lost power — setting compile-time fallback.");
      rtc3231.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  } else if (rtc1307.begin()) {
    rtcFound = true;
    useDS1307 = true;
    Serial.println("Found DS1307 RTC");
    if (!rtc1307.isrunning()) {
      Serial.println("DS1307 not running — setting compile-time fallback.");
      rtc1307.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  } else {
    Serial.println("Couldn't find any RTC");
    rtcFound = false;
  }

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("IoT Energy Hub");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  
  // Connect Wi-Fi with non-blocking WiFiManager
  WiFi.mode(WIFI_STA);
  wm.setConfigPortalBlocking(false);
  
  if (WiFi.SSID() != "") {
    Serial.print("Attempting to connect to known network: ");
    Serial.println(WiFi.SSID());
  }
  
  if (!wm.autoConnect("IoT-Hub-AP")) {
    Serial.println("Config portal running in background");
  } else {
    Serial.println("\nWiFi connected successfully!");
    Serial.println(WiFi.localIP());
  }
  
  Serial.println("\nWiFi connected successfully!");
  Serial.println(WiFi.localIP());
  
  // Start mDNS
  if (!MDNS.begin("iot-hub")) {
    Serial.println("Error setting up MDNS responder!");
  } else {
    Serial.println("mDNS responder started at iot-hub.local");
  }
  
  // Setup MQTT
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(1024);

  lastEnergyCalc = millis();
}

void loop() {
  wm.process(); // Handle captive portal in background
  
  // PIR motion latch: capture any HIGH reading between sendUpdate() calls
  if (digitalRead(PIR_PIN) == HIGH) {
    motionLatch = true;
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      reconnectMQTT();
    }
    mqttClient.loop();
  }
  
  unsigned long nowMs = millis();

  // Handle SNTP Sync
  if (WiFi.status() == WL_CONNECTED && !sntpConfigured) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    sntpConfigured = true;
  }
  
  static unsigned long lastTimeSync = 0;
  if (WiFi.status() == WL_CONNECTED && (nowMs - lastTimeSync > 3600000 || lastTimeSync == 0)) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10)) { // 10ms wait
      if (rtcFound && timeinfo.tm_year > 120) {
        if (useDS1307) {
          rtc1307.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
        } else {
          rtc3231.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
        }
        Serial.println("RTC synced with NTP");
        lastTimeSync = nowMs;
      }
    }
  }

  static unsigned long lastTopUpdate = 0;
  static unsigned long lastScrollUpdate = 0;
  static int scrollIndex = 0;
  
  if (nowMs - lastTopUpdate >= 2000) {
    lastTopUpdate = nowMs;
    
    // True AC RMS Sampling: 3 consecutive 20ms cycles
    // ALL channels use MIN consistency gate: real loads are consistent, noise is erratic.
    float cycZ[3], cycAcs[3];
    float cycA1[3], cycA2[3], cycA3[3], cycA4[3];
    const int NUM_CYCLES = 3;

    for (int cyc = 0; cyc < NUM_CYCLES; cyc++) {
      double zmptSumSq = 0, acsSumSq = 0;
      double a1SumSq = 0, a2SumSq = 0, a3SumSq = 0, a4SumSq = 0;
      double zmptSum = 0, acsSum = 0;
      double a1Sum = 0, a2Sum = 0, a3Sum = 0, a4Sum = 0;
      int samples = 0;
      unsigned long startSample = millis();
      while (millis() - startSample < 20) {
        long zRaw = analogRead(ZMPT101B_PIN);
        long aRaw = analogRead(ACS712_MAIN);
        long a1Raw = analogRead(ACS712_PIN1);
        long a2Raw = analogRead(ACS712_PIN2);
        long a3Raw = analogRead(ACS712_PIN3);
        long a4Raw = analogRead(ACS712_PIN4);
        zmptSum += zRaw;  acsSum += aRaw;
        a1Sum += a1Raw;   a2Sum += a2Raw;
        a3Sum += a3Raw;   a4Sum += a4Raw;
        zmptSumSq += (double)zRaw * zRaw;   acsSumSq += (double)aRaw * aRaw;
        a1SumSq += (double)a1Raw * a1Raw;   a2SumSq += (double)a2Raw * a2Raw;
        a3SumSq += (double)a3Raw * a3Raw;   a4SumSq += (double)a4Raw * a4Raw;
        samples++;
      }
      float zM  = zmptSum / samples;  float aM  = acsSum / samples;
      float a1M = a1Sum   / samples;  float a2M = a2Sum  / samples;
      float a3M = a3Sum   / samples;  float a4M = a4Sum  / samples;

      float zV  = (zmptSumSq / samples) - (zM  * zM);
      float aV  = (acsSumSq  / samples) - (aM  * aM);
      float a1V = (a1SumSq   / samples) - (a1M * a1M);
      float a2V = (a2SumSq   / samples) - (a2M * a2M);
      float a3V = (a3SumSq   / samples) - (a3M * a3M);
      float a4V = (a4SumSq   / samples) - (a4M * a4M);

      // Store individual cycle RMS for ALL sensors' consistency check
      cycZ[cyc]   = zV  > 0 ? sqrt(zV)  : 0;
      cycAcs[cyc] = aV  > 0 ? sqrt(aV)  : 0;
      cycA1[cyc]  = a1V > 0 ? sqrt(a1V) : 0;
      cycA2[cyc]  = a2V > 0 ? sqrt(a2V) : 0;
      cycA3[cyc]  = a3V > 0 ? sqrt(a3V) : 0;
      cycA4[cyc]  = a4V > 0 ? sqrt(a4V) : 0;
    }

    // All sensors: use MINIMUM across all 3 cycles as the consistency gate
    float zRmsAvg   = (cycZ[0]   + cycZ[1]   + cycZ[2])   / NUM_CYCLES;
    float acsRmsMin = min(cycAcs[0], min(cycAcs[1], cycAcs[2]));
    float acsRmsAvg = (cycAcs[0] + cycAcs[1] + cycAcs[2]) / NUM_CYCLES;

    float a1RmsMin = min(cycA1[0], min(cycA1[1], cycA1[2]));
    float a2RmsMin = min(cycA2[0], min(cycA2[1], cycA2[2]));
    float a3RmsMin = min(cycA3[0], min(cycA3[1], cycA3[2]));
    float a4RmsMin = min(cycA4[0], min(cycA4[1], cycA4[2]));
    float a1RmsAvg = (cycA1[0] + cycA1[1] + cycA1[2]) / NUM_CYCLES;
    float a2RmsAvg = (cycA2[0] + cycA2[1] + cycA2[2]) / NUM_CYCLES;
    float a3RmsAvg = (cycA3[0] + cycA3[1] + cycA3[2]) / NUM_CYCLES;
    float a4RmsAvg = (cycA4[0] + cycA4[1] + cycA4[2]) / NUM_CYCLES;
    
    // Scale to real values
    float instVoltage = zRmsAvg * CAL_VOLTAGE;

    // Apply quadratic noise subtraction to recover clean signal RMS (subtracting base noise floor)
    float acsRmsClean = sqrt(max(0.0f, (acsRmsAvg * acsRmsAvg) - (NOISE_RMS_MAIN * NOISE_RMS_MAIN)));
    float a1RmsClean  = sqrt(max(0.0f, (a1RmsAvg * a1RmsAvg) - (NOISE_RMS_S1 * NOISE_RMS_S1)));
    float a2RmsClean  = sqrt(max(0.0f, (a2RmsAvg * a2RmsAvg) - (NOISE_RMS_S2 * NOISE_RMS_S2)));
    float a3RmsClean  = sqrt(max(0.0f, (a3RmsAvg * a3RmsAvg) - (NOISE_RMS_S3 * NOISE_RMS_S3)));
    float a4RmsClean  = sqrt(max(0.0f, (a4RmsAvg * a4RmsAvg) - (NOISE_RMS_S4 * NOISE_RMS_S4)));

    float acsMinClean = sqrt(max(0.0f, (acsRmsMin * acsRmsMin) - (NOISE_RMS_MAIN * NOISE_RMS_MAIN)));
    float a1MinClean  = sqrt(max(0.0f, (a1RmsMin * a1RmsMin) - (NOISE_RMS_S1 * NOISE_RMS_S1)));
    float a2MinClean  = sqrt(max(0.0f, (a2RmsMin * a2RmsMin) - (NOISE_RMS_S2 * NOISE_RMS_S2)));
    float a3MinClean  = sqrt(max(0.0f, (a3RmsMin * a3RmsMin) - (NOISE_RMS_S3 * NOISE_RMS_S3)));
    float a4MinClean  = sqrt(max(0.0f, (a4RmsMin * a4RmsMin) - (NOISE_RMS_S4 * NOISE_RMS_S4)));

    // Calibration log output to serial monitor
    static unsigned long lastCalibLog = 0;
    if (millis() - lastCalibLog > 2000) {
      lastCalibLog = millis();
      Serial.printf("CALIBRATION DEBUG: raw zRmsAvg = %.3f, instVoltage = %.2fV\n", zRmsAvg, instVoltage);
      Serial.printf("CURRENT DEBUG: MainMin=%.3f, MainAvg=%.3f | S2Min=%.3f, S2Avg=%.3f\n",
                    acsRmsMin, acsRmsAvg, a2RmsMin, a2RmsAvg);
    }
    // Main line: only report if consistent across all 3 cycles (not adapter self-draw noise)
    float instCurrent = (acsMinClean * CAL_CURRENT_MAIN >= 0.03f) ? (acsRmsClean * CAL_CURRENT_MAIN) : 0.0f;
    // Socket sensors: use average clean RMS to avoid dips rejecting real loads
    float instC1 = (a1RmsClean * CAL_CURRENT_S1 >= 0.04f) ? (a1RmsClean * CAL_CURRENT_S1) : 0.0f;
    float instC2 = (a2RmsClean * CAL_CURRENT_S2 >= 0.04f) ? (a2RmsClean * CAL_CURRENT_S2) : 0.0f;
    float instC3 = (a3RmsClean * CAL_CURRENT_S3 >= 0.04f) ? (a3RmsClean * CAL_CURRENT_S3) : 0.0f;
    float instC4 = (a4RmsClean * CAL_CURRENT_S4 >= 0.04f) ? (a4RmsClean * CAL_CURRENT_S4) : 0.0f;
    
    // EMA: 85% old value, 15% new — more resistant to single-cycle spikes
    static float smoothedVoltage = 0, smoothedCurrent = 0;
    static float s1 = 0, s2 = 0, s3 = 0, s4 = 0;
    
    if (smoothedVoltage == 0) {
      smoothedVoltage = instVoltage;
      smoothedCurrent = instCurrent;
      s1 = instC1; s2 = instC2; s3 = instC3; s4 = instC4;
    }
    
    smoothedVoltage = (smoothedVoltage * 0.85f) + (instVoltage * 0.15f);
    smoothedCurrent = (instCurrent == 0.0f) ? 0.0f : (smoothedCurrent * 0.85f) + (instCurrent * 0.15f);
    s1 = (instC1 == 0.0f) ? 0.0f : (s1 * 0.85f) + (instC1 * 0.15f);
    s2 = (instC2 == 0.0f) ? 0.0f : (s2 * 0.85f) + (instC2 * 0.15f);
    s3 = (instC3 == 0.0f) ? 0.0f : (s3 * 0.85f) + (instC3 * 0.15f);
    s4 = (instC4 == 0.0f) ? 0.0f : (s4 * 0.85f) + (instC4 * 0.15f);
    
    // Apply noise floor thresholds and ensure OFF sockets are strictly 0
    currentVoltage  = (smoothedVoltage < 10.0f) ? 0 : smoothedVoltage;
    currentAmperage = (smoothedCurrent < 0.03f) ? 0 : smoothedCurrent;
    currentS1 = (socket1State && s1 >= 0.03f) ? s1 : 0;
    currentS2 = (socket2State && s2 >= 0.03f) ? s2 : 0;
    currentS3 = (socket3State && s3 >= 0.03f) ? s3 : 0;
    currentS4 = (socket4State && s4 >= 0.03f) ? s4 : 0;

    // If no sockets are active, force total current to 0 (system self-draw is below threshold/ignored)
    bool anySocketActive = socket1State || socket2State || socket3State || socket4State;
    if (!anySocketActive) {
      currentAmperage = 0;
    }

    // Total power: use sum of socket readings (more reliable at low loads).
    // Fall back to main line measurement only when at least one socket is on and reporting.
    float socketPowerSum = (currentS1 + currentS2 + currentS3 + currentS4) * currentVoltage;
    if (anySocketActive && socketPowerSum > 0.5f) {
      currentPower = socketPowerSum;
      currentAmperage = currentS1 + currentS2 + currentS3 + currentS4;
    } else {
      currentPower = currentAmperage * currentVoltage;
    }
    
    float p1 = currentS1 * currentVoltage;
    float p2 = currentS2 * currentVoltage;
    float p3 = currentS3 * currentVoltage;
    float p4 = currentS4 * currentVoltage;

    // Track Energy
    calculateEnergy(currentPower, p1, p2, p3, p4, currentVoltage);
  
    // Load Shedding Logic
    if (dailyEnergyLimitKwh > 0.0 && !hasShedLoadsToday) {
      // Add 0.0005 to match the dashboard's .toFixed(3) visual rounding
      if (((dailyEnergyTotal / 1000.0f) + 0.0005f) >= dailyEnergyLimitKwh) {
        if (socket3State || socket4State) {
          socket3State = false;
          socket4State = false;
          updateRelays();
          sendUpdate(); // Instantly update the dashboard
          Serial.println("SHEDDING LOADS: Daily Energy Limit Exceeded!");
        }
        hasShedLoadsToday = true;
      }
    }

    // LCD Top Row — uses currentVoltage globals to stay in sync with dashboard
    char topStr[17];
    snprintf(topStr, sizeof(topStr), "%-3.0fV %-3.1fA %-4.0fW",
             currentVoltage, currentAmperage, currentPower);
    while(strlen(topStr) < 16) strcat(topStr, " ");
    lcd.setCursor(0, 0);
    lcd.print(topStr);
  }

  if (nowMs - lastScrollUpdate >= 350) { // 350ms scroll speed
    lastScrollUpdate = nowMs;
    
    String fullText;
    if (WiFi.status() != WL_CONNECTED) {
      fullText = "   No Internet Connection   ";
    } else {
      char timeStr[20];
      struct tm timeinfo;
      if (getLocalTime(&timeinfo, 10)) {
        sprintf(timeStr, "Time: %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      } else if (rtcFound) {
        DateTime now = useDS1307 ? rtc1307.now() : rtc3231.now();
        sprintf(timeStr, "Time: %02d:%02d:%02d", now.hour(), now.minute(), now.second());
      } else {
        strcpy(timeStr, "Time: --:--:--");
      }
      fullText = String(timeStr) + "   Status: Online   ";
    }
    
    int len = fullText.length();
    String disp = "";
    for (int i = 0; i < 16; i++) {
      disp += fullText[(scrollIndex + i) % len];
    }
    
    lcd.setCursor(0, 1);
    lcd.print(disp);
    
    scrollIndex = (scrollIndex + 1) % len;
  }

  // Handle OTA updates every 2 seconds
  if (nowMs - lastUpdate > 2000) {
    lastUpdate = nowMs;
    sendUpdate();
  }

  // Log to Firebase every 1 minute
  if (nowMs - lastFirebaseLog > 60000) {
    lastFirebaseLog = nowMs;
    logEnergyToFirebase();
  }
}
