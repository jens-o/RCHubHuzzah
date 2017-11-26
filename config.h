// DEVICE ESP2
#define HOUSE = "[A site id]"
#define DEVICE_ID "[The device id (Azure device)]"

// The port to listen for incoming TCP connections
#define LISTEN_PORT           80

// The 433 TX pin
#define TX_PIN 16

// The DHT data pin
#define DHTPIN 2 

// The DHT data type
#define DHTTYPE DHT11 

//#define DALLAS

//#define IO

//#define HASTA

//#define NEXA

// MQTT
//#define MQTT
#define MQTT_SERVER "192.168.1.6"
#define MQTT_PORT 1883

// REST
//#define WIFI_SERVER

// AZURE
//#define AZURE
static char connectionString[] = "HostName=k16-hub.azure-devices.net;DeviceId=k16-1;SharedAccessKey=vPgFDhop85kvdusH0hphBPrSE1+LZbfTbAZYyNbbml0="; 

// The default nbr of 433 calls
#define DEFAULT_NBR_CALLS 5

// Data wire is plugged into port on the Arduino
#define ONE_WIRE_BUS 12

// Wifi credentials
static char ssid[] = "...";
static char pass[] = "...";

// Interval time(s) for sending message to IoT Hub
#define INTERVAL 30

#define MESSAGE_MAX_LEN 256
