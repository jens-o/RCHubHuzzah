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

#include "wifiparams.h"

int nbrCalls = 3;

// Data wire is plugged into port on the Arduino
#define ONE_WIRE_BUS 12

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress insideThermometer;

// The port to listen for incoming TCP connections
#define LISTEN_PORT           80

// The port to listen for incoming TCP connections
#define TX_PIN                13

// Create an instance of the server
WiFiServer server(LISTEN_PORT);

Hasta hastaBlind = Hasta(TX_PIN);

void setup(void)
{
  // Start Serial
  Serial.begin(115200);

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
}


String process(WiFiClient client) {
  
  // Jump over HTTP action
  client.readStringUntil('/');

  // Get the command
  String command = client.readStringUntil('/');

  if (command == "SwitchOn") {
    switchOnOff(true, client);
  }
  else if (command == "SwitchOff") {
    switchOnOff(false, client);
  }
  else if (command == "BlindStop") {
    blind(0, client);
  }
  else if (command == "BlindDown") {
    blind(1, client);
  }
  else if (command == "BlindUp") {
    blind(2, client);
  }
  else if (command == "weather") {
    return temperature(client);
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

// Custom function accessible by the API
void switchOnOff(bool on, WiFiClient client) {

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

void blind(int command, WiFiClient client) {
  
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
    switch(command)
    {
      case 0: // stop
        hastaBlind.stop(house, unit);
        break;
      case 1: // down
        hastaBlind.down(house, unit);
        break;
      case 2: // up
        hastaBlind.up(house, unit);
        break;
    }
  }
}

String temperature(WiFiClient client)
{
  Serial.print("Requesting temperatures...");
  sensors.requestTemperatures(); // Send the command to get temperatures
  Serial.println("DONE");

  int unit = client.parseInt();
  
  float tempC = sensors.getTempC(insideThermometer);
  Serial.print("Temp C: ");
  Serial.println(tempC);
  
  client.flush();

  return String(
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/json\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\r\n"
            "\"temperature\": ") +
         String(tempC) +
         String(
            ",\r\n"
            "\"humidity\": ") +
         String(0) +
         String(
            "\r\n"
            "}\r\n"
            "\r\n");

  /*
  client.print(
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/json\r\n"
            "Connection: close\r\n"
            "\r\n");
            
  client.print("{\r\n");
  client.print("\"temperature\": ");
  client.print(tempC);
  client.print(",\r\n");
  client.print("\"humidity\": ");
  client.print(0);
  client.print("\r\n");
  client.print("}\r\n");
  client.print("\r\n");
  */
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
