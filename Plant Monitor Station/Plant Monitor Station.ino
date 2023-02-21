/*
 *  Code by:  Dimitrios Vlachos
 *            dimitri.j.vlachos@gmail.com
 *
 *  Adapted from:
 *            
 *            https://makersportal.com/blog/2020/5/26/capacitive-soil-moisture-calibration-with-arduino
 *            UoL Lectures
 */

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Arduino_JSON.h>
#include "DHT.h"
#include <Servo.h>
#include <TaskScheduler.h>

/* * * * * * * * * * * * * * * */
#pragma region Networking
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

// For OTA
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// Allocate the JSON document
// Allows to allocated memory to the document dinamically.
DynamicJsonDocument doc(1024);

// Set the PORT for the web server
ESP8266WebServer server(80);

// The WiFi details
const char* ssid = "REPLACE_WITH_YOUR_SSID";
const char* password = "REPLACE_WITH_YOUR_PASSWORD";
#pragma endregion
/* * * * * * * * * * * * * * * */

Scheduler userScheduler;

// Prototype methods to insantiate the tasks with
void updateReadings();
void blinkOn();
void blinkLED(int blinks, long interval = TASK_SECOND / 10);  // Prototype for default parameter values

// Create tasks
Task taskUpdateReadings(TASK_SECOND * 5, TASK_FOREVER, &updateReadings);
Task taskLedBlink(TASK_SECOND / 2, 4, &blinkOn);

/* * * * * * * * * * * * * * * */

const int led_pin = D4;
const int dht_pin = D5;
const int servo_pin = D6;
const int pump_pin = D7;
const int soil_pin = A0;

// Values to calibrate for soil moisture sensor
const int air_value = 620;
const int water_value = 310;

// Initialise the components
DHT dht(dht_pin, DHT11);
Servo serv;

int soil_moisture_percent;
int temperature;
int humidity;

void setup() {
  Serial.begin(115200);

  pinMode(led_pin, OUTPUT);
  pinMode(pump_pin, OUTPUT);

  // Start the components
  dht.begin();
  serv.attach(servo_pin);

  // serv.write(0);
  // delay(2000);
  // serv.write(90);
  // delay(2000);
  // digitalWrite(pump_pin, HIGH);
  // delay(2000);
  // digitalWrite(pump_pin, LOW);

  updateReadings();

#pragma region WiFi Setup
  //Connect to the WiFi network
  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Waiting to connect...");
  }

#pragma region OTA
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("Plant Monitor Station");

  // No authentication by default
  ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
#pragma endregion

  //Print the board IP address
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", get_index);     // Get the index page on root route
  server.on("/json", get_json);  // Get the json data on the '/json' route

  server.begin();  //Start the server
  Serial.println("Server listening");
#pragma endregion

  userScheduler.addTask(taskUpdateReadings);
  userScheduler.addTask(taskLedBlink);
  taskUpdateReadings.enable();
}

void loop() {
  // Handle OTA events
  ArduinoOTA.handle();
  //Handling of incoming client requests
  server.handleClient();
  userScheduler.execute();
}

/* * * * * * * * * * * * * * * */

void updateReadings() {
  soil_moisture_percent = getSoilMoisture();
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  Serial.print("Temp: ");
  Serial.println(temperature);
  Serial.print("Hum: ");
  Serial.println(humidity);
  Serial.print("Soil: ");
  Serial.println(soil_moisture_percent);
}

int getSoilMoisture() {
  // Get voltage reading
  int soil_moisture_value = analogRead(soil_pin);
  // Convert into 'calibrated' percentage (need to calibrate)
  int percentageHumidity = map(soil_moisture_value, air_value, water_value, 100, 0);
  return percentageHumidity;
}

#pragma region Server code

void get_index() {

  String html = "<!DOCTYPE html> <html> ";
  html += "<head><meta http-equiv=\"refresh\" content=\"2\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"></head>";
  html += "<body> <h1>Greenhouse Base Station Dashboard</h1>";
  html += "<p> Welcome to the base station dashboard </p>";
  html += "<div> <p> <strong> Soil moisture: ";
  html += soil_moisture_percent;
  html += "</strong> % </p>";
  html += "<div> <p> <strong> temperature: ";
  html += temperature;
  html += "</strong> °C </p>";
  html += "<div> <p> <strong> Humidity: ";
  html += humidity;
  html += "</strong> % </p>";
  html += "</div>";
  html += "</body> </html>";

  //Print a welcoming message on the index page
  server.send(200, "text/html", html);
}

// Utility function to send JSON data
void get_json() {

  // Create JSON data
  updateJSON();

  // Make JSON data ready for the http request
  String jsonStr;
  serializeJsonPretty(doc, jsonStr);

  // Send the JSON data
  server.send(200, "application/json", jsonStr);
}

void updateJSON() {
  // Add JSON request data
  doc["Content-Type"] = "application/json";
  doc["Status"] = 200;

  // Add distance sensor JSON object data
  JsonObject sensorReadings = doc.createNestedObject("Readings");
  sensorReadings["soil_moisture_percent"] = soil_moisture_percent;
  sensorReadings["temperature"] = temperature;
  sensorReadings["humidity"] = humidity;
}

#pragma endregion

#pragma region LED Blink functions
void blinkLED(int blinks, long interval) {
  taskLedBlink.setInterval(interval);
  taskLedBlink.setIterations(blinks * 2);  // Double the iterations as on->off->on is two cycles, not one
  taskLedBlink.enable();
}

void blinkOn() {
  LEDOn();
  taskLedBlink.setCallback(&blinkOff);

  if (taskLedBlink.isLastIteration()) {
    LEDOff();
  }
}

void blinkOff() {
  LEDOff();
  taskLedBlink.setCallback(&blinkOn);

  if (taskLedBlink.isLastIteration()) {
    LEDOff();
  }
}

inline void LEDOn() {
  digitalWrite(led_pin, HIGH);
}

inline void LEDOff() {
  digitalWrite(led_pin, LOW);
}
#pragma endregion

/*
void loop() {
  checkBattery();
  delay(1000);
}

int checkBattery() {
  float batteryLevel = map(analogRead(A0), 0.0f, 4095.0f, 0, 100);
  Serial.println(batteryLevel);
}*/