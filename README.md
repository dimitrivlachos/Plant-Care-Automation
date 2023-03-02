# Plant Care Automation
ESP8266 microcontroller based, automated plant care project for ```CM3040 - Physical Computing and Internet of Things``` University of London.

To build out this system, I decided I would have a server-client type relationship between the controllers. The ```Base Station``` would be the server/host/controller of the entire system and it would serve multiple ```Plant Monitoring Stations```, which were the clients. I only built one example of each, but the general concept should be near infinitely scalable. These nodes communicate through REST-based HTTP requests, transferring JSON data as requested.

## Nodes
The ```Base Station``` would monitor and display ambient environmental factors, as well as the water resevoir for the plants, checking in with each monitor node and issuing commands, such as to water the plant, turn on a light or open the greenhouse for fresh air. This node would be plugged into a mains power source and be somewhere prominent.

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
* Integrated LED - Used to indicate when the system is active.

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

The location of the text on the OLED display regularly shifts in order to prevent burn-in.

The base station also checks in with each of the plant monitoring stations, it sends an HTTP request for a JSON file containing the status of each node.



### Circuit Diagram
![Base station breadboard](https://github.com/dimitrivlachos/Plant-Care-Automation/blob/main/Base_Station/Base%20station%20bread%20board.png?raw=true)

### Web Interface
![Base station web interface](https://github.com/dimitrivlachos/Plant-Care-Automation/blob/main/Base_Station/Base%20station%20web%20dashboard.png)


## Plant Monitoring Station

### Input / Sensors

### Output / Actuators

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

### Circuit Diagram
![Plant monitoring station breadboard](https://github.com/dimitrivlachos/Plant-Care-Automation/blob/main/Plant_Monitor_Station/Plant%20monitoring%20station%20bread%20board.png?raw=true)

### Web Interface
![Plant monitoring station web interface](https://github.com/dimitrivlachos/Plant-Care-Automation/blob/main/Plant_Monitor_Station/Plant%20monitoring%20station%20web%20dashboard.png)