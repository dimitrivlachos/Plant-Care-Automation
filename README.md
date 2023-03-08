# Plant Care Automation
ESP8266 microcontroller based, automated plant care project for ```CM3040 - Physical Computing and Internet of Things``` University of London.

To build out this system, I decided I would have a server-client type relationship between the controllers. The ```Base Station``` would be the server/host/controller of the entire system and it would serve multiple ```Plant Monitoring Stations```, which were the clients. I only built one example of each, but the general concept should be near infinitely scalable. These nodes communicate through REST-based HTTP requests, transferring JSON data as requested.

## Nodes
The ```Base Station``` would monitor and display ambient environmental factors, as well as the water reservoir for the plants, checking in with each monitor node and issuing commands, such as to water the plant, turn on a light or open the greenhouse for fresh air. This node would be plugged into a mains power source and be somewhere prominent where it can display information.

The ```Plant Monitoring Station```, on the other hand, would be attached to each plant/greenhouse container. It would monitor the direct conditions affecting the plant, such as the soil moisture, temperature and humidity. It would also control the pump, servo and lights that water, open and illuminate the plant, based on commands issued from the base station.

---

# Breakdown of circuits
## Base Station

### Input / Sensors
* BMP280 - Temperature, humidity and barometric pressure sensor.
* BH1750 - Ambient light sensor.
* HCSR04 - Ultrasonic distance sensor.
* Push button - Used to manually trigger watering.

### Output / Actuators
* SSD1306 - 128x64 OLED display.
* Integrated LED - Used to indicate when the system is active on the network.

### JSON Data
The base station node sends JSON data in the following format:
```json
{
  "Content-Type": "application/json",
  "Status": 200,
  "Readings": {
    "temperature": 21.18000031,
    "humidity": 64.43847656,
    "pressure": 1017.714844,
    "lux": 49.16666412,
    "fill_percent": 100
  }
}
```

### Operation
The base station node constantly checks its sensors and updates its readings, displaying them on the OLED display. If an HTTP request is made to the base station, depending on the request, it will respond with either an HTML web dashboard or a JSON object containing its sensor readings.

The web page contains code that regularly sends an HTTP request to the base station, to get updated sensor readings in the form of a JSON. It then updates the web page with the new readings.

The physical push button is duplicated on the web page, this can be pressed to manually trigger watering. When the button is pressed, the base station will send an HTTP command to the plant monitoring stations to water the plants.

The base station also has a built-in LED that is used to indicate when the system is active on the network. It flashes three times when a request is made for the web page, and it flashes twice when a request is made for JSON data instead. It also flashes four times when initialising the BME280 sensor on startup.

The location of the text on the OLED display regularly shifts in order to prevent burn-in.

The ultrasonic distance sensor is used to check the water level in the reservoir. It is mounted above the water in the resevoir that each plant stations' pump draws from. This information can be checked on the web page.

The base station checks in with each of the plant monitoring stations regularly, it sends an HTTP request for a JSON file containing the status of each node.

It then processes the JSON data and decides whether to issue a command to the plant monitoring station. The commands are as follows:
1. If the pump is **not** running, it will check if the soil moisture is below a certain threshold. If it is, it will send an HTTP command to the plant monitoring station to water the plant.
2. If the lid (controlled by the servo) is **closed**, it will check if the temperature inside the greenhouse is too hot. If it is, it compares it with the outside, ambient temperature. If the greenhouse is hotter, it will send an HTTP command to the plant monitoring station to open the lid.
3. If the lid (controlled by the servo) is **open**, it will check if the temperature inside the greenhouse is too cold. If it is, it will send an HTTP command to the plant monitoring station to close the lid.
4. If the light is **off**, it will check if the ambient light is too low. If it is, it will send an HTTP command to the plant monitoring station to turn the plant light on.
5. If the light is **on**, it will check if the ambient light is adequate. If it is, it will send an HTTP command to the plant monitoring station to turn the plant light off.

### Circuit Diagram
* The **BME280** and **BH1750** sensors are connected to the ESP8266 via I2C, along with the **SSD1306** display. This is using the default I2C pins, SCL on pin D1 and SDA on pin D2.
* The **HCSR04** ultrasonic distance sensor is connected to the ESP8266 via the default pins, trigger on pin D6 and echo on pin D7.
* The push button is connected to pin D5 and has a pull-down resistor to ground.
* The integrated LED is internally connected to pin D4 (and the signal is inverted).

![Base station breadboard diagram](https://github.com/dimitrivlachos/Plant-Care-Automation/blob/main/Base_Station/Base%20station%20bread%20board.png?raw=true)

### Web Interface
![Base station web interface screenshot](https://github.com/dimitrivlachos/Plant-Care-Automation/blob/main/Base_Station/Base%20station%20web%20dashboard.png)


## Plant Monitoring Station

### Input / Sensors
* DHT11 - Temperature and humidity sensor.
* Capacitive soil moisture sensor.

### Output / Actuators
* Servo - Used to open and close the lid of the greenhouse.
* Relay - Used to control the power to the pump.
* Pump - Used to water the plant.
* LED - Used to illuminate the plant.
* Integrated LED - Used to indicate when the system is active on the network.

### JSON Data
The plant monitoring station node sends JSON data in the following format:
```json
{
  "Content-Type": "application/json",
  "Status": 200,
  "Readings": {
    "soil_moisture_percent": 97,
    "temperature": 21,
    "humidity": 60
  },
  "States": {
    "servo_state": false,
    "pump_state": false,
    "light_state": true
  }
}
```

## Operation
The plant monitoring station node constantly checks its sensors and updates its readings. If an HTTP request is made to the plant monitoring station, depending on the request, it will respond with either an HTML web dashboard or a JSON object containing its sensor readings.

The web page contains code that regularly sends an HTTP request to the plant monitoring station, to get updated sensor readings in the form of a JSON. It then updates the web page with the new readings.

The web page includes a 'Water plant now' button, this can be pressed to manually trigger watering. When the button is pressed, the base station will run its ```water_plant()``` function manually, allowing users to trigger individual nodes to water.

The plant monitoring station also has a built-in LED that is used to indicate when the system is active on the network. It flashes three times when a request is made for the web page, and it flashes twice when a request is made for JSON data instead.

The plant monitoring station does not act on its own, it only responds to HTTP commands from the base station, such as:

* Request sensor readings - This is done by sending a JSON object containing the sensor readings.
* Request index page - This is done by sending an HTML web page.
* Water the plant - This is done by turning on the pump for a set amount of time before turning it off again.
* Open the lid - This is done by rotating the servo to a set angle.
* Close the lid - This is done by rotating the servo to its original angle.
* Turn the light on - This is done by turning on the LED pin. (For a more powerful LED, this can be replaced with a relay to control a mains powered light.)
* Turn the light off - This is done by turning off the LED pin.

### Circuit Diagram
* The **DHT11** sensor is connected to the ESP8266 via the D5 pin.
* The **capacitive soil moisture sensor** is connected to the ESP8266 via the A0 pin (This prevents us from being able to measure battery percentage).
* The **servo** is connected to the ESP8266 via the D6 pin.
* The **relay** (and **pump**) are connected to the ESP8266 via the D7 pin.
* The **LED** is connected to the ESP8266 via the D1 pin.

In this diagram, the entire circuit is powered by a battery pack. However, this is only representative of the solar power circuit intended to power it, as its exact operation is not important to this project. For the sake of environmental friendliness and ease of installation, these nodes are intended to be solar powered and contain a battery pack to store the energy.

![Plant monitoring station breadboard diagram](https://github.com/dimitrivlachos/Plant-Care-Automation/blob/main/Plant_Monitor_Station/Plant%20monitoring%20station%20bread%20board.png?raw=true)

### Web Interface
![Plant monitoring station web interface screenshot](https://github.com/dimitrivlachos/Plant-Care-Automation/blob/main/Plant_Monitor_Station/Plant%20monitoring%20station%20web%20dashboard.png)
