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

#define STEPS_PER_REVOLUTION 200
enum TurntableMode
{
  TURNTABLE_CONTINUOUS,
  TURNTABLE_RELATIVE_MOVE
};
TurntableMode turntableMode = TURNTABLE_CONTINUOUS;

// WiFi credentials
#include "secrets.h"

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

// Web server
AsyncWebServer server(80);

bool motorEnabled = false;
bool wifiWasConnected = false;
unsigned long lastWifiAttempt = 0;
const unsigned long wifiRetryInterval = 10000;

void startWifiConnection()
{
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  lastWifiAttempt = millis();
  Serial.println("Connecting to WiFi...");
}

void handleWifiConnection()
{
  bool connected = WiFi.status() == WL_CONNECTED;

  if (connected)
  {
    if (!wifiWasConnected)
    {
      Serial.println("Connected to WiFi");
      Serial.println(WiFi.localIP());
      wifiWasConnected = true;
    }
    return;
  }

  if (wifiWasConnected)
  {
    Serial.println("WiFi connection lost");
    wifiWasConnected = false;
  }

  if (millis() - lastWifiAttempt >= wifiRetryInterval)
  {
    Serial.println("Retrying WiFi connection...");
    WiFi.disconnect(false, false);
    WiFi.begin(ssid, password);
    lastWifiAttempt = millis();
  }
}

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
  if (!motorEnabled)
  {
    return;
  }

  if (turntableMode == TURNTABLE_CONTINUOUS)
  {
    stepper.setSpeed(directionCW ? motorSpeed : -motorSpeed);
    stepper.runSpeed();
  }
  else
  {
    stepper.run();
    if (!stepper.isRunning())
    {
      disableMotor();
      turntableMode = TURNTABLE_CONTINUOUS;
    }
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

  // WiFi connection is handled in loop() so setup and motor control
  // remain responsive while the network is unavailable.
  startWifiConnection();

  // Stepper configuration
  stepper.setMaxSpeed(2000);
  stepper.setAcceleration(300);
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
      turntableMode = TURNTABLE_CONTINUOUS;

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

  server.on("/move/degrees", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
      if (index != 0)
      {
        return;
      }

      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, data, len);
      if (error)
      {
        sendJsonError(request, "Invalid JSON data");
        return;
      }

      if (doc["degrees"].isNull())
      {
        sendJsonError(request, "Missing degrees");
        return;
      }

      float degrees = doc["degrees"].as<float>();
      if (!isfinite(degrees) || degrees <= 0.0 || degrees > 360.0)
      {
        sendJsonError(request, "Degrees must be greater than 0 and no more than 360");
        return;
      }

      if (!doc["direction"].isNull())
      {
        String direction = doc["direction"].as<String>();
        if (direction != "CW" && direction != "CCW")
        {
          sendJsonError(request, "Direction must be CW or CCW");
          return;
        }
        directionCW = (direction == "CW");
      }

      if (!doc["speed"].isNull())
      {
        int speed = doc["speed"].as<int>();
        if (speed < 1 || speed > 2000)
        {
          sendJsonError(request, "Speed must be between 1 and 2000");
          return;
        }
        motorSpeed = speed;
      }

      long steps = lround(degrees * STEPS_PER_REVOLUTION / 360.0);
      if (steps < 1)
      {
        sendJsonError(request, "Degrees are too small for the configured step resolution");
        return;
      }

      stepper.setMaxSpeed(max(motorSpeed, 1));
      stepper.move(directionCW ? steps : -steps);
      enableMotor();
      turntableMode = TURNTABLE_RELATIVE_MOVE;

      JsonDocument responseDoc;
      JsonObject dataObj = responseDoc.createNestedObject("data");
      dataObj["degrees"] = degrees;
      dataObj["steps"] = steps;
      dataObj["direction"] = directionCW ? "CW" : "CCW";
      dataObj["speed"] = motorSpeed;
      dataObj["enabled"] = motorEnabled;
      dataObj["moving"] = motorEnabled;
      dataObj["mode"] = "relative_move";
      sendJsonSuccess(request, "Relative degree movement started", dataObj); });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    JsonDocument doc;
    JsonObject dataObj = doc.createNestedObject("data");
    bool isMoving = turntableMode == TURNTABLE_RELATIVE_MOVE
                        ? motorEnabled && stepper.isRunning()
                        : motorEnabled;
    dataObj["enabled"] = motorEnabled;
    dataObj["moving"] = isMoving;
    dataObj["direction"] = directionCW ? "CW" : "CCW";
    dataObj["speed"] = motorSpeed;
    dataObj["mode"] = turntableMode == TURNTABLE_RELATIVE_MOVE ? "relative_move" : "continuous";
    dataObj["positionDegrees"] = stepper.currentPosition() * 360.0 / STEPS_PER_REVOLUTION;
    sendJsonSuccess(request, "Status retrieved", dataObj); });
  // Static files
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  server.serveStatic("/styles.css", LittleFS, "/styles.css");

  server.begin();
}

void loop()
{
  handleWifiConnection();
  runTurntable();
}
