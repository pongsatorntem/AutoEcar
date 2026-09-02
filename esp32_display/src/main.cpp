#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "device_config.h"

MatrixPanel_I2S_DMA *matrix = nullptr;
EthernetClient ethClient;
PubSubClient mqtt(ethClient);
String stateText = "BOOT";
String stateColor = "yellow";
String sensorFaultText = "";
unsigned long lastMqttAttempt = 0;
unsigned long lastCommandMs = 0;

char commandTopic[96];
char statusTopic[96];

uint16_t stateBackground(const String &name) {
  if (name == "red") return matrix->color565(255, 0, 0);
  if (name == "yellow") return matrix->color565(255, 210, 0);
  return matrix->color565(0, 210, 0);
}

String effectiveFault() {
  if (!mqtt.connected() || (lastCommandMs && millis() - lastCommandMs > MQTT_COMMAND_TIMEOUT_MS)) return "LINK ERR";
  return sensorFaultText;
}

void drawFaultBadge(const String &fault) {
  if (!fault.length()) return;
  String f = fault;
  if (f.length() > 10) f = f.substring(0, 10);

  // Small corner warning only. It must not replace the traffic indication.
  matrix->fillRect(0, 24, 64, 8, matrix->color565(0, 0, 0));
  matrix->setTextSize(1);
  matrix->setTextColor(matrix->color565(255, 255, 255));
  matrix->setCursor(1, 24);
  matrix->print(f);
}

void drawScreen() {
  // Traffic indication is a FULL-SCREEN background so it is visible from far away.
  matrix->fillScreen(stateBackground(stateColor));
  matrix->setTextWrap(false);

  if (stateColor == "green" || stateText == "GO") {
    // GREEN = full green background only. No normal-state text.
  } else if (stateColor == "yellow" || stateText == "CAUTION") {
    // YELLOW = black text for maximum contrast.
    matrix->setTextSize(1);
    matrix->setTextColor(matrix->color565(0, 0, 0));
    matrix->setCursor(11, 12);
    matrix->print("CAUTION");
  } else if (stateColor == "red" || stateText == "STOP") {
    // RED = large white STOP.
    matrix->setTextSize(2);
    matrix->setTextColor(matrix->color565(255, 255, 255));
    matrix->setCursor(8, 8);
    matrix->print("STOP");
  } else {
    // Safe visual fallback for unexpected state values.
    matrix->fillScreen(matrix->color565(255, 210, 0));
    matrix->setTextSize(1);
    matrix->setTextColor(matrix->color565(0, 0, 0));
    matrix->setCursor(11, 12);
    matrix->print("CAUTION");
  }

  drawFaultBadge(effectiveFault());
}

void callback(char* topic, byte* payload, unsigned int length) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) return;
  stateText = String((const char*)(doc["text"] | "GO"));
  stateColor = String((const char*)(doc["color"] | "green"));
  sensorFaultText = String((const char*)(doc["fault_text"] | ""));
  lastCommandMs = millis();
  drawScreen();
}

void connectMqtt() {
  if (mqtt.connected() || millis() - lastMqttAttempt < 2000) return;
  lastMqttAttempt = millis();
  char clientId[32]; snprintf(clientId, sizeof(clientId), "display-%d", DISPLAY_ID);
  if (mqtt.connect(clientId, statusTopic, 1, true, "offline")) {
    mqtt.publish(statusTopic, "online", true);
    mqtt.subscribe(commandTopic, 1);
  }
  drawScreen();
}

void setup() {
  Serial.begin(115200);
  snprintf(commandTopic, sizeof(commandTopic), "factory/trafficlight/junction/1/display");
  snprintf(statusTopic, sizeof(statusTopic), "factory/trafficlight/junction/1/display/%d/status", DISPLAY_ID);

  // Exact ESP-HUB75 (ArtronShop 03K26) mapping from the board's published
  // 64x32 Adafruit_Protomatter example, converted to the DMA library format.
  // R1,G1,B1,R2,G2,B2,A,B,C,D,E,LAT,OE,CLK
  HUB75_I2S_CFG::i2s_pins hub75Pins = {
    42, 41, 40, 39, 38, 37,
    48, 36, 45, 35, -1,
    47, 14, 2
  };
  HUB75_I2S_CFG mxconfig(PANEL_W, PANEL_H, PANEL_CHAIN, hub75Pins);
  matrix = new MatrixPanel_I2S_DMA(mxconfig);
  matrix->begin();
  matrix->setBrightness8(MATRIX_BRIGHTNESS);
  drawScreen();

  SPI.begin(W5500_SCK, W5500_MISO, W5500_MOSI, W5500_CS);
  Ethernet.init(W5500_CS);
  byte mac[] = {0x02,0x54,0x4C,0x00,0x00,(byte)DISPLAY_ID};
  Ethernet.begin(mac, DEVICE_IP, DNS_IP, GATEWAY_IP, SUBNET_MASK);
  mqtt.setServer(MQTT_IP, MQTT_PORT);
  mqtt.setCallback(callback);
  mqtt.setBufferSize(512);
}

void loop() {
  connectMqtt();
  mqtt.loop();
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw > 500) { lastDraw = millis(); drawScreen(); }
}
