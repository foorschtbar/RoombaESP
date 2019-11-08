#ifndef myTypes_h
#define myTypes_h

#include <WString.h>


// 'byte' und 'word' doesn't work!
typedef struct {
  int configisvalid;
  char api_username[30];
  char api_password[30];
  char admin_username[30];
  char admin_password[30];
  char note[30];
  int telnet;
  char wifi_ssid[30];
  char wifi_psk[30];
  char hostname[30];
} configData_t;

#endif
