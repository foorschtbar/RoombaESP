#ifndef config_h
#define config_h

#include <WString.h>


// 'byte' und 'word' doesn't work!
typedef struct {
  uint8_t configisvalid;
  char api_username[30];
  char api_password[30];
  char admin_username[30];
  char admin_password[30];
  char note[30];
  uint8_t telnet;
  char wifi_ssid[30];
  char wifi_psk[30];
  char hostname[30];
} configData_t;

#endif
