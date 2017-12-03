/*
  RC Hub for Feather Huzzah
  
  Written in 2017 by Jens Olsson.
*/

// Import required libraries
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <NewRemoteTransmitter.h>
#include <Hasta.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "DHT.h"
#include <PubSubClient.h>
#include <stdlib.h>

#include <AzureIoTHub.h>
#include <AzureIoTProtocol_MQTT.h>
#include <AzureIoTUtility.h>

#include "config_esp2.h"

static bool messagePending = false;
static bool messageSending = true;

static int interval = INTERVAL;
int lastPublish = 0;

int nbrCalls = DEFAULT_NBR_CALLS;

#ifdef DALLAS
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress insideThermometer;
#endif

#ifdef WIFI_SERVER
// Create an instance of the server
WiFiServer server(LISTEN_PORT);
#endif

#ifdef HASTA
Hasta hastaBlind = Hasta(TX_PIN);
#endif

#ifdef DHTTYPE
// Initialize DHT sensor.
DHT dht(DHTPIN, DHTTYPE);
#endif

// Create Variable to store altitude in (m) for calculations;
double base_altitude = 1655.0; // Altitude of SparkFun's HQ in Boulder, CO. in (m)

#ifdef MQTT
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char *token, *house, *device, *subDevice1, *subDevice2, *saveptr;
  char pld[length];

  Serial.print("Message arrived: ");
  Serial.println(topic);

  token = strtok_r(topic, "/", &saveptr);
  // Just skip cmd
  house = strtok_r(NULL, "/", &saveptr);
  device = strtok_r(NULL, "/", &saveptr);
  subDevice1 = strtok_r(NULL, "/", &saveptr);
  subDevice2 = strtok_r(NULL, "/", &saveptr);

  // Convert payload to char[]
  for (int i = 0; i < length; i++) {
    pld[i] = (char)payload[i];
  }

  if (strcmp(device, "nexa") == 0) {
    unsigned long remoteId;
    unsigned int switchId;
    
    if ((remoteId = strtoul(subDevice1, NULL, 10)) > 0) {
      if ((switchId = (unsigned int)strtoul(subDevice2, NULL, 10)) > 0) {
        if (switchId <= 3) {
          if ((strcmp(pld, "on") == 0) || (strcmp(pld, "1") == 0) || (strcmp(pld, "true") == 0))
            setSwitch(remoteId, switchId, true);
          else
            setSwitch(remoteId, switchId, false);
        }
      }
    }
  }
  else if (strcmp(device, "hasta") == 0) {
    unsigned long remoteId;
    unsigned int unitId;
    
    if ((remoteId = strtoul(subDevice1, NULL, 10)) > 0) {
      if ((unitId = (unsigned int)strtoul(subDevice2, NULL, 10)) >= 0) {
        if (unitId <= 3) {
#ifdef HASTA
          for(int i=0; i<nbrCalls; i++)
          {
            if (strcmp(pld, "stop") == 0) {
              hastaBlind.stop(remoteId, unitId);
            }
            else if (strcmp(pld, "down") == 0) {
              hastaBlind.down(remoteId, unitId);
            }
            else if (strcmp(pld, "up") == 0) {
              hastaBlind.up(remoteId, unitId);
            }
          }
#endif
        }
      }
    }
  }
  else if (strcmp(device, "io") == 0) {
    int pin;
    
    if ((pin = atoi(subDevice1)) > 0) {
      if ((strcmp(pld, "on") == 0) || (strcmp(pld, "1") == 0)) 
        setIO(pin, true);
      else
        setIO(pin, false);
    }
  }
}

// Initialize Mqtt Client
WiFiClient client;
PubSubClient mqttClient(MQTT_SERVER, MQTT_PORT, mqttCallback, client);
#endif

char payload[20];

static IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle;
void setup(void)
{
  // Start Serial
  Serial.begin(115200);

  initDallas();

  initIO();

  initDhtSensor();

  pinMode(TX_PIN, OUTPUT);

  initWifi();

#ifdef WIFI_SERVER
  // Start the server
  server.begin();
#endif

  initTime();

#ifdef AZURE
  // initIoThubClient();
  iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(connectionString, MQTT_Protocol);
  if (iotHubClientHandle == NULL)
  {
    Serial.println("Failed on IoTHubClient_CreateFromConnectionString.");
    while (1);
  }

  Serial.println("IoT Hub done!.");

  IoTHubClient_LL_SetOption(iotHubClientHandle, "product_info", "HappyPath_AdafruitFeatherHuzzah-C");
  IoTHubClient_LL_SetMessageCallback(iotHubClientHandle, receiveMessageCallback, NULL);
  IoTHubClient_LL_SetDeviceMethodCallback(iotHubClientHandle, deviceMethodCallback, NULL);
  IoTHubClient_LL_SetDeviceTwinCallback(iotHubClientHandle, twinCallback, NULL);
#endif

  Serial.println("Setup done!.");
}

static int messageCount = 1;
void loop()
{
#ifdef WIFI_SERVER
  WiFiClient client = server.available(); 
  // wait for a client (web browser) to connect
  if (client)
  {
    Serial.println("\n[Client connected]");
    while (client.connected())
    {
      // read line by line what the client (web browser) is requesting
      if (client.available())
      {
        client.print(process(client));
 
        delay(1); // give the web browser time to receive the data

        // close the connection:
        client.stop();
      }
   }
 
    Serial.println("[Client disonnected]");
  }
#endif

#ifdef MQTT
  if (!mqttClient.connected()) {
    reconnect();
  }

  mqttClient.loop();
#endif

  if(time(NULL) > lastPublish + interval) {
    publishSensors();
    lastPublish = time(NULL);
  }

#ifdef AZURE
  IoTHubClient_LL_DoWork(iotHubClientHandle);
#endif

  delay(10);
}

void initWifi() {
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, pass);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
 
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


void initTime() {
  time_t epochTime;
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  while (true)
  {
    epochTime = time(NULL);

    if (epochTime == 0)
    {
      Serial.println("Fetching NTP epoch time failed! Waiting 2 seconds to retry.");
      delay(2000);
    }
    else
    {
      Serial.printf("Fetched NTP epoch time is: %lu.\r\n", epochTime);
      break;
    }
  }
}

void initDallas() {
  Serial.println();
#ifdef DALLAS
  Serial.println("Setup Dallas Temperature Sensor");

  // locate devices on the bus
  Serial.print("Locating devices...");
  sensors.begin();
  Serial.print("Found ");
  //Serial.print(sensors.getDeviceCount(), DEC);
  Serial.print(sensors.getDeviceCount());
  Serial.println(" devices.");

  // report parasite power requirements
  Serial.print("Parasite power is: "); 
  if (sensors.isParasitePowerMode()) Serial.println("ON");
  else Serial.println("OFF");
  
  //Get devices
  if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Device 0"); 

  // show the addresses we found on the bus
  Serial.print("Device 0 Address: ");
  printAddress(insideThermometer);
  Serial.println();

  // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensors.setResolution(insideThermometer, 9);
 
  Serial.print("Device 0 Resolution: ");
  //Serial.print(sensors.getResolution(insideThermometer), DEC);
  Serial.print(sensors.getResolution(insideThermometer)); 
  Serial.println();
#else
  Serial.println("Dallas Temperature Sensor not defined");
#endif
}

void initDhtSensor() {
#ifdef DHTTYPE
  dht.begin();
#endif
}

void initIO() {
#ifdef IO
  pinMode(12, OUTPUT); // Blue
  pinMode(13, OUTPUT); // Green
  pinMode(14, OUTPUT); // Yellow
  pinMode(15, OUTPUT); // Red
#endif
}


void publishSensors() {
  float temp1;
  float temp2;
  float hum1;

#ifdef MQTT
  if (!mqttClient.connected()) {
    reconnect();
  }
  //mqttClient.loop();
#endif

#ifdef DHTTYPE
  temp1 = dht.readTemperature();
  hum1 = dht.readHumidity();
#endif

#ifdef DALLAS
  sensors.requestTemperatures(); // Send the command to get temperatures
  temp2 = sensors.getTempC(insideThermometer);
#endif

#ifdef MQTT
  dtostrf(temp1,3,1,payload);
  mqttClient.publish("k16/esp1/temp1", payload);

  dtostrf(hum1,2,0,payload);
  mqttClient.publish("k16/esp1/hum1", payload);

  dtostrf(temp2,3,1,payload);
  mqttClient.publish("k16/esp1/temp2", payload);  
  Serial.print("Published to mqtt broker: ");
  Serial.println(MQTT_SERVER);
#endif

#ifdef AZURE
  if (!messagePending && messageSending)
  {
    char messagePayload[MESSAGE_MAX_LEN];
    bool temperatureAlert = readMessage(messageCount, temp1, hum1, temp2, messagePayload);
    sendMessage(iotHubClientHandle, messagePayload, temperatureAlert);
    messageCount++;
  }
  else {
    Serial.println("No message sent to Azure: ");
    Serial.print("* messagePending: ");
    Serial.println(messagePending);
    Serial.print("* messageSending: ");
    Serial.println(messageSending);
  }
#endif
}

#ifdef WIFI_SERVER
String process(WiFiClient client) {
  
  // Jump over HTTP action
  client.readStringUntil('/');

  // Just return if not an API call
  if(client.readStringUntil('/') != "api") {
    return String("HTTP/1.1 400 Bad Request\r\n") +
            "Content-Type: text/html\r\n" +
            "\r\n";
  }

  // Get the command
  String command = client.readStringUntil('/');

  if (command == "setswitch") {
    setSwitch(client);
  }
  else if (command == "setblind") {
#ifdef HASTA
    setBlind(client);
#endif
  }
  else if (command == "getweather") {
    return getWeather(client);
  }
  else {
    Serial.print("Received unknown command: ");
    Serial.println(command);

    return String("HTTP/1.1 400 Bad Request\r\n") +
            "Content-Type: text/html\r\n" +
            "\r\n";
  }
    
  return String("HTTP/1.1 200 OK\r\n") +
            "Content-Type: text/html\r\n" +
            "\r\n";
}
#endif

void setSwitch(unsigned long remoteId, unsigned int switchId, bool on) {
  Serial.print("Remote: ");
  Serial.println(remoteId);

  Serial.print("Switch: ");
  Serial.println(switchId);

  Serial.print("On: ");
  Serial.println(on);
  
  NewRemoteTransmitter transmitter(remoteId, TX_PIN, 275, nbrCalls);

  //for(int i=0; i<nbrCalls; i++)
  //{
    transmitter.sendUnit(switchId - 1, on);
  //  delay(10);
  //}
}

// Custom function accessible by the API
void setSwitch(WiFiClient client) {
  bool on = false;
  
  if (client.readStringUntil('/') == "on") {
    on = true;
  }

  // Get first param
  unsigned int device = client.parseInt();

  unsigned long remoteId = (unsigned long)client.parseInt();

  NewRemoteTransmitter transmitter(remoteId, TX_PIN, 275, nbrCalls);

  client.flush();
  
  bool group = false;

  Serial.print("switchOnOff() called: ");
  Serial.print(remoteId);
  Serial.print(":");
  Serial.print(device);
  Serial.print(":");
  Serial.println(on);

  transmitter.sendUnit(device-1, on);
}

#ifdef HASTA
void setBlind(WiFiClient client) {
  String command = client.readStringUntil('/');
  int unit = client.parseInt();
  int house = client.parseInt();

  client.flush();
  
  Serial.print("blind() called: ");
  Serial.print(house);
  Serial.print(":");
  Serial.print(unit);
  Serial.print(":");
  Serial.println(command);

  for(int i=0; i<nbrCalls; i++)
  {
    if (command == "stop") {
      hastaBlind.stop(house, unit);
    }
    else if (command == "down") {
      hastaBlind.down(house, unit);
    }
    else if (command == "up") {
      hastaBlind.up(house, unit);
    }
  }
}
#endif

void setIO(int pin, bool on)
{
  if ((pin >= 12) && (pin <= 16)) {
    if (on) {
      digitalWrite(pin, HIGH);
    }
    else {
      digitalWrite(pin, LOW);
    }
  }
}

String getWeather(WiFiClient client)
{
  int unit = client.parseInt();

  float t;
  float h;

  Serial.print("Requesting temperatures...");
  
  switch(unit) {
    case 0: // Dallas      
#ifdef DALLAS
      sensors.requestTemperatures(); // Send the command to get temperatures
      Serial.println("DONE");
      t = sensors.getTempC(insideThermometer);
      h = 0;
#endif
      break;
    case 1: // DHT
#ifdef DHTTYPE
      h = dht.readHumidity();
      t = dht.readTemperature();

      // Check if any reads failed and exit early (to try again).
      if (std::isnan(h) || std::isnan(t)) {
        Serial.println("Failed to read from DHT sensor!");

        return String(
                "HTTP/1.1 405 Internal Server Error\r\n"
                "Content-Type: text/html\r\n"
                "\r\n");
      }
#endif
      break;
  } 
    
  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.println(" *C ");
  
  client.flush();

  return String(
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/json\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\r\n"
            "\"temperature\": ") +
         String(t) +
         String(
            ",\r\n"
            "\"humidity\": ") +
         String(h) +
         String(
            "\r\n"
            "}\r\n"
            "\r\n");
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

#ifdef MQTT
// Mqtt reconnect
void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(DEVICE_ID)) {
      Serial.println("connected");
      // Resubscribe
#ifdef NEXA
      mqttClient.subscribe("cmd/k16/nexa/#");
#endif
#ifdef HASTA
      mqttClient.subscribe("cmd/k16/hasta/#");
#endif
#ifdef IO
      mqttClient.subscribe("cmd/k16/io/#");
#endif
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
#endif




