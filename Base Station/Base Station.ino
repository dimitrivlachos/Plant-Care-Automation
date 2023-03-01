/*
 *  Code by:  Dimitrios Vlachos
 *            dimitri.j.vlachos@gmail.com
 *
 *  Adapted from:
 *            UoL Lectures
 *            https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/
 *            https://randomnerdtutorials.com/esp8266-nodemcu-http-get-post-arduino/
 *            https://randomnerdtutorials.com/esp8266-0-96-inch-oled-display-with-arduino-ide/
 *            https://arduinojson.org/
 *
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
// Allows to allocated memory to the document dynamically.
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

Scheduler userScheduler;

/* * * * * * * * * * * * * * * */
// Function prototypes
/**
 * Updates global sensor reading variables with
 * new information from the sensors
 */
void updateReadings();
void updateDisplay();
void oledSaver();
void blinkOn();
/*
 * blink the onboard LED
 *
 * @param blinks Number of times to blink the LED
 * @param interval Delay between blinks
 */
void blinkLED(int blinks, long interval = TASK_SECOND / 10);  // Prototype for default parameter values
void getMonitorReadings();
void resetButtonDebounce();
void sendState(String command, int state = -1);

/* * * * * * * * * * * * * * * */
//Create tasks
Task taskUpdateReadings(TASK_SECOND * 1, TASK_FOREVER, &updateReadings);
Task taskUpdateDisplay(TASK_SECOND * 1, TASK_FOREVER, &updateDisplay);
Task taskOledSaver(TASK_MINUTE * 5, TASK_FOREVER, &oledSaver);
Task taskLedBlink(TASK_SECOND / 2, 4, &blinkOn);
Task taskCheckMonitor(TASK_SECOND * 10, TASK_FOREVER, &getMonitorReadings);
Task taskDebounceReset(TASK_SECOND * 1, TASK_FOREVER, &resetButtonDebounce);

/* * * * * * * * * * * * * * * */
// Pin declarations
const int led_pin = D4;
const int button = D5;

// Pins for ultrasonic sensor
const int trig_pin = D6;
const int echo_pin = D7;

// Monitor node address
const String monitorAddress = "http://192.168.0.202";

/* * * * * * * * * * * * * * * */
// Sensor declarations

// BME object on the default I2C pins
Adafruit_BME280 bme;
// BH1750 object on the default I2C pins
BH1750 lightMeter;
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

/* * * * * * * * * * * * * * * */
// Dynamic global variables

// Text positions - see @oledSaver()
int screen_x;
int screen_y;

// Sensor readings
float temperature;
float humidity;
float pressure;
float lux;
int fill_percent;

// Flag for software debouncing
bool debouncer = false;

// Own IP address
String IP;

/* * * * * * * * * * * * * * * */

void setup() {
  // Start serial communication
  Serial.begin(115200);
  // Set led pin mode (integrated LED)
  pinMode(led_pin, OUTPUT);
  // Set the button pin mode
  pinMode(button, INPUT);
  // Set pins for distance sensor
  pinMode(trig_pin, OUTPUT);  // Sets the trigPin as an Output
  pinMode(echo_pin, INPUT);   // Sets the echoPin as an Input

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

  #pragma region OTA Setup
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
  IP = WiFi.localIP().toString();
  Serial.println(IP);

  server.on("/", get_index);     // Get the index page on root route
  server.on("/json", get_json);  // Get the json data on the '/json' route
  server.on("/waterplant", sendWaterCommand); // Activates te sendWaterCommand to send to the monitor node

  server.begin();  //Start the server
  Serial.println("Server listening");
  #pragma endregion

  // Async task setup
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

  // Initialise JSON object structure
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

/*
 * Check for button press and implement
 * software-ased debouncing.
 */
void checkButton() {
  // Guard statement for software debouncing
  if (debouncer) {
    return;
  }
  // read the state of the pushbutton value:
  int buttonState = digitalRead(button);

  if (buttonState == HIGH) {
    Serial.println("Button Pressed");
    sendWaterCommand();
    debouncer = true;
    taskDebounceReset.enableDelayed(TASK_SECOND);
  }
}

/**
 * Processes JSON data received from monitoring station
 * and sends necessary commands back to the station.
 *
 * @param j JSONVar received from monitor station.
 */
void handleMonitor(JSONVar j) {
  // Parse JSON data
  int soil_moisture_percent = j["Readings"]["soil_moisture_percent"];
  int monitor_temp = j["Readings"]["temperature"];
  int monitor_hum = j["Readings"]["humidity"];
  bool isServoOpen = j["States"]["servo_state"];
  bool isPumping = j["States"]["pump_state"];
  bool light_state = j["States"]["light_state"];

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
  if (!light_state) {
    if (lux < 100) {
      // Turn on the light
      Serial.println("Light levels low, activating LED");
      changeLightState(1);
    }
  }

  // If there are less than 100 lumens
  if (light_state) {
    if (lux > 1000) {
      // Turn off the lights
      Serial.println("Light levels sufficient, deactivating LED");
      changeLightState(0);
    }
  }
}

// Send monitor command to conduct 'water' operation
void sendWaterCommand() {
  sendCommand("/waterplant");
}

// Set servo/lid state of the greenhouse
void changeServoState(int state) {
  sendState("/setservo", state);
}

// Set light state of the greenhouse
void changeLightState(int state) {
  sendState("/setlight", state);
}

/*
 * Checks the remaining water percentage and
 * updates global reading variable
 */
int getFillLevel() {
  const int empty = 30;  // 30cm to bottom of water tank
  const int full = 10;   // 10cm between top of tank and sensor

  int distance = distanceCentimeter();

  // Map the distance values to percentages
  int percent = map(distance, 30, 10, 100, 0);
  // constrain percentages to make sense
  percent = constrain(percent, 0, 100);

  return percent;
}

/*
 * Pulses the ultrasonic distance sensor 
 * and calculates a distance in cm
 *
 * @return distance in cm to the water's surface
 */
int distanceCentimeter() {
  // Clears the trigPin
  digitalWrite(trig_pin, LOW);
  delayMicroseconds(2);

  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trig_pin, HIGH);
  delayMicroseconds(10);

  // Clears the trigPin
  digitalWrite(trig_pin, LOW);

  // Reads the echoPin, returns the sound wave travel time in microseconds
  long duration = pulseIn(echo_pin, HIGH);

  // Calculating the distance in cm
  int distance = (duration * 0.034) / 2;
  /* 
  *  pulseIn measures the time taken between the pulse and when the pin goes HIGH on echo.
  *  We then multiply by the speed of sound (0.034cm per microsecond)
  *  Then we divide by two, as it is the time it has taken to both travel to the target and return
  */
  return distance;
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
  fill_percent = getFillLevel();
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

  display.setCursor(screen_x, 0); // This stays in the yellow 'header' section of the OLED display, but shifts around horizontally
  display.print("IP: ");
  display.print(IP);

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
  Serial.println("Getting monitor readings");
  // Make sure we are still connected to the wifi
  if (WiFi.status() == WL_CONNECTED) {
    //Serial.println("Attempting to receive info");
    WiFiClient client;
    HTTPClient http;

    String serverPath = monitorAddress + "/json";

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
// Web dashboard HTML
#pragma region HTML
/*
 * Here we create the static web dashboard for this controller.
 * By using the variable modifier PROGMEM, we store this code
 * in the flash memory instead of into SRAM. This frees up a
 * considerable amount of RAM that would otherwise just hold
 * onto a static string.
 */
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Greenhouse Base Station Dashboard</title>
    <style>
      body {
        font-family: Arial, sans-serif;
        margin: 0;
        padding: 0;
      }
      header {
        background-color: #4CAF50;
        color: white;
        padding: 20px;
        text-align: center;
      }
      .card {
        border-radius: 25px;
        box-shadow: 0 0 10px rgba(0, 0, 0, 0.2);
        padding: 20px;
        text-align: center;
        margin: 20px auto;
        max-width: 400px;
      }
      p {
        margin: 0;
        font-size: 18px;
        text-align: center;
      }
      strong {
        font-size: 24px;
      }
      button {
        margin: 20px;
        padding: 10px 20px;
        font-size: 16px;
        border-radius: 25px;
        background-color: #4CAF50;
        color: white;
        border: none;
        cursor: pointer;
      }
    </style>
  </head>
  <body>
    <header>
      <h1>Greenhouse Base Station Dashboard</h1>
    </header>
    <div>
      <p> </p>
      <p>Welcome to the base station dashboard!</p>
      <p>These are the ambient room conditions outside of the greenhouse:</p>
    </div>
    <div class="card">
      <p><strong>Temperature:</strong> <span id="temperature"></span> &degC</p>
    </div>
    <div class="card">
      <p><strong>Humidity:</strong> <span id="humidity"></span> %</p>
    </div>
    <div class="card">
      <p><strong>Pressure:</strong> <span id="pressure"></span> hPa</p>
    </div>
    <div class="card">
      <p><strong>Light levels:</strong> <span id="lux"></span> lux</p>
    </div>
    <div class="card">
      <p><strong>Fill level:</strong> <span id="fill-percent"></span> %</p>
      <button onclick="waterPlant()">Water plant now</button>
    </div>

    <script>
      function updateValues() {
        const url = window.location.href + 'json';

        fetch(url)
          .then(response => response.json())
          .then(data => {
            const temperature = data.Readings.temperature;
            const humidity = data.Readings.humidity;
            const pressure = data.Readings.pressure;
            const lux = data.Readings.lux;
            const fill_percent = data.Readings.fill_percent;

            document.getElementById('temperature').innerHTML = temperature.toFixed(2);
            document.getElementById('humidity').innerHTML = humidity.toFixed(2);
            document.getElementById('pressure').innerHTML = pressure.toFixed(2);
            document.getElementById('lux').innerHTML = lux.toFixed(2);
            document.getElementById('fill-percent').innerHTML = fill_percent;
          })
          .catch(error => {
            console.error('Error fetching data:', error);
          });
      }

      function waterPlant() {
        const url = window.location.href + 'waterplant';

        fetch(url, { method: 'POST' })
          .then(response => {
            if (response.ok) {
              console.log('Plant watered successfully!');
            } else {
              console.error('Error watering plant:', response.statusText);
            }
          })
          .catch(error => {
            console.error('Error watering plant:', error);
          });
      }
      
      updateValues();
      setInterval(updateValues, 5000);
    </script>
  </body>
</html>
)rawliteral";
#pragma endregion

// Function called to send webpage to client
void get_index() {
  // Blink LED for visual queue
  blinkLED(3);
  //Send the webpage
  server.send(200, "text/html", index_html);
}

// Function called to send JSON data to client
void get_json() {
  // Blink LED for visual queue
  blinkLED(2);
  // Create JSON data
  updateJSON();
  // Make JSON data ready for the http request
  String jsonStr;
  serializeJsonPretty(doc, jsonStr);
  // Send the JSON data
  server.send(200, "application/json", jsonStr);
}

/*
 * Update JSON doc with most up-to-date
 * readings from the sensors.
 */
void updateJSON() {
  doc["Readings"]["temperature"] = temperature;
  doc["Readings"]["humidity"] = humidity;
  doc["Readings"]["pressure"] = pressure;
  doc["Readings"]["lux"] = lux;
  doc["Readings"]["fill_percent"] = fill_percent;
}

/*
 * Initialise JSON doc file structure and
 * populate the fields with initial
 * sensor values.
 */
void setupJSON() {
  // Add JSON request data
  doc["Content-Type"] = "application/json";
  doc["Status"] = 200;

  // Add sensor readings to JSON object data
  JsonObject sensorReadings = doc.createNestedObject("Readings");
  sensorReadings["temperature"] = temperature;
  sensorReadings["humidity"] = humidity;
  sensorReadings["pressure"] = pressure;
  sensorReadings["lux"] = lux;
  sensorReadings["fill_percent"] = fill_percent;
}

/*
 * Sends REST-based command to change the state of
 * the monitor node actuators.
 *
 * @param command Command URL to send
 * @param state State to set the actuator to (Optional)
 */
void sendState(String command, int state) {
  // Make sure we are still connected to the wifi
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Sending state command: ");
    Serial.println(command + state);

    WiFiClient client;
    HTTPClient http;

    String serverPath = monitorAddress + command;
    if(state >= 0) serverPath += "?state=" + String(state);
    Serial.println(serverPath);
    // Your Domain name with URL path or IP address with path
    http.begin(client, serverPath.c_str());

    // Send HTTP GET request
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);

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

/*
 * Sends a stateless command to the monitor station
 * in order to execute some function.
 *
 * @param command Command URL to send
 */
void sendCommand(String command) {
  sendState(command);
}
#pragma endregion

/* * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Task based LED blinking functionality.            *
 * This works by using a Task to callback on         *
 * alternating 'on-off' functions.                   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * */
#pragma region LED Blink functions
/*
 * blink the onboard LED
 *
 * @param blinks Number of times to blink the LED
 * @param interval Delay between blinks
 */
void blinkLED(int blinks, long interval) {
  taskLedBlink.setInterval(interval);
  taskLedBlink.setIterations(blinks * 2);  // Double the iterations as on->off->on is two cycles, not one
  taskLedBlink.enable();
}

// Turns LED on and changes task callback to turn off on next iteration
void blinkOn() {
  LEDOn();
  taskLedBlink.setCallback(&blinkOff);

  if (taskLedBlink.isLastIteration()) {
    LEDOff();
  }
}

// Turns LED off and changes task callback to turn on on next iteration
void blinkOff() {
  LEDOff();
  taskLedBlink.setCallback(&blinkOn);

  if (taskLedBlink.isLastIteration()) {
    LEDOff();
  }
}

// Writes LED state to pin
inline void LEDOn() {
  digitalWrite(led_pin, HIGH);
}

// Writes LED state to pin
inline void LEDOff() {
  digitalWrite(led_pin, LOW);
}
#pragma endregion

/**
 * Initialises BME280 sensor
 */
void initBME() {
  // Blink LED for visual queue
  blinkLED(4);
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1)
      ;
  }
}