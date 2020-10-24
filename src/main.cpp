/* BOARD SETTINGS
  Board : NodeMCU 1.0
*/

// Remote debug over telnet - not recommended for production, only for development
#include <RemoteDebug.h> //https://github.com/JoaoLopesF/RemoteDebug
#include <NTPClient.h>
#include <WiFiUdp.h> // needed by NTPClient.h
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h> // API Doc: https://pubsubclient.knolleary.net/api.html
#include <ArduinoJson.h>  // API Doc: https://arduinojson.org/v6/doc/
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <EEPROM.h>
#include <FS.h>
#include <settings.h> // Include my type definitions (must be in a separate file!)

// ++++++++++++++++++++++++++++++++++++++++
//
// CONSTANTS
//
// ++++++++++++++++++++++++++++++++++++++++

// Constants - HW pins
#define PIN_LED_WIFI D2 // Blue/Wifi LED (LED_BUILTIN=Nodemcu, D2 extern, D4=ESP-Chip-LED)
//#define PIN_LED_NODEMCU LED_BUILTIN
#define PIN_BRC D1
#define PIN_BUTTON D3

// Constants - Misc
const char FIRMWARE_VERSION[] = "1.7";
const char COMPILE_DATE[] = __DATE__ " " __TIME__;
const char *logFileName = "log.txt";

// Constants - Sensor
#define SENSORBYTES_LENGHT 10
unsigned long lastSensorStatusTime = 0;
char sensorbytes[SENSORBYTES_LENGHT];
#define CHARGE_STATE (int)(sensorbytes[0])
#define VOLTAGE (int)((sensorbytes[1] << 8) + sensorbytes[2])
#define CURRENT (signed short int)((sensorbytes[3] << 8) + sensorbytes[4])
#define TEMP (int)(sensorbytes[5])
#define CHARGE (int)((sensorbytes[6] << 8) + sensorbytes[7])
#define CAPACITY (int)((sensorbytes[8] << 8) + sensorbytes[9])

// Constants - Intervals (all in ms)
const int LED_WEB_MIN_TIME = 300; // interval at which to blink (milliseconds)
const int TIME_BUTTON_LONGPRESS = 10000;
const long INTERVAL_SENSOR_STATUS = 1000;
const int STATE_PUBLISH_INTERVAL = 5000;
const int MQTT_RECONNECT_INTERVAL = 2000;

// Constants - MQTT
const char MQTT_SUBSCRIBE_CMD_TOPIC1[] = "%scmd";                // Subscribe patter without hostname
const char MQTT_SUBSCRIBE_CMD_TOPIC2[] = "%s%s/cmd";             // Subscribe patter with hostname
const char MQTT_PUBLISH_STATUS_TOPIC[] = "%s%s/status";          // Public pattern for status (normal and LWT) with hostname
const char MQTT_LWT_MESSAGE[] = "{\"device\":\"disconnected\"}"; // LWT message
const char MQTT_DEFAULT_PREFIX[] = "roombaesp";                  // Default MQTT topic prefix

// ++++++++++++++++++++++++++++++++++++++++
//
// ENUMS
//
// ++++++++++++++++++++++++++++++++++++++++

// Variables wont change
enum class RoombaCMDs
{
  RMB_WAKE,
  RMB_START,
  RMB_STOP,
  RMB_CLEAN,
  RMB_MAX,
  RMB_SPOT,
  RMB_DOCK,
  RMB_POWER,
  RMB_RESET
};

enum class StatusTrigger
{
  PERIODIC,
  WEB,
  MQTT,
  NONE // Triggers no update
};

// ++++++++++++++++++++++++++++++++++++++++
//
// LIBS
//
// ++++++++++++++++++++++++++++++++++++++++

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
RemoteDebug Debug;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
WiFiClient espClient;
PubSubClient client(espClient);

// ++++++++++++++++++++++++++++++++++++++++
//
// VARS
//
// ++++++++++++++++++++++++++++++++++++++++

// Config
uint8_t cfgStart = 0;         // Start address in EEPROM for structure 'cfg'
configData_t cfg;             // Instance 'cfg' is a global variable with 'configData_t' structure now
bool configIsDefault = false; // true if no valid config found in eeprom and defaults settings loaded
const int CURRENT_CONFIG_VERSION = 2;

// Variables will change
int wifiledState = HIGH;
unsigned long lastLEDTime = 0;              // will store last time LED was updated
unsigned long lastButtonTimer = 0;          // will store how long button was pressed
unsigned long mqttLastReconnectAttempt = 0; // will store last time reconnect to mqtt broker
unsigned long nextPublishTime = 0;          // will store last publish time
char mqtt_prefix[50];                       // prefix fpr mqtt topic
bool previousButtonState = 1;               // will store last Button state. 1 = unpressed, 0 = pressed
bool bIsConnected = false;

// buffers
String html;
char buff[255];
char lastClean[50] = "---"; // will store last Clean

// function prototype
void HTMLHeader(const char *section, unsigned int refresh = 0, const char *url = "/");
void MQTTpublishStatus(StatusTrigger statusTrigger);

// ++++++++++++++++++++++++++++++++++++++++
//
// EVENT HANDLER
//
// ++++++++++++++++++++++++++++++++++++++++

WiFiEventHandler mConnectHandler;
WiFiEventHandler mDisConnectHandler;

// ++++++++++++++++++++++++++++++++++++++++
//
// MAIN CODE
//
// ++++++++++++++++++++++++++++++++++++++++

void clearSerialBuffer()
{
  while (Serial.available())
  {
    Serial.read();
  }
}

void logWrite(const char *text)
{
  /*
  File logFile = SPIFFS.open(logFileName, "a+");
  if (!logFile) {
    //rdebugA("Opening file for write failed%s", "\n");
  } else {
    logFile.print(timeClient.getFormattedDate());
    logFile.print('\t');
    logFile.println(text);
    logFile.close();
  }
  */
  // Send to telnet debug
  //rdebugA("%s\n", text);
}

void saveConfig()
{
  EEPROM.begin(512);
  EEPROM.put(cfgStart, cfg);
  delay(200);
  EEPROM.commit(); // Only needed for ESP8266 to get data written
  EEPROM.end();
}

void eraseConfig()
{
  EEPROM.begin(512);
  for (uint8_t i = cfgStart; i < sizeof(cfg); i++)
  {
    EEPROM.write(i, 0);
  }
  delay(200);
  EEPROM.commit();
  EEPROM.end();
}

void handleButton()
{
  bool inp = digitalRead(PIN_BUTTON);
  if (inp == 0)
  {
    if (inp != previousButtonState)
    {
      //rdebugA("Button pressed short\n");
      //ESP.reset();
      lastButtonTimer = millis();
    }
    if ((millis() - lastButtonTimer >= TIME_BUTTON_LONGPRESS))
    {
      //rdebugA("Button pressed long\n");
      eraseConfig();
      ESP.reset();
    }

    // Delay a little bit to avoid bouncing
    delay(50);
  }
  previousButtonState = inp;
}

void roombaCmd(RoombaCMDs cmd, StatusTrigger statusTrigger = StatusTrigger::NONE)
{

  switch (cmd)
  {
  case RoombaCMDs::RMB_WAKE:
    logWrite("Send command RMB_WAKE to Roomba");
    digitalWrite(PIN_BRC, HIGH);
    delay(50);
    digitalWrite(PIN_BRC, LOW);
    delay(50);
    digitalWrite(PIN_BRC, HIGH);
    delay(50);
    digitalWrite(PIN_BRC, LOW);
    delay(50);
    break;

  case RoombaCMDs::RMB_START:
    logWrite("Send command RMB_START to Roomba");
    Serial.write(128); // Start
    delay(50);
    break;

  case RoombaCMDs::RMB_STOP:
    roombaCmd(RoombaCMDs::RMB_WAKE);
    logWrite("Send command RMB_STOP to Roomba");
    Serial.write(173); // Stop
    break;

  case RoombaCMDs::RMB_CLEAN:
    roombaCmd(RoombaCMDs::RMB_WAKE);
    roombaCmd(RoombaCMDs::RMB_START);
    logWrite("Send command RMB_CLEAN to Roomba");
    Serial.write(135); // Clean
    break;

  case RoombaCMDs::RMB_MAX:
    roombaCmd(RoombaCMDs::RMB_WAKE);
    roombaCmd(RoombaCMDs::RMB_START);
    logWrite("Send command RMB_MAX to Roomba");
    Serial.write(136); // Max
    break;

  case RoombaCMDs::RMB_SPOT:
    roombaCmd(RoombaCMDs::RMB_WAKE);
    roombaCmd(RoombaCMDs::RMB_START);
    logWrite("Send command RMB_SPOT to Roomba");
    Serial.write(134); // Spot
    break;

  case RoombaCMDs::RMB_DOCK:
    roombaCmd(RoombaCMDs::RMB_WAKE);
    roombaCmd(RoombaCMDs::RMB_START);
    logWrite("Send command RMB_DOCK to Roomba");
    Serial.write(143); // Seek Dock
    break;

  case RoombaCMDs::RMB_POWER:
    roombaCmd(RoombaCMDs::RMB_WAKE);
    roombaCmd(RoombaCMDs::RMB_START);
    logWrite("Send command RMB_POWER to Roomba");
    Serial.write(133); // powers down Roomba
    break;

  case RoombaCMDs::RMB_RESET:
    roombaCmd(RoombaCMDs::RMB_WAKE);
    roombaCmd(RoombaCMDs::RMB_START);
    logWrite("Send command RMB_POWER to Roomba");
    Serial.write(7); // resets down Roomba
    break;
  }

  if (statusTrigger != StatusTrigger::NONE)
  {
    MQTTpublishStatus(statusTrigger);
  }
}

void showWEBMQTTAction(bool isWebAction = true)
{
  // Blink LED
  digitalWrite(PIN_LED_WIFI, HIGH);
  lastLEDTime = millis();

  // Log Access to telnet
  if (isWebAction)
  {
    snprintf(buff, sizeof(buff), "%s %s %s ", server.client().remoteIP().toString().c_str(), (server.method() == HTTP_GET ? "GET" : "POST"), server.uri().c_str());

    // Log Access to telnet
    //rdebugA("%s\n", buff);

    // Log Access to Logfile
    logWrite(buff);
  }
}

unsigned int getSensorStatus(bool force = false)
{
  ulong lastSensorStatusDiff = (millis() - lastSensorStatusTime);

  if (force || lastSensorStatusDiff >= INTERVAL_SENSOR_STATUS || lastSensorStatusTime == 0)
  {
    //rdebugA("Get new sensor values\n");
    uint8_t i = 0;
    int8_t availabled = 0;

    clearSerialBuffer();

    yield();

    roombaCmd(RoombaCMDs::RMB_WAKE);
    roombaCmd(RoombaCMDs::RMB_START);

    Serial.write(142);
    Serial.write(3);
    delay(50);

    availabled = Serial.available();
    //rdebugA("Bytes to read: %i\n", availabled);

    yield();

    if (availabled == SENSORBYTES_LENGHT)
    {
      while (Serial.available())
      {
        int c = Serial.read();

        if (i < SENSORBYTES_LENGHT)
        {
          sensorbytes[i++] = c;
        }
      }

      lastSensorStatusTime = millis();
      //rdebugA("Successful read sensor status\n");
    }
    else
    {
      clearSerialBuffer();
      //rdebugA("Error read sensor status. To less or many bytes. Exspecting %d\n", SENSORBYTES_LENGHT);
    }

    yield();

    /*
    //rdebugA("CHARGE_STATE: %i\n", CHARGE_STATE);
    //rdebugA("VOLTAGE: %i\n", VOLTAGE);
    //rdebugA("CURRENT: %i\n", CURRENT);
    //rdebugA("TEMP: %i\n", TEMP);
    //rdebugA("CHARGE: %i\n", CHARGE);
    //rdebugA("CAPACITY: %i\n", CAPACITY);*/
    return availabled;
  }
  else
  {
    //rdebugA("Use cached sensor values (next refresh in %lus)\n", (INTERVAL_SENSOR_STATUS - lastSensorStatusDiff) / 1000);
  }

  return 0;
}

bool getRoombaSensorPacket(int PacketID, int &result)
{

  int low = 0;
  int high = 0;

  clearSerialBuffer();

  yield();

  roombaCmd(RoombaCMDs::RMB_WAKE);
  roombaCmd(RoombaCMDs::RMB_START);

  Serial.write(142);
  Serial.write(PacketID);
  delay(50);

  yield();

  if (Serial.available() > 0)
  {
    //rdebugA("Bytes to read: %i\n", Serial.available());
    if (Serial.available() == 1)
    {
      sint8_t value = Serial.read();
      //rdebugA("Serial.read: %i\n", value);
      //rdebugA("Serial.read: %i\n", (signed int)value);
      //rdebugA("Serial.read: %i\n", (unsigned int)value);
      //rdebugA("Serial.read: %x\n", value);
      //rdebugA("Serial.read: %i\n", value);
      //rdebugA("Serial.read: %u\n", value);
      //rdebugA("Serial.read: %i\n", (signed int)value);
      //rdebugA("Serial.read: %u\n", (unsigned int)value);
      //rdebugA("Serial.read: %u\n", (signed int)value);
      //rdebugA("Serial.read: %u\n", (unsigned int)value);
      result = value;
      return true;
    }
    else if (Serial.available() == 2)
    {
      high = Serial.read();
      low = Serial.read();
      //rdebugA("Serial.read: %i (%i, %i)\n", (high * 256 + low), high, low);
      //rdebugA("Serial.read: %i, %i\n", lowByte(high * 256 + low), highByte(high * 256 + low));
      //rdebugA("Serial.read: %i\n", (signed int)(low + (high << 8)));
      //rdebugA("Serial.read: %i\n", (unsigned int)(low + (high << 8)));
      result = (high * 256 + low);
      return true;
    }
    else
    {
      while (Serial.available())
      {
        //rdebugA("%i\n", Serial.read());
      }
    }
  }
  else
  {
    //rdebugA("Es gibt nichts zu lesen! %s\n", "");
    return false;
  }

  return false;
}

bool isRoombaCleaning()
{
  getSensorStatus();
  if (CHARGE_STATE == 1 || CHARGE_STATE == 2 || CHARGE_STATE == 3 || CHARGE_STATE == 5)
  {
    return false;
  }
  else
  {
    if (CURRENT < -500)
    {
      return true;
    }
    else
    {
      return false;
    }
  }
}

bool isRoombaCharging()
{
  getSensorStatus();
  if (CHARGE_STATE == 1 || CHARGE_STATE == 2 || CHARGE_STATE == 3 || CHARGE_STATE == 5)
  {
    return true;
  }
  else
  {
    return false;
  }
}

String getStatusTriggerString(StatusTrigger statusTrigger)
{
  switch (statusTrigger)
  {
  case StatusTrigger::PERIODIC:
    return "periodic";
    break;
  case StatusTrigger::WEB:
    return "web";
    break;
  case StatusTrigger::NONE:
    return "none";
    break;
  default:
    return "unkown";
    break;
  }
}

void MQTTpublishStatus(StatusTrigger statusTrigger)
{
  showWEBMQTTAction(false);
  //Serial.println(F("-----------------------"));
  //Serial.print(F("Publish MQTT status message\n"));
  //Serial.print(F("State: "));
  uint16_t mqtt_buffersize = client.getBufferSize();

  char payload[mqtt_buffersize];
  DynamicJsonDocument jsondoc(mqtt_buffersize);

  //getSensorStatus(true);
  jsondoc["cleaning"] = isRoombaCleaning();
  jsondoc["charging"] = isRoombaCharging();
  jsondoc["trigger"] = getStatusTriggerString(statusTrigger);
  jsondoc["note"] = cfg.note;
  jsondoc["firmware"] = FIRMWARE_VERSION;
  jsondoc["wifi_rssi"] = WiFi.RSSI();

  size_t payloadSize = serializeJson(jsondoc, payload, sizeof(payload));

  snprintf(buff, sizeof(buff), MQTT_PUBLISH_STATUS_TOPIC, mqtt_prefix, WiFi.hostname().c_str());

  //Serial.printf_P(PSTR("Payload-/Buffersize: %i/%i bytes (%i%%)\n"), payloadSize, mqtt_buffersize, (int)((100.00 / (double)mqtt_buffersize) * payloadSize));
  //Serial.printf_P(PSTR("Topic: %s\nMessage: "), buff);
  serializeJsonPretty(jsondoc, Serial);
  //Serial.println();

  if (!client.publish(buff, (uint8_t *)payload, (unsigned int)payloadSize, true))
  {
    ////rdebugAln("Failed to publish message!");
  }

  nextPublishTime = millis() + (cfg.mqtt_periodic_update_interval * 1000);
}

long RSSI2Quality(long dBm)
{
  if (dBm <= -100)
    return 0;
  else if (dBm >= -50)
    return 100;
  else
    return 2 * (dBm + 100);
}

void loadDefaults()
{
  String TmpStr = "";

  // Config NOT from EEPROM
  configIsDefault = true;

  // Valid-Falg to verify config
  cfg.configisvalid = CURRENT_CONFIG_VERSION;

  memcpy(cfg.wifi_ssid, "", sizeof(cfg.wifi_ssid) / sizeof(*cfg.wifi_ssid));
  memcpy(cfg.wifi_psk, "", sizeof(cfg.wifi_psk) / sizeof(*cfg.wifi_psk));

  memcpy(cfg.hostname, "", sizeof(cfg.hostname) / sizeof(*cfg.hostname));
  memcpy(cfg.note, "", sizeof(cfg.note) / sizeof(*cfg.note));

  memcpy(cfg.admin_username, "", sizeof(cfg.admin_username) / sizeof(*cfg.admin_username));
  memcpy(cfg.admin_password, "", sizeof(cfg.admin_password) / sizeof(*cfg.admin_password));

  memcpy(cfg.api_username, "", sizeof(cfg.api_username) / sizeof(*cfg.api_username)); // Legancy setting
  memcpy(cfg.api_password, "", sizeof(cfg.api_password) / sizeof(*cfg.api_password)); // Legancy setting

  cfg.telnet = 0;

  memcpy(cfg.mqtt_server, "", sizeof(cfg.mqtt_server) / sizeof(*cfg.mqtt_server));
  memcpy(cfg.mqtt_user, "", sizeof(cfg.mqtt_user) / sizeof(*cfg.mqtt_user));
  cfg.mqtt_port = 1883;
  memcpy(cfg.mqtt_password, "", sizeof(cfg.mqtt_password) / sizeof(*cfg.mqtt_password));
  memcpy(cfg.mqtt_prefix, MQTT_DEFAULT_PREFIX, sizeof(cfg.mqtt_prefix) / sizeof(*cfg.mqtt_prefix));
  cfg.mqtt_periodic_update_interval = 10;
}

void loadConfig()
{
  //Serial.println("loadConfig!");
  EEPROM.begin(512);
  EEPROM.get(cfgStart, cfg);
  EEPROM.end();

  if (cfg.configisvalid != CURRENT_CONFIG_VERSION)
  {
    loadDefaults();
  }
  else
  {
    configIsDefault = false; // Config from EEPROM
  }
}

void updateSensors(byte packet)
{
  // Serial.print(142);
  // Serial.print(packet);
  // delay(100); // wait for sensors
  // char i = 0;
  //
  // while(Serial.available()) {
  //   int c = Serial.read();
  //   if( c==-1 ) {
  //      //rdebugA("Sensor Errror! %i\n", c);
  //   }
  //   sensorbytes[i++] = c;
  // }
  //
  // //rdebugA("VOLTAGE: %s\n", VOLTAGE);
  // //rdebugA("VOLTAGE: %i\n", VOLTAGE);
  // //rdebugA("VOLTAGE: %c\n", VOLTAGE);
  //
  // if(i < 6) {     // Size of smallest sensor packet is 6
  //   //rdebugA("Sensor Errrrrror! %i\n", i);
  // }
}

void HTMLHeader(const char *section, unsigned int refresh, const char *url)
{

  char title[50];
  char hostname[50];
  WiFi.hostname().toCharArray(hostname, 50);
  snprintf(title, 50, "Roomba@%s - %s", hostname, section);

  html = "<!DOCTYPE html>";
  html += "<html>\n";
  html += "<head>\n";
  html += "<meta name='viewport' content='width=600' />\n";
  if (refresh != 0)
  {
    html += "<META http-equiv='refresh' content='";
    html += refresh;
    html += ";URL=";
    html += url;
    html += "'>\n";
  }
  html += "<title>";
  html += title;
  html += "</title>\n";
  html += "<style>\n";
  html += "body {\n";
  html += " background-color: #EDEDED;\n";
  html += " font-family: Arial, Helvetica, Sans-Serif;\n";
  html += " Color: #333;\n";
  html += "}\n";
  html += "\n";
  html += "h1 {\n";
  html += "  background-color: #333;\n";
  html += "  display: table-cell;\n";
  html += "  margin: 20px;\n";
  html += "  padding: 20px;\n";
  html += "  color: white;\n";
  html += "  border-radius: 10px 10px 0 0;\n";
  html += "  font-size: 20px;\n";
  html += "}\n";
  html += "\n";
  html += "ul {\n";
  html += "  list-style-type: none;\n";
  html += "  margin: 0;\n";
  html += "  padding: 0;\n";
  html += "  overflow: hidden;\n";
  html += "  background-color: #333;\n";
  html += "  border-radius: 0 10px 10px 10px;";
  html += "}\n";
  html += "\n";
  html += "li {\n";
  html += "  float: left;\n";
  html += "}\n";
  html += "\n";
  html += "li a {\n";
  html += "  display: block;\n";
  html += "  color: #FFF;\n";
  html += "  text-align: center;\n";
  html += "  padding: 16px;\n";
  html += "  text-decoration: none;\n";
  html += "}\n";
  html += "\n";
  html += "li a:hover {\n";
  html += "  background-color: #111;\n";
  html += "}\n";
  html += "\n";
  html += "#main {\n";
  html += "  padding: 20px;\n";
  html += "  background-color: #FFF;\n";
  html += "  border-radius: 10px;\n";
  html += "  margin: 10px 0;\n";
  html += "}\n";
  html += "\n";
  html += "#footer {\n";
  html += "  border-radius: 10px;\n";
  html += "  background-color: #333;\n";
  html += "  padding: 10px;\n";
  html += "  color: #FFF;\n";
  html += "  font-size: 12px;\n";
  html += "  text-align: center;\n";
  html += "}\n";

  html += "table  {\n";
  html += "border-spacing: 0;\n";
  html += "}\n";

  html += "table td, table th {\n";
  html += "padding: 5px;\n";
  html += "}\n";

  html += "table tr:nth-child(even) {\n";
  html += "background: #EDEDED;\n";
  html += "}";

  html += "input[type=\"submit\"] {\n";
  html += "background-color: #333;\n";
  html += "border: none;\n";
  html += "color: white;\n";
  html += "padding: 5px 25px;\n";
  html += "text-align: center;\n";
  html += "text-decoration: none;\n";
  html += "display: inline-block;\n";
  html += "font-size: 16px;\n";
  html += "margin: 4px 2px;\n";
  html += "cursor: pointer;\n";
  html += "}\n";

  html += "input[type=\"submit\"]:hover {\n";
  html += "background-color:#4e4e4e;\n";
  html += "}\n";

  html += "input[type=\"submit\"]:disabled {\n";
  html += "opacity: 0.6;\n";
  html += "cursor: not-allowed;\n";
  html += "}\n";

  html += "</style>\n";
  //html += "<link href=\"data:image/x-icon;base64,AAABAAEAEBAAAAEAIABoBAAAFgAAACgAAAAQAAAAIAAAAAEAIAAAAAAAQAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAGgAAAAXAAAABAAAAAAAAAARAAAASQAAAEkAAAARAAAAAAAAAAQAAAAXAAAAZQAAAAAAAAAAAAAAAAAAAHwAAABgAAAAmRcXE0IrKCTQGRcL+zkyBP44MgT+FhMH+RwaFccQEAxAAAAAmQAAAF8AAAB6AAAAAAAAAFQAAABlAAAAlzIvLKR7d3H81dHJ/4eDcf9mXhv/ZV0a/4F8af+gloD/TUU27RwYE5QAAACXAAAAYgAAAFQAAABwAAAAlDg1L4yYlI3+3NjP/9zYz//V0cn/npyW/56clv/V0cn/3NjP/7atm/9fVkL3IBwUfgAAAJQAAABwAAAADAcDA01iX1r63NjP/9zYz/9xdon/GSxp/xc4pP8VNpv/IzBa/31+hP/c2M//qp+K/zw1KecDAABNAAAADAAAAAA0Myy5zMnA/9zYz/+WmaP/FTOT/yJX//8hVPX/IVT1/yBU+P8bMXr/paOi/9bSx/+Ed1z/GRgTogAAAAAAAAAAQD4459zYz//c2M//IjJl/yJW/f8WOab/ECh2/xAodv8XO63/G0rm/zY+Wf/c2M//pZqE/yQgGc4AAAAALy8pnGVjXPnc2M//3NjP/xYscP8iV///DiNn/yJX//8iV///Dydz/x1P8P8nNWL/3NjP/7SrmP8+OCvjFhQPilZSTfPc2M//3NjP/9zYz/8bMHD/Ilf//xApeP8aQcD/GUG//xAqev8cTev/MT1k/9zYz//W0sf/rqSQ/zMuI9pJRkDm29fO/8K/t//c2M//RlBy/x5N4/8hVPX/Fzuu/xc7rv8hU/T/FT/G/1lda//c2M//wr+3/7Oqlv8rJh7OLSsnuaGelP9HQA7/d3Ja/9LOxf8iL1v/HEfP/yJX//8iVv3/GD66/zU8U//SzsX/d3Ja/0dBDv93b1z/HRoUqgoKCjJtaWH5VlEu/3JnEv+VkYL/1tLJ/2dsgP8vPWj/QUhi/3V2fP/W0sn/lZGA/3FmEf9UTyz/SEEy6goKCjIAAAAAJSQesqCblP9VTiH/dWkO/2NfQv+yrqb/19PK/9fTyv+zr6b/ZF9C/3VpDf9TTiH/fHRi/hoXEqgAAAAAAAAAAAAAAAgpJyLPp6KY/15ZOf9uYxD/c2cL/2deGv9nXhr/c2cL/29jEf9fWjr/k4t8/h0ZFMQAAAAHAAAAAAAAAAAAAAAAAAAACCQgHbFmYVn2sK2k/3VxW/9lYED/ZWBA/3VxW/+xrKP/XllP8xoYEqoAAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACgoKNCEeGLE6Ni/dSUQ960hFPOo7Ny/dHh0XsAoKCjQAAAAAAAAAAAAAAAAAAAAAxCMAAIABAAAAAAAAAAAAAAAAAACAAQAAgAEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgAEAAIABAADAAwAA8A8AAA==\" rel=\"icon\" type=\"image/x-icon\">";
  html += "</head>\n";
  html += "<body>\n";
  html += "<h1>";
  html += title;
  html += "</h1>\n";
  html += "<ul>\n";
  html += "<li><a href='/'>Home</a></li>\n";
  html += "<li><a href='/actions'>Actions</a></li>\n";
  html += "<li><a href='/status'>Status</a></li>\n";
  html += "<li><a href='/settings'>Settings</a></li>\n";
  html += "<li><a href='/logs'>Logs</a></li>\n";
  html += "<li><a href='/wifiscan'>WiFi Scan</a></li>\n";
  html += "<li><a href='/fwupdate'>FW Update</a></li>\n";
  html += "<li><a href='/reboot'>Reboot </a></li>\n";
  html += "</ul>\n";
  html += "<div id='main'>";
}

void HTMLFooter()
{
  html += "</div>";
  html += "<div id='footer'>&copy; 2018 Fabian Otto - Firmware v";
  html += FIRMWARE_VERSION;
  html += " - Compiled: ";
  html += COMPILE_DATE;
  html += "</div>\n";
  html += "</body>\n";
  html += "</html>\n";
}

void handleRoot()
{
  showWEBMQTTAction();

  HTMLHeader("Main");

  html += "<table>\n";

  char timebuf[20];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;
  int days = hr / 24;
  snprintf(timebuf, 20, " %02d:%02d:%02d:%02d", days, hr % 24, min % 60, sec % 60);

  html += "<tr>\n<td>Uptime</td>\n<td>";
  html += timebuf;
  html += "</td>\n</tr>\n";

  html += "<tr>\n<td>Current Time</td>\n<td>";
  html += timeClient.getFormattedDate();
  html += "</td>\n</tr>\n";

  html += "<tr>\n<td>Firmware</td>\n<td>v";
  html += FIRMWARE_VERSION;
  html += "</td>\n</tr>\n";

  html += "<tr>\n<td>Compiled</td>\n<td>";
  html += COMPILE_DATE;
  html += "</td>\n</tr>\n";

  html += "<tr>\n<td>MQTT state:</td>\n<td>";
  if (client.connected())
  {
    html += "Connected";
  }
  else
  {
    html += "Not Connected";
  }
  html += "</td>\n</tr>\n";
  html += "<tr>\n<td>Cleaning State</td>\n<td>";
  html += (isRoombaCleaning() ? "ON" : "OFF");
  html += "</td>\n</tr>\n";

  html += "<tr>\n<td>Last clean</td>\n<td>";
  html += lastClean;
  html += "</td>\n</tr>\n";

  html += "<tr>\n<td>Dock</td>\n<td>";
  if (isRoombaCharging())
  {
    html += "YES";
  }
  else
  {
    html += "NO";
  }
  html += "</td>\n</tr>\n";

  html += "<tr>\n<td>Charge State</td>\n<td>";
  switch (CHARGE_STATE)
  {
  case 0:
    html += "Not charging";
    break;
  case 1:
    html += "Reconditioning Charging";
    break;
  case 2:
    html += "Full Charging";
    break;
  case 3:
    html += "Trickle Charging";
    break;
  case 4:
    html += "Waiting";
    break;
  case 5:
    html += "Charging Fault Condition";
    break;
  }
  html += "</td>\n</tr>\n";

  html += "<tr>\n<td>Voltage</td>\n<td>";
  snprintf(buff, sizeof(buff), "%.2f", ((float)VOLTAGE / 1000));
  html += buff;
  html += "V</td>\n</tr>\n";

  html += "<tr>\n<td>Current</td>\n<td>";
  html += CURRENT;
  html += "mA</td>\n</tr>\n";

  html += "<tr>\n<td>Temperature</td>\n<td>";
  html += TEMP;
  html += "&deg;C</td>\n</tr>\n";

  html += "<tr>\n<td>Charge level</td>\n<td>";
  if (CAPACITY > 0 && CHARGE > 0)
  {
    snprintf(buff, sizeof(buff), "%.2f", (100 / (float)CAPACITY) * CHARGE);
    html += buff;
  }
  else
  {
    html += "?";
  }
  html += "%</td>\n</tr>\n";

  html += "<tr>\n<td>Accu capacity</td>\n<td>";
  html += CHARGE;
  html += "mA of ";
  html += CAPACITY;
  html += "mA</td>\n</tr>\n";

  html += "<tr>\n<td>Note</td>\n<td>";
  if (strcmp(cfg.note, "") == 0)
  {
    html += "---";
  }
  else
  {
    html += cfg.note;
  }
  html += "</td>\n</tr>\n";

  html += "<tr>\n<td>Hostname</td>\n<td>";
  html += WiFi.hostname().c_str();
  html += "</td>\n</tr>\n";

  html += "<tr>\n<td>IP</td>\n<td>";
  html += WiFi.localIP().toString();
  html += "</td>\n</tr>\n";

  html += "<tr>\n<td>Gateway</td>\n<td>";
  html += WiFi.gatewayIP().toString();
  html += "</td>\n</tr>\n";

  html += "<tr>\n<td>Subnetmask</td>\n<td>";
  html += WiFi.subnetMask().toString();
  html += "</td>\n</tr>\n";

  html += "<tr>\n<td>MAC</td>\n<td>";
  html += WiFi.macAddress().c_str();
  html += "</td>\n</tr>\n";

  html += "<tr>\n<td>Signal strength</td>\n<td>";
  html += RSSI2Quality(WiFi.RSSI());
  html += "% (";
  html += WiFi.RSSI();
  html += "dBm)</td>\n</tr>\n";

  html += "<tr>\n<td>Client IP:</td>\n<td>";
  html += server.client().remoteIP().toString().c_str();
  html += "</td>\n</tr>\n";

  html += "<tr>\n<td>Telnet</td>\n<td>";
  html += (cfg.telnet == 1 ? "On" : "Off");
  html += " (Active: ";
  html += Debug.isActive(Debug.ANY);
  html += ")</td>\n</tr>\n";

  // Filesystem
  FSInfo fs_info;
  SPIFFS.info(fs_info);
  html += "<tr>\n<td>Speicherplatz</td>\n<td>";
  html += (fs_info.totalBytes / 1000);
  html += " kByte</td>\n</tr>\n";
  html += "<tr>\n<td>Benutzt</td>\n<td>";
  html += (fs_info.usedBytes / 1000);
  html += " kByte (";
  html += (100 / fs_info.totalBytes) * fs_info.usedBytes;
  html += "%)</td>\n</tr>\n";
  /*html += "<tr>\n<td>pageSize</td>\n<td>";
    html += fs_info.pageSize;
    html += "</td>\n</tr>\n";
    html += "<tr>\n<td>maxOpenFiles</td>\n<td>";
    html += fs_info.maxOpenFiles;
    html += "</td>\n</tr>\n";
    html += "<tr>\n<td>maxPathLength</td>\n<td>";
    html += fs_info.maxPathLength;
    html += "</td>\n</tr>\n";*/

  html += "</table>\n";

  HTMLFooter();
  server.send(200, "text/html", html);
}

void handleLogs()
{
  showWEBMQTTAction();
  if (!server.authenticate(cfg.admin_username, cfg.admin_password))
  {
    return server.requestAuthentication();
  }
  else
  {
    if (server.method() == HTTP_POST)
    {
      for (uint8_t i = 0; i < server.args(); i++)
      {
        if (server.argName(i) == "delete")
        {
          SPIFFS.remove(logFileName);
          logWrite("deleted logfile");
        }
        else if (server.argName(i) == "format")
        {
          SPIFFS.format();
          logWrite("formated filesystem");
        }
      }
    }

    HTMLHeader("Logs");

    html += "<form method='POST' action='/logs'>";

    File logFile = SPIFFS.open(logFileName, "r");

    if (!logFile)
    {
      html += "Oeffnen der Datei fehlgeschlagen";
    }
    else
    {
      html += "Logfile: ";
      html += logFileName;
      html += " (";
      html += (logFile.size() / 1000);
      html += " kByte)<br /><hr><pre>\n";
      while (logFile.available())
      {
        html += char(logFile.read());
      }
      logFile.close();
      html += "</pre>\n";
    }

    html += "<hr><input type='submit' name='delete' value='Delete Logfile'>";
    html += " <input type='submit' name='format' value='Format FS' disabled>";
    html += "</form>";

    HTMLFooter();
    server.send(200, "text/html", html);
  }
}

void handleSettings()
{
  showWEBMQTTAction();
  // HTTP Auth
  if (!server.authenticate(cfg.admin_username, cfg.admin_password))
  {
    return server.requestAuthentication();
  }
  else
  {
    boolean saveandreboot = false;
    String value;
    if (server.method() == HTTP_POST)
    { // Save Settings

      // Telnet - disable first and update only when on is in form data because its a checkbox
      cfg.telnet = 0;

      for (uint8_t i = 0; i < server.args(); i++)
      {
        // Trim String
        value = server.arg(i);
        value.trim();

        memcpy(cfg.api_username, "", sizeof(cfg.api_username) / sizeof(*cfg.api_username)); // Legancy setting
        memcpy(cfg.api_password, "", sizeof(cfg.api_password) / sizeof(*cfg.api_password)); // Legancy setting

        // Note
        if (server.argName(i) == "note")
        {
          value.toCharArray(cfg.note, 30);
        } // HTTP Auth Adminaccess Username
        else if (server.argName(i) == "admin_username")
        {
          value.toCharArray(cfg.admin_username, 30);
        } // HTTP Auth Adminaccess Password
        else if (server.argName(i) == "admin_password")
        {
          value.toCharArray(cfg.admin_password, 30);
        } // WiFi SSID
        else if (server.argName(i) == "ssid")
        {
          value.toCharArray(cfg.wifi_ssid, 30);
        } // WiFi PSK
        else if (server.argName(i) == "psk")
        {
          value.toCharArray(cfg.wifi_psk, 30);
        } // Hostname
        else if (server.argName(i) == "hostname")
        {
          value.toCharArray(cfg.hostname, 30);
        } // MQTT Server
        else if (server.argName(i) == "mqtt_server")
        {
          value.toCharArray(cfg.mqtt_server, sizeof(cfg.mqtt_server) / sizeof(*cfg.mqtt_server));
        } // MQTT Port
        else if (server.argName(i) == "mqtt_port")
        {
          cfg.mqtt_port = value.toInt();
        } // MQTT User
        else if (server.argName(i) == "mqtt_user")
        {
          value.toCharArray(cfg.mqtt_user, sizeof(cfg.mqtt_user) / sizeof(*cfg.mqtt_user));
        } // MQTT Password
        else if (server.argName(i) == "mqtt_password")
        {
          value.toCharArray(cfg.mqtt_password, sizeof(cfg.mqtt_password) / sizeof(*cfg.mqtt_password));
        } // MQTT Prefix
        else if (server.argName(i) == "mqtt_prefix")
        {
          value.toCharArray(cfg.mqtt_prefix, sizeof(cfg.mqtt_prefix) / sizeof(*cfg.mqtt_prefix));
        } // MQTT periodic update interval
        else if (server.argName(i) == "mqtt_periodic_update_interval")
        {
          cfg.mqtt_periodic_update_interval = value.toInt();
        } // Telnet
        else if (server.argName(i) == "telnet")
        {
          cfg.telnet = 1;
        }
        saveandreboot = true;
      }
    }

    if (saveandreboot)
    {
      HTMLHeader("Settings", 10, "/settings");
      html += ">>> New Settings saved! Device will be reboot <<< ";
    }
    else
    {
      HTMLHeader("Settings");

      html += "Current Settings Source is ";
      html += (configIsDefault ? "NOT " : "");
      html += "from EEPROM.<br />";
      html += "<br />\n";

      html += "<form action='/settings' method='post'>\n";
      html += "<table>\n";
      html += "<tr>\n";
      html += "<td>Hostname:</td>\n";
      html += "<td><input name='hostname' type='text' maxlength='30' autocapitalize='none' placeholder='";
      html += WiFi.hostname().c_str();
      html += "' value='";
      html += cfg.hostname;
      html += "'></td></tr>\n";

      html += "<tr>\n<td>\nSSID:</td>\n";
      html += "<td><input name='ssid' type='text' autocapitalize='none' maxlength='30' value='";
      bool showssidfromcfg = true;
      if (server.method() == HTTP_GET)
      {
        if (server.arg("ssid") != "")
        {
          html += server.arg("ssid");
          showssidfromcfg = false;
        }
      }
      if (showssidfromcfg)
      {
        html += cfg.wifi_ssid;
      }
      html += "'> <a href='/wifiscan' onclick='return confirm(\"Go to scan side? Changes will be lost!\")'>Scan</a></td>\n</tr>\n";

      html += "<tr>\n<td>\nPSK:</td>\n";
      html += "<td><input name='psk' type='password' maxlength='30' value='";
      html += cfg.wifi_psk;
      html += "'></td>\n</tr>\n";

      html += "<tr>\n<td>\nNote:</td>\n";
      html += "<td><input name='note' type='text' maxlength='30' value='";
      html += cfg.note;
      html += "'></td>\n</tr>\n";

      html += "<tr>\n<td>\nAdminaccess Username:</td>\n";
      html += "<td><input name='admin_username' type='text' maxlength='30' autocapitalize='none' value='";
      html += cfg.admin_username;
      html += "'></td>\n</tr>\n";

      html += "<tr>\n<td>\nAdminaccess Password:</td>\n";
      html += "<td><input name='admin_password' type='password' maxlength='30' value='";
      html += cfg.admin_password;
      html += "'></td>\n</tr>\n";

      html += "<tr>\n<td>\nMQTT server:</td>\n";
      html += "<td><input name='mqtt_server' type='text' maxlength='30' autocapitalize='none' value='";
      html += cfg.mqtt_server;
      html += "'></td>\n</tr>\n";

      html += "<tr>\n<td>\nMQTT port:</td>\n";
      html += "<td><input name='mqtt_port' type='text' maxlength='5' autocapitalize='none' value='";
      html += cfg.mqtt_port;
      html += "'> (Default 1883)</td>\n</tr>\n";

      html += "<tr>\n<td>\nMQTT username:</td>\n";
      html += "<td><input name='mqtt_user' type='text' maxlength='50' autocapitalize='none' value='";
      html += cfg.mqtt_user;
      html += "'></td>\n</tr>\n";

      html += "<tr>\n<td>\nMQTT password:</td>\n";
      html += "<td><input name='mqtt_password' type='password' maxlength='50' autocapitalize='none' value='";
      html += cfg.mqtt_password;
      html += "'></td>\n</tr>\n";

      html += "<tr>\n<td>\nMQTT prefix:</td>\n";
      html += "<td><input name='mqtt_prefix' type='text' maxlength='30' autocapitalize='none' value='";
      html += cfg.mqtt_prefix;
      html += "'></td>\n</tr>\n";

      html += "<tr>\n<td>\nMQTT periodic update interval:</td>\n";
      html += "<td><input name='mqtt_periodic_update_interval' type='text' maxlength='5' autocapitalize='none' value='";
      html += cfg.mqtt_periodic_update_interval;
      html += "'> (in sec. 0 to disable)</td>\n</tr>\n";

      html += "<tr>\n<td>\nEnable Telnet:</td>\n";
      html += "<td><input type='checkbox' name='telnet' ";
      html += (cfg.telnet ? "checked" : "");
      html += "></td>\n</tr>\n";

      html += "</table>\n";

      html += "<br />\n";
      html += "<input type='submit' value='Save'>\n";
      html += "</form>\n";
    }
    HTMLFooter();
    server.send(200, "text/html", html);

    if (saveandreboot)
    {
      saveConfig();
      ESP.reset();
    }
  }
}

void handleActions()
{
  showWEBMQTTAction();

  if (!server.authenticate(cfg.admin_username, cfg.admin_password))
  {
    return server.requestAuthentication();
  }
  else
  {

    if (server.method() == HTTP_POST)
    {
      for (uint8_t i = 0; i < server.args(); i++)
      {
        if (server.argName(i) == "action")
        {
          if (server.arg(i) == "Wake")
          {
            roombaCmd(RoombaCMDs::RMB_WAKE, StatusTrigger::WEB);
          }
          else if (server.arg(i) == "Start (OI)")
          {
            roombaCmd(RoombaCMDs::RMB_START, StatusTrigger::WEB);
          }
          else if (server.arg(i) == "Stop (OI)")
          {
            roombaCmd(RoombaCMDs::RMB_STOP, StatusTrigger::WEB);
          }
          else if (server.arg(i) == "Toggle Clean")
          {
            roombaCmd(RoombaCMDs::RMB_CLEAN, StatusTrigger::WEB);
          }
          else if (server.arg(i) == "Max")
          {
            roombaCmd(RoombaCMDs::RMB_MAX, StatusTrigger::WEB);
          }
          else if (server.arg(i) == "Spot")
          {
            roombaCmd(RoombaCMDs::RMB_SPOT, StatusTrigger::WEB);
          }
          else if (server.arg(i) == "Dock")
          {
            roombaCmd(RoombaCMDs::RMB_DOCK, StatusTrigger::WEB);
          }
          else if (server.arg(i) == "Power off")
          {
            roombaCmd(RoombaCMDs::RMB_POWER, StatusTrigger::WEB);
          }
          else if (server.arg(i) == "Reset Roomba")
          {
            roombaCmd(RoombaCMDs::RMB_RESET, StatusTrigger::WEB);
          }
        }
      }
    }

    HTMLHeader("Switch Socket");
    html += "<form method='POST' action='/actions'>";
    html += "<input type='submit' name='action' value='Wake'>";
    html += "<input type='submit' name='action' value='Start (OI)'>";
    html += "<input type='submit' name='action' value='Stop (OI)'>";
    html += "<br /><br />";
    html += "<input type='submit' name='action' value='Toggle Clean'>";
    html += "<input type='submit' name='action' value='Max'>";
    html += "<input type='submit' name='action' value='Spot'>";
    html += "<input type='submit' name='action' value='Dock'>";
    html += "<br /><br />";
    html += "<input type='submit' name='action' value='Power off'>";
    html += "<input type='submit' name='action' value='Reset Roomba'>";
    html += "</form>";

    HTMLFooter();
    server.send(200, "text/html", html);
  }
}

void handleStatus()
{
  showWEBMQTTAction();

  if (!server.authenticate(cfg.admin_username, cfg.admin_password))
  {
    return server.requestAuthentication();
  }
  else
  {

    HTMLHeader("Status");
    html += "<form method='POST' action='/status'><br />";
    html += "<input type='text' name='singlesensorid' value=''>";
    html += "<input type='submit' name='singlesensor' value='Single Sensor'>";
    html += "<input type='submit' name='sensorgroup' value='Sensor Group 3'>";

    if (server.method() == HTTP_POST)
    {
      html += "<br /><br /><b>Result:</b>";
      for (uint8_t i = 0; i < server.args(); i++)
      {
        if (server.argName(i) == "sensorgroup")
        {
          unsigned int sensorPackets = getSensorStatus(true);

          snprintf(buff, sizeof(buff), "<br />Packets: %i<br />", sensorPackets);
          html += buff;

          if (sensorPackets > 0)
          {
            snprintf(buff, sizeof(buff), "CHARGE_STATE: %i<br />", CHARGE_STATE);
            html += buff;
            snprintf(buff, sizeof(buff), "VOLTAGE: %i<br />", VOLTAGE);
            html += buff;
            snprintf(buff, sizeof(buff), "CURRENT: %i<br />", CURRENT);
            html += buff;
            snprintf(buff, sizeof(buff), "TEMP: %i<br />", TEMP);
            html += buff;
            snprintf(buff, sizeof(buff), "CHARGE: %i<br />", CHARGE);
            html += buff;
            snprintf(buff, sizeof(buff), "CAPACITY: %i<br />", CAPACITY);
            html += buff;
          }
          else
          {
            html += "No data";
          }
        }
        else if (server.argName(i) == "singlesensorid")
        {
          String packetID;
          packetID = server.arg(i);
          packetID.trim();
          if (packetID != "")
          {
            //rdebugA("PackedID: %s (%i)\n", packetID.c_str(), (int)packetID.toInt());
            int result;
            if (getRoombaSensorPacket(packetID.toInt(), result))
            {
              html += result;
            }
            else
            {
              html += "No data";
            }
          }
        }
      }
    }

    html += "</form>";

    HTMLFooter();
    server.send(200, "text/html", html);
  }
}

void handleWiFiScan()
{
  showWEBMQTTAction();
  if (!server.authenticate(cfg.admin_username, cfg.admin_password))
  {
    return server.requestAuthentication();
  }
  else
  {

    HTMLHeader("WiFi Scan");

    int n = WiFi.scanNetworks();
    if (n == 0)
    {
      html += "No networks found.\n";
    }
    else
    {
      html += "<table>\n";
      html += "<tr>\n";
      html += "<th>#</th>\n";
      html += "<th>SSID</th>\n";
      html += "<th>Channel</th>\n";
      html += "<th>Signal</th>\n";
      html += "<th>RSSI</th>\n";
      html += "<th>Encryption</th>\n";
      html += "<th>BSSID</th>\n";
      html += "</tr>\n";
      for (int i = 0; i < n; ++i)
      {
        html += "<tr>\n";
        snprintf(buff, sizeof(buff), "%02d", (i + 1));
        html += String("<td>") + buff + String("</td>");
        html += "<td>\n";
        if (WiFi.isHidden(i))
        {
          html += "[hidden SSID]";
        }
        else
        {
          html += "<a href='/settings?ssid=";
          html += WiFi.SSID(i).c_str();
          html += "'>";
          html += WiFi.SSID(i).c_str();
          html += "</a>";
        }
        html += "</td>\n<td>";
        html += WiFi.channel(i);
        html += "</td>\n<td>";
        html += RSSI2Quality(WiFi.RSSI(i));
        html += "%</td>\n<td>";
        html += WiFi.RSSI(i);
        html += "dBm</td>\n<td>";
        switch (WiFi.encryptionType(i))
        {
        case ENC_TYPE_WEP: // 5
          html += "WEP";
          break;
        case ENC_TYPE_TKIP: // 2
          html += "WPA TKIP";
          break;
        case ENC_TYPE_CCMP: // 4
          html += "WPA2 CCMP";
          break;
        case ENC_TYPE_NONE: // 7
          html += "OPEN";
          break;
        case ENC_TYPE_AUTO: // 8
          html += "WPA";
          break;
        }
        html += "</td>\n<td>";
        html += WiFi.BSSIDstr(i).c_str();
        html += "</td>\n";
        html += "</tr>\n";
      }
      html += "</table>";
    }

    HTMLFooter();

    server.send(200, "text/html", html);
  }
}

void handleReboot()
{
  showWEBMQTTAction();
  if (!server.authenticate(cfg.admin_username, cfg.admin_password))
  {
    return server.requestAuthentication();
  }
  else
  {
    boolean reboot = false;
    if (server.method() == HTTP_POST)
    {
      HTMLHeader("Reboot", 10, "/");
      html += "Reboot in progress...";
      reboot = true;
    }
    else
    {
      HTMLHeader("Reboot");
      html += "<form method='POST' action='/reboot'>";
      html += "<input type='submit' value='Reboot'>";
      html += "</form>";
    }
    HTMLFooter();

    server.send(200, "text/html", html);

    if (reboot)
    {
      delay(200);
      ESP.reset();
    }
  }
}

void handleFWUpdate()
{
  showWEBMQTTAction();
  if (!server.authenticate(cfg.admin_username, cfg.admin_password))
  {
    return server.requestAuthentication();
  }
  else
  {
    HTMLHeader("Firmware Update");

    html += "<form method='POST' action='/dofwupdate' enctype='multipart/form-data'>\n";
    html += "<table>\n";
    html += "<tr>\n";
    html += "<td>Current Version</td>\n";
    html += String("<td>") + FIRMWARE_VERSION + String("</td>\n");
    html += "</tr>\n";
    html += "<tr>\n";
    html += "<td>Compiled</td>\n";
    html += String("<td>") + COMPILE_DATE + String("</td>\n");
    html += "</tr>\n";
    html += "<tr>\n";
    html += "<td>Upload</td>\n";
    html += "<td><input type='file' name='update'></td>\n";
    html += "</tr>\n";
    html += "</table>\n";
    html += "<br />";
    html += "<input type='submit' value='Update'>";
    html += "</form>";
    HTMLFooter();
    server.send(200, "text/html", html);
  }
}

void handleNotFound()
{
  showWEBMQTTAction();
  HTMLHeader("File Not Found");
  html += "URI: ";
  html += server.uri();
  html += "<br />\nMethod: ";
  html += (server.method() == HTTP_GET) ? "GET" : "POST";
  html += "<br />\nArguments: ";
  html += server.args();
  html += "<br />\n";
  HTMLFooter();
  for (uint8_t i = 0; i < server.args(); i++)
  {
    html += " " + server.argName(i) + ": " + server.arg(i) + "<br />\n";
  }

  server.send(404, "text/html", html);
}

void onConnected(const WiFiEventStationModeConnected &evt)
{
  logWrite("WiFi connected");
  bIsConnected = true;
}

void onDisconnected(const WiFiEventStationModeDisconnected &evt)
{
  if (bIsConnected)
  { // First time disconnect
    logWrite("WiFi disconnected");
    bIsConnected = false;
  }
}

void MQTTprocessCommand(JsonObject &json)
{
  //Serial.println(F("Processing incomming MQTT command"));

  // Power on/off
  if (json.containsKey("clean"))
  {
    if (json["clean"].as<boolean>())
    {
      roombaCmd(RoombaCMDs::RMB_CLEAN, StatusTrigger::MQTT);
      timeClient.getFormattedDate().toCharArray(lastClean, sizeof(lastClean) / sizeof(*lastClean));
    }
    else if (!json["clean"].as<boolean>())
    {
      if (isRoombaCleaning())
      {
        roombaCmd(RoombaCMDs::RMB_CLEAN); // Stop cleaning
      }
    }
  }

  if (json.containsKey("dock"))
  {
    if (json["dock"].as<boolean>())
    {
      if (isRoombaCleaning())
      {
        roombaCmd(RoombaCMDs::RMB_CLEAN); // Stop cleaning
      }
      delay(2000);
      roombaCmd(RoombaCMDs::RMB_DOCK, StatusTrigger::MQTT);
    }
    else if (!json["dock"].as<boolean>())
    {
      // ignor it
    }
  }

  // Trigger status update
  if (json.containsKey("status"))
  {
    getSensorStatus(true); // force status update
    MQTTpublishStatus(StatusTrigger::MQTT);
  }
}

void MQTTcallback(char *topic, byte *payload, unsigned int length)
{
  showWEBMQTTAction();
  //Serial.println(F("Neq MQTT message (MQTTcallback)"));
  //Serial.print(F("> Lenght: "));
  //Serial.println(length);
  //Serial.print(F("> Topic: "));
  //Serial.println(topic);

  if (length)
  {
    StaticJsonDocument<256> jsondoc;
    DeserializationError err = deserializeJson(jsondoc, payload);
    if (err)
    {
      //Serial.print(F("deserializeJson() failed: "));
      //Serial.println(err.c_str());
    }
    else
    {

      //Serial.print(F("> JSON: "));
      serializeJsonPretty(jsondoc, Serial);
      //Serial.println();

      JsonObject object = jsondoc.as<JsonObject>();
      MQTTprocessCommand(object);
    }
  }
}

boolean MQTTreconnect()
{

  //Serial.printf_P(PSTR("Connecting to MQTT Broker \"%s:%i\"..."), cfg.mqtt_server, cfg.mqtt_port);
  if (strcmp(cfg.mqtt_server, "") == 0)
  {
    //Serial.println(F("failed. No server configured."));
    return false;
  }
  else
  {

    client.setServer(cfg.mqtt_server, cfg.mqtt_port);
    client.setCallback(MQTTcallback);

    //last will and testament topic
    snprintf(buff, sizeof(buff), MQTT_PUBLISH_STATUS_TOPIC, mqtt_prefix, WiFi.hostname().c_str());

    if (client.connect(WiFi.hostname().c_str(), cfg.mqtt_user, cfg.mqtt_password, buff, 0, 1, MQTT_LWT_MESSAGE))
    {
      //Serial.println(F("connected!"));

      snprintf(buff, sizeof(buff), MQTT_SUBSCRIBE_CMD_TOPIC1, mqtt_prefix);
      client.subscribe(buff);
      //Serial.printf_P(PSTR("Subscribed to topic %s\n"), buff);

      snprintf(buff, sizeof(buff), MQTT_SUBSCRIBE_CMD_TOPIC2, mqtt_prefix, WiFi.hostname().c_str());
      client.subscribe(buff);
      //Serial.printf_P(PSTR("Subscribed to topic %s\n"), buff);
      return true;
    }
    else
    {
      //Serial.print(F("failed with state "));
      //Serial.println(client.state());
      return false;
    }
  }
}

void setup(void)
{

  // Setting the I/O pin modes
  pinMode(PIN_BUTTON, INPUT);
  pinMode(PIN_LED_WIFI, OUTPUT);
  //pinMode(PIN_LED_NODEMCU, OUTPUT);
  pinMode(PIN_BRC, OUTPUT);

  // WiFi Status LED on
  digitalWrite(PIN_LED_WIFI, HIGH);

  // NodeMCU LED on
  //digitalWrite(PIN_LED_WIFI, LOW);

  // Baudrate Change/Wake-Pin Low
  digitalWrite(PIN_BRC, LOW);

  // Serial
  Serial.begin(115200);
  Serial.swap();

  // SPIFFS
  SPIFFS.begin();
  logWrite("- - - S T A R T - - -");

  // Load Config
  loadConfig();

  // Begin Wifi
  WiFi.mode(WIFI_OFF);

  // Register WiFi event handlers.
  mConnectHandler = WiFi.onStationModeConnected(&onConnected);
  mDisConnectHandler = WiFi.onStationModeDisconnected(&onDisconnected);

  // AP or Infrastructire mode
  if (configIsDefault)
  {
    // Start AP
    //Serial.println("configIsDefault: AP Mode");
    WiFi.softAP("Roomba", "");
    digitalWrite(PIN_LED_WIFI, HIGH);
  }
  else
  {
    // Connecting to a WiFi network
    //Serial.println("configIsNotDefault: Connecting to WiFi");
    WiFi.mode(WIFI_STA);
    if (strcmp(cfg.hostname, "") != 0)
    {
      WiFi.hostname(cfg.hostname);
    }
    WiFi.begin(cfg.wifi_ssid, cfg.wifi_psk);

    while (WiFi.status() != WL_CONNECTED)
    {
      delay(250);
      //Serial.print(".");

      // Blink WiFi LED
      if (wifiledState == HIGH)
      {
        wifiledState = LOW;
      }
      else
      {
        wifiledState = HIGH;
      }
      digitalWrite(PIN_LED_WIFI, wifiledState);

      // handleButton
      handleButton();
    }

    //Serial.println();
    //Serial.print("WiFi connected with ip ");
    //Serial.println(WiFi.localIP());
    digitalWrite(PIN_LED_WIFI, LOW);

    //WiFi.printDiag(Serial);
  }

  // Telnet debug
  if (cfg.telnet)
  {
    Debug.begin(WiFi.hostname()); // Initiaze the telnet server
    //Debug.setSerialEnabled(true);   // All messages too send to serial too, and can be see in serial monitor
    //Debug.setResetCmdEnabled(true); // Enable the reset command
  }

  // NTPClient
  timeClient.begin();
  rdebugVln("NTP Client started!");

  // Arduino OTA Update
  httpUpdater.setup(&server, "/dofwupdate", cfg.admin_username, cfg.admin_password);

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.on("/settings", handleSettings);
  server.on("/reboot", handleReboot);
  server.on("/actions", handleActions);
  server.on("/status", handleStatus);
  server.on("/fwupdate", handleFWUpdate);
  server.on("/wifiscan", handleWiFiScan);
  server.on("/logs", handleLogs);
  server.begin();

  logWrite("HTTP server started");

  // Update NTPClient for the first time
  timeClient.update();

  // finish setup
  logWrite("Setup function done");
}

void loop(void)
{

  //showWEBMQTTAction
  if (millis() - lastLEDTime >= LED_WEB_MIN_TIME)
  {
    digitalWrite(PIN_LED_WIFI, LOW);
  }

  // Button
  handleButton();

  // handle if we have a wifi connection
  if (WiFi.status() == WL_CONNECTED)
  {
    // Webserver
    server.handleClient();

    // Remote Debug
    if (cfg.telnet)
    {
      //Debug.handle();
    }

    // NTPClient Update
    timeClient.update();

    // MQTT - if config valid
    if (!configIsDefault)
    {

      if (!client.connected())
      {
        // MQTT connect
        if (mqttLastReconnectAttempt == 0 || (millis() - mqttLastReconnectAttempt) >= MQTT_RECONNECT_INTERVAL)
        {
          mqttLastReconnectAttempt = millis();

          // try to reconnect
          if (MQTTreconnect())
          {
            mqttLastReconnectAttempt = 0;
          }
        }
      }
      else
      {
        // Handle MQTT msgs
        client.loop();

        // send periodic update if enabled
        if (cfg.mqtt_periodic_update_interval > 0)
        {
          if (millis() >= nextPublishTime)
          {
            MQTTpublishStatus(StatusTrigger::PERIODIC);
          }
        }
      }
    }
  }
}