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

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels

Scheduler userScheduler;  // to control your personal task

// Prototype methods to insantiate the tasks with
void pingNodeCycle();
void updateReadings();
void updateDisplay();
void oledSaver();
void blinkOn();
void blinkLED(int blinks, long interval = TASK_SECOND / 10); // Prototype for default parameter values

//Create tasks
Task taskUpdateReadings(TASK_SECOND * 1, TASK_FOREVER, &updateReadings);
Task taskUpdateDisplay(TASK_SECOND * 1, TASK_FOREVER, &updateDisplay);
Task taskOledSaver(TASK_MINUTE * 5, TASK_FOREVER, &oledSaver);
Task taskLedBlink (TASK_SECOND / 2, 4, &blinkOn);

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

  userScheduler.addTask(taskUpdateReadings);
  userScheduler.addTask(taskUpdateDisplay);
  userScheduler.addTask(taskOledSaver);
  userScheduler.addTask(taskLedBlink);
  taskUpdateReadings.enable();
  taskUpdateDisplay.enable();
  taskOledSaver.enable();
}

void loop() {
  
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