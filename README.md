# Plant Care Automation
ESP8266 microcontroller based, automated plant care project for ```CM3040 - Physical Computing and Internet of Things``` University of London.

To build out this system, I decided I would have a server-client type relationship between the controllers. The ```Base Station``` would be the server/host/controller of the entire system and it would serve multiple ```Plant Monitoring Stations```, which were the clients. I only built one example of each, but the general concept should be near infinitely scalable. These nodes communicate through REST-based HTTP requests, transferring JSON data as requested.

## Nodes
The ```Base Station``` would monitor and display ambient environmental factors, as well as the water resevoir for the plants, checking in with each monitor node and issuing commands, such as to water the plant, turn on a light or open the greenhouse for fresh air. This node would be plugged into a mains power source and be somewhere prominent.

The ```Plant Monitoring Station```, on the other hand, would be attached to each plant/greenhouse container. It would monitor the direct conditions affecting the plant, such as the soil moisture, temperature and humidity. It would also control the pump, servo and lights that water, open and illuminate the plant, based on commands issued from the base station.

---

# Breakdown of circuits
## Base Station

![Base station breadboard](https://github.com/dimitrivlachos/Plant-Care-Automation/blob/main/Base_Station/Base%20station%20bread%20board.png?raw=true)

## Plant Monitoring Station

![Plant monitoring station breadboard](https://github.com/dimitrivlachos/Plant-Care-Automation/blob/main/Plant_Monitor_Station/Plant%20monitoring%20station%20bread%20board.png?raw=true)
