/*
  RC Hub for Feather Huzzah
  
  Written in 2017 by Jens Olsson.
*/

// Import required libraries
#include <ESP8266WiFi.h>
#include <NewRemoteTransmitter.h>
#include <Hasta.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "DHT.h"
#include <PubSubClient.h>
#include <stdlib.h>

#include "wifiparams.h"

// The port to listen for incoming TCP connections
#define LISTEN_PORT           80

// The 433 TX pin
#define TX_PIN                13

// The DHT data pin
#define DHTPIN 2 

// The DHT data type
//#define DHTTYPE DHT11 

#define DALLAS

// The default nbr of 433 calls
#define DEFAULT_NBR_CALLS 5

// Default interval reading sensor values
#define DEFAULT_SENSOR_INTERVAL 30000 // 30s

// Data wire is plugged into port on the Arduino
#define ONE_WIRE_BUS 12

int nbrCalls = DEFAULT_NBR_CALLS;

#ifdef DALLAS
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress insideThermometer;
#endif

// Create an instance of the server
WiFiServer server(LISTEN_PORT);
WiFiClient client;

Hasta hastaBlind = Hasta(TX_PIN);

#ifdef DHTTYPE
// Initialize DHT sensor.
DHT dht(DHTPIN, DHTTYPE);
#endif

// Initialize Mqtt Client
PubSubClient mqttClient(client);
char msg[20];
long lastMsg = 0;
long sensorInterval = DEFAULT_SENSOR_INTERVAL;

void setup(void)
{
  // Start Serial
  Serial.begin(115200);

#ifdef DALLAS
  Serial.println("Setup Temperature Sensor");

  // locate devices on the bus
  Serial.print("Locating devices...");
  sensors.begin();
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
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
  Serial.print(sensors.getResolution(insideThermometer), DEC); 
  Serial.println();
#endif

#ifdef DHTTYPE
  dht.begin();
#endif

  pinMode(TX_PIN, OUTPUT);

  Serial.println();
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

  // Start the server
  server.begin();

  // Setup Mqtt Client
  mqttClient.setServer("192.168.1.6", 1883);
}

void loop()
{
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

  

  long now = millis();
  if(now - lastMsg > sensorInterval) {
    lastMsg = now;
    publishSensors();
    Serial.println("Published messages");
  }
}

void publishSensors() {
  float value;

  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();

#ifdef DALLAS
  sensors.requestTemperatures(); // Send the command to get temperatures
  value = sensors.getTempC(insideThermometer);
  dtostrf(value,3,1,msg);
  mqttClient.publish("huzzah1/exttemp", msg);
#endif

#ifdef DHTTYPE
  value = dht.readTemperature();
  dtostrf(value,3,1,msg);
  mqttClient.publish("huzzah1/temp", msg);

  value = dht.readHumidity();
  dtostrf(value,3,1,msg);
  mqttClient.publish("huzzah1/hum", msg);
#endif
}


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
    setBlind(client);
  }
  /*
  else if (command == "getweather") {
    return getWeather(client);
  }
  */
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

// Custom function accessible by the API
void setSwitch(WiFiClient client) {
  bool on = false;
  
  if (client.readStringUntil('/') == "on") {
    on = true;
  }

  // Get first param
  unsigned int device = client.parseInt();

  unsigned long remoteId = (unsigned long)client.parseInt();

  NewRemoteTransmitter transmitter(remoteId, TX_PIN, 275, 1);

  client.flush();
  
  bool group = false;

  Serial.print("switchOnOff() called: ");
  Serial.print(remoteId);
  Serial.print(":");
  Serial.print(device);
  Serial.print(":");
  Serial.println(on);

  transmitter.sendUnit(device-1, on);

/*
  String msg = on ? "on" : "off";
  String topic = "nexa/" + String(remoteId) + "/" + String(device);

  char msgBuf[4];
  msg.toCharArray(msgBuf, 4);

  char topicBuf[20];
  topic.toCharArray(topicBuf, 20);
  
  mqttClient.publish(topicBuf, msgBuf);
*/
}

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

/*
String getWeather(WiFiClient client)
{
  int unit = client.parseInt();

  float t;
  float h;

  Serial.print("Requesting temperatures...");
  
  switch(unit) {
    case 0: // Dallas      
      sensors.requestTemperatures(); // Send the command to get temperatures
      Serial.println("DONE");
      t = sensors.getTempC(insideThermometer);
      h = 0;
      break;
    case 1: // DHT
      h = dht.readHumidity();
      t = dht.readTemperature();

      // Check if any reads failed and exit early (to try again).
      if (isnan(h) || isnan(t)) {
        Serial.println("Failed to read from DHT sensor!");

        return String(
                "HTTP/1.1 405 Internal Server Error\r\n"
                "Content-Type: text/html\r\n"
                "\r\n");
      }
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
*/

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

// Mqtt reconnect
void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect("ESP8266Client")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttClient.publish("temp", "hello world");
      // ... and resubscribe
      //client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
