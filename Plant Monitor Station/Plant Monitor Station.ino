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

  userScheduler.addTask(taskUpdateReadings);
  userScheduler.addTask(taskLedBlink);
  taskUpdateReadings.enable();
}

void loop() {
  
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