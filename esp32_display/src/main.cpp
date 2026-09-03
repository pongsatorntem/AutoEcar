#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <stdarg.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <esp_system.h>
#include "device_config.h"

MatrixPanel_I2S_DMA *matrix = nullptr;
EthernetClient ethClient;
PubSubClient mqtt(ethClient);
String stateText = "BOOT";
String stateColor = "yellow";
String sensorFaultText = "";
unsigned long lastMqttAttempt = 0;
unsigned long lastCommandMs = 0;
unsigned long lastHeartbeatMs = 0;
EthernetLinkStatus lastLinkStatus = Unknown;
bool lastMqttConnected = false;
String lastDrawKey = "";

char commandTopic[96];
char statusTopic[96];

void logEvent(const char *category, const char *format, ...) {
  char message[192];
  va_list args;
  va_start(args, format);
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);
  Serial.printf("[%08lu] %-7s | %s\n", millis(), category, message);
}

String ipToString(const IPAddress &ip) {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  return String(buffer);
}

const char *hardwareStatusText(EthernetHardwareStatus status) {
  switch (status) {
    case EthernetNoHardware: return "NO_HARDWARE";
    case EthernetW5100: return "W5100";
    case EthernetW5200: return "W5200";
    case EthernetW5500: return "W5500";
    default: return "UNKNOWN";
  }
}

const char *linkStatusText(EthernetLinkStatus status) {
  switch (status) {
    case Unknown: return "UNKNOWN";
    case LinkON: return "ON";
    case LinkOFF: return "OFF";
    default: return "INVALID";
  }
}

const char *resetReasonText(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXTERNAL";
    case ESP_RST_SW: return "SOFTWARE";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "UNKNOWN";
  }
}

void printBanner() {
  Serial.println();
  Serial.println("================================================");
  Serial.println(" E-CAR TRAFFIC LIGHT DISPLAY");
  Serial.printf(" Firmware : %s\n", FIRMWARE_VERSION);
  Serial.printf(" Display  : %d\n", DISPLAY_ID);
  Serial.println("================================================");
}

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

void logDrawAction(const String &fault) {
  String drawKey = stateText + "|" + stateColor + "|" + fault;
  if (drawKey == lastDrawKey) return;
  lastDrawKey = drawKey;

  if (stateColor == "green" || stateText == "GO") {
    logEvent("DISPLAY", "draw GREEN state=%s color=%s", stateText.c_str(), stateColor.c_str());
  } else if (stateColor == "yellow" || stateText == "CAUTION") {
    logEvent("DISPLAY", "draw YELLOW/CAUTION state=%s color=%s", stateText.c_str(), stateColor.c_str());
  } else if (stateColor == "red" || stateText == "STOP") {
    logEvent("DISPLAY", "draw RED/STOP state=%s color=%s", stateText.c_str(), stateColor.c_str());
  } else {
    logEvent("DISPLAY", "draw FALLBACK_CAUTION state=%s color=%s", stateText.c_str(), stateColor.c_str());
  }
  if (fault.length()) logEvent("DISPLAY", "fault overlay=%s", fault.c_str());
}

void drawScreen() {
  String fault = effectiveFault();
  logDrawAction(fault);

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

  drawFaultBadge(fault);
}

void callback(char* topic, byte* payload, unsigned int length) {
  logEvent("MQTT", "RX topic=%s bytes=%u", topic, length);
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    logEvent("MQTT", "JSON parse_error=%s", error.c_str());
    return;
  }
  String previousText = stateText;
  String previousColor = stateColor;
  String previousFault = sensorFaultText;
  stateText = String((const char*)(doc["text"] | "GO"));
  stateColor = String((const char*)(doc["color"] | "green"));
  sensorFaultText = String((const char*)(doc["fault_text"] | ""));
  lastCommandMs = millis();
  logEvent("MQTT", "parsed state=%s text=%s color=%s fault_text=%s",
           (const char*)(doc["state"] | ""), stateText.c_str(), stateColor.c_str(), sensorFaultText.c_str());
  if (stateText != previousText || stateColor != previousColor || sensorFaultText != previousFault) {
    logEvent("STATE", "changed text=%s color=%s fault=%s", stateText.c_str(), stateColor.c_str(), sensorFaultText.c_str());
  }
  drawScreen();
}

void connectMqtt() {
  if (mqtt.connected() || millis() - lastMqttAttempt < 2000) return;
  lastMqttAttempt = millis();
  char clientId[32]; snprintf(clientId, sizeof(clientId), "display-%d", DISPLAY_ID);
  logEvent("MQTT", "connecting client=%s broker=%s:%u", clientId, ipToString(MQTT_IP).c_str(), MQTT_PORT);
  if (mqtt.connect(clientId, statusTopic, 1, true, "offline")) {
    logEvent("MQTT", "connected");
    bool published = mqtt.publish(statusTopic, "online", true);
    logEvent("MQTT", "status publish topic=%s result=%s", statusTopic, published ? "OK" : "FAIL");
    bool subscribed = mqtt.subscribe(commandTopic, 1);
    logEvent("MQTT", "subscribed topic=%s result=%s", commandTopic, subscribed ? "OK" : "FAIL");
  } else {
    logEvent("MQTT", "connect_failed state=%d", mqtt.state());
  }
  drawScreen();
}

void logNetworkStatus(const char *category) {
  logEvent(category, "hardware=%s", hardwareStatusText(Ethernet.hardwareStatus()));
  logEvent(category, "link=%s", linkStatusText(Ethernet.linkStatus()));
  logEvent(category, "IP=%s", ipToString(Ethernet.localIP()).c_str());
  logEvent(category, "subnet=%s", ipToString(Ethernet.subnetMask()).c_str());
  logEvent(category, "gateway=%s", ipToString(Ethernet.gatewayIP()).c_str());
}

void logHeartbeat() {
  if (millis() - lastHeartbeatMs < 5000) return;
  lastHeartbeatMs = millis();
  logEvent("HEART", "ETH=%s LINK=%s IP=%s MQTT=%s STATE=%s/%s heap=%u",
           hardwareStatusText(Ethernet.hardwareStatus()),
           linkStatusText(Ethernet.linkStatus()),
           ipToString(Ethernet.localIP()).c_str(),
           mqtt.connected() ? "ON" : "OFF",
           stateText.c_str(), stateColor.c_str(), ESP.getFreeHeap());
}

void checkNetworkTransitions() {
  EthernetLinkStatus link = Ethernet.linkStatus();
  if (link != lastLinkStatus) {
    logEvent("ETH", "link_change %s -> %s", linkStatusText(lastLinkStatus), linkStatusText(link));
    lastLinkStatus = link;
    drawScreen();
  }

  bool connected = mqtt.connected();
  if (connected != lastMqttConnected) {
    logEvent("MQTT", "connection_change %s", connected ? "CONNECTED" : "DISCONNECTED");
    if (!connected) logEvent("MQTT", "disconnect_state=%d", mqtt.state());
    lastMqttConnected = connected;
    drawScreen();
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  printBanner();
  logEvent("BOOT", "setup START");
  logEvent("BOOT", "reset=%s", resetReasonText(esp_reset_reason()));
  logEvent("BOOT", "free_heap=%u flash=%u psram=%u", ESP.getFreeHeap(), ESP.getFlashChipSize(), ESP.getPsramSize());
  snprintf(commandTopic, sizeof(commandTopic), "factory/trafficlight/junction/1/display");
  snprintf(statusTopic, sizeof(statusTopic), "factory/trafficlight/junction/1/display/%d/status", DISPLAY_ID);
  logEvent("CONFIG", "DISPLAY_ID=%d", DISPLAY_ID);
  logEvent("CONFIG", "IP=%s", ipToString(DEVICE_IP).c_str());
  logEvent("CONFIG", "MQTT=%s:%u", ipToString(MQTT_IP).c_str(), MQTT_PORT);
  logEvent("CONFIG", "command_topic=%s", commandTopic);
  logEvent("CONFIG", "status_topic=%s", statusTopic);

  // Exact ESP-HUB75 (ArtronShop 03K26) mapping from the board's published
  // 64x32 Adafruit_Protomatter example, converted to the DMA library format.
  // R1,G1,B1,R2,G2,B2,A,B,C,D,E,LAT,OE,CLK
  HUB75_I2S_CFG::i2s_pins hub75Pins = {
    42, 41, 40, 39, 38, 37,
    48, 36, 45, 35, -1,
    47, 14, 2
  };
  HUB75_I2S_CFG mxconfig(PANEL_W, PANEL_H, PANEL_CHAIN, hub75Pins);
  logEvent("HUB75", "init START");
  matrix = new MatrixPanel_I2S_DMA(mxconfig);
  matrix->begin();
  matrix->setBrightness8(MATRIX_BRIGHTNESS);
  logEvent("HUB75", "init DONE brightness=%u", MATRIX_BRIGHTNESS);
  drawScreen();

  logEvent("SPI", "begin START");
  logEvent("SPI", "SCK=%u MISO=%u MOSI=%u CS=%u", W5500_SCK, W5500_MISO, W5500_MOSI, W5500_CS);
  SPI.begin(W5500_SCK, W5500_MISO, W5500_MOSI, W5500_CS);
  delay(100);
  logEvent("SPI", "begin DONE");
  logEvent("ETH", "Ethernet.init START");
  Ethernet.init(W5500_CS);
  logEvent("ETH", "Ethernet.init DONE");
  byte mac[] = {0x02,0x54,0x4C,0x00,0x00,(byte)DISPLAY_ID};
  logEvent("ETH", "MAC=%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  logEvent("ETH", "Ethernet.begin START");
  Ethernet.begin(mac, DEVICE_IP, DNS_IP, GATEWAY_IP, SUBNET_MASK);
  logEvent("ETH", "Ethernet.begin DONE");
  logNetworkStatus("ETH");
  lastLinkStatus = Ethernet.linkStatus();
  logEvent("MQTT", "broker=%s:%u", ipToString(MQTT_IP).c_str(), MQTT_PORT);
  mqtt.setServer(MQTT_IP, MQTT_PORT);
  mqtt.setCallback(callback);
  mqtt.setBufferSize(512);
  logEvent("MQTT", "buffer_size=512");
  logEvent("BOOT", "setup DONE");
}

void loop() {
  connectMqtt();
  mqtt.loop();
  checkNetworkTransitions();
  logHeartbeat();
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw > 500) { lastDraw = millis(); drawScreen(); }
}
