#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <AccelStepper.h>
#include <ArduinoJson.h>

// Define stepper motor connections and interface type
#define DIR_PIN 3  // Direction pin
#define STEP_PIN 4 // Step pin
#define EN_PIN 2   // Enable pin (DRV8825 enable)
#define MOTOR_INTERFACE_TYPE AccelStepper::DRIVER

AccelStepper stepper(MOTOR_INTERFACE_TYPE, STEP_PIN, DIR_PIN);

bool directionCW = true;
int motorSpeed = 500;

// WiFi credentials
#include "secrets.h"

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

// Web server
AsyncWebServer server(80);

bool motorEnabled = false;

// JSON Helper Functions
void sendJsonSuccess(AsyncWebServerRequest *request, const char *message, JsonObject &data)
{
  JsonDocument doc;
  doc["success"] = true;
  doc["message"] = message;
  if (!data.isNull())
  {
    doc["data"] = data;
  }
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void sendJsonError(AsyncWebServerRequest *request, const char *message)
{
  JsonDocument doc;
  doc["success"] = false;
  doc["message"] = message;
  String response;
  serializeJson(doc, response);
  request->send(400, "application/json", response);
}
void enableMotor()
{
  digitalWrite(EN_PIN, LOW);
  delay(100); // Let driver stabilize
  motorEnabled = true;
  Serial.println("Motor On");
}

void disableMotor()
{
  digitalWrite(EN_PIN, HIGH);
  motorEnabled = false;
  Serial.println("Motor Off");
}

void runTurntable()
{
  if (motorEnabled)
  {
    stepper.setSpeed(directionCW ? motorSpeed : -motorSpeed);
    stepper.runSpeed();
  }
}

void setup()
{
  Serial.begin(115200);

  // Give USB Serial/JTAG time to enumerate and the serial monitor time to open.
  unsigned long serialStart = millis();
  while (!Serial && millis() - serialStart < 3000)
  {
    delay(10);
  }

  Serial.println("\nESP32-C3 booting");

  // Mount LittleFS
  if (!LittleFS.begin())
  {
    Serial.println("LittleFS mount failed. Trying to format...");
    if (LittleFS.format() && LittleFS.begin())
    {
      Serial.println("LittleFS formatted and mounted successfully.");
    }
    else
    {
      Serial.println("LittleFS format failed!");
      return;
    }
  }

  // WiFi connection
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.println(WiFi.localIP());

  // Stepper configuration
  stepper.setMaxSpeed(2000);
  stepper.setSpeed(0);
  pinMode(EN_PIN, OUTPUT);
  disableMotor();
  Serial.println("Turntable motor initialized");

  // Turntable control
  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
      if (index != 0)
      {
        return;
      }

      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, data, len);
      if (error)
      {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        sendJsonError(request, "Invalid JSON data");
        return;
      }

      if (doc["enabled"].isNull() || doc["direction"].isNull() || doc["speed"].isNull())
      {
        sendJsonError(request, "Missing enabled, direction or speed");
        return;
      }

      bool enabled = doc["enabled"].as<bool>();
      String direction = doc["direction"].as<String>();
      int speed = doc["speed"].as<int>();

      if (direction != "CW" && direction != "CCW")
      {
        sendJsonError(request, "Direction must be CW or CCW");
        return;
      }
      if (speed < 0 || speed > 2000)
      {
        sendJsonError(request, "Speed must be between 0 and 2000");
        return;
      }

      directionCW = (direction == "CW");
      motorSpeed = speed;

      if (enabled && motorSpeed > 0)
      {
        enableMotor();
      }
      else
      {
        motorEnabled = false;
        disableMotor();
      }

      JsonDocument responseDoc;
      JsonObject dataObj = responseDoc.createNestedObject("data");
      dataObj["enabled"] = motorEnabled;
      dataObj["direction"] = directionCW ? "CW" : "CCW";
      dataObj["speed"] = motorSpeed;
      sendJsonSuccess(request, motorEnabled ? "Turntable started" : "Turntable stopped", dataObj); });
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    JsonDocument doc;
    JsonObject dataObj = doc.createNestedObject("data");
    dataObj["enabled"] = motorEnabled;
    dataObj["moving"] = motorEnabled;
    dataObj["direction"] = directionCW ? "CW" : "CCW";
    dataObj["speed"] = motorSpeed;
    sendJsonSuccess(request, "Status retrieved", dataObj); });
  // Static files
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  server.serveStatic("/styles.css", LittleFS, "/styles.css");

  server.begin();
}

void loop()
{
  runTurntable();
}
