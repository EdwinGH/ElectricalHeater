/*
  Turn off and on 4 relays driving 3 phases and a neutral
  Relays connected to D1,D2,D6,D7 on WeMOS NodeMCU
*/

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>

// constants won't change
const char* host = "BoilerHeater";

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
const char* wifi_password = "<WIFIPWD>";

unsigned long previousMillis = 0; // will store last time loop was run

AsyncWebServer server(80); // web server for (duplication of) serial printlines to webpage (IP/webserial)

// receiving messages from serial webserver for remote commands
void recvMsg(uint8_t *data, size_t len) {
  Serial.print("Received Data: ");
  WebSerial.print("Received Data: ");
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
  } else {
    digitalWrite(relay_L1, LOW);
  }
  if(d.indexOf("2") >= 0) {
    Serial.println("recvMsg() Phase 2 on");
    WebSerial.println("recvMsg() Phase 2 on");
    digitalWrite(relay_L2, HIGH);
  } else {
    digitalWrite(relay_L2, LOW);
  }
  if(d.indexOf("3") >= 0) {
    Serial.println("recvMsg() Phase 3 on");
    WebSerial.println("recvMsg() Phase 3 on");
    digitalWrite(relay_L3, HIGH);
  } else {
    digitalWrite(relay_L3, LOW);
  }
  if(d.indexOf("N") >= 0) {
    Serial.println("recvMsg() Neutral on");
    WebSerial.println("recvMsg() Neutral on");
    digitalWrite(relay_N, HIGH);
  } else {
    digitalWrite(relay_N, LOW);
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
  Serial.print("\r\nsetup() begin");
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
  Serial.println("\r\nsetup() configure Arduino OTA");
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
  WebSerial.msgCallback(recvMsg);
  server.begin();
  WebSerial.print("IP address: ");
  WebSerial.println(WiFi.localIP());
  WebSerial.printf("RSSI: %d dBm\n", WiFi.RSSI());

  Serial.println("\r\nsetup() end.");
  WebSerial.println("\r\nsetup() end. Type 1, 2, 3, N to switch on relays (e.g. '12N' and <enter>), R to reset, anything else to switch off");
  digitalWrite(LED_2, LED_OFF);  // turn the ESP board LED off (done with setup)
}

// Called all the time after setup
void loop() {
  unsigned long currentMillis = millis();

  // Handle new firmware OTA if requested
  ArduinoOTA.handle();

  if (currentMillis - previousMillis >= interval) {
    // save the last time when action taken
    previousMillis = currentMillis;
    digitalWrite(LED_2, LED_ON);  // turn LED 2 on to show we are taking action
    Serial.printf("%s RSSI %d dBm", wifi_ssid, WiFi.RSSI());
    WebSerial.printf("%s RSSI %d dBm", wifi_ssid, WiFi.RSSI());

    // Show the current relay positions
    Serial.print     ((digitalRead(relay_L1) == LOW) ? ", Phase 1 off" : ", Phase 1 on");
    WebSerial.print  ((digitalRead(relay_L1) == LOW) ? ", Phase 1 off" : ", Phase 1 on");
    Serial.print     ((digitalRead(relay_L2) == LOW) ? ", Phase 2 off" : ", Phase 2 on");
    WebSerial.print  ((digitalRead(relay_L2) == LOW) ? ", Phase 2 off" : ", Phase 2 on");
    Serial.print     ((digitalRead(relay_L3) == LOW) ? ", Phase 3 off" : ", Phase 3 on");
    WebSerial.print  ((digitalRead(relay_L3) == LOW) ? ", Phase 3 off" : ", Phase 3 on");
    Serial.println   ((digitalRead(relay_N) == LOW)  ? ", Neutral off" : ", Neutral on");
    WebSerial.println((digitalRead(relay_N) == LOW)  ? ", Neutral off" : ", Neutral on");

    // Only handling inputs via WebSerial...
    WebSerial.println("type 1, 2, 3, N to switch on relays (e.g. '12N' and <enter>), R to reset, anything else to switch off");

    delay(100);
    digitalWrite(LED_2, LED_OFF);  // turn LED 2 off to show we are done taking action
  } // if millis / take action
}
