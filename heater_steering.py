#!/usr/bin/env python

# Script to turn boiler heater relays on via MQTT based on rules
# (c) 2023 Edwin Zuidema
#
# 2023-12-27 First version
#
# To be done
# * Add gas vs electricity price (if electricity cheaper swith heater on (and gas off?))

progname='heater_steering'
version = "2023-12-27"

# Defaults
MAX_TEMP = 70 # Maximal boiler water temperature (degrees Celsius)
CHECK_LOOP_SLEEP_TIME = 60 # loop to check and switch heater power modes (seconds)
SWITCH_HEATER_CONFIG_TIME = 60*60 # Time during which one heating element configuration is active (in seconds)

import sys
from time import time, sleep
from datetime import datetime 
import logging
import argparse
import configparser
import mysql.connector
from paho.mqtt import client as mqtt_client

# There are 4 power steps, with each some possible configurations (123N):
# * 1.8 kW (L-N:   1001, 0101, or 0011)
# * 2.7 kW (L-L:   1100, 0110, or 1010)
# * 3.6 kW (2xL-N: 1101, 1011, or 0111)
# * 5.4 kW (L-L-L: 1110).

# Index for the modes in the POWER array
MODE_P00 = 0
MODE_P18 = 1
MODE_P27 = 2
MODE_P36 = 3
MODE_P54 = 4

# Array with different relay settings for different power schemes (including alternative configurations)
#                L1     L2     L3      N       L1     L2     L3      N       L1     L2     L3      N
POWER = [ 0, [False, False, False, False] ], \
        [18, [ True, False, False,  True], [False,  True, False,  True], [False, False,  True,  True] ], \
        [27, [ True,  True, False, False], [False,  True,  True, False], [ True, False,  True, False] ], \
        [36, [ True,  True, False,  True], [ True, False,  True,  True], [False,  True,  True,  True] ], \
        [54, [ True,  True,  True, False] ]
# Current heating element configuration; to be increased (and rolled) after some (long) time to have all heating elements used over time
POWER_INDEX = [1,1,1,1,1]

MQTT_BROKER = '192.168.10.10'
MQTT_PORT = 1883
MQTT_TOPIC = "boilerheater"
MQTT_CLIENT_ID = progname

# Globals
logger = None
config = None

def parse_arguments(logger):
  # Commandline arguments parsing
  parser = argparse.ArgumentParser(description='Turn boiler heater relays on via MQTT based on rules', epilog="Copyright (c) E. Zuidema")
  parser.add_argument("-l", "--log", help="Logging level, can be 'none', 'info', 'warning', 'debug', default='warning'", default='warning', type=str.lower)
  parser.add_argument("-f", "--logfile", help="Logging output, can be 'stdout', or filename with path, default='stdout'", default='stdout')
  args = parser.parse_args()

  if (args.logfile == 'stdout'):
    if (args.log == 'info'):
      # info logging to systemd which already lists timestamp
      logging.basicConfig(format='%(name)s - %(message)s', level=logging.WARNING)
    else:
      logging.basicConfig(format='%(asctime)s - %(name)s - %(levelname)s - %(lineno)d - %(message)s', level=logging.WARNING)
  else:
    logging.basicConfig(filename=args.logfile, format='%(asctime)s - %(levelname)s - %(lineno)d - %(message)s', level=logging.WARNING)

  if (args.log == 'debug'):
    logger.setLevel(logging.DEBUG)
  if (args.log == 'info'):
    logger.setLevel(logging.INFO)
  if (args.log == 'warning'):
    logger.setLevel(logging.WARNING)
  if (args.log == 'error'):
    logger.setLevel(logging.ERROR)

  # return parsed values (none in this case)
  return

def connect_mqtt():
  def on_connect(client, userdata, flags, rc):
    if rc == 0:
      logger.info("Connected to MQTT Broker!")
    else:
      logger.error("Failed to connect, return code %d\n", rc)

  client = mqtt_client.Client(MQTT_CLIENT_ID)
  # client.username_pw_set(username, password)
  client.on_connect = on_connect
  client.connect(MQTT_BROKER, MQTT_PORT)
  return client

def publish(client, topic, message):
  result = client.publish(topic, message)
  # result: [0, 1]
  status = result[0]
  if status == 0:
    logger.debug("Sent `%s` to topic `%s`", message, topic)
  else:
    logger.error("Failed to send message to topic %s", topic)

def subscribe(client: mqtt_client):
  def on_message(client, userdata, msg):
    logger.debug("Received `%s` from `%s` topic", msg.payload.decode(), msg.topic)

  client.subscribe(MQTT_TOPIC)
  client.on_message = on_message

def get_boiler_temp():
  # Return current boiler top temperature in degrees Celsius
  boiler_top_temp = -1
  try:
    logger.debug("Connecting to database %s on %s with user %s", config['Solar']['database'], config['Solar']['host'], config['Solar']['user'])
    db = mysql.connector.connect(user=config['Solar']['user'], password=config['Solar']['password'], \
                                 host=config['Solar']['host'], database=config['Solar']['database'])
    c = db.cursor()
    logger.debug("Connected to Solar database")
  except mysql.connector.Error as err:
    logger.error("ERROR: %s", err)
    sys.exit(1)
  query = "SELECT * FROM mx_log WHERE DATE(`timestamp`) = UTC_DATE() ORDER BY `timestamp` DESC LIMIT 1;";
  c.execute(query)
  records = c.fetchall()
  if (c.rowcount == 1):
    logger.debug("Last recording time %s (UTC)", records[0][0])
    timestamp = records[0][0]
    # Looking for meaning_temperature_X to be "Boiler Top Temperature", previous 2 entries are the temp and temp unit
    for item in records[0]:
      if(item == 'Boiler Top Temperature'):
        index = records[0].index(item)
#        logger.debug("Boiler Top Temp found in index ", index)
        logger.debug("Boiler Top Temp = %d (of %d)", records[0][index-2], MAX_TEMP)
        boiler_top_temp = int(float(records[0][index-2]))
  else:
    logger.error("ERROR: Cannot read current boiler temp from database")
    sys.exit(1)
  c.close()
  if (db.is_connected()):
    db.close()
  if (timestamp == None or boiler_top_temp < 0):
    logger.error("ERROR: No boiler top temp recorded in database (None / NULL / -1)")
    sys.exit(1)
  return boiler_top_temp

def get_PV_power():
  # Return current electricity produced with PV panels in Watts
  power_pv = -1
  try:
    logger.debug("Connecting to database %s on %s with user %s", config['PV']['database'], config['PV']['host'], config['PV']['user'])
    db = mysql.connector.connect(user="smatool", password="SMATool", host="localhost", database="smatool")
    c = db.cursor()
    logger.debug("Connected to PV database")
  except mysql.connector.Error as err:
    logger.error("ERROR: %s", err)
    sys.exit(1)
  query = "SELECT `DateTime`,`Value` FROM `LiveData` WHERE `Description`='Total Power' ORDER BY `DateTime` DESC LIMIT 1";
  c.execute(query)
  records = c.fetchall()
  if (c.rowcount == 1):
    logger.debug("Last recording time %s (UTC)", records[0][0])
    logger.debug("PV Power %s W", records[0][1])
    timestamp = records[0][0]
    power_pv = int(float(records[0][1]))
  else:
    logger.error("ERROR: Cannot read current PV power from database")
    sys.exit(1)
  c.close()
  if (db.is_connected()):
    db.close()
  if (timestamp == None or power_pv < 0):
    logger.error("ERROR: No PV power recorded in database (None / NULL)")
    sys.exit(1)
  return power_pv

def get_P1_power():
  # Return current P1 meter electricity in and out values in Watts
  power_in = power_out = -1
  try:
    logger.debug("Connecting to database %s on %s with user %s", config['P1']['database'], config['P1']['host'], config['P1']['user'])
    db = mysql.connector.connect(user="p1user", password="P1Password", host="localhost", database="p1")
    c = db.cursor()
    logger.debug("Connected to P1 database")
  except mysql.connector.Error as err:
    logger.error("ERROR: %s", err)
    sys.exit(1)
  query = "SELECT `p1_timestamp`, `p1_current_power_in`, `p1_current_power_out` FROM p1_log ORDER BY `p1_timestamp` DESC LIMIT 1";
  c.execute(query)
  records = c.fetchall()
  if (c.rowcount == 1):
    logger.debug("Last recording time %s (UTC)", records[0][0])
    logger.debug("Power in  %f kW", records[0][1])
    logger.debug("Power out %f kW", records[0][2])
    timestamp = records[0][0]
    power_in = int(float(records[0][1])*1000)
    power_out = int(float(records[0][2])*1000)
  else:
    logger.error("ERROR: Cannot read current P1 power from database")
    sys.exit(1)
  c.close()
  if (db.is_connected()):
    db.close()
  if (timestamp == None or power_in < 0 or power_out < 0):
    logger.error("ERROR: No P1 power recorded in database (None / NULL)")
    sys.exit(1)
  return (power_in, power_out)

def set_relay(client, mode, index):
  if(POWER[mode][index][0]): publish(client, "boilerheater/relayL1/command", "on")
  else: publish(client, "boilerheater/relayL1/command", "off")
  if(POWER[mode][index][1]): publish(client, "boilerheater/relayL2/command", "on")
  else: publish(client, "boilerheater/relayL2/command", "off")
  if(POWER[mode][index][2]): publish(client, "boilerheater/relayL3/command", "on")
  else: publish(client, "boilerheater/relayL3/command", "off")
  if(POWER[mode][index][3]): publish(client, "boilerheater/relayN/command", "on")
  else: publish(client, "boilerheater/relayN/command", "off")

def print_relay(mode, index):
  if(POWER[mode][index][0]): logger.info("boilerheater/relayL1 on")
  else: logger.info("boilerheater/relayL1 off")
  if(POWER[mode][index][1]): logger.info("boilerheater/relayL2 on")
  else: logger.info("boilerheater/relayL2 off")
  if(POWER[mode][index][2]): logger.info("boilerheater/relayL3 on")
  else: logger.info("boilerheater/relayL3 off")
  if(POWER[mode][index][3]): logger.info("boilerheater/relayN on")
  else: logger.info("boilerheater/relayN off")

def main():
  # Main program
  global logger, config

  # Printing to stdout, not via logger yet
  print("%s - %s - Version %s" % (datetime.now().strftime("%Y-%m-%d %H:%M:%S"), progname, version))
  print("Python version %s.%s.%s" % sys.version_info[:3])

  logger = logging.getLogger(progname)
  logger.info("Started program %s, version %s", progname, version)
  parse_arguments(logger)

  # Load MySQL DB and login data from file
  config = configparser.ConfigParser()
  config.read(progname + '_config.txt')
  logger.debug("Loaded config file with sections %s", config.sections())

  # Start the MQTT part
  client = connect_mqtt()
  client.loop_start()

  # Starting heater power is off (0)
  heater_power = 0
  mode = MODE_P00
  logger.debug("Switching heater off for init")
  set_relay(client, mode, POWER_INDEX[mode])
  # sleep until power measuring has picked up the stats without heater
  sleep(5)

  # Now start automation based on rules:
  # * Check every minute:
  # *   IF excess electricity production > 2kW
  # *   AND boiler top or middle temperature < 70 degrees Celsius (MAX_TEMP)
  # *   THEN switch on heater based on excess energy

  # Timer for changing heating element configuration
  t_start = time()
  try: # To catch potential keyboard interrupt used while debugging
    while True:
      # if the boiler can use some (electrical) heating
      if (get_boiler_temp() < MAX_TEMP):
        # Check for excess electricity
        power_pv = get_PV_power()
        (power_in, power_out) = get_P1_power()
        power_use = power_in - power_out
        # If the heater is already on, calculate the net use (ex heater)
        net_power_use = power_use - heater_power
        net_excess_power = power_out + heater_power
        logger.info("Power use %d W (%d in + %d PV - %d out), excess power %d W", power_use, power_in, power_pv, power_out, power_out)
        logger.info("Net power use %d W (%d used by heater), net excess power %d W", net_power_use, heater_power, net_excess_power)

        # Check in which interval excess energy is
        # if between 2.0 and 3.0 -> mode 1.8kW
        # if between 3.0 and 4.0 -> mode 2.7kW
        # if between 4.0 and 5.5 -> mode 3.6kW
        # if       > 5.5       -> mode 5.4kW
        if (net_excess_power < 2000 and heater_power != 0):
          # mode off
          logger.info("Setting heater off")
          mode = MODE_P00
          set_relay(client, mode, POWER_INDEX[mode])
          heater_power = 0
        elif (net_excess_power >= 2000 and net_excess_power < 3000 and heater_power != 1800):
          # mode 1.8kW
          logger.info("Setting heater to 1.8kW")
          mode = MODE_P18
          set_relay(client, mode, POWER_INDEX[mode])
          heater_power = 1800
        elif (net_excess_power >= 3000 and net_excess_power < 4000):
          # mode 2.7kW
          logger.info("Setting heater to 2.7kW")
          mode = MODE_P27
          set_relay(client, mode, POWER_INDEX[mode])
          heater_power = 2700
        elif (net_excess_power >= 4000 and net_excess_power < 5500):
          # mode 3.6kW
          logger.info("Setting heater to 3.6kW")
          mode = MODE_P36
          set_relay(client, mode, POWER_INDEX[mode])
          heater_power = 3600
        elif (net_excess_power >= 5500):
          # mode 5.4kW
          logger.info("Setting heater to 5.4kW")
          mode = MODE_P54
          set_relay(client, mode, POWER_INDEX[mode])
          heater_power = 5400
        else:
          logger.debug("Same power, check to change elements configuration")
          t_now = time()
          if (t_now - t_start) >= SWITCH_HEATER_CONFIG_TIME:
            # Next combination of heating elements
            logger.info("Changing elements")
            POWER_INDEX[mode] = POWER_INDEX[mode] % (len(POWER[mode])-1) + 1
            set_relay(client, mode, POWER_INDEX[mode])
            t_start = t_now
        logger.debug("Done if (boiler temp not maxed out)")
        logger.info("Current heater config: (changing to next in %d s)", SWITCH_HEATER_CONFIG_TIME - int(time() - t_start))
        print_relay(mode, POWER_INDEX[mode])
      else: # Boiler temp is maxed out
        logger.info("Boiler temp on max, stop heating if needed")
        logger.debug("Setting heater off")
        mode = MODE_P00
        set_relay(client, mode, POWER_INDEX[mode])
        heater_power = 0

      # Wait some time before doing next boiler temp probe
      sleep(CHECK_LOOP_SLEEP_TIME)
    # Somehow broke out of the while True loop
    logger.debug("Done while (True)")
  except KeyboardInterrupt:
    print("CTRL-C pressed, switching off and closing")
  finally:
    set_relay(client, MODE_P00, POWER_INDEX[MODE_P00])
    client.loop_stop()
    print("Done.")

if __name__ == '__main__':
    main()