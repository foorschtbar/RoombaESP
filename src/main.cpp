/* BOARD SETTINGS
  Board : NodeMCU 1.0
*/

// Remote debug over telnet - not recommended for production, only for development
#include <RemoteDebug.h> //https://github.com/JoaoLopesF/RemoteDebug
#include <NTPClient.h>
#include <WiFiUdp.h> // needed by NTPClient.h
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <FS.h>
#include <myTypes.h> // Include my type definitions (must be in a separate file!)

// Pins
#define PinWiFiLED D2 // Blue/Wifi LED (LED_BUILTIN=Nodemcu, D2 extern, D4=ESP-Chip-LED)
#define SERIAL_RX D5  // pin for SoftwareSerial RX
#define SERIAL_TX D6  // pin for SoftwareSerial TX
#define BRC_PIN D1
#define PinPushBtn D3

char sensorbytes[10];
#define CHARGE_STATE (int)(sensorbytes[0])
#define VOLTAGE (int)((sensorbytes[1] << 8) + sensorbytes[2])
#define CURRENT (signed short int)((sensorbytes[3] << 8) + sensorbytes[4])
#define TEMP (int)(sensorbytes[5])
#define CHARGE (int)((sensorbytes[6] << 8) + sensorbytes[7])
#define CAPACITY (int)((sensorbytes[8] << 8) + sensorbytes[9])

//Softserial
SoftwareSerial Roomba(SERIAL_RX, SERIAL_TX);

// optional parameters in function prototype
void HTMLHeader(char *section, int refresh = 0, char *url = "/");

// Firmware Info
const char *firmware = "1.4";
const char compile_date[] = __DATE__ " " __TIME__;

// Config
int cfgStart = 0;             // Start address in EEPROM for structure 'cfg'
configData_t cfg;             // Instance 'cfg' is a global variable with 'configData_t' structure now
bool configIsDefault = false; // true if no valid config found in eeprom and defaults settings loaded

//Webserver
ESP8266WebServer server(80);

// OTA Updater
ESP8266HTTPUpdateServer httpUpdater;

// Telnet debug
RemoteDebug Debug;

// NTP Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);

// Variables will change
int wifiledState = HIGH;
boolean postWSActIsActive = false;
unsigned long previousMillis = 0;               // will store last time LED was updated
unsigned long previousMillisStatusBlinking = 0; // will store last time LED was updated
unsigned long buttonTimer = 0;                  // will store how long button was pressed
unsigned long lastClean = 0;                    // will store last Clean
unsigned long statusLEDinterval = 0;
bool inp_status = 1;
bool bIsConnected = false;

// buffers
String html;
char buff[255];

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
  RMB_POWER
};
enum class APICMDs
{
  API_ON,
  API_OFF,
  API_STATUS,
  API_DOCK,
  API_DOCK_STATUS
};
const long wifiledoffinterval = 300; // interval at which to blink (milliseconds)
const long checkWiFiinterval = 10000;
const long longPressTime = 10000;
const char *logFileName = "log.txt";

//Wifi
WiFiEventHandler mConnectHandler;
WiFiEventHandler mDisConnectHandler;

void logWrite(char *text)
{
  /*
  File logFile = SPIFFS.open(logFileName, "a+");
  if (!logFile) {
    rdebugA("Opening file for write failed%s", "\n");
  } else {
    logFile.print(timeClient.getFormattedDate());
    logFile.print('\t');
    logFile.println(text);
    logFile.close();
  }
  */
  // Send to telnet debug
  rdebugA("%s\n", text);
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
  for (int i = cfgStart; i < sizeof(cfg); i++)
  {
    EEPROM.write(i, 0);
  }
  delay(200);
  EEPROM.commit();
  EEPROM.end();
}

void handleButton()
{
  bool inp = digitalRead(PinPushBtn);
  if (inp == 0)
  {
    if (inp != inp_status)
    {
      rdebugA("Button pressed %s", "\n");
      Serial.println("Button pressed short");
      ESP.reset();
      buttonTimer = millis();
    }
    if ((millis() - buttonTimer >= longPressTime))
    {
      Serial.println("Button pressed long");
      eraseConfig();
      ESP.reset();
    }
  }
  inp_status = inp;
}

void rmb_cmd(RoombaCMDs cmd)
{

  // RMB_WAKE, RMB_START, RMB_STOP, RMB_CLEAN, RMB_MAX, RMB_SPOT, RMB_DOCK, RMB_POWER
  switch (cmd)
  {
  case RoombaCMDs::RMB_WAKE:
    logWrite("Send command RMB_WAKE to Roomba");
    pinMode(BRC_PIN,OUTPUT);
    delay(50);
    digitalWrite(BRC_PIN,LOW);
    // delay(100);
    // digitalWrite(BRC_PIN,HIGH);
    // delay(100);
    // digitalWrite(BRC_PIN,LOW);
    // delay(100);
    // digitalWrite(BRC_PIN,HIGH);
    // delay(100);
    // digitalWrite(BRC_PIN,LOW);
    // delay(100);
    pinMode(BRC_PIN,INPUT);
    delay(200);
    // Roomba.begin(19200, SERIAL_RX, SERIAL_TX);
    delay(200);
    break;
  case RoombaCMDs::RMB_START:
    logWrite("Send command RMB_START to Roomba");
    Roomba.write(128); // Start
    delay(500);
    break;
  case RoombaCMDs::RMB_STOP:
    logWrite("Send command RMB_STOP to Roomba");
    rmb_cmd(RoombaCMDs::RMB_WAKE);
    rmb_cmd(RoombaCMDs::RMB_START);
    Roomba.write(173); // Stop
    break;
  case RoombaCMDs::RMB_CLEAN:
    logWrite("Send command RMB_CLEAN to Roomba");
    rmb_cmd(RoombaCMDs::RMB_WAKE);
    rmb_cmd(RoombaCMDs::RMB_START);
    Roomba.write(135); // Clean
    lastClean = millis();
    break;
  case RoombaCMDs::RMB_MAX:
    logWrite("Send command RMB_MAX to Roomba");
    rmb_cmd(RoombaCMDs::RMB_WAKE);
    rmb_cmd(RoombaCMDs::RMB_START);
    Roomba.write(136); // Max
    break;
  case RoombaCMDs::RMB_SPOT:
    logWrite("Send command RMB_SPOT to Roomba");
    rmb_cmd(RoombaCMDs::RMB_WAKE);
    rmb_cmd(RoombaCMDs::RMB_START);
    Roomba.write(134); // Spot
    break;
  case RoombaCMDs::RMB_DOCK:
    logWrite("Send command RMB_DOCK to Roomba");
    rmb_cmd(RoombaCMDs::RMB_WAKE);
    rmb_cmd(RoombaCMDs::RMB_START);
    Roomba.write(143); // Seek Dock
    break;
  case RoombaCMDs::RMB_POWER:
    logWrite("Send command RMB_POWER to Roomba");
    rmb_cmd(RoombaCMDs::RMB_WAKE);
    rmb_cmd(RoombaCMDs::RMB_START);
    Roomba.write(133); // powers down Roomba
  default:
    break;
  }
}

bool getRoombaSensorPackets()
{

  Roomba.flush();

  rmb_cmd(RoombaCMDs::RMB_WAKE);
  rmb_cmd(RoombaCMDs::RMB_START);
  Roomba.write(142);
  delay(50);
  Roomba.write(3);
  delay(500);

  if (Roomba.available() > 0)
  {
    rdebugA("Es gibt etwas zu lesen! %i\n", Roomba.available());
    char i = 0;

    while (Roomba.available())
    {
      int c = Roomba.read();
      sensorbytes[i++] = c;
    }
    rdebugA("CHARGE_STATE: %i\n", CHARGE_STATE);
    rdebugA("VOLTAGE: %i\n", VOLTAGE);
    rdebugA("CURRENT: %i\n", CURRENT);
    rdebugA("TEMP: %i\n", TEMP);
    rdebugA("CHARGE: %i\n", CHARGE);
    rdebugA("CAPACITY: %i\n", CAPACITY);
    return true;
  }
  else
  {
    rdebugA("Es gibt nichts zu lesen! %s\n", "");
    return false;
  }
}

bool getRoombaSensorPacket(char PacketID, int &result)
{

  int low = 0;
  int high = 0;

  Roomba.flush();

  rmb_cmd(RoombaCMDs::RMB_WAKE);
  rmb_cmd(RoombaCMDs::RMB_START);
  Roomba.write(142);
  delay(50);
  Roomba.write(PacketID);
  delay(50);

  if (Roomba.available() > 0)
  {
    rdebugA("Es gibt etwas zu lesen! %i\n", Roomba.available());
    if (Roomba.available() == 1)
    {
      high = Roomba.read();
      rdebugA("Roomba.read: %i\n", high);
      result = high;
      return true;
    }
    else if (Roomba.available() == 2)
    {
      high = Roomba.read();
      low = Roomba.read();
      rdebugA("Roomba.read: %i (%i, %i)\n", (high * 256 + low), high, low);
      rdebugA("Roomba.read: %i, %i\n", lowByte(high * 256 + low), highByte(high * 256 + low));
      rdebugA("Roomba.read: %i\n", (signed int)(low + (high << 8)));
      rdebugA("Roomba.read: %i\n", (unsigned int)(low + (high << 8)));
      result = (high * 256 + low);
      return true;
    }
  }
  else
  {
    rdebugA("Es gibt nichts zu lesen! %s\n", "");
    return false;
  }
}

void postWSActions()
{
  // Blink LED
  digitalWrite(PinWiFiLED, HIGH);
  previousMillis = millis();
  postWSActIsActive = true;

  snprintf(buff, sizeof(buff), "%s %s %s ", server.client().remoteIP().toString().c_str(), (server.method() == HTTP_GET ? "GET" : "POST"), server.uri().c_str());

  // Log Access to telnet
  rdebugA("%s\n", buff);

  // Log Access to Logfile
  logWrite(buff);
}

bool isRoombaCleaning()
{
  /*
  getRoombaSensorPackets();
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
  }*/
}

void loadDefaults()
{
  String TmpStr = "";

  // Config NOT from EEPROM
  configIsDefault = true;

  // Valid-Falg to verify config
  cfg.configisvalid = 1;

  // Note
  TmpStr = "";
  TmpStr.toCharArray(cfg.note, 30);

  // HTTP Auth Settings
  TmpStr = "api";
  TmpStr.toCharArray(cfg.api_username, 30);
  TmpStr.toCharArray(cfg.api_password, 30);
  TmpStr = "admin";
  TmpStr.toCharArray(cfg.admin_username, 30);
  TmpStr.toCharArray(cfg.admin_password, 30);

  // Telnet
  cfg.telnet = 0;

  // Wifi
  TmpStr = "";
  TmpStr.toCharArray(cfg.wifi_ssid, 30);
  TmpStr.toCharArray(cfg.wifi_psk, 30);

  // Hostname
  TmpStr = WiFi.hostname().c_str();
  TmpStr.toCharArray(cfg.hostname, 30);
}

void loadConfig()
{
  Serial.println("loadConfig!");
  EEPROM.begin(512);
  EEPROM.get(cfgStart, cfg);
  EEPROM.end();

  if (cfg.configisvalid != 1)
  {
    loadDefaults();
  }
  else
  {
    configIsDefault = false; // Config from EEPROM
  }
}

long dBm2Quality(long dBm)
{
  if (dBm <= -100)
    return 0;
  else if (dBm >= -50)
    return 100;
  else
    return 2 * (dBm + 100);
}

void updateSensors(byte packet)
{
  // Roomba.print(142);
  // Roomba.print(packet);
  // delay(100); // wait for sensors
  // char i = 0;
  //
  // while(Roomba.available()) {
  //   int c = Roomba.read();
  //   if( c==-1 ) {
  //      rdebugA("Sensor Errror! %i\n", c);
  //   }
  //   sensorbytes[i++] = c;
  // }
  //
  // rdebugA("VOLTAGE: %s\n", VOLTAGE);
  // rdebugA("VOLTAGE: %i\n", VOLTAGE);
  // rdebugA("VOLTAGE: %c\n", VOLTAGE);
  //
  // if(i < 6) {     // Size of smallest sensor packet is 6
  //   rdebugA("Sensor Errrrrror! %i\n", i);
  // }
}

void HTMLHeader(char *section, int refresh, char *url)
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
  html += "<li><a href='/reboot'>Reboot</a></li>\n";
  html += "</ul>\n";
  html += "<div id='main'>";
}

void HTMLFooter()
{
  html += "</div>";
  html += "<div id='footer'>&copy; 2018 Fabian Otto - Firmware v";
  html += firmware;
  html += " Compiled: ";
  html += compile_date;
  html += "</div>\n";
  html += "</body>\n";
  html += "</html>\n";
}

void handleRoot()
{
  postWSActions();

  HTMLHeader("Main");

  html += "<table>\n";

  char uptime[20];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;
  int days = hr / 24;
  snprintf(uptime, 20, " %02d:%02d:%02d:%02d", days, hr % 24, min % 60, sec % 60);

  html += "<tr>\n<td>Uptime</td>\n<td>";
  html += uptime;
  html += "</td>\n</tr>\n";

  html += "<tr>\n<td>Current Time</td>\n<td>";
  html += timeClient.getFormattedTime();
  html += "</td>\n</tr>\n";

  html += "<tr>\n<td>Firmware</td>\n<td>v";
  html += firmware;
  html += "</td>\n</tr>\n";

  html += "<tr>\n<td>Compiled</td>\n<td>";
  html += compile_date;
  html += "</td>\n</tr>\n";

  html += "<tr>\n<td>Cleaning State</td>\n<td>";
  html += (isRoombaCleaning() ? "ON" : "OFF");
  html += "</td>\n</tr>\n";

  sec = (millis() - lastClean) / 1000;
  if (sec < 0 || lastClean == 0)
  {
    snprintf(uptime, 20, "---");
  }
  else
  {
    min = sec / 60;
    hr = min / 60;
    days = hr / 24;
    snprintf(uptime, 20, " %02d:%02d:%02d:%02d", days, hr % 24, min % 60, sec % 60);
  }

  html += "<tr>\n<td>Last clean</td>\n<td>";
  html += uptime;
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
  html += dBm2Quality(WiFi.RSSI());
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
  postWSActions();
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
  postWSActions();
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

        // RF Note
        if (server.argName(i) == "note")
        {
          value.toCharArray(cfg.note, 30);

          // HTTP Auth Useraccess Username
        }
        else if (server.argName(i) == "api_username")
        {
          value.toCharArray(cfg.api_username, 30);

          // HTTP Auth Useraccess Password
        }
        else if (server.argName(i) == "api_password")
        {
          value.toCharArray(cfg.api_password, 30);

          // HTTP Auth Adminaccess Username
        }
        else if (server.argName(i) == "admin_username")
        {
          value.toCharArray(cfg.admin_username, 30);

          // HTTP Auth Adminaccess Password
        }
        else if (server.argName(i) == "admin_password")
        {
          value.toCharArray(cfg.admin_password, 30);

          // WiFi SSID
        }
        else if (server.argName(i) == "ssid")
        {
          value.toCharArray(cfg.wifi_ssid, 30);

          // WiFi PSK
        }
        else if (server.argName(i) == "psk")
        {
          value.toCharArray(cfg.wifi_psk, 30);

          // Hostname
        }
        else if (server.argName(i) == "hostname")
        {
          value.toCharArray(cfg.hostname, 30);

          // Telnet
        }
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

      html += "<tr>\n<td>\nAPI Username:</td>\n";
      html += "<td><input name='api_username' type='text' maxlength='30' autocapitalize='none' value='";
      html += cfg.api_username;
      html += "'></td>\n</tr>\n";

      html += "<tr>\n<td>\nAPI Password:</td>\n";
      html += "<td><input name='api_password' type='password' maxlength='30' value='";
      html += cfg.api_password;
      html += "'></td>\n</tr>\n";

      html += "<tr>\n<td>\nAdminaccess Username:</td>\n";
      html += "<td><input name='admin_username' type='text' maxlength='30' autocapitalize='none' value='";
      html += cfg.admin_username;
      html += "'></td>\n</tr>\n";

      html += "<tr>\n<td>\nAdminaccess Password:</td>\n";
      html += "<td><input name='admin_password' type='password' maxlength='30' value='";
      html += cfg.admin_password;
      html += "'></td>\n</tr>\n";

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
  postWSActions();
  int charge = 0;
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
            rmb_cmd(RoombaCMDs::RMB_WAKE);
          }
          else if (server.arg(i) == "Start")
          {
            rmb_cmd(RoombaCMDs::RMB_START);
          }
          else if (server.arg(i) == "Stop")
          {
            rmb_cmd(RoombaCMDs::RMB_STOP);
          }
          else if (server.arg(i) == "Clean")
          {
            rmb_cmd(RoombaCMDs::RMB_CLEAN);
          }
          else if (server.arg(i) == "Max")
          {
            rmb_cmd(RoombaCMDs::RMB_MAX);
          }
          else if (server.arg(i) == "Spot")
          {
            rmb_cmd(RoombaCMDs::RMB_SPOT);
          }
          else if (server.arg(i) == "Dock")
          {
            rmb_cmd(RoombaCMDs::RMB_DOCK);
          }
          else if (server.arg(i) == "Power")
          {
            rmb_cmd(RoombaCMDs::RMB_POWER);
          }
        }
      }
    }

    HTMLHeader("Switch Socket");
    html += "<form method='POST' action='/actions'>";
    html += "<input type='submit' name='action' value='Wake'>";
    html += "<input type='submit' name='action' value='Start'>";
    html += "<input type='submit' name='action' value='Stop'>";
    html += "<input type='submit' name='action' value='Clean'>";
    html += "<input type='submit' name='action' value='Max'>";
    html += "<input type='submit' name='action' value='Spot'>";
    html += "<input type='submit' name='action' value='Dock'>";
    html += "<input type='submit' name='action' value='Power'>";
    html += "</form>";

    HTMLFooter();
    server.send(200, "text/html", html);
  }
}

void handleStatus()
{
  postWSActions();

  if (!server.authenticate(cfg.admin_username, cfg.admin_password))
  {
    return server.requestAuthentication();
  }
  else
  {

    HTMLHeader("Status");
    html += "<form method='POST' action='/status'><br />";
    html += "<input type='text' name='packetid' value=''>";
    html += "<input type='submit' name='single' value='SingleValue'>";
    html += "<input type='submit' name='packet' value='Packet'>";

    if (server.method() == HTTP_POST)
    {
      html += "<br /><br />Result: ";
      for (uint8_t i = 0; i < server.args(); i++)
      {
        if (server.argName(i) == "packet")
        {
          if (getRoombaSensorPackets())
          {
            snprintf(buff, sizeof(buff), "<br />CHARGE_STATE: %i<br />", CHARGE_STATE);
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
            html += "No Data 1";
          }
        }
        else if (server.argName(i) == "packetid")
        {
          String packetID;
          packetID = server.arg(i);
          packetID.trim();
          if (packetID != "")
          {
            rdebugA("PackedID: %s (%i)\n", packetID.c_str(), packetID.toInt());
            int result;
            if (getRoombaSensorPacket(char(packetID.toInt()), result))
            {
              html += result;
            }
            else
            {
              html += "No Data";
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
  postWSActions();
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
        html += dBm2Quality(WiFi.RSSI(i));
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
  postWSActions();
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
  postWSActions();
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
    html += String("<td>") + firmware + String("</td>\n");
    html += "</tr>\n";
    html += "<tr>\n";
    html += "<td>Compiled</td>\n";
    html += String("<td>") + compile_date + String("</td>\n");
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
  postWSActions();
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

void handleAPI(APICMDs cmd)
{
  postWSActions();
  String out;
  // HTTP Auth
  if (!server.authenticate(cfg.api_username, cfg.api_password))
  {
    return server.requestAuthentication();
  }
  else
  {
    switch (cmd)
    {
    case APICMDs::API_ON:
      rmb_cmd(RoombaCMDs::RMB_CLEAN);
      out = "1";
      logWrite("API: RMB_CLEAN");
      break;
    case APICMDs::API_OFF:
      rmb_cmd(RoombaCMDs::RMB_POWER);
      logWrite("API: RMB_POWER");
      out = "0";
      break;
    case APICMDs::API_STATUS:
      if (isRoombaCleaning())
      {
        out = "1";
      }
      else
      {
        out = "0";
      }
      logWrite("API: APICMDs::API_STATUS");
      break;
    case APICMDs::API_DOCK:
      rmb_cmd(RoombaCMDs::RMB_DOCK);
      logWrite("API: RMB_DOCK");
      out = "1";
      break;
    default:
      break;
    }
    server.send(200, "text/plain", out);
  }
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

void setup(void)
{

  // Setting the I/O pin modes
  pinMode(PinPushBtn, INPUT);
  pinMode(PinWiFiLED, OUTPUT);
  pinMode(SERIAL_RX, INPUT);
  pinMode(SERIAL_TX, OUTPUT);
  pinMode(BRC_PIN, INPUT); // High-impedence on the BRC_PIN

  // Status LED on
  digitalWrite(PinWiFiLED, HIGH);

  Serial.begin(115200);
  Roomba.begin(115200);

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
    Serial.println("configIsDefault: AP Mode");
    WiFi.softAP("Roomba", "");
    digitalWrite(PinWiFiLED, HIGH);
  }
  else
  {
    // Connecting to a WiFi network
    Serial.println("configIsNotDefault: Connecting to WiFi");
    WiFi.mode(WIFI_STA);
    if (strcmp(cfg.hostname, "") != 0)
    {
      WiFi.hostname(cfg.hostname);
    }
    WiFi.begin(cfg.wifi_ssid, cfg.wifi_psk);

    while (WiFi.status() != WL_CONNECTED)
    {
      delay(250);
      Serial.print(".");

      // Blink WiFi LED
      if (wifiledState == HIGH)
      {
        wifiledState = LOW;
      }
      else
      {
        wifiledState = HIGH;
      }
      digitalWrite(PinWiFiLED, wifiledState);

      // handleButton
      handleButton();
    }

    Serial.println();
    Serial.print("WiFi connected with ip ");
    Serial.println(WiFi.localIP());
    digitalWrite(PinWiFiLED, LOW);

    WiFi.printDiag(Serial);
  }

  // Telnet debug
  if (cfg.telnet)
  {
    Debug.begin(WiFi.hostname());   // Initiaze the telnet server
    Debug.setSerialEnabled(true);   // All messages too send to serial too, and can be see in serial monitor
    Debug.setResetCmdEnabled(true); // Enable the reset command
  }

  // NTPClient
  timeClient.begin();
  rdebugVln("NTP Client started!");

  // Arduino OTA Update
  httpUpdater.setup(&server, "/dofwupdate", cfg.admin_username, cfg.admin_password);

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);

  server.on("/api/on", []() {
    handleAPI(APICMDs::API_ON);
  });
  server.on("/api/off", []() {
    handleAPI(APICMDs::API_OFF);
  });
  server.on("/api/status", []() {
    handleAPI(APICMDs::API_STATUS);
  });
  server.on("/api/dock", []() {
    handleAPI(APICMDs::API_DOCK_STATUS);
  });
  server.on("/settings", []() {
    handleSettings();
  });
  server.on("/reboot", []() {
    handleReboot();
  });
  server.on("/actions", []() {
    handleActions();
  });
  server.on("/status", []() {
    handleStatus();
  });
  server.on("/fwupdate", []() {
    handleFWUpdate();
  });
  server.on("/wifiscan", []() {
    handleWiFiScan();
  });
  server.on("/logs", []() {
    handleLogs();
  });

  server.begin();
  logWrite("HTTP server started");

  // Update NTPClient for the first time
  timeClient.update();

  // finish setup
  logWrite("Setup function done");
}

void loop(void)
{

  // NTPClient Update
  timeClient.update();

  unsigned long currentMillis = millis();

  //postWSActions
  if (postWSActIsActive)
  {
    if (currentMillis - previousMillis >= wifiledoffinterval)
    {
      digitalWrite(PinWiFiLED, LOW);
      postWSActIsActive = false;
    }

    // Status blinking
  }
  else
  {
    if (currentMillis - previousMillis >= statusLEDinterval)
    {
      previousMillis = currentMillis;
      if (wifiledState == HIGH)
      {
        wifiledState = LOW; // Note that this switches the LED *off*
        statusLEDinterval = 5000;
      }
      else
      {
        wifiledState = HIGH; // Note that this switches the LED *on*
        statusLEDinterval = 100;
      }
      digitalWrite(PinWiFiLED, wifiledState);

      // fast flashing if no wifi connection
      if (WiFi.status() != WL_CONNECTED)
      {
        statusLEDinterval = 100;
      }
    }
  }

  /*
    /

    // Status blinking
    if (currentMillis - previousMillisStatusBlinking >= statusLEDinterval) {
    previousMillisStatusBlinking = currentMillis;

    rdebugA("blink %i %s", wifiledState, "\n");

    if (wifiledState == LOW) { // is on then
      rdebugA("off %s", "\n");
      statusLEDinterval = 500;
      digitalWrite(PinWiFiLED, LOW);
      wifiledState = HIGH;
    } else {
      rdebugA("on %s", "\n");
      statusLEDinterval = defaultStatusLEDinterval;
      digitalWrite(PinWiFiLED, LOW);
      wifiledState = LOW;
    }
    }
  */
  // Button
  handleButton();

  // Webserver & Debug
  server.handleClient();

  // Serial to debug
  //  if (Roomba.available() > 0) {
  //      rdebugA("%s", Roomba.read());
  //  }

  // Remote Debug
  if (cfg.telnet)
  {
    Debug.handle();
  }
}