# Intro
I have a 800 liter boiler which can be heated via a gas heater ('CV Ketel'), and recently added a 3-phase heating element to also be able to heat it up with electricity.

The Python script heater_steering.py checks the excess electricity (as provided by PV panels) from a database, and turns on 4 (remote) relays via MQTT.
The script reads the (live) data from SQL databases (login data from external config file).

## Boiler and Heating Element 
The heating element used is the Askoma Askoheat-s AHIR-B-S-6.0 012-4708 (see https://www.oeg.net/nl/verwarmingselement-6-0-kw-517900196),
a 3-phase 6.0kW heating element with a length of 600mm.
I can drive the 3 heating elements as follows:
* L to N (230V) -> roughly using 1.8kW
* L to L (400V) -> roughly using 2.7kW
By different combinations I can heat from 1.8kW up to 6 (5.4?) kW.

I use a Wemos D1 NodeMCU with solid state relays in my boiler room to switch the 3 phases and Neutral, by listening to MQTT commands. Electrical diagram:
![image](https://github.com/EdwinGH/ElectricalHeater/assets/36776350/3d9c1fcd-5ede-4691-8109-d2c88f1f8925)
The Arduino script for the NodeMCU is [Here](/Solar_Boiler_Heater_NodeMCU_Smart_3-phase_switcher_manual.ino)
It supports OTA, USB as well as WebSerial debugging. And talks MQTT of course!  

## Server script
The Python script [heater_steering.py](heater_steering.py) runs on a central server with databases with all kiinds of information. With this information the script will steer the relays via MQTT.

### Config file
The heater_steering.py script reads the config file heater_steering_config.txt for the DB data; example file:
```
[Solar]
database = solar
host = localhost
user = solar
password = password
[PV]
database = pv
host = localhost
user = smatool
password = password
[P1]
database = p1
host = localhost
user = p1
password = password
```

