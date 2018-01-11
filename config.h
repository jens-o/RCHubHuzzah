// NODE ESP2

// MQTT Topic details
#define HOUSE "[Site name]"
#define NODE_ID "[Node Id]"

// Azure IoT Device
#define DEVICE_ID "[Azure Device ID]"

// The port to listen for incoming TCP connections
#define LISTEN_PORT           80

// The 433 TX pin
#define TX_PIN 16

// The DHT data pin
#define DHTPIN 2 

// The DHT data type
#define DHTTYPE DHT11 

//#define DALLAS

// Barometric sensor T5403
#define T5403UNIT

//#define IO

//#define HASTA

//#define NEXA

// MQTT
#define MQTT
#define MQTT_SERVER "[MQTT Server name or ip"
#define MQTT_PORT 1883

// REST
#define WIFI_SERVER

// AZURE
#define AZURE
static char connectionString[] = "[Azure IoT device connection string]"; 

// The default nbr of 433 calls
#define DEFAULT_NBR_CALLS 3

// Default interval reading sensor values
#define DEFAULT_SENSOR_INTERVAL 30000 // 30s

// Data wire is plugged into port on the Arduino
#define ONE_WIRE_BUS 12

// Wifi credentials
static char ssid[] = "...";
static char pass[] = "...";

// Interval time(s) for sending message to hubs
#define INTERVAL 30

#define MESSAGE_MAX_LEN 256
