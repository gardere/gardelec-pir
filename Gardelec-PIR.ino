#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <IotWebConf.h>
#include <MQTT.h>


const char thingName[] = "Gardelec-PIR";

#define STRING_LEN 128
#define NUMBER_LEN 32

#define CONFIG_VERSION "20181111_001"
#define CONFIG_PIN D4
#define PIR_PIN D6

#define STATUS_PIN LED_BUILTIN

void configSaved();
boolean formValidator();

DNSServer dnsServer;
ESP8266WebServer server(80);

MQTTClient mqttClient;
WiFiClient net;
void mqttMessageReceived(String &topic, String &payload);
boolean needMqttConnect = false;
boolean needReset = false;
unsigned long lastReport = 0;
unsigned long lastMqttConnectionAttempt = 0;



char mqttHostParamValue[STRING_LEN];
char mqttTopicParamValue[STRING_LEN];
char secsB4ResetParamValue[NUMBER_LEN];

int pirLastStatus;


IotWebConf iotWebConf(thingName, &dnsServer, &server, "", CONFIG_VERSION);
IotWebConfParameter mqttHostParam = IotWebConfParameter("MQTT Broker Host", "mqttHostParam", mqttHostParamValue, STRING_LEN);
IotWebConfParameter mqttTopicParam = IotWebConfParameter("MQTT Topic", "mqttTopicParam", mqttTopicParamValue, STRING_LEN);
IotWebConfParameter secsB4Reset = IotWebConfParameter("secs before reset (30 is a good one!)", "secsB4Reset", secsB4ResetParamValue, NUMBER_LEN, "number", "e.g 30", NULL, "min='0' step='1'");
IotWebConfSeparator separator1 = IotWebConfSeparator();
IotWebConfSeparator separator2 = IotWebConfSeparator();


void setup() 
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting up...");

  iotWebConf.setStatusPin(STATUS_PIN);
  iotWebConf.setConfigPin(CONFIG_PIN);
  iotWebConf.addParameter(&separator1);
  iotWebConf.addParameter(&mqttHostParam);
  iotWebConf.addParameter(&mqttTopicParam);
  iotWebConf.addParameter(&separator2);
  iotWebConf.addParameter(&secsB4Reset);

  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.getApTimeoutParameter()->visible = true;

  // -- Initializing the configuration.
  boolean validConfig = iotWebConf.init();
  if (!validConfig)
  {
    mqttHostParamValue[0] != '\0';
  }
  mqttClient.begin(mqttHostParamValue, net);
  mqttClient.onMessage(mqttMessageReceived);

  // -- Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  server.on("/config", []{ iotWebConf.handleConfig(); });
  server.onNotFound([](){ iotWebConf.handleNotFound(); });

  pirLastStatus = 0;  

  Serial.println("Ready.");
}

void loop() 
{
  // -- doLoop should be called as frequently as possible.
  iotWebConf.doLoop();
  mqttClient.loop();

  if (needMqttConnect)
  {
    if (connectMqtt())
    {
      needMqttConnect = false;
    }
  }
  else if ((iotWebConf.getState() == IOTWEBCONF_STATE_ONLINE) && (!mqttClient.connected()))
  {
    Serial.println("MQTT reconnect");
    connectMqtt();
  }

  if (needReset)
  {
    Serial.println("Rebooting after 1 second.");
    iotWebConf.delay(1000);
    ESP.restart();
  }
  
  int val = digitalRead(PIR_PIN);
  if (pirLastStatus != val) {
    digitalWrite(BUILTIN_LED, LOW);
    sendMQTTMessage(val);
    delay(250);
    digitalWrite(BUILTIN_LED, HIGH);
    pirLastStatus = val;
  }
}

/**
 * Handle web requests to "/" path.
 */
void handleRoot()
{
  // -- Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
  {
    // -- Captive portal request were already served.
    return;
  }
  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>\n";
  s += "<title>Gardelec PIR Motion Sensor</title></head><body style=\"font-family: 'Helvetica'\">\n";
  s += "<h1>Gardelec PIR Motion Sensor</h1>\n";
  s += "<br/><p>On the next page please enter the following configuration settings</p>\n";
  s += "<ul>\n";
  s += "<li>WiFi settings (your WiFi network SSID and password)</li>\n";
  s += "<li>MQTT broker details: IP address / hostname, port number (usually xxxx)</li>\n";
  s += "<li>MQTT topic: the unique topic you are going to publish to when the PIR state changes</li>\n";
  s += "</ul>\n";
  s += "<br/><br/>Go to <a href='config'>configure page</a> to change values.\n";
  s += "</body></html>\n";

  Serial.println(s);

  server.send(200, "text/html", s);
}

void configSaved()
{
  Serial.println("Configuration was updated.");
}

boolean formValidator()
{
  return true;
}

void sendMQTTMessage(int value) {
  Serial.print("Sending ");
  Serial.print(value);
  Serial.print(" to ");
  Serial.println(mqttTopicParamValue);
  mqttClient.publish(mqttTopicParamValue, value == HIGH ? "ON" : "OFF");
}

boolean connectMqtt() {
  unsigned long now = millis();
  if ((lastMqttConnectionAttempt + 1000) > now)
  {
    // Do not repeat within 1 sec.
    return false;
  }
  Serial.println("Connecting to MQTT server...");
  if (!connectMqttOptions()) {
    lastMqttConnectionAttempt = now;
    return false;
  }
  Serial.println("mqtt connected!");

  return true;
}

boolean connectMqttOptions()
{
  boolean result;
//  if (mqttUserPasswordValue[0] != '\0')
//  {
//    result = mqttClient.connect(iotWebConf.getThingName(), mqttUserNameValue, mqttUserPasswordValue);
//  }
//  else if (mqttUserNameValue[0] != '\0')
//  {
//    result = mqttClient.connect(iotWebConf.getThingName(), mqttUserNameValue);
//  }
//  else
//  {
    result = mqttClient.connect(iotWebConf.getThingName());
//  }
  return result;
}

void mqttMessageReceived(String &topic, String &payload)
{
  Serial.println("Incoming: " + topic + " - " + payload);
}

