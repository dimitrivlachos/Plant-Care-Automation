/*
 *  Code by:  Dimitrios Vlachos
 *            dimitri.j.vlachos@gmail.com
 *
 *  Adapted from:
 *            UoL Lectures
 *            https://randomnerdtutorials.com/esp8266-nodemcu-http-get-post-arduino/
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
#include <ESP8266HTTPClient.h>
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

void updateReadings();
void updateDisplay();
void oledSaver();
void blinkOn();
void blinkLED(int blinks, long interval = TASK_SECOND / 10);  // Prototype for default parameter values
void getMonitorReadings();
void resetButtonDebounce();

//Create tasks
Task taskUpdateReadings(TASK_SECOND * 1, TASK_FOREVER, &updateReadings);
Task taskUpdateDisplay(TASK_SECOND * 1, TASK_FOREVER, &updateDisplay);
Task taskOledSaver(TASK_MINUTE * 5, TASK_FOREVER, &oledSaver);
Task taskLedBlink(TASK_SECOND / 2, 4, &blinkOn);
Task taskCheckMonitor(TASK_SECOND * 10, TASK_FOREVER, &getMonitorReadings);
Task taskDebounceReset(TASK_SECOND * 1, TASK_FOREVER, &resetButtonDebounce);

/* * * * * * * * * * * * * * * */

const int led_pin = D4;
const int debug_button = D5;

const String monitorAddress = "http://192.168.0.202";

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

bool debouncer = false;

/* * * * * * * * * * * * * * * */

void setup() {
  // Start serial communication
  Serial.begin(115200);
  // Set led pin mode (integrated LED)
  pinMode(led_pin, OUTPUT);
  // Set the button pin mode
  pinMode(debug_button, INPUT);

  // Initialise onboard sensors
  initBME();
  lightMeter.begin();

  // Populate initial sensor data
  updateReadings();

  // Initialise text position
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
  userScheduler.addTask(taskCheckMonitor);
  userScheduler.addTask(taskDebounceReset);
  taskUpdateReadings.enable();
  taskUpdateDisplay.enable();
  taskOledSaver.enable();
  taskCheckMonitor.enable();

  setupJSON();
}

void loop() {
  // Handle OTA events
  ArduinoOTA.handle();
  // Handling of incoming client requests
  server.handleClient();
  // Handling async tasks
  userScheduler.execute();
  // Check the button
  checkButton();
}

void checkButton() {
  // Guard statement for software debouncing
  if (debouncer) {
    return;
  }
  // read the state of the pushbutton value:
  int buttonState = digitalRead(debug_button);

  if (buttonState == HIGH) {
    Serial.println("Button Pressed");
    sendWaterCommand();
    debouncer = true;
    taskDebounceReset.enableDelayed(TASK_SECOND);
  }
}

/**
 * Processes JSON data received from monitoring station
 * and sends necessary commands back to the station
 */
void handleMonitor(JSONVar j) {
  int soil_moisture_percent = j["Readings"]["soil_moisture_percent"];
  int monitor_temp = j["Readings"]["temperature"];
  int monitor_hum = j["Readings"]["humidity"];
  bool isServoOpen = j["States"]["servo_state"];
  bool isPumping = j["States"]["pump_state"];

  // If the pump is not running, check moisture levels
  if (!isPumping) {
    // If the moisture content is low, tell the plant monitor to water
    if (soil_moisture_percent < 30) {
      Serial.println("Soil moisture detected to be low, watering");
      sendWaterCommand();
    }
  }

  // If the servo (lid) is closed
  if (!isServoOpen) {
    // Then check if it is too hot
    if (monitor_temp > 24) {
      // If it is too hot, check if it is colder outside
      float difference = temperature - monitor_temp;
      if (difference < -2) {
        // Opening the lid will cool the plant
        Serial.println("Temp higher than ambient, opening");
        changeServoState(1);
      }
    }
  }

  // If the servo (lid) is open
  if (isServoOpen) {
    // Check if it is cold
    if (monitor_temp < 14) {
      // It is too cold, check if it is warmer outside
      float difference = temperature - monitor_temp;
      if (difference < 2) {
        // Closing the lid will warm the plant
        Serial.println("Temp lower than ambient, closing");
        changeServoState(0);
      }
    }
  }

  // If there are less than 100 lumens
  if (lux < 100) {
    // Turn on the light
    Serial.println("Light levels low, activating LED");
    changeLightState(1);
  }

  // If there are less than 100 lumens
  if (lux > 1000) {
    // Turn off the lights
    Serial.println("Light levels sufficient, deactivating LED");
    changeLightState(0);
  }
}

void sendWaterCommand() {
  sendCommand("/waterplant");
}

void changeServoState(int state) {
  sendState("/setservo", state);
}

void changeLightState(int state) {
  sendState("/setlight", state);
}

void sendState(String command, int state) {
  // Make sure we are still connected to the wifi
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Sending state command: ");
    Serial.println(command + state);
    
    WiFiClient client;
    HTTPClient http;

    String serverPath = monitorAddress + command + "?state=" + state;

    // Your Domain name with URL path or IP address with path
    http.begin(client, serverPath.c_str());

    // Send HTTP GET request
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);

      //String payload = http.getString();
      //JSONVar jsonObject = JSON.parse(payload);
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    // Free resources
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

void sendCommand(String command) {
  // Make sure we are still connected to the wifi
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Sending command: ");
    Serial.println(command);

    WiFiClient client;
    HTTPClient http;

    String serverPath = monitorAddress + command;

    // Your Domain name with URL path or IP address with path
    http.begin(client, serverPath.c_str());

    // Send HTTP GET request
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);

      //String payload = http.getString();
      //JSONVar jsonObject = JSON.parse(payload);
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }

    http.end();

  } else {
    Serial.println("WiFi Disconnected");
  }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * *
 * This region contains functions that get called by *
 * the task Scheduler, allowing for asynchronous     *
 * task execution without the need for using delay   *
 * functions. Check above to see how often each of   *
 * these tasks get pinged.                           *
 * * * * * * * * * * * * * * * * * * * * * * * * * * */
#pragma region Tasks
/**
 * Updates global sensor reading variables with
 * new information from the sensors
 */
void updateReadings() {
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  pressure = bme.readPressure() / 100.0F;
  lux = lightMeter.readLightLevel();
}

/**
 * Updates data on display, using the co-ordinates
 * set by the oledSaver() function as the starting
 * location for writing
 */
void updateDisplay() {
  // Each character is 6x8 pixels

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  /*
   * using setCursor in increments of 8 is equivalent
   * to using display.println(). However, by doing it
   * this way we are able to 'shift' the text around
   * the screen, allowing us to implement the
   * oledSaver() function to prevent burn in on data
   * displayed for extended periods of time.
   */

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

/**
 * Randomly shifts the starting location of text on
 * the OLED output in order to prevent burn-in when
 * left on for extended periods.
 *
 * The exact 'start region' possible has been
 * manually set to fit the text displayed by the
 * updateDisplay() function
 */
void oledSaver() {
  int new_x = random(0, 20);
  int new_y = random(16, 33);
  screen_x = new_x;
  screen_y = new_y;
}

/**
 * Requests JSON data from monitoring station
 * using REST API based requests
 */
void getMonitorReadings() {
  // Make sure we are still connected to the wifi
  if (WiFi.status() == WL_CONNECTED) {
    //Serial.println("Attempting to receive info");
    WiFiClient client;
    HTTPClient http;

    String serverPath = monitorAddress + "/json";  // + "?temperature=24.37";

    // Your Domain name with URL path or IP address with path
    http.begin(client, serverPath.c_str());

    // Send HTTP GET request
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      //Serial.print("HTTP Response code: ");
      //Serial.println(httpResponseCode);

      String payload = http.getString();
      JSONVar jsonObject = JSON.parse(payload);

      handleMonitor(jsonObject);
      //Serial.println(payload);
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    // Free resources
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

/**
 * Resets bool used for software debouncing 
 * to prevent the button press firing more 
 * than once per second
 */
void resetButtonDebounce() {
  debouncer = false;
  taskDebounceReset.disable();
}
#pragma endregion

/* * * * * * * * * * * * * * * * * * * * * * * * * * *
 * This region contains functions necessary for the  *
 * webserver to run, enabling the dashboard and REST *
 * requests to be made.                              *
 * * * * * * * * * * * * * * * * * * * * * * * * * * */
#pragma region Network code

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
  doc["Readings"]["temperature"] = temperature;
  doc["Readings"]["humidity"] = humidity;
  doc["Readings"]["pressure"] = pressure;
  doc["Readings"]["lux"] = lux;
}

void setupJSON() {
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

/* * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Task based LED blinking functionality             *
 * * * * * * * * * * * * * * * * * * * * * * * * * * */
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

/**
 * Initialises BME280 sensor
 */
void initBME() {
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1)
      ;
  }
}