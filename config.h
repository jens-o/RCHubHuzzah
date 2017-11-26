#define DEVICE_ID "[Device id]"

// The port to listen for incoming TCP connections
#define LISTEN_PORT           80

// The 433 TX pin
#define TX_PIN                13

// The DHT data pin
#define DHTPIN 2 

// The DHT data type
#define DHTTYPE DHT11 

//#define DALLAS

// The default nbr of 433 calls
#define DEFAULT_NBR_CALLS 5

// Default interval reading sensor values
#define DEFAULT_SENSOR_INTERVAL 30000 // 30s

// Data wire is plugged into port on the Arduino
#define ONE_WIRE_BUS 12

#define WIFI_SERVER

// Wifi credentials
static char ssid[] = "...";
static char pass[] = "...";


