/*
  eSRA: Smart Room Automation Firmware
  Board: ESP32 Dev Module (ESP32-D0WD-V3)

  Dependencies:
  - Adafruit DHT Sensor Library
  - WebSockets by Markus Sattler
  - ArduinoJson
*/

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <Preferences.h>
#include <NimBLEDevice.h>
#include <WebSocketsServer.h>
#include <DHT.h>
#include <ArduinoJson.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define SCAN_CHARACTERISTIC_UUID "d991b58a-36b6-4f7f-8566-f3353549666c"
#define IP_CHARACTERISTIC_UUID "c77b4a2c-d64e-46ad-9720-6d8048f3f4e2"

#define DHTPIN 4
#define DHTTYPE DHT11
#define LDRPIN 34
#define LEDPIN 2
#define MOTORPIN 5 // Note: ensure flyback diode across motor

// PWM PWM parameters for Motor
const int motorChannel = 0;
const int freq = 5000;
const int resolution = 8; // 0-255
const int motorSpeed = 255; // Set to absolute maximum

DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);
WebSocketsServer webSocket(81);
Preferences preferences;

// Global State
bool isAutoMode = true;
bool isFanOn = false;
bool isLedOn = false;
float currentTemp = 0.0;
bool isDark = false;
float tempThreshold = 30.0; // Default threshold

// Timers
unsigned long lastSensorRead = 0;
const long SENSOR_INTERVAL = 2000;

// BLE
BLEServer* pServer = NULL;
BLECharacteristic* pIpChar = NULL;
bool credentialsReceived = false;
String newSSID = "";
String newPass = "";
bool provisioned = false;

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {}
    void onDisconnect(BLEServer* pServer) {
        NimBLEDevice::startAdvertising();
    }
};

class MyCallbacks : public BLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo) override {
      Serial.println(">>> onWrite callback TRIGGERED! <<<");
      std::string rxValue = pCharacteristic->getValue();
      
      if (rxValue.length() > 0) {
        Serial.print("Received BLE Payload: ");
        for (int i = 0; i < rxValue.length(); i++) {
          Serial.print(rxValue[i]);
        }
        Serial.println();
        
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, rxValue.c_str());
        if (!error) {
          newSSID = doc["ssid"].as<String>();
          newPass = doc["pass"].as<String>();
          credentialsReceived = true;
          Serial.println("Credentials successfully parsed!");
        } else {
          Serial.print("JSON Parse Error: ");
          Serial.println(error.c_str());
        }
      } else {
        Serial.println("Payload was empty!");
      }
    }
};

class MyScanCallbacks : public BLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
      Serial.println("WiFi Scan Requested...");
      
      // Ensure WiFi is in station mode and disconnected from any previous attempts
      WiFi.mode(WIFI_STA);
      WiFi.disconnect();
      delay(100);

      int n = WiFi.scanNetworks();
      
      // Retry once if it fails
      if (n < 0) {
         Serial.println("Scan failed. Retrying...");
         delay(500);
         n = WiFi.scanNetworks();
      }

      Serial.print("Scan done, found: ");
      Serial.println(n);
      
      StaticJsonDocument<1024> doc; 
      JsonArray networks = doc.to<JsonArray>();
      
      if (n > 0) {
        // Use a set-like approach to avoid duplicates if multiple APs have same SSID
        for (int i = 0; i < n; ++i) {
          bool exists = false;
          for (JsonVariant v : networks) {
            if (v.as<String>() == WiFi.SSID(i)) {
              exists = true;
              break;
            }
          }
          if (!exists && WiFi.SSID(i).length() > 0) {
            networks.add(WiFi.SSID(i));
          }
        }
      }
      
      String response;
      serializeJson(doc, response);
      pCharacteristic->setValue(response);
      WiFi.scanDelete();
    }
};

void setupBLE() {
  BLEDevice::init("eSRA_ESP32");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         NIMBLE_PROPERTY::WRITE
                                       );
  pCharacteristic->setCallbacks(new MyCallbacks());

  BLECharacteristic *pScanChar = pService->createCharacteristic(
                                         SCAN_CHARACTERISTIC_UUID,
                                         NIMBLE_PROPERTY::READ
                                       );
  pScanChar->setCallbacks(new MyScanCallbacks());

  pIpChar = pService->createCharacteristic(
                                         IP_CHARACTERISTIC_UUID,
                                         NIMBLE_PROPERTY::READ
                                       );
  pIpChar->setValue("0.0.0.0");
  
  pService->start();
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();
  Serial.println("BLE Started. Waiting for credentials...");
}

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>eSRA Local UI</title>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; margin: 0; padding: 20px; background-color: #f4f4f9; }
    h1 { color: #333; }
    .card { background: white; padding: 20px; margin: 10px auto; max-width: 400px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }
    .btn { padding: 10px 20px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; color: white; font-size: 16px; }
    .btn-auto { background-color: #007bff; }
    .btn-on { background-color: #28a745; }
    .btn-off { background-color: #dc3545; }
  </style>
</head>
<body>
  <h1>eSRA Control Panel</h1>
  <div class="card">
    <h3>Sensors</h3>
    <p>Temperature: <span id="temp">--</span> &deg;C</p>
    <p>Light: <span id="light">--</span></p>
  </div>
  <div class="card">
    <h3>Actuators</h3>
    <p>Mode: <span id="mode">--</span></p>
    <p>LED: <span id="led">--</span></p>
    <p>Fan: <span id="fan">--</span></p>
  </div>
  <div class="card">
    <h3>Controls</h3>
    <button class="btn btn-auto" onclick="sendCmd('mode', 'toggle')">Toggle Mode</button><br>
    <button class="btn btn-on" onclick="sendCmd('led', 'toggle')">Toggle LED</button>
    <button class="btn btn-off" onclick="sendCmd('fan', 'toggle')">Toggle Fan</button>
  </div>
  <script>
    var ws = new WebSocket('ws://' + window.location.hostname + ':81/');
    ws.onmessage = function(event) {
      var data = JSON.parse(event.data);
      document.getElementById('temp').innerText = data.temp;
      document.getElementById('light').innerText = data.light;
      document.getElementById('mode').innerText = data.mode;
      document.getElementById('led').innerText = data.led;
      document.getElementById('fan').innerText = data.fan;
    };
    function sendCmd(device, action) {
      ws.send(JSON.stringify({command: device, value: action}));
    }
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void broadcastState() {
  StaticJsonDocument<256> doc;
  doc["temp"] = currentTemp;
  doc["light"] = isDark ? "Dark" : "Bright";
  doc["mode"] = isAutoMode ? "AUTO" : "MANUAL";
  doc["led"] = isLedOn ? "ON" : "OFF";
  doc["fan"] = isFanOn ? "ON" : "OFF";
  doc["threshold"] = tempThreshold;
  String out;
  serializeJson(doc, out);
  webSocket.broadcastTXT(out);
}

void setMotorPower(bool on) {
  if (on && !isFanOn) {
    // Soft Start: Ramp up to avoid power surge
    for (int speed = 50; speed <= motorSpeed; speed += 10) {
      ledcWrite(MOTORPIN, speed);
      delay(20);
    }
    isFanOn = true;
  } else if (!on && isFanOn) {
    ledcWrite(MOTORPIN, 0);
    isFanOn = false;
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if(type == WStype_TEXT) {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      String cmd = doc["command"].as<String>();

      if (cmd == "mode") {
        isAutoMode = !isAutoMode;
      } else if (cmd == "led" && !isAutoMode) {
        isLedOn = !isLedOn;
        digitalWrite(LEDPIN, isLedOn ? HIGH : LOW);
      } else if (cmd == "fan" && !isAutoMode) {
        setMotorPower(!isFanOn);
      } else if (cmd == "threshold") {
        tempThreshold = doc["value"].as<float>();
        preferences.putFloat("threshold", tempThreshold);
      } else if (cmd == "reset") {
        Serial.println("Factory Reset Requested via App...");
        preferences.putString("ssid", "");
        preferences.putString("pass", "");
        preferences.end();
        delay(500);
        ESP.restart();
      }
      broadcastState();
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LEDPIN, OUTPUT);
  pinMode(LDRPIN, INPUT);

  // Configure PWM for motor (Arduino Core 3.x compatible)
  ledcAttach(MOTORPIN, freq, resolution);

  dht.begin();

  preferences.begin("esra", false);
  String savedSSID = preferences.getString("ssid", "");
  String savedPass = preferences.getString("pass", "");
  tempThreshold = preferences.getFloat("threshold", 30.0);

  if (savedSSID == "") {
    setupBLE();
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
      delay(500);
      Serial.print(".");
      retries++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi connected.");
      Serial.println(WiFi.localIP());
      
      if (MDNS.begin("esra")) {
        Serial.println("MDNS responder started: esra.local");
      }
      
      provisioned = true;
    } else {
      Serial.println("\nWiFi failed. Starting BLE.");
      setupBLE();
    }
  }

  if (provisioned) {
    server.on("/", handleRoot);
    server.begin();

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
  }
}

void updateActuatorsAuto() {
  if (!isAutoMode) return;

  if (isDark) {
    isLedOn = true;
  } else {
    isLedOn = false;
  }
  digitalWrite(LEDPIN, isLedOn ? HIGH : LOW);

  if (currentTemp > tempThreshold) {
    setMotorPower(true);
  } else {
    setMotorPower(false);
  }
}

void loop() {
  if (!provisioned && credentialsReceived) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    WiFi.begin(newSSID.c_str(), newPass.c_str());
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
      delay(500);
      Serial.print(".");
      retries++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nProvisioned successfully!");
      Serial.println(WiFi.localIP());
      
      if (pIpChar != NULL) {
        pIpChar->setValue(WiFi.localIP().toString());
      }
      
      if (MDNS.begin("esra")) {
        Serial.println("MDNS responder started: esra.local");
      }
      
      preferences.putString("ssid", newSSID);
      preferences.putString("pass", newPass);
      provisioned = true;
      if (pServer != NULL) {
         pServer->getAdvertising()->stop();
      }
      server.on("/", handleRoot);
      server.begin();
      webSocket.begin();
      webSocket.onEvent(webSocketEvent);
    } else {
      Serial.println("\nProvisioning failed. Try again.");
      credentialsReceived = false;
    }
  }

  if (provisioned) {
    server.handleClient();
    webSocket.loop();

    unsigned long currentMillis = millis();
    if (currentMillis - lastSensorRead >= SENSOR_INTERVAL) {
      lastSensorRead = currentMillis;

      float t = dht.readTemperature();
      if (!isnan(t)) {
        currentTemp = t;
      }

      int ldrVal = digitalRead(LDRPIN);
      isDark = (ldrVal == HIGH); // Adjust logic depending on your LDR module setup

      updateActuatorsAuto();
      broadcastState();
    }
  }
}
