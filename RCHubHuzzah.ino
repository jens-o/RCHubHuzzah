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
#define DHTPIN 5 

// The DHT data type
#define DHTTYPE DHT22 

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

// Wifi client
WiFiClient client;

Hasta hastaBlind = Hasta(TX_PIN);

#ifdef DHTTYPE
// Initialize DHT sensor.
DHT dht(DHTPIN, DHTTYPE);
#endif

// Initialize Mqtt Client
PubSubClient mqttClient(client);
char payload[20];
long lastMsg = 0;
long sensorInterval = DEFAULT_SENSOR_INTERVAL;

String deviceId = "esp1";

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

  // Setup Mqtt Client
  mqttClient.setServer("192.168.1.6", 1883);
  mqttClient.setCallback(callback);
}

void loop()
{
  if (!mqttClient.connected()) {
    reconnect();
  }
  
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
  
#ifdef DALLAS
  sensors.requestTemperatures(); // Send the command to get temperatures
  value = sensors.getTempC(insideThermometer);
  dtostrf(value,3,1,payload);
  mqttClient.publish("k16/esp1/temp2", payload);
#endif

#ifdef DHTTYPE
  value = dht.readTemperature();
  dtostrf(value,3,1,payload);
  mqttClient.publish("k16/esp1/temp1", payload);

  value = dht.readHumidity();
  dtostrf(value,2,0,payload);
  mqttClient.publish("k16/esp1/hum1", payload);
#endif
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

// Mqtt reconnect
void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect("esp1")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttClient.publish("log/k16", "connected");
      // ... and resubscribe
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

void callback(char* topic, byte* payload, unsigned int length) {
  String t[6];
  String Topic = String(topic);
  int lastIdx = 0;
  char status[1];

  for(int i=0; i<6; i++) {
    int idx = Topic.indexOf('/', lastIdx + 1);
    t[i] = Topic.substring(lastIdx, idx);
    lastIdx = idx+1; 

    Serial.println(t[i]);
  }

  if(t[0] != "cmd") {
    Serial.println("Not a command.");
    
    return;
  }

  if(t[1] != "k16") {
    Serial.println("Not for k16.");
    
    return;
  }
  /*
   * t[0] - cmd
   * t[1] - k16
   * t[2] - <bridge> - ex. nexa, hasta
   * t[3] - <command> - ex. seton
   * t[4] - <remote/house>
   * t[5] - <unit>
   */

  if(t[2] == "nexa")
  {
    unsigned int device = t[5].toInt();
    unsigned long remoteId = (unsigned long)t[4].toInt();
    bool on = false;

/*
    Serial.print("RemoteId: ");
    Serial.println(remoteId);
    Serial.print("TX_PIN: ");
    Serial.println(TX_PIN);
*/    
    NewRemoteTransmitter transmitter(remoteId, TX_PIN, 275, 3);

    if(t[3] == "seton") {
      Serial.print("Payload: ");
      Serial.println((char)payload[0]);

      if(((char)payload[0] == 't') || ((char)payload[0] == 'T') || ((char)payload[0] == '1')) {
        on = true;
        status[0] = '1';
      } 
      else {
        on = false;
        status[0] = '0';
      }
        
      transmitter.sendUnit(device-1, on);   
/*
        Serial.print("Sent ");
        Serial.print(on);
        Serial.print(" to ");
        Serial.print(remoteId);
        Serial.print(":");
        Serial.println(device-1);
*/
    }
  }
  else if (t[2] == "hasta") {
    long house = t[4].toInt();
    int unit = t[5].toInt();
    
    for(int i=0; i<nbrCalls; i++)
    {
      if((char)payload[0] == 's') {  //stop
        hastaBlind.stop(house, unit);
        status[0] = '1';
        Serial.println("Stop");
      }
      else if(((char)payload[0] == 'd') || ((char)payload[0] == '1')) { //down
        hastaBlind.down(house, unit);
        status[0] = '1';
        Serial.println("Down");
      }
      else if(((char)payload[0] == 'u') || ((char)payload[0] == '0')) { //up
        hastaBlind.up(house, unit);
        status[0] = '0';
        Serial.println("Up");
      }
    }
  }

  String strResponseTopic = "k16/" + t[2] + "/" + t[4] + "/" + t[5] + "/" + t[3];
  char responseTopic[strResponseTopic.length() + 1];
  
  Serial.println(responseTopic);

  strResponseTopic.toCharArray(responseTopic, strResponseTopic.length() + 1);
  
  mqttClient.publish(responseTopic, status);
}

void ByteToChar(byte* bytes, char* chars, unsigned int count) {
    for(unsigned int i = 0; i < count; i++)
         chars[i] = (char)bytes[i];
}

