#ifndef PREFS_H
#define PREFS_H

#include <Arduino.h>
#include <Preferences.h>

#define EESIZE (offsetof(Prefs, end) - offsetof(Prefs, size) )

class Prefs
{
public:
  Prefs(){};
  void init(void);
  void update(void);

  uint16_t  size = EESIZE;          // if size changes, use defauls
  uint16_t  sum = 0xAAAA;           // if sum is diiferent from memory struct, write
  char      szSSID[32] = ""; // SSID of router (blank for EspTouch)
  char      szSSIDPassword[64] = "password"; // password for router
  char      szName[32] = "Scale1"; // mDNS and OTA name
  uint8_t   hostIP[4] = {192,168,31,100}; // Use to send JSON updates to a host
  uint16_t  hostPort = 80;
  uint16_t  sendRate = 15; // rate sent to host in seconds
  uint16_t  logRate = 10; // chart logging rate in seconds
  uint16_t  displayRate = 500; // web page update in ms
  float     fCal = 0.477f;
  int16_t   calWt = 50; // default 50g calibration weight
  int16_t   emptyWt = 200; // grams
  uint8_t   res[30];
  uint8_t   end;
private:
  uint16_t Fletcher16( uint8_t* data, int count);
  Preferences pfs;
};

extern Prefs prefs;

#endif // PREFS_H
