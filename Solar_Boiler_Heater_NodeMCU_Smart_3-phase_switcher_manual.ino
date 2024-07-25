/*
  Turn on and off 4 relays driving 3 phases and a neutral (in this case for a boiler heater)
  Relays connected to D1,D2,D6,D7 on WeMOS NodeMCU
  Activated with MQTT commands
  Firmware upgradable with Arduino OTA
  Logging via USB and Web Serial

  Author: Edwin Zuidema
  (c) 2023, 2024
*/

#define VERSION "1.1" // Fixed WebSerial to V2.0 (different callback), added version

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>
#include <WebSerial.h>
#include <PubSubClient.h>

// constants won't change
const char* host = "boilerheater";

const int LED_1 = 16; // On GPIO pin 16 (D0) on NodeMCU main board (close to USB)
const int LED_2 =  2; // On GPIO pin  2 (D4) on ESP daughter board
const int LED_ON = LOW; // LEDs seem to be low active
const int LED_OFF = HIGH;
const int relay_L1 =  5; // On GPIO pin  5 (D1)
const int relay_L2 =  4; // On GPIO pin  4 (D2)
const int relay_L3 = 12; // On GPIO pin 12 (D6)
const int relay_N  = 13; // On GPIO pin 13 (D7)
const long interval = 30000; // interval at which to run the inner loop (milliseconds)

const char* wifi_ssid = "<WIFI>";
const char* wifi_password = "<PASSWORD>";
const char* mqtt_server = "<IP OF MQTT>";
const char* mqtt_username = "";
const char* mqtt_password = "";
const char* topic_1 = "relayL1";
const char* topic_2 = "relayL2";
const char* topic_3 = "relayL3";
const char* topic_N = "relayN";

unsigned long previousMillis = 0; // will store last time loop was run
char host_topic_1[128];
char host_topic_2[128];
char host_topic_3[128];
char host_topic_N[128];

char host_topic_1_command[128];
char host_topic_2_command[128];
char host_topic_3_command[128];
char host_topic_N_command[128];

AsyncWebServer server(80); // web server for (duplication of) serial printlines to webpage (IP/webserial)
WiFiClient wifiClient;
PubSubClient mqttClient;

void mqttCallback(const char* topic, byte* payload, unsigned int length) {
  // handle MQTT message arrived
  if(strcmp(topic, host_topic_1_command) == 0) {
    if(strncmp((char *)payload, "on", length) == 0 or strncmp((char *)payload, "1", length) == 0) {
      digitalWrite(relay_L1, HIGH); 
      mqttClient.publish(host_topic_1, "on", true);
      Serial.println("Turning L1 on");
      WebSerial.println("Turning L1 on");
    } else {
      digitalWrite(relay_L1, LOW);
      mqttClient.publish(host_topic_1, "off", true);
      Serial.println("Turning L1 off");
      WebSerial.println("Turning L1 off");
    }
  } else if(strcmp(topic, host_topic_2_command) == 0) {
    if(strncmp((char *)payload, "on", length) == 0 or strncmp((char *)payload, "1", length) == 0) {
      digitalWrite(relay_L2, HIGH); 
      mqttClient.publish(host_topic_2, "on", true);
      Serial.println("Turning L2 on");
      WebSerial.println("Turning L2 on");
    } else {
      digitalWrite(relay_L2, LOW);
      mqttClient.publish(host_topic_2, "off", true);
      Serial.println("Turning L2 off");
      WebSerial.println("Turning L2 off");
    }
  } else if(strcmp(topic, host_topic_3_command) == 0) {
    if(strncmp((char *)payload, "on", length) == 0 or strncmp((char *)payload, "1", length) == 0) {
      digitalWrite(relay_L3, HIGH); 
      mqttClient.publish(host_topic_3, "on", true);
      Serial.println("Turning L3 on");
      WebSerial.println("Turning L3 on");
    } else {
      digitalWrite(relay_L3, LOW);
      mqttClient.publish(host_topic_3, "off", true);
      Serial.println("Turning L3 off");
      WebSerial.println("Turning L3 off");
    }
  } else if(strcmp(topic, host_topic_N_command) == 0) {
    if(strncmp((char *)payload, "on", length) == 0 or strncmp((char *)payload, "1", length) == 0) {
      digitalWrite(relay_N, HIGH); 
      mqttClient.publish(host_topic_N, "on", true);
      Serial.println("Turning N on");
      WebSerial.println("Turning N on");
    } else {
      digitalWrite(relay_N, LOW);
      mqttClient.publish(host_topic_N, "off", true);
      Serial.println("Turning N off");
      WebSerial.println("Turning N off");
    }
  } else {
    WebSerial.print("MQTT received unknown data: ");
    WebSerial.print(topic);
    WebSerial.print(" => ");
    WebSerial.println((char *)payload);
  }
}
void mqttReconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    WebSerial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(host)) {
      Serial.println("connected");
      WebSerial.println("connected");
      // Subscribe to relay commands
      mqttClient.subscribe(host_topic_1_command);
      mqttClient.subscribe(host_topic_2_command);
      mqttClient.subscribe(host_topic_3_command);
      mqttClient.subscribe(host_topic_N_command);
    } else {
      Serial.print("failed, rc=");
      WebSerial.print("failed, rc=");
      Serial.print(mqttClient.state());
      WebSerial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      WebSerial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// receiving messages from serial webserver for remote commands
void recvMsg(uint8_t *data, size_t len) {
  Serial.print("WebSerial received Data: ");
  WebSerial.print("WebSerial received Data: ");
  String d = "";
  for(int i=0; i < len; i++) {
    d += char(data[i]);
  }
  Serial.println(d);
  WebSerial.println(d);
  if(d.indexOf("1") >= 0) {
    Serial.println("recvMsg() phase 1 on");
    WebSerial.println("recvMsg() phase 1 on");
    digitalWrite(relay_L1, HIGH);
    mqttClient.publish(host_topic_1, "on", true);
  } else {
    digitalWrite(relay_L1, LOW);
    mqttClient.publish(host_topic_1, "off", true);
  }
  if(d.indexOf("2") >= 0) {
    Serial.println("recvMsg() Phase 2 on");
    WebSerial.println("recvMsg() Phase 2 on");
    digitalWrite(relay_L2, HIGH);
    mqttClient.publish(host_topic_2, "on", true);
  } else {
    digitalWrite(relay_L2, LOW);
    mqttClient.publish(host_topic_2, "off", true);
  }
  if(d.indexOf("3") >= 0) {
    Serial.println("recvMsg() Phase 3 on");
    WebSerial.println("recvMsg() Phase 3 on");
    digitalWrite(relay_L3, HIGH);
    mqttClient.publish(host_topic_3, "on", true);
  } else {
    digitalWrite(relay_L3, LOW);
    mqttClient.publish(host_topic_3, "off", true);
  }
  if(d.indexOf("N") >= 0) {
    Serial.println("recvMsg() Neutral on");
    WebSerial.println("recvMsg() Neutral on");
    digitalWrite(relay_N, HIGH);
    mqttClient.publish(host_topic_N, "on", true);
  } else {
    digitalWrite(relay_N, LOW);
    mqttClient.publish(host_topic_N, "off", true);
  }
  if(d.indexOf("R") >= 0) {
    Serial.println("recvMsg() RESET");
    WebSerial.println("recvMsg() RESET");
    delay(5000);
    ESP.restart();
  }
  WebSerial.println("DONE\r\n");
}

// Called once at reboot
void setup() {
  // Use the LEDs to show we are powered and taking action
  pinMode(LED_1, OUTPUT); digitalWrite(LED_1, LED_ON);  // turn the Main board LED on
  pinMode(LED_2, OUTPUT); digitalWrite(LED_2, LED_ON);  // turn the ESP board LED on
  pinMode(relay_L1, OUTPUT); digitalWrite(relay_L1, LOW); // turn all relays off
  pinMode(relay_L2, OUTPUT); digitalWrite(relay_L2, LOW);
  pinMode(relay_L3, OUTPUT); digitalWrite(relay_L3, LOW);
  pinMode(relay_N, OUTPUT);  digitalWrite(relay_N, LOW);

  // In case connected via USB: Write some serial messages
  Serial.begin(115200);
  delay(500);
  Serial.println("\r\nsetup() begin");
  Serial.println("Version " VERSION ", built on " __DATE__ " at " __TIME__);

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }  
  // The ESP8266 tries to reconnect automatically when the connection is lost
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  // Bootstrap OTA
  Serial.println("setup() configure Arduino OTA");
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Arduino OTA Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // WebSerial is accessible at "<IP Address>/webserial" in browser
  WebSerial.begin(&server);
  // WebSerial Callback changed since V2, is now onMessage
  WebSerial.onMessage([&](uint8_t *data, size_t len) {
    recvMsg(data, len);
  });
  server.begin();
  delay(5000);
  WebSerial.println ("Version " VERSION ", built on " __DATE__ " at " __TIME__);
  WebSerial.print("IP address: ");
  WebSerial.println(WiFi.localIP());
  WebSerial.printf("RSSI: %d dBm\n", WiFi.RSSI());

  // Setup MQTT
  snprintf(host_topic_1, 127, "%s/%s", host, topic_1);
  snprintf(host_topic_2, 127, "%s/%s", host, topic_2);
  snprintf(host_topic_3, 127, "%s/%s", host, topic_3);
  snprintf(host_topic_N, 127, "%s/%s", host, topic_N);
  snprintf(host_topic_1_command, 127, "%s/%s/%s", host, topic_1, "command");
  snprintf(host_topic_2_command, 127, "%s/%s/%s", host, topic_2, "command");
  snprintf(host_topic_3_command, 127, "%s/%s/%s", host, topic_3, "command");
  snprintf(host_topic_N_command, 127, "%s/%s/%s", host, topic_N, "command");
  mqttClient.setClient(wifiClient);
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setKeepAlive(10);
  mqttClient.setBufferSize(2048);
  mqttClient.setCallback(mqttCallback);

  Serial.println("setup() end, starting loop\n");
  WebSerial.println("\r\nsetup() end, starting loop");
  digitalWrite(LED_2, LED_OFF);  // turn the ESP board LED off (done with setup)
}

// Called all the time after setup
void loop() {
  unsigned long currentMillis = millis();

  // Handle new firmware OTA if requested
  ArduinoOTA.handle();

  if (!mqttClient.connected()) {
    Serial.println("MQTT NOT connected");
    WebSerial.println("MQTT NOT connected");
    mqttReconnect();
  }
  mqttClient.loop();

  if (currentMillis - previousMillis >= interval) {
    // save the last time when action taken
    previousMillis = currentMillis;
    digitalWrite(LED_2, LED_ON);  // turn LED 2 on to show we are taking action
    Serial.printf("%s RSSI %d dBm", wifi_ssid, WiFi.RSSI());
    WebSerial.printf("%s RSSI %d dBm\n", wifi_ssid, WiFi.RSSI());

    // Show the current relay positions
    Serial.print     ((digitalRead(relay_L1) == LOW) ? ", Phase 1 off" : ", Phase 1 on");
    WebSerial.println((digitalRead(relay_L1) == LOW) ?   "Phase 1 off" :   "Phase 1 on");
    mqttClient.publish(host_topic_1, (digitalRead(relay_L1) == LOW) ? "off" : "on", true);
    Serial.print     ((digitalRead(relay_L2) == LOW) ? ", Phase 2 off" : ", Phase 2 on");
    WebSerial.println((digitalRead(relay_L2) == LOW) ?   "Phase 2 off" :   "Phase 2 on");
    mqttClient.publish(host_topic_2, (digitalRead(relay_L2) == LOW) ? "off" : "on", true);
    Serial.print     ((digitalRead(relay_L3) == LOW) ? ", Phase 3 off" : ", Phase 3 on");
    WebSerial.println((digitalRead(relay_L3) == LOW) ?   "Phase 3 off" :   "Phase 3 on");
    mqttClient.publish(host_topic_3, (digitalRead(relay_L3) == LOW) ? "off" : "on", true);
    Serial.println   ((digitalRead(relay_N) == LOW)  ? ", Neutral off" : ", Neutral on");
    WebSerial.println((digitalRead(relay_N) == LOW)  ?   "Neutral off" :   "Neutral on");
    mqttClient.publish(host_topic_N, (digitalRead(relay_N) == LOW) ? "off" : "on", true);

    // Only handling inputs via WebSerial...
    WebSerial.println("Type 1, 2, 3, N to switch on relays (e.g. '12N' and <enter>), R to reset, anything else to switch off");

    delay(100);
    digitalWrite(LED_2, LED_OFF);  // turn LED 2 off to show we are done taking action
  } // if millis / take action
}
