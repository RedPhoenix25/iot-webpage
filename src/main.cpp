#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Wire.h>
#include <RTClib.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>

// --- Configuration ---
// WiFi credentials are now managed dynamically by WiFiManager!

// WebSocket Server on port 81
WebSocketsServer webSocket = WebSocketsServer(81);

// Preferences for data persistence
Preferences preferences;

// RTC Module (DS3231)
RTC_DS3231 rtc;
bool rtcFound = false;

// --- Pins ---
// Relays for outlets
const int RELAY_SOCKET_1 = 26;
const int RELAY_SOCKET_2 = 27;
const int RELAY_SOCKET_3 = 14;
const int RELAY_BULB     = 12;

// Environment Sensors
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

const int PIR_PIN = 5;
const int LDR_PIN = 32;

// Energy Sensors (Analog Pins)
const int ACS712_PIN  = 33;
const int ZMPT101B_PIN = 34;

// --- State Variables ---
bool socket1State = false;
bool socket2State = false;
bool socket3State = false;
bool bulbState = false;

// Energy Tracking (in Watt-hours)
double dailyEnergy = 0.0;
double weeklyEnergy = 0.0;

int currentDay = -1;
int currentWeek = -1;

unsigned long lastEnergyCalc = 0;
unsigned long lastUpdate = 0;
unsigned long lastSaveTime = 0;

void saveEnergyData() {
  preferences.begin("energy", false);
  preferences.putDouble("dailyEnergy", dailyEnergy);
  preferences.putDouble("weeklyEnergy", weeklyEnergy);
  preferences.putInt("savedDay", currentDay);
  preferences.putInt("savedWeek", currentWeek);
  preferences.end();
  Serial.println("Energy data saved to Flash.");
}

void loadEnergyData() {
  preferences.begin("energy", true);
  dailyEnergy = preferences.getDouble("dailyEnergy", 0.0);
  weeklyEnergy = preferences.getDouble("weeklyEnergy", 0.0);
  int savedDay = preferences.getInt("savedDay", -1);
  int savedWeek = preferences.getInt("savedWeek", -1);
  preferences.end();

  // If the loaded day or week doesn't match the current RTC day/week, we might need to reset.
  // This is handled in the calculateEnergy() loop.
}

// Simple rule engine for automated thresholds
void checkThresholds(float temp, float humidity, int lightLevel, bool motion) {
  // Example Rule: Turn on Socket 1 (Fan) if Temp > 30.0 C
  if (temp > 30.0 && !socket1State) {
    socket1State = true;
    digitalWrite(RELAY_SOCKET_1, HIGH);
    Serial.println("Threshold Rule triggered: Temp > 30.0C -> Socket 1 (Fan) turned ON.");
  }

  // Placeholder for future rules:
  // if (lightLevel < 20 && !bulbState) { bulbState = true; digitalWrite(RELAY_BULB, HIGH); }
  // if (motion && !socket2State) { socket2State = true; digitalWrite(RELAY_SOCKET_2, HIGH); }
}

void calculateEnergy(float totalPowerW) {
  DateTime now = rtcFound ? rtc.now() : DateTime((uint32_t)0);
  
  // Calculate days since epoch for daily rollover
  // Simplistic approach for week: (days since epoch) / 7
  int today = now.unixtime() / 86400;
  int thisWeek = today / 7;

  // Check for day/week rollovers
  if (currentDay == -1) {
    // Initial boot load
    currentDay = today;
    currentWeek = thisWeek;
  }

  if (today != currentDay) {
    dailyEnergy = 0.0; // Reset daily
    currentDay = today;
  }

  if (thisWeek != currentWeek) {
    weeklyEnergy = 0.0; // Reset weekly
    currentWeek = thisWeek;
  }

  // Calculate elapsed time since last energy calculation in seconds
  unsigned long currentTime = millis();
  float elapsedHours = (currentTime - lastEnergyCalc) / 3600000.0; // convert ms to hours
  lastEnergyCalc = currentTime;

  // Energy (Wh) = Power (W) * Time (hours)
  double energyToAdd = totalPowerW * elapsedHours;
  dailyEnergy += energyToAdd;
  weeklyEnergy += energyToAdd;
}

void sendUpdate() {
  // Read Sensors
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  int pirState = digitalRead(PIR_PIN);
  int ldrValue = analogRead(LDR_PIN);
  
  // Mock calculations for sensors
  int zmptRaw = analogRead(ZMPT101B_PIN);
  float voltage = 220.0 + ((zmptRaw - 2048) / 4096.0) * 10.0; 
  
  float current1 = socket1State ? 1.2 : 0;
  float current2 = socket2State ? 0.8 : 0;
  float current3 = socket3State ? 2.5 : 0;
  float currentBulb = bulbState ? 0.1 : 0;
  
  float power1 = voltage * current1;
  float power2 = voltage * current2;
  float power3 = voltage * current3;
  float powerBulb = voltage * currentBulb;

  float totalPower = power1 + power2 + power3 + powerBulb;
  int lightLevel = map(ldrValue, 0, 4095, 0, 100);

  // Apply automations BEFORE sending the update
  checkThresholds(t, h, lightLevel, pirState == HIGH);

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
  
  // Hardcoded to null (Offline) until physical sensors are actually connected.
  // Analog/Digital pins "float" (pick up random noise) when disconnected, so they must be manually disabled.
  env["motion"] = nullptr; 
  env["lightLevel"] = nullptr; 
  env["voltage"] = nullptr;
  
  // Energy Reports
  JsonObject energy = doc["energy"].to<JsonObject>();
  energy["dailyWh"] = dailyEnergy;
  energy["weeklyWh"] = weeklyEnergy;

  // Outlets
  JsonArray outlets = doc["outlets"].to<JsonArray>();
  
  JsonObject o1 = outlets.add<JsonObject>();
  o1["id"] = "socket1";
  o1["name"] = "Socket 1 (Fan)";
  o1["state"] = socket1State;
  o1["power"] = power1;
  o1["current"] = current1;

  JsonObject o2 = outlets.add<JsonObject>();
  o2["id"] = "socket2";
  o2["name"] = "Socket 2";
  o2["state"] = socket2State;
  o2["power"] = power2;
  o2["current"] = current2;

  JsonObject o3 = outlets.add<JsonObject>();
  o3["id"] = "socket3";
  o3["name"] = "Socket 3";
  o3["state"] = socket3State;
  o3["power"] = power3;
  o3["current"] = current3;

  JsonObject o4 = outlets.add<JsonObject>();
  o4["id"] = "bulb";
  o4["name"] = "Smart Bulb";
  o4["state"] = bulbState;
  o4["power"] = powerBulb;
  o4["current"] = currentBulb;

  String jsonString;
  serializeJson(doc, jsonString);
  
  webSocket.broadcastTXT(jsonString);
}

void handleWebSocketMessage(uint8_t num, uint8_t * payload, size_t length) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.println("deserializeJson() failed");
    return;
  }
  
  if (doc["action"] == "toggle") {
    String id = doc["id"];
    bool newState = doc["state"];
    
    if (id == "socket1") {
      socket1State = newState;
      digitalWrite(RELAY_SOCKET_1, socket1State ? HIGH : LOW);
    } else if (id == "socket2") {
      socket2State = newState;
      digitalWrite(RELAY_SOCKET_2, socket2State ? HIGH : LOW);
    } else if (id == "socket3") {
      socket3State = newState;
      digitalWrite(RELAY_SOCKET_3, socket3State ? HIGH : LOW);
    } else if (id == "bulb") {
      bulbState = newState;
      digitalWrite(RELAY_BULB, bulbState ? HIGH : LOW);
    }
    
    sendUpdate();
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      break;
    case WStype_CONNECTED:
      sendUpdate();
      break;
    case WStype_TEXT:
      handleWebSocketMessage(num, payload, length);
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  
  // Setup Pins
  pinMode(RELAY_SOCKET_1, OUTPUT);
  pinMode(RELAY_SOCKET_2, OUTPUT);
  pinMode(RELAY_SOCKET_3, OUTPUT);
  pinMode(RELAY_BULB, OUTPUT);
  
  digitalWrite(RELAY_SOCKET_1, LOW);
  digitalWrite(RELAY_SOCKET_2, LOW);
  digitalWrite(RELAY_SOCKET_3, LOW);
  digitalWrite(RELAY_BULB, LOW);
  
  pinMode(PIR_PIN, INPUT);
  dht.begin();
  
  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    rtcFound = false;
  } else {
    rtcFound = true;
    if (rtc.lostPower()) {
      Serial.println("RTC lost power, let's set the time!");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }

  // Load Energy from Flash
  loadEnergyData();
  
  // Connect Wi-Fi using WiFiManager
  // This will auto-connect to the last saved network.
  // If it fails or no network is saved, it spins up an AP named "IoT-Hub-AP"
  WiFiManager wifiManager;
  
  // Uncomment the line below to erase saved Wi-Fi credentials
  // wifiManager.resetSettings();

  bool connected = wifiManager.autoConnect("IoT-Hub-AP");
  if (!connected) {
    Serial.println("Failed to connect to WiFi and hit timeout. Rebooting...");
    ESP.restart();
    delay(1000);
  }
  
  Serial.println("\nWiFi connected successfully!");
  Serial.println(WiFi.localIP());
  
  // Start mDNS
  if (!MDNS.begin("iot-hub")) {
    Serial.println("Error setting up MDNS responder!");
  } else {
    Serial.println("mDNS responder started at iot-hub.local");
  }
  
  // Start WebSocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  lastEnergyCalc = millis();
}

void loop() {
  webSocket.loop();
  
  // Calculate total power dynamically for energy calculation
  float current1 = socket1State ? 1.2 : 0;
  float current2 = socket2State ? 0.8 : 0;
  float current3 = socket3State ? 2.5 : 0;
  float currentBulb = bulbState ? 0.1 : 0;
  // Assumed nominal voltage of 220 for quick integration step
  float totalPowerW = 220.0 * (current1 + current2 + current3 + currentBulb); 

  // Run energy calculation rapidly to accumulate properly
  calculateEnergy(totalPowerW);
  
  // Send WS update every 2 seconds
  unsigned long nowMs = millis();
  if (nowMs - lastUpdate > 2000) {
    lastUpdate = nowMs;
    sendUpdate();
  }

  // Save to flash every 10 minutes (600000 ms) to avoid flash wear
  if (nowMs - lastSaveTime > 600000) {
    lastSaveTime = nowMs;
    saveEnergyData();
  }
}
