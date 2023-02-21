/*
 *  Code by:  Dimitrios Vlachos
 *            dimitri.j.vlachos@gmail.com
 *
 *  Adapted from:
 *            
 *            UoL Lectures
 */

#include <Arduino_JSON.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Wire.h>
#include <BH1750.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
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

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels

Scheduler userScheduler;  // to control your personal task

// Prototype methods to insantiate the tasks with
void pingNodeCycle();
void updateReadings();
void updateDisplay();
void oledSaver();
void blinkOn();
void blinkLED(int blinks, long interval = TASK_SECOND / 10);  // Prototype for default parameter values

//Create tasks
Task taskUpdateReadings(TASK_SECOND * 1, TASK_FOREVER, &updateReadings);
Task taskUpdateDisplay(TASK_SECOND * 1, TASK_FOREVER, &updateDisplay);
Task taskOledSaver(TASK_MINUTE * 5, TASK_FOREVER, &oledSaver);
Task taskLedBlink(TASK_SECOND / 2, 4, &blinkOn);

/* * * * * * * * * * * * * * * */

const int led_pin = D4;

// BME object on the default I2C pins
Adafruit_BME280 bme;
// BH1750 object on the default I2C pins
BH1750 lightMeter;

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Text positions
int screen_x;
int screen_y;

// BME and BH readings
float temperature;
float humidity;
float pressure;
float lux;

// Data from plant stations
int soil_moisture_percent;


/* * * * * * * * * * * * * * * */

void setup() {
  Serial.begin(115200);

  pinMode(led_pin, OUTPUT);

  initBME();
  lightMeter.begin();

  updateReadings();

  oledSaver();

  // Initialise display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    for (;;)
      ;  // Don't proceed, loop forever
  }

  delay(2000);
  display.clearDisplay();

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
  ArduinoOTA.setHostname("Base Station");

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
  userScheduler.addTask(taskUpdateDisplay);
  userScheduler.addTask(taskOledSaver);
  userScheduler.addTask(taskLedBlink);
  taskUpdateReadings.enable();
  taskUpdateDisplay.enable();
  taskOledSaver.enable();
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
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  pressure = bme.readPressure() / 100.0F;
  lux = lightMeter.readLightLevel();
}

void updateDisplay() {
  // Each character is 6x8 pixels

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  //display.setCursor(0, 16); // This takes it down to the blue area

  display.setCursor(screen_x, screen_y);
  display.print("Temp: \t");
  display.print(String(temperature, 2));
  display.print("C");

  display.setCursor(screen_x, screen_y + 8);
  display.print("Hum:  \t");
  display.print(String(humidity, 2));
  display.print("%");

  display.setCursor(screen_x, screen_y + 16);
  display.print("Pres: \t");
  display.print(String(pressure, 2));
  display.print(" hPa");

  display.setCursor(screen_x, screen_y + 24);
  display.print("Lux:  \t");
  display.print(String(lux, 2));
  display.print(" lx");

  display.display();
}

void oledSaver() {
  int new_x = random(0, 20);
  int new_y = random(16, 33);
  screen_x = new_x;
  screen_y = new_y;
}

//Init BME280
void initBME() {
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1)
      ;
  }
}

#pragma region Server code

void get_index() {

  String html = "<!DOCTYPE html> <html> ";
  html += "<head><meta http-equiv=\"refresh\" content=\"2\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"></head>";
  html += "<body> <h1>Greenhouse Base Station Dashboard</h1>";
  html += "<p> Welcome to the base station dashboard </p>";
  html += "<div> <p> <strong> Temperature: ";
  html += temperature;
  html += "</strong> °C </p>";
  html += "<div> <p> <strong> Humidity: ";
  html += humidity;
  html += "</strong> % </p>";
  html += "<div> <p> <strong> Pressure: ";
  html += pressure;
  html += "</strong> hPa </p>";
  html += "<div> <p> <strong> Light levels: ";
  html += lux;
  html += "</strong> lux </p>";
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
  sensorReadings["temperature"] = temperature;
  sensorReadings["humidity"] = humidity;
  sensorReadings["pressure"] = pressure;
  sensorReadings["lux"] = lux;
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