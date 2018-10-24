#define ETH_PHY_ADDR 1
#define ETH_PHY_POWER 17
#include <ETH.h>
static bool eth_connected = false;

#include <WebServer.h>
WebServer server(80);

#include "Esp32Rmt.h"
Esp32Rmt remote(33, 32);
#define RATE 1000
unsigned long last_time = 0;

void handleRoot()
{
  server.send(200, "text/plain", "Hello, world.");
}

void handleNotFound()
{
  server.send(404, "text/plain", "Error 404: Not Found.");
}

void WiFiEvent(WiFiEvent_t event)
{
  switch (event) {
    case SYSTEM_EVENT_ETH_START:
      Serial.println("ETH Started");
      //set eth hostname here
      ETH.setHostname("esp32");
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      Serial.print("ETH MAC: ");
      Serial.print(ETH.macAddress());
      Serial.print(", IPv4: ");
      Serial.print(ETH.localIP());
      if (ETH.fullDuplex()) {
        Serial.print(", FULL_DUPLEX");
      }
      Serial.print(", ");
      Serial.print(ETH.linkSpeed());
      Serial.println("Mbps");
      eth_connected = true;
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      eth_connected = false;
      break;
    case SYSTEM_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected = false;
      break;
    default:
      break;
  }
}

void setup()
{
  Serial.begin(115200);
  remote._NEC_rx_init();  // Used to be called automatically during object construction
  WiFi.onEvent(WiFiEvent);
  ETH.begin();

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();

  // Initialize IR library
  Serial.println("Trying to enable IR reception...");
  remote.enable_ir_reception();
}

void loop()
{
  if (eth_connected) {
    // Handle Web Server requests
    server.handleClient();

    // IR transmission
    unsigned long current_time = millis();
    if (current_time - last_time >= RATE)
    {
      remote.send_NEC(0xf990fa05);
      last_time = current_time;
    }

    // IR reception
    if (remote.recv_NEC()) {
      Serial.printf("%x\n", remote.received_data);
    }
  }
}
