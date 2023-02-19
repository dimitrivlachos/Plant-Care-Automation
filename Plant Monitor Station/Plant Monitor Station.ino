/*
 *  Code by:  Dimitrios Vlachos
 *            dimitri.j.vlachos@gmail.com
 *
 *  Adapted from:
 *            https://randomnerdtutorials.com/esp-mesh-esp32-esp8266-painlessmesh/
 *            https://makersportal.com/blog/2020/5/26/capacitive-soil-moisture-calibration-with-arduino
 *            UoL Lectures
 */
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "painlessMesh.h"
#include <Arduino_JSON.h>
#include "DHT.h"
#include <Servo.h>

/* * * * * * * * * * * * * * * */

#pragma region MESH variables

#define MESH_PREFIX "biscuitMesh"
#define MESH_PASSWORD "fact0ryFloor"
#define MESH_PORT 5555

Scheduler userScheduler;  // to control your personal task
painlessMesh mesh;

// Prototype methods to insantiate the tasks with
void updateReadings();
void blinkOn();
void blinkLED(int blinks, long interval = TASK_SECOND / 10);  // Prototype for default parameter values

// Create tasks
Task taskUpdateReadings(TASK_SECOND * 5, TASK_FOREVER, &updateReadings);
Task taskLedBlink(TASK_SECOND / 2, 4, &blinkOn);

//Number for this node
const int node_number = 1;

#pragma endregion

/* * * * * * * * * * * * * * * */

const int led_pin = D4;
const int dht_pin = D5;
const int servo_pin = D6;
const int pump_pin = D7;
const int soil_pin = A0;

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

  serv.write(0);
  delay(2000);
  serv.write(90);
  delay(2000);
  digitalWrite(pump_pin, HIGH);
  delay(2000);
  digitalWrite(pump_pin, LOW);

  updateReadings();

#pragma region mesh setup
  mesh.setDebugMsgTypes(ERROR | STARTUP);  // set before init() so that you can see startup messages

  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
#pragma endregion

  userScheduler.addTask(taskUpdateReadings);
  userScheduler.addTask(taskLedBlink);
  taskUpdateReadings.enable();
}

void loop() {
  mesh.update();
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

#pragma region Mesh functions
void handleJson(JSONVar j) {
  int node = j["node"];
  int instruction = j["instruction"];
  int target = j["target"];

  /*
   * Instructions:
   * 1 - State request
   * 2 - State broadcast
   *
  */

  if (target != node_number) return;  // Guard statement - ignore messages not for node

  switch (instruction) {
    case 1:  // State request
      Serial.printf("State request received from node %u\n", node);
      broadcastState(node);
      break;

    case 2:  // State broadcast
      Serial.printf("State broadcast received from node %u\n", node);
      
      break;

    default:
      Serial.println("Error: Request failed");
      break;
  }
}

void broadcastState(int target) {
  blinkLED(2);
  Serial.println("Broadcasting state");

  JSONVar state;
  state["node"] = node_number;
  state["instruction"] = 2;
  state["target"] = target;
  state["temperature"] = temperature;
  state["humidity"] = humidity;
  state["moisture"] = soil_moisture_percent;

  state = JSON.stringify(state);

  mesh.sendBroadcast(state);
}

void receivedCallback(uint32_t from, String &msg) {
  JSONVar jsonObject = JSON.parse(msg.c_str());
  int node = jsonObject["node"];
  // Serial.printf("Recieved message from node %u: %u. Message:\n", node, from);
  // Serial.println(msg.c_str());

  // Parse instruction
  handleJson(jsonObject);
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.printf("Changed connections\n");
}

void nodeTimeAdjustedCallback(int32_t offset) {
  Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
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