/*
 *  Code by:  Dimitrios Vlachos
 *            dimitri.j.vlachos@gmail.com
 *
 *  Adapted from:
 *            UoL Lectures
 *            https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/
 *            https://makersportal.com/blog/2020/5/26/capacitive-soil-moisture-calibration-with-arduino
 *            https://randomnerdtutorials.com/esp8266-nodemcu-http-get-post-arduino/
 *            https://arduinojson.org/
 *
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

Scheduler userScheduler;

/* * * * * * * * * * * * * * * */
// Function prototypes
/**
 * Updates global sensor reading variables with
 * new information from the sensors
 */
void updateReadings();
void blinkOn();
/*
 * blink the onboard LED
 *
 * @param blinks Number of times to blink the LED
 * @param interval Delay between blinks
 */
void blinkLED(int blinks, long interval = TASK_SECOND / 10);  // Prototype for default parameter values
void stopWatering();

/* * * * * * * * * * * * * * * */
// Create tasks
Task taskUpdateReadings(TASK_SECOND * 5, TASK_FOREVER, &updateReadings);
Task taskLedBlink(TASK_SECOND / 2, 4, &blinkOn);
Task taskStopWateringTimer(TASK_SECOND * 5, TASK_FOREVER, &stopWatering);

/* * * * * * * * * * * * * * * */
// Pin declarations
const int light_pin = D1;
const int led_pin = D4;
const int dht_pin = D5;
const int servo_pin = D6;
const int pump_pin = D7;
const int soil_pin = A0;

// Values to calibrate for soil moisture sensor
const int air_value = 620;
const int water_value = 310;

// Base node address
const String baseAddress = "http://192.168.0.201";

/* * * * * * * * * * * * * * * */
// Sensor declarations
// Initialise the components

DHT dht(dht_pin, DHT11);
Servo serv;

/* * * * * * * * * * * * * * * */
// Dynamic global variables

int soil_moisture_percent;
int temperature;
int humidity;

// Boolean to check open/close status of servo
bool isServoOpen = false;
bool isPumping = false;
bool light = false;

void setup() {
  // Start serial communication
  Serial.begin(115200);
  // Set the light relay pin mode
  pinMode(light_pin, OUTPUT);
  // Set led pin mode (integrated LED)
  pinMode(led_pin, OUTPUT);
  // Set pump pin mode
  pinMode(pump_pin, OUTPUT);

  // Initialise sensor
  dht.begin();
  // Initialise actuator
  serv.attach(servo_pin);
  // Populate initial sensor data
  updateReadings();

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
  server.on("/waterplant", water_plant); // Activate the watering function
  server.on("/setservo", setServo); // Allows for REST-based servo state setting
  server.on("/setlight", setLight); // Allows for REST-based light state setting

  server.begin();  //Start the server
  Serial.println("Server listening");
#pragma endregion

  // Async task setup
  userScheduler.addTask(taskUpdateReadings);
  userScheduler.addTask(taskLedBlink);
  userScheduler.addTask(taskStopWateringTimer);
  taskUpdateReadings.enable();

  // Initialise JSON object structure
  setupJSON();
}

void loop() {
  // Handle OTA events
  ArduinoOTA.handle();
  //Handling of incoming client requests
  server.handleClient();
  // Handling async tasks
  userScheduler.execute();
}

/**
 * Read data from the soil moisture sensor and use
 * the calibration to map the voltage to percentage
 */
int getSoilMoisture() {
  // Get voltage reading
  int soil_moisture_value = analogRead(soil_pin);
  // Convert into 'calibrated' percentage (need to calibrate)
  int percentageHumidity = map(soil_moisture_value, air_value, water_value, 100, 0);
  return percentageHumidity;
}

/*
 * Water plant function. This runs the pump in 5 second
 * intervals, pumping water into the plant pot as necessary.
 */
void water_plant() {
  Serial.println("Water plant request received");
  startWatering();
  server.send(200,"application/json",
  "{\"Content-Type\": \"application/json\",\"Status\": 200}");
}

/*
 * This function is called by the server upon receiving the
 * appropriate HTTP request. It pulls the state from the
 * HTTP arguments and sets the servo accordingly
 */
void setServo() {
  Serial.println("Set servo request received");
  bool state = getArgs();
  setServoState(state);
}

/*
 * This function is called by the server upon receiving the
 * appropriate HTTP request. It pulls the state from the
 * HTTP arguments and sets the light accordingly
 */
void setLight() {
  Serial.println("Set light request received");
  bool state = getArgs();
  setLightState(state);
}

/*
 * This function is called by the server upon receiving the
 * appropriate HTTP request.
 *
 * It sets the pump to on and activates a task that will
 * callback the stopWatering() function after 5 seconds.
 */
void startWatering() {
  digitalWrite(pump_pin, HIGH);
  isPumping = true;
  taskStopWateringTimer.enableDelayed(TASK_SECOND * 5);
}

/*
 * This function is called by the taskStopWateringTimer task.
 * After the set delay, this turns the pump off and disables
 * the task for use again later.
 */
void stopWatering() {
  digitalWrite(pump_pin, LOW);
  isPumping = false;
  taskStopWateringTimer.disable();
}

/*
 * This functions flips the servo motor between two positions
 * based on the 'state' requested. This opens and closes the
 * lid of the small greenhouse the servo is attached to.
 *
 * @param open Boolean for the state to set the lid to
 */
void setServoState(bool open) {
  if(open) {
    serv.write(180);
    isServoOpen = true;
  }
  else {
    serv.write(0);
    isServoOpen = false;
  }
}

/*
 * This functions turns the LED array on or offbased on the
 * 'state' requested. This provides light to the plants when
 * ambient lighting conditions are inadequate.
 *
 * @param on Boolean for the state to set the LED to
 */
void setLightState (bool on) {
  if(on) {
    digitalWrite(light_pin, HIGH);
    light = true;
  }
  else {
    digitalWrite(light_pin, LOW);
    light = false;
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
  soil_moisture_percent = constrain(getSoilMoisture(), 0, 100);
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  /*
  // Debug
  Serial.print("Temp: ");
  Serial.println(temperature);
  Serial.print("Hum: ");
  Serial.println(humidity);
  Serial.print("Soil: ");
  Serial.println(soil_moisture_percent);
  */
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
    <title>Greenhouse Plant Monitoring Dashboard</title>
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
      <h1>Greenhouse Plant Monitoring Dashboard</h1>
    </header>
    <div>
      <p></p>
      <p>Welcome to the plant monitor dashboard!</p>
      <p>These are the conditions inside of the greenhouse:</p>
    </div>
    
    <section id="readings">
      <div class="card">
        <p><strong>Temperature:</strong> <span id="temperature"></span> &degC</p>
      </div>
      <div class="card">
        <p><strong>Humidity:</strong> <span id="humidity"></span> %</p>
      </div>
      <div class="card">
        <p><strong>Soil Moisture:</strong> <span id="soil-moisture"></span> %</p>
        <button onclick="waterPlant()">Water plant now</button>
      </div>
    </section>

    <hr>
    <p>Current status of greenhouse:</p>
    
    <section id="states">
      <div class="card">
        <p><strong>Lid:</strong> <span id="servo-state"></span></p>
      </div>
      <div class="card">
        <p><strong>Pump:</strong> <span id="pump-state"></span></p>
      </div>
      <div class="card">
        <p><strong>Light:</strong> <span id="light-state"></span></p>
      </div>
    </section>

    <script>
      function updateValues() {
        const url = window.location.href + 'json';

        fetch(url)
          .then(response => response.json())
          .then(data => {
            const temperature = data.Readings.temperature;
            const humidity = data.Readings.humidity;
            const soil_moisture = data.Readings.soil_moisture_percent;
            const servo_state = data.States.servo_state;
            const pump_state = data.States.pump_state;
            const light_state = data.States.light_state;

            document.getElementById('temperature').innerHTML = temperature.toFixed(2);
            document.getElementById('humidity').innerHTML = humidity.toFixed(2);
            document.getElementById('soil-moisture').innerHTML = soil_moisture;
            document.getElementById('servo-state').innerHTML = servo_state ? "OPEN" : "CLOSED";
            document.getElementById('pump-state').innerHTML = pump_state ? "ON" : "OFF";
            document.getElementById('light-state').innerHTML = light_state ? "ON" : "OFF";
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
  doc["Readings"]["soil_moisture_percent"] = soil_moisture_percent;
  doc["Readings"]["temperature"] = temperature;
  doc["Readings"]["humidity"] = humidity;

  doc["States"]["servo_state"] = isServoOpen;
  doc["States"]["pump_state"] = isPumping;
  doc["States"]["light_state"] = light;
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
  sensorReadings["soil_moisture_percent"] = soil_moisture_percent;
  sensorReadings["temperature"] = temperature;
  sensorReadings["humidity"] = humidity;

  // Add actuator states to JSON object data
  JsonObject actuatorStates = doc.createNestedObject("States");
  actuatorStates["servo_state"] = isServoOpen;
  actuatorStates["pump_state"] = isPumping;
  actuatorStates["light_state"] = light;
}

/*
 * This function pulls the parameters out of the previously
 * received HTTP request. These parameters will be on of
 * two states, the function returns a bool accordingly.
 *
 * @returns state State requested from parameters
 */
bool getArgs () {
  int query = 0;
  bool state = false;

  // Check the query string
  if (server.arg("state") != "") {  // Parameter found
    // Parse the value from the query
    query = server.arg("state").toInt();
    // Update the minimum distance value
    state = query;
  }
  server.send(200,"application/json",
  "{\"Content-Type\": \"application/json\",\"Status\": 200}");
  return state;
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

/* * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Functions for battery monitoring. Currently       *
 * unused due to the lack of available pins.         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * */
#pragma region Battery code
// void loop() {
//   checkBattery();
//   delay(1000);
// }

/*
 * Checks the battery voltage using a voltage divider.
 * It then converts that voltage into a percent value.
 *
 * @return batteryLevel Percent of battery left
 */
// int checkBattery() {
//   float batteryLevel = map(analogRead(A0), 0.0f, 4095.0f, 0, 100);
//   Serial.println(batteryLevel);
//   return batteryLevel;
// }
#pragma endregion