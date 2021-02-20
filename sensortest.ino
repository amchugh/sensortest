#include <Arduino.h>

// Include the webpage
#include "webpage.h"

// Library for air quality sensor
#include <pms.h>

// Libraries for featherwing display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Libraries for ESP chip
#include <ESP8266WiFi.h>

// Library for MQTT
#include <PubSubClient.h>

#define ESP_RX 13
#define ESP_TX 13

#define BUTTON_A  0
#define BUTTON_B 16
#define BUTTON_C  2

#define MQTT_SERVER "192.168.50.41"
#define MQTT_PORT 1883

#define DO_WEBSERVER
#if defined(DO_WEBSERVER)
  #include <ESP8266WebServer.h>
  
  //#define USE_MDNS
  #if defined(USE_MDNS)
    #include <ESP8266mDNS.h>        // Include the mDNS library
  #endif
#endif

const char* ssid     = "tm";
const char* password = "teammchugh";

WiFiClient WIFI_CLIENT;

#if defined(DO_WEBSERVER)
ESP8266WebServer server(80);
#endif

PubSubClient mqtt_client;
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);

Pmsx003 pms(ESP_RX, ESP_TX);

const char *shortNames[Pmsx003::nValues_PmsDataNames]{
  "PM1.0, CF=1",
	"PM2.5, CF=1",
	"PM10.  CF=1",
	"PM1.0",
	"PM2.5",
	"PM10.",
  
  "P < 0.3 micron",
  "P < 0.5 micron",
  "P < 1.0 micron",
  "P < 2.5 micron",
  "P < 5.0 micron",
  "P < 10. micron",
  
  "Reserved",
};

void handleRoot();
void handleLED();
void handleNotFound();

void setup(void) {
	Serial.begin(115200);
	while (!Serial) {};
  
  pinMode(BUTTON_A, INPUT_PULLUP);
  
  Serial.println("Connecting ...");
  WiFi.begin(ssid, password);
  
	pms.begin();
	pms.waitForData(Pmsx003::wakeupTime);
	pms.write(Pmsx003::cmdModeActive);
  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  
  // Clear display
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.display();
  
  // Wait to print until we are done connecting.
  while (WiFi.status() != WL_CONNECTED) {
    delay(10);
    yield();
  }
  
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());


#if defined(USE_MDNS)
  if (!MDNS.begin("esp8266")) {             // Start the mDNS responder for esp8266.local
    Serial.println("Error setting up MDNS responder!");
  } else {
    Serial.println("mDNS responder started");
  }
#endif

  server.on("/", HTTP_GET, handleRoot);
  server.on("/message", HTTP_POST, handleMessage);
  server.onNotFound(handleNotFound);
  server.begin();
}

char * space_helper(uint16_t x) {
  if (x >= 100000) return "";
  if (x >= 10000) return " ";
  if (x >= 1000) return "  ";
  if (x >= 100) return "   ";
  if (x >= 10) return "    ";
  return             "     ";
}

bool reconnect_mqtt() {
  mqtt_client.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt_client.setClient(WIFI_CLIENT);
  Serial.println("Attempting to connect MQTT");
  mqtt_client.connect("airqualitysensor");
  delay(500);
  if (!mqtt_client.connected()) {
    Serial.println("Failed to connect MQTT");
    return false;
  }
  Serial.println("Connected to MQTT");
  return true;
}

auto lastRead = millis();
const auto n = Pmsx003::Reserved;
Pmsx003::pmsData lastData[n];

void loop(void) {

	Pmsx003::pmsData data[n];

	Pmsx003::PmsStatus status = pms.read(data, n);
  
	switch (status) {
		case Pmsx003::OK:
		{
			
			auto newRead = millis();
			
      /*
      We aren't going to be pushing to serial.
      
      Serial.println("_________________");
      Serial.print("Wait time ");
			Serial.println(newRead - lastRead);
			
			// For loop starts from 3
			// Skip the first three data (PM1dot0CF1, PM2dot5CF1, PM10CF1)
			for (size_t i = Pmsx003::PM1dot0; i < n; ++i) {
        Serial.print(data[i]);
				Serial.print("\t");
				Serial.print(Pmsx003::dataNames[i]);
				Serial.print(" [");
				Serial.print(Pmsx003::metrics[i]);
				Serial.print("]");
				Serial.println();
			}
      */
      
      // Push to display
      display.clearDisplay();
      display.setCursor(0,0);
      display.print(WiFi.localIP());
      if (mqtt_client.connected()) display.println(" | OK");
      else display.println(" | BAD");
      for (size_t i = Pmsx003::PM1dot0CF1; i <= Pmsx003::PM10dot0CF1; ++i) {
        display.print(data[i]);
        display.print(space_helper(data[i]));
        display.println(shortNames[i]);
      }
      display.display();
      
      // Copy the data
      memcpy(lastData, data, sizeof(data[0])*n);
      
      // Push MQTT data
      if (mqtt_client.connected() || reconnect_mqtt()) {
        char mqttMessage[100];
        
        sprintf(mqttMessage, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", 
          data[Pmsx003::PM1dot0CF1 ],
          data[Pmsx003::PM2dot5CF1],
          data[Pmsx003::PM10dot0CF1],
          data[Pmsx003::PM1dot0],
          data[Pmsx003::PM2dot5],
          data[Pmsx003::PM10dot0],
          data[Pmsx003::Particles0dot3],
          data[Pmsx003::Particles0dot5],
          data[Pmsx003::Particles1dot0],
          data[Pmsx003::Particles2dot5],
          data[Pmsx003::Particles5dot0],
          data[Pmsx003::Particles10],
          newRead - lastRead  // Delta time between this read and the last
          );
          
        mqtt_client.publish("aq", mqttMessage);
      }
      
      lastRead = newRead;
			break;
		}
		case Pmsx003::noData:
			break;
		default:
			Serial.println("_________________");
			Serial.println(Pmsx003::errorMsg[status]);
	};
  
  server.handleClient();
}

void handleRoot() {
  /*
  String msg = "<!DOCTYPE html><html><head><title>Air Quality Sensor</title><style>table,th,td{border:1px solid black;border-collapse:collapse;}</style></head><body><table>";
  for (size_t i = Pmsx003::PM1dot0CF1; i <= Pmsx003::Particles10; ++i) {
    msg = msg + "<tr><td>" + String(lastData[i]) + "</td><td>" + String(Pmsx003::dataNames[i]) + "</td><td>" + String(Pmsx003::metrics[i]) + "</td></tr>";
  }
  msg = msg + "</table>";
  //msg = msg + "<br /><form action=\"/message\" method=\"POST\"><input type=\"text\" name=\"msg\"><input type=\"submit\" value=\"Submit\"></form>";
  msg = msg + "<script>setTimeout(()=>{window.location.replace(\"/\");}, 5000);</script>";
  msg = msg + "</body></html>";
  server.send(200, "text/html", msg);
  */
  server.send(200, "text/html", PAGEHTML);
}

void handleMessage() {
  Serial.println("Got message request");
  String msg = server.arg("msg");
  Serial.print("Got message: \"");
  Serial.print(msg);
  Serial.println("\"");
  
  // Display the message given to us.
  // Clear display first
  display.clearDisplay();
  display.setCursor(0,0);
  // Now write the message all in one go.
  // The message may be cut off.
  display.print(msg);
  display.display();
  
  // Redirect back to the main page.
  server.sendHeader("Location","/");
  server.send(303);
}

void handleNotFound(){
  server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}

