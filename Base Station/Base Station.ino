/*
 *  Code by:  Dimitrios Vlachos
 *            dimitri.j.vlachos@gmail.com
 *
 *  Adapted from:
 *            https://randomnerdtutorials.com/esp-mesh-esp32-esp8266-painlessmesh/
 *            UoL Lectures
 */
#include "painlessMesh.h"
#include <Arduino_JSON.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Wire.h>
#include <BH1750.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* * * * * * * * * * * * * * * */

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels

#pragma region MESH variables

#define MESH_PREFIX "biscuitMesh"
#define MESH_PASSWORD "fact0ryFloor"
#define MESH_PORT 5555

Scheduler userScheduler;  // to control your personal task
painlessMesh mesh;

// Prototype methods to insantiate the tasks with
void pingNodeCycle();
void updateReadings();
void updateDisplay();
void oledSaver();
void blinkOn();
void blinkLED(int blinks, long interval = TASK_SECOND / 10); // Prototype for default parameter values

//Create tasks
Task taskCheckNodes(TASK_SECOND * 10, TASK_FOREVER, &pingNodeCycle);
Task taskUpdateReadings(TASK_SECOND * 1, TASK_FOREVER, &updateReadings);
Task taskUpdateDisplay(TASK_SECOND * 1, TASK_FOREVER, &updateDisplay);
Task taskOledSaver(TASK_MINUTE * 5, TASK_FOREVER, &oledSaver);
Task taskLedBlink (TASK_SECOND / 2, 4, &blinkOn);

//Number for this node
const int node_number = 0;

#pragma endregion

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

// String to send to other nodes with sensor readings
String readings;

const int number_of_nodes = 2;
int node_queue = 0;

// Data from plant stations
int soil_moisture_percent[]


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

#pragma region mesh setup
  //mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  mesh.setDebugMsgTypes(ERROR | STARTUP);  // set before init() so that you can see startup messages

  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
#pragma endregion

  userScheduler.addTask(taskCheckNodes);
  userScheduler.addTask(taskUpdateReadings);
  userScheduler.addTask(taskUpdateDisplay);
  userScheduler.addTask(taskOledSaver);
  userScheduler.addTask(taskLedBlink);
  taskCheckNodes.enable();
  taskUpdateReadings.enable();
  taskUpdateDisplay.enable();
  taskOledSaver.enable();
}

void loop() {
  mesh.update();
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

// String getReadings() {
//   JSONVar jsonReadings;
//   jsonReadings["node"] = node_number;
//   jsonReadings["temp"] = temperature;
//   jsonReadings["hum"] = humidity;
//   jsonReadings["pres"] = pressure;
//   jsonReadings["lux"] = lux;

//   readings = JSON.stringify(jsonReadings);

//   return readings;
// }

#pragma region Mesh functions
void pingNodeCycle() {
  int target = nextNode();
  Serial.print("Pinging: ");
  Serial.println(target);
  JSONVar request;

  request["node"] = node_number;
  request["instruction"] = 1;
  request["target"] = target;

  request = JSON.stringify(request);

  mesh.sendBroadcast(request);
}

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

  if (target != node_number) return; // Guard statement - ignore messages not for node
  
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
  state["temp"] = temperature;
  state["hum"] = humidity;
  state["pres"] = pressure;
  state["lux"] = lux;

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
  Serial.printf("New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.printf("Changed connections\n");
}

void nodeTimeAdjustedCallback(int32_t offset) {
  Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
}
#pragma endregion

int nextNode() {
  bool changed = true;
  node_queue++;
  while (changed) {
    changed = false;
    if (node_queue >= number_of_nodes) {
      node_queue = 0;
      changed = true;
    }
    if (node_queue == node_number) {
      node_queue++;
      changed = true;
    }
  }

  return node_queue;
}

//Init BME280
void initBME() {
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1)
      ;
  }
}

#pragma region LED Blink functions
void blinkLED(int blinks, long interval) {
  taskLedBlink.setInterval(interval);
  taskLedBlink.setIterations(blinks * 2); // Double the iterations as on->off->on is two cycles, not one
  taskLedBlink.enable();
}

void blinkOn() {
  LEDOn();
  taskLedBlink.setCallback( &blinkOff );

  if ( taskLedBlink.isLastIteration() ) {
    LEDOff();
  }
}

void blinkOff() {
  LEDOff();
  taskLedBlink.setCallback( &blinkOn );

  if ( taskLedBlink.isLastIteration() ) {
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