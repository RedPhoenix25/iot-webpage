#include <Arduino.h>
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
#include <HTTPClient.h>

// --- Configuration ---
// WiFi credentials are now managed dynamically by WiFiManager!

const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_topic_data = "iot-hub/redphoenix25-v1-x8f9a2/data";
const char* mqtt_topic_cmd  = "iot-hub/redphoenix25-v1-x8f9a2/cmd";

const char* firebase_url = "https://iot-energy-hub-eea5f-default-rtdb.firebaseio.com";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// RTC Module (DS3231)
RTC_DS3231 rtc;
bool rtcFound = false;

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
double hourlyEnergy = 0.0;
int currentHour = -1;

unsigned long lastEnergyCalc = 0;
unsigned long lastUpdate = 0;
unsigned long lastFirebaseLog = 0;

// Simple rule engine for automated thresholds
void checkThresholds(float temp, float humidity, int lightLevel, bool motion) {
  // Example Rule: Turn on Socket 1 (Fan) if Temp > 30.0 C
  if (temp > 30.0 && !socket1State) {
    socket1State = true;
    digitalWrite(RELAY_SOCKET_1, HIGH);
    Serial.println("Threshold Rule triggered: Temp > 30.0C -> Socket 1 (Fan) turned ON.");
  }

  // Placeholder for future rules:
  // if (lightLevel < 20 && !socket4State) { socket4State = true; digitalWrite(RELAY_SOCKET_4, HIGH); }
  // if (motion && !socket2State) { socket2State = true; digitalWrite(RELAY_SOCKET_2, HIGH); }
}

void calculateEnergy(float totalPowerW) {
  DateTime now = rtcFound ? rtc.now() : DateTime((uint32_t)0);
  int thisHour = now.hour();

  if (currentHour == -1) {
    currentHour = thisHour;
  }

  if (thisHour != currentHour) {
    hourlyEnergy = 0.0; // Reset hourly accumulator
    currentHour = thisHour;
  }

  unsigned long currentTime = millis();
  float elapsedHours = (currentTime - lastEnergyCalc) / 3600000.0; 
  lastEnergyCalc = currentTime;

  hourlyEnergy += (totalPowerW * elapsedHours);
}

void logEnergyToFirebase() {
  if (WiFi.status() != WL_CONNECTED || !rtcFound) return;

  DateTime now = rtc.now();
  int year = now.year();
  int month = now.month();
  int day = now.day();
  int hour = now.hour();

  String url = String(firebase_url) + "/energy_logs/" + String(year) + "/" + String(month) + "/" + String(day) + "/" + String(hour) + ".json";
  
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  String payload = "{\"wh\":" + String(hourlyEnergy, 4) + "}";
  int httpResponseCode = http.PATCH(payload);
  
  if (httpResponseCode > 0) {
    Serial.print("Firebase Logged: ");
    Serial.println(payload);
  } else {
    Serial.print("Firebase Error: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

void sendUpdate() {
  // Read Sensors
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  int pirState = digitalRead(PIR_PIN);
  int ldrState = digitalRead(LDR_PIN);
  
  // Mock calculations for sensors
  int zmptRaw = analogRead(ZMPT101B_PIN);
  float voltage = 220.0 + ((zmptRaw - 2048) / 4096.0) * 10.0; 
  
  float current1 = socket1State ? 1.2 : 0;
  float current2 = socket2State ? 0.8 : 0;
  float current3 = socket3State ? 2.5 : 0;
  float current4 = socket4State ? 0.1 : 0;
  
  float power1 = voltage * current1;
  float power2 = voltage * current2;
  float power3 = voltage * current3;
  float power4 = voltage * current4;

  float totalPower = power1 + power2 + power3 + power4;
  int lightLevel = ldrState == HIGH ? 100 : 0;

  // Update LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Pwr: ");
  lcd.print(totalPower, 1);
  lcd.print("W ");
  lcd.print(voltage, 0);
  lcd.print("V");
  lcd.setCursor(0, 1);
  if (WiFi.status() == WL_CONNECTED) {
    lcd.print(WiFi.localIP().toString());
  } else {
    lcd.print("Offline");
  }

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
  env["mainCurrent"] = nullptr;
  
  env["voltage"] = nullptr;
  env["mainCurrent"] = nullptr;

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
  o4["id"] = "socket4";
  o4["name"] = "Socket 4";
  o4["state"] = socket4State;
  o4["power"] = power4;
  o4["current"] = current4;

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
    } else if (id == "socket4") {
      socket4State = newState;
      digitalWrite(RELAY_SOCKET_4, socket4State ? HIGH : LOW);
    }
    
    sendUpdate();
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

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("IoT Energy Hub");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  
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
  
  // Connect Wi-Fi with custom retry logic
  WiFi.mode(WIFI_STA);
  if (WiFi.SSID() != "") {
    Serial.print("Attempting to connect to known network: ");
    Serial.println(WiFi.SSID());
    WiFi.begin();
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 5) {
      delay(5000); // 5 second interval
      Serial.print(".");
      attempts++;
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nCould not connect to known network after 5 attempts. Starting Access Point...");
    WiFiManager wifiManager;
    
    // Uncomment the line below to erase saved Wi-Fi credentials
    // wifiManager.resetSettings();
    
    // Force the Captive Portal to open since connection failed
    if (!wifiManager.startConfigPortal("IoT-Hub-AP")) {
      Serial.println("Failed to connect and hit timeout. Rebooting...");
      ESP.restart();
      delay(1000);
    }
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

  lastEnergyCalc = millis();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      reconnectMQTT();
    }
    mqttClient.loop();
  }
  
  // Calculate total power dynamically for energy calculation
  float current1 = socket1State ? 1.2 : 0;
  float current2 = socket2State ? 0.8 : 0;
  float current3 = socket3State ? 2.5 : 0;
  float current4 = socket4State ? 0.1 : 0;
  // Assumed nominal voltage of 220 for quick integration step
  float totalPowerW = 220.0 * (current1 + current2 + current3 + current4); 

  // Run energy calculation rapidly to accumulate properly
  calculateEnergy(totalPowerW);
  
  // Send WS update every 2 seconds
  unsigned long nowMs = millis();
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
