#ifndef settings_h
#define settings_h

// 'byte' und 'word' doesn't work!
//  int valid;
//  char singleChar;
//  long longVar;
//  float field1;
//  float field2;
//  //String note;          // the length is not defined in this case
//  char charstrg1[30];     // The string could be max. 29 chars. long, the last char is '\0'
typedef struct
{
  uint8_t configisvalid;
  char api_username[30]; // legancy setting. keep only as placehoder
  char api_password[30]; // legancy setting. keep only as placehoder
  char admin_username[30];
  char admin_password[30];
  char note[30];
  uint8_t telnet;
  char wifi_ssid[30];
  char wifi_psk[30];
  char hostname[30];
  char mqtt_server[30];
  uint16_t mqtt_port;
  char mqtt_user[50];
  char mqtt_password[50];
  char mqtt_prefix[50];
  uint16_t mqtt_periodic_update_interval;
  uint8_t fancyled;
} configData_t;

#endif
