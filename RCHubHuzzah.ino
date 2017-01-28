

/*
  RC Hub for Feather Huzzah
  
  Written in 2017 by Jens Olsson.
*/

// Import required libraries
#include <ESP8266WiFi.h>
#include <NewRemoteTransmitter.h>
#include <Hasta.h>

#include "wifiparams.h"

int nbrCalls = 3;

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

  //for(int i=0; i<3; i++) {
    switch(command)
    {
      case 0: // stop
        for(int i=0; i<3; i++)
        {
          hastaBlind.stop(house, unit);
        }
        break;
      case 1: // down
        for(int i=0; i<3; i++)
        {
          hastaBlind.down(house, unit);
        }
        break;
      case 2: // up
        for(int i=0; i<3; i++)
        {
          hastaBlind.up(house, unit);
        }
        break;
    }
  //}
}
