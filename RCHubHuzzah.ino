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

#include "config_esp2.h"

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

WiFiClient client;

Hasta hastaBlind = Hasta(TX_PIN);

#ifdef DHTTYPE
// Initialize DHT sensor.
DHT dht(DHTPIN, DHTTYPE);
#endif

// Create Variable to store altitude in (m) for calculations;
double base_altitude = 1655.0; // Altitude of SparkFun's HQ in Boulder, CO. in (m)

void callback(char* topic, byte* payload, unsigned int length) {
  char *token, *device, *subDevice1, *subDevice2, *saveptr;

  Serial.print("Message arrived: ");
  Serial.println(topic);

  token = strtok_r(topic, "/", &saveptr);
  // Just skip cmd
  token = strtok_r(NULL, "/", &saveptr);
  // Just skip house (k16)
  device = strtok_r(NULL, "/", &saveptr);
  //Serial.print("Device: "); Serial.println(device);
  subDevice1 = strtok_r(NULL, "/", &saveptr);
  //Serial.print("SubDevice1: "); Serial.println(subDevice1);
  subDevice2 = strtok_r(NULL, "/", &saveptr);
  //Serial.print("SubDevice2: "); Serial.println(subDevice2);

  /*
  Serial.print("Payload:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  */

  if (strcmp(device, "nexa") == 0) {
    unsigned long remoteId;
    unsigned int switchId;
    bool on = false;

    if ((remoteId = strtoul(subDevice1, NULL, 10)) > 0) {
      if ((switchId = (unsigned int)strtoul(subDevice2, NULL, 10)) > 0) {
        if (switchId <= 3) {
          
          setSwitch(remoteId, switchId, on);
        }
      }
    }
    
    
  }
  else if (strcmp(device, "hasta") == 0) {
    
  }
}

/*
  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }
  */


// Initialize Mqtt Client
PubSubClient mqttClient("192.168.1.6", 1883, callback, client);
char payload[20];
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
}

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

  if (!mqttClient.connected()) {
    reconnect();
  }
#endif

  mqttClient.loop();

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
  //mqttClient.loop();

#ifdef DALLAS
  sensors.requestTemperatures(); // Send the command to get temperatures
  value = sensors.getTempC(insideThermometer);
  dtostrf(value,3,1,payload);
  mqttClient.publish("k16/esp1/temp2", payload);
#endif

#ifdef DHTTYPE
  value = dht.readTemperature();
  dtostrf(value,3,1,payload);
  mqttClient.publish("k16/esp2/temp1", payload);

  value = dht.readHumidity();
  dtostrf(value,2,0,payload);
  mqttClient.publish("k16/esp2/hum1", payload);
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

void setSwitch(unsigned long remoteId, unsigned int switchId, bool on) {
  Serial.print("Remote: ");
  Serial.println(remoteId);

  Serial.print("Switch: ");
  Serial.println(switchId);

  Serial.print("On: ");
  Serial.println(on);
  /*
  NewRemoteTransmitter transmitter(remoteId, TX_PIN, 275, 1);

  transmitter.sendUnit(deviceId-1, on);
  */
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
    if (mqttClient.connect(DEVICE_ID)) {
      Serial.println("connected");
      // ... resubscribe
      mqttClient.subscribe("cmd/k16/nexa/#");
      mqttClient.subscribe("cmd/k16/hasta/#");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}




