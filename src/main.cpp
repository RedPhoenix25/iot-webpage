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
double hourlyEnergyTotal = 0.0;
double hourlyEnergyS1 = 0.0;
double hourlyEnergyS2 = 0.0;
double hourlyEnergyS3 = 0.0;
double hourlyEnergyS4 = 0.0;

// Voltage Analytics
float hourlyVoltageSum = 0.0;
int hourlyVoltageCount = 0;
float hourlyVoltageMin = 999.0;
float hourlyVoltageMax = 0.0;

int currentHour = -1;

// Safety Cutoff
bool voltageFault = false;
unsigned long voltageSafeStartTime = 0;

unsigned long lastEnergyCalc = 0;
unsigned long lastUpdate = 0;
unsigned long lastFirebaseLog = 0;

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

void calculateEnergy(float totalPowerW, float p1, float p2, float p3, float p4, float voltage) {
  DateTime now = rtcFound ? rtc.now() : DateTime((uint32_t)0);
  int thisHour = now.hour();

  if (currentHour == -1) {
    currentHour = thisHour;
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

  unsigned long currentTime = millis();
  float elapsedHours = (currentTime - lastEnergyCalc) / 3600000.0; 
  lastEnergyCalc = currentTime;

  hourlyEnergyTotal += (totalPowerW * elapsedHours);
  hourlyEnergyS1 += (p1 * elapsedHours);
  hourlyEnergyS2 += (p2 * elapsedHours);
  hourlyEnergyS3 += (p3 * elapsedHours);
  hourlyEnergyS4 += (p4 * elapsedHours);

  if (voltage > 10.0) { // filter bad zero readings
    hourlyVoltageSum += voltage;
    hourlyVoltageCount++;
    if (voltage < hourlyVoltageMin) hourlyVoltageMin = voltage;
    if (voltage > hourlyVoltageMax) hourlyVoltageMax = voltage;
  }
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
  float mainVoltage = 220.0 + ((zmptRaw - 2048) / 4096.0) * 10.0; 
  
  float c1 = socket1State ? 1.2 : 0;
  float c2 = socket2State ? 0.8 : 0;
  float c3 = socket3State ? 2.5 : 0;
  float c4 = socket4State ? 0.1 : 0;
  float mainCurrent = c1 + c2 + c3 + c4;
  
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

  int lightLevel = ldrState == HIGH ? 100 : 0;

  // Update LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Pwr: ");
  lcd.print(totalPower, 1);
  lcd.print("W ");
  lcd.print(mainVoltage, 0);
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
  
  env["motion"] = nullptr; 
  env["lightLevel"] = nullptr; 
  
  env["voltage"] = mainVoltage;
  env["mainCurrent"] = mainCurrent;
  
  // Provide a fault flag to UI so it knows why everything is OFF
  env["voltageFault"] = voltageFault;

  // Outlets
  JsonArray outlets = doc["outlets"].to<JsonArray>();
  
  JsonObject o1 = outlets.add<JsonObject>();
  o1["id"] = "socket1";
  o1["name"] = "Socket 1 (Fan)";
  o1["state"] = socket1State;
  o1["power"] = power1;
  o1["current"] = c1;

  JsonObject o2 = outlets.add<JsonObject>();
  o2["id"] = "socket2";
  o2["name"] = "Socket 2";
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

  String message = "";
  for (int i = 0; i < length; i++) message += (char)payload[i];
  
  int sepIndex = message.indexOf(':');
  if (sepIndex == -1) return;
  
  String id = message.substring(0, sepIndex);
  bool state = message.substring(sepIndex + 1) == "ON";
  
  if (id == "socket1") socket1State = state;
  else if (id == "socket2") socket2State = state;
  else if (id == "socket3") socket3State = state;
  else if (id == "socket4") socket4State = state;

  updateRelays();
  sendUpdate();
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
  
  // Mock calculations for sensors for energy tracking
  int zmptRaw = analogRead(ZMPT101B_PIN);
  float mainVoltage = 220.0 + ((zmptRaw - 2048) / 4096.0) * 10.0; 
  float c1 = socket1State ? 1.2 : 0;
  float c2 = socket2State ? 0.8 : 0;
  float c3 = socket3State ? 2.5 : 0;
  float c4 = socket4State ? 0.1 : 0;
  float mainCurrent = c1 + c2 + c3 + c4;
  
  float p1 = c1 * mainVoltage;
  float p2 = c2 * mainVoltage;
  float p3 = c3 * mainVoltage;
  float p4 = c4 * mainVoltage;
  float trueTotalPower = mainCurrent * mainVoltage;

  // Track Energy
  calculateEnergy(trueTotalPower, p1, p2, p3, p4, mainVoltage);

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
