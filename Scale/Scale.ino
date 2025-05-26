/**The MIT License (MIT)

Copyright (c) 2025 by Greg Cunningham, CuriousTech

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// Scale with HX711 and ESP32-C3-super mini

// Build with Arduino IDE 1.8.19, ESP32 2.0.14
// Partition: Default 4MB with ffat (use USB first, OTA doesn't create ffat partition)

#include <ESPAsyncWebServer.h> // https://github.com/ESP32Async/ESPAsyncWebServer (3.7.2)
#include <TimeLib.h> // https://github.com/PaulStoffregen/Time
#include <UdpTime.h> // https://github.com/CuriousTech/ESP07_WiFiGarageDoor/tree/master/libraries/UdpTime
#include <JsonParse.h> //https://github.com/CuriousTech/ESP-HVAC/tree/master/Libraries/JsonParse
#include <JsonClient.h> // https://github.com/CuriousTech/ESP-HVAC/tree/master/Libraries/JsonClient
#include <HX711.h>  // https://github.com/bogde/HX711
#include <Wire.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include "Prefs.h"
#include <FS.h>
#include "pages.h"
#include "jsonstring.h"
#include "uriString.h"
#include "WeightArray.h"

const int LOADCELL_DOUT_PIN = 8;
const int LOADCELL_SCK_PIN = 9;

int serverPort = 80;

enum reportReason
{
  Reason_Setup,
  Reason_Status,
  Reason_Alert,
};

IPAddress lastIP;

uint32_t sleepTimer = 60; // seconds delay after startup to enter sleep (Note: even if no AP found)
int8_t nWsConnected;

AsyncWebServer server( serverPort );
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws
int WsClientID;

void jsonCallback(int16_t iName, int iValue, char *psValue);
JsonParse jsonParse(jsonCallback);
void jsonPushCallback(int16_t iName, int iValue, char *psValue);
JsonClient jsonPush(jsonPushCallback);

Prefs prefs;
UdpTime udpTime;
HX711 loadcell;

bool bConfigDone = false;
bool bStarted = false;
uint32_t connectTimer;

WeightArray wa;
int32_t weight;
int32_t flOzDiff;

String settingsJson()
{
  jsonString js("settings");

  js.Var("srate", prefs.sendRate);
  js.Var("lrate", prefs.logRate);
  js.Var("drate", prefs.displayRate);
  js.Var("cal", String(prefs.fCal, 4));
  js.Var("wt", prefs.calWt);
  js.Var("empty", prefs.emptyWt);
  return js.Close();
}

String dataJson()
{
  jsonString js("state");

  js.Var("t", (uint32_t)now());
  js.Var("weight", weight );
  js.Var("wd", -flOzDiff);
  js.Var("fla", wa.flOzAccum);
  int sig = WiFi.RSSI();
  js.Var("rssi", sig);
  return js.Close();
}

const char *jsonList1[] = {
  "srate",
  "lrate",
  "drate",
  "hostip",
  "hist",
  "tare",
  "calibrate",
  "calval",
  "emptywt",
  "clear",
  "floz", // 10
  NULL
};

void parseParams(AsyncWebServerRequest *request)
{
  lastIP = request->client()->remoteIP();

  for ( uint8_t i = 0; i < request->params(); i++ )
  {
    const AsyncWebParameter* p = request->getParam(i);
    String s = request->urlDecode(p->value());

    uint8_t idx;
    for(idx = 0; jsonList1[idx]; idx++)
      if( p->name().equals(jsonList1[idx]) )
        break;
    if(jsonList1[idx])
    {
      int iValue = s.toInt();
      if(s == "true") iValue = 1;
      jsonCallback(idx, iValue, (char *)s.c_str());
    }
  }
}

bool bDataMode;

void jsonCallback(int16_t iName, int iValue, char *psValue)
{
  switch(iName)
  {
    case 0: // srate
      prefs.sendRate = iValue;
      break;
    case 1: // lrate
      prefs.logRate = iValue;
      break;
    case 2: // drate
      prefs.displayRate = constrain(iValue, 100, 10000);
      break;
    case 3: // hostip
      prefs.hostPort = 80;
      prefs.hostIP[0] = lastIP[0];
      prefs.hostIP[1] = lastIP[1];
      prefs.hostIP[2] = lastIP[2];
      prefs.hostIP[3] = lastIP[3];
      CallHost(Reason_Setup, ""); // test
      break;
    case 4: // hist
      wa.historyDump(true, ws, WsClientID);
      break;
    case 5: // tare
      loadcell.tare();
      break;
    case 6: // calibrate
      prefs.calWt = iValue;
      calibrate();
      break;
    case 7: // calval
      prefs.fCal = atof(psValue);
      loadcell.set_scale(prefs.fCal);
      break;
    case 8: // empty weight
      prefs.emptyWt = iValue;
      break;
    case 9: // clear
      wa.flOzAccum = 0;
      break;
    case 10: // manual adjust
      wa.flOzAccum = iValue * 10;
      break;
  }
}

void calibrate()
{
  loadcell.set_scale();
  loadcell.tare();
  long raw = loadcell.get_units(5);

  prefs.fCal = (float)raw / (float)prefs.calWt;
  loadcell.set_scale(prefs.fCal);
  ws.textAll( settingsJson() );
}

const char *jsonListPush[] = {
  "xxx", // 0
  NULL
};

void jsonPushCallback(int16_t iName, int iValue, char *psValue)
{
}

struct cQ
{
  IPAddress ip;
  String sUri;
  uint16_t port;
};
#define CQ_CNT 8
cQ queue[CQ_CNT];
uint8_t qI;

void checkQueue()
{
  if(WiFi.status() != WL_CONNECTED)
    return;

  int idx;
  for(idx = 0; idx < CQ_CNT; idx++)
  {
    if(queue[idx].port)
      break;
  }
  if(idx == CQ_CNT || queue[idx].port == 0) // nothing to do
    return;

  if( jsonPush.begin(queue[idx].ip, queue[idx].sUri.c_str(), queue[idx].port, false, false, NULL, NULL, 1) )
  {
    jsonPush.setList(jsonListPush);
    queue[idx].port = 0;
  }
}

bool callQueue(IPAddress ip, String sUri, uint16_t port)
{
  int idx;
  for(idx = 0; idx < CQ_CNT; idx++)
  {
    if(queue[idx].port == 0)
      break;
  }
  if(idx == CQ_CNT) // nothing to do
  {
    jsonString js("print");
    js.Var("text", "Q full");
    WsSend(js.Close());
    return false;
  }

  queue[idx].ip = ip;
  queue[idx].sUri = sUri;
  queue[idx].port = port;

  return true;
}

void CallHost(reportReason r, String sStr)
{
  if(WiFi.status() != WL_CONNECTED || prefs.hostIP[0] == 0)
    return;

  uriString uri("/wifi");
  uri.Param("name", prefs.szName);

  switch(r)
  {
    case Reason_Setup:
      uri.Param("reason", "setup");
      uri.Param("port", serverPort);
      break;
    case Reason_Status:
      uri.Param("reason", "status");
      uri.Param("weight", weight);
      break;
    case Reason_Alert:
      uri.Param("reason", "alert");
      uri.Param("value", sStr);
      break;
  }

  IPAddress ip(prefs.hostIP);
  callQueue(ip, uri.string().c_str(), prefs.hostPort);
}

void sendState()
{
  if(nWsConnected)
    ws.textAll(dataJson());
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{  //Handle WebSocket event

  switch(type)
  {
    case WS_EVT_CONNECT:      //client connected
      client->keepAlivePeriod(50);
      client->text( settingsJson() );
      client->text( dataJson() );
      client->ping();
      nWsConnected++;
      break;
    case WS_EVT_DISCONNECT:    //client disconnected
      bDataMode = false; // turn off numeric display and frequent updates
      if(nWsConnected)
        nWsConnected--;
      break;
    case WS_EVT_ERROR:    //error was received from the other end
      break;
    case WS_EVT_PONG:    //pong message was received (in response to a ping request maybe)
      break;
    case WS_EVT_DATA:  //data packet
      AwsFrameInfo * info = (AwsFrameInfo*)arg;
      if(info->final && info->index == 0 && info->len == len){
        //the whole message is in a single frame and we got all of it's data
        if(info->opcode == WS_TEXT){
          data[len] = 0;
          uint32_t ip = client->remoteIP();
          WsClientID = client->id();
          jsonParse.process((char *)data);
        }
      }
      break;
  }
}

void WsSend(String s)
{
  ws.textAll(s);
}

void alert(String txt)
{
  jsonString js("alert");
  js.Var("text", txt);
  ws.textAll(js.Close());
}

void getWeight()
{
  static bool bFirst = true;
  static int32_t warr[4];

  if(bFirst && loadcell.is_ready())
  {
    bFirst = false;
    loadcell.tare();
  }
    
  if (!loadcell.is_ready())
    return;

  static int32_t last;
  int32_t newwt = (loadcell.get_units(7) / 100); // grams * 10

  if(newwt != last) // a little settle check
  {
     last = newwt;
     return;
  }
  weight = newwt;

  int32_t floz = ( weight - prefs.emptyWt*10 ) / 28.35 / 0.958611418535; // grams to fluid oz
  if(floz < 0) floz = 0;

  if(floz == warr[3]) // record only changes
    return;

  bool bFix = false;

  if(floz > warr[3] && warr[3] != 0) // replace when settling
    bFix = true;
  else
    memcpy(warr, warr+1, sizeof(warr) - sizeof(int32_t) );
  warr[3] = floz;

  if(warr[2] == 0 && floz ) // glass sat down
  {
    if(bFix)
      wa.flOzAccum += flOzDiff; // settled more, erase last
    flOzDiff = floz - warr[1];
    if(flOzDiff > 0) // refill/increased
      flOzDiff = 0;
    wa.flOzAccum -= flOzDiff; // difference from last weight
  }
}


void setup()
{
  prefs.init();

  ets_printf("Starting\r\n");

  WiFi.hostname(prefs.szName);
  WiFi.mode(WIFI_STA);

  if ( prefs.szSSID[0] )
  {
    WiFi.begin(prefs.szSSID, prefs.szSSIDPassword);
    WiFi.setHostname(prefs.szName);
    bConfigDone = true;
  }
  else
  {
    ets_printf("No SSID. Waiting for EspTouch\r\n");
    WiFi.beginSmartConfig();
  }
  connectTimer = now();

  // attach AsyncWebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on( "/", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
    parseParams(request);
    bDataMode = true;
    request->send_P(200, "text/html", page_index);
  });
  server.on( "/s", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
    parseParams(request);

    String page = "{\"ip\": \"";
    page += WiFi.localIP().toString();
    page += ":";
    page += serverPort;
    page += "\"}";
    request->send( 200, "text/json", page );
  });
  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse_P(200, "image/x-icon", favicon, sizeof(favicon));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(404);
  });

  server.begin();

  ArduinoOTA.setHostname(prefs.szName);
  ArduinoOTA.begin();
  ArduinoOTA.onStart([]() {
    prefs.update();
    wa.saveData();
    alert("OTA Update Started");
    ws.closeAll();
  });

  jsonParse.setList(jsonList1);
  if(prefs.sendRate == 0) prefs.sendRate = 15;
  if(prefs.displayRate < 500 ) prefs.displayRate = 500;
  wa.init();

  loadcell.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  loadcell.set_scale(prefs.fCal);
}

void loop()
{
  static uint8_t hour_save, sec_save;
  static uint8_t cnt = 0;
  bool bNew;

  ArduinoOTA.handle();

  static int htimer = 10;
  if(--htimer == 0)
  {
    wa.historyDump(false, ws, WsClientID);
    htimer = 30;
  }

  checkQueue();

  static uint32_t lastMS;
  if(millis() - lastMS > prefs.displayRate)
  {
    lastMS = millis(); 
    sendState();
  }

  if(sec_save != second()) // only do stuff once per second (loop is maybe 20-30 Hz)
  {
    sec_save = second();

    getWeight();

    if(!bConfigDone)
    {
      if( WiFi.smartConfigDone())
      {
        ets_printf("SmartConfig set\r\n");
        bConfigDone = true;
        connectTimer = now();
        WiFi.SSID().toCharArray(prefs.szSSID, sizeof(prefs.szSSID)); // Get the SSID from SmartConfig or last used
        WiFi.psk().toCharArray(prefs.szSSIDPassword, sizeof(prefs.szSSIDPassword) );
        prefs.update();
      }
    }
    if(bConfigDone)
    {
      if(WiFi.status() == WL_CONNECTED)
      {
        if(!bStarted)
        {
          ets_printf("WiFi Connected\r\n");
          WiFi.mode(WIFI_STA);
          MDNS.begin( prefs.szName );
          MDNS.addService("sensors", "tcp", serverPort);
          CallHost(Reason_Setup, "");
          bStarted = true;
          udpTime.start();
        }
        if(WiFi.status() == WL_CONNECTED)
          if(udpTime.check(0)) // get GMT time, no TZ
          {
          }
      }
      else if(now() - connectTimer > 10) // failed to connect for some reason
      {
        ets_printf("Connect failed. Starting SmartConfig\r\n");
        connectTimer = now();
        WiFi.mode(WIFI_AP_STA);
        WiFi.beginSmartConfig();
        bConfigDone = false;
        bStarted = false;
      }
    }

    if(hour_save != hour())
    {
      hour_save = hour();
      if((hour_save&1) == 0)
        CallHost(Reason_Setup, "");

      if(hour_save == 0)
        wa.newDay(day() );

      wa.saveData();
      prefs.update(); // update EEPROM if needed while we're at it (give user time to make many adjustments)
      if(hour_save == 2 && WiFi.status() == WL_CONNECTED)
        udpTime.start(); // update time daily at DST change
    }

    static uint8_t timer = 5;
    if(--timer == 0)
    {
      timer = 30;
      CallHost(Reason_Status, "");
    }

    static uint8_t addTimer = 10;
    if(--addTimer == 0)
    {
      addTimer = prefs.logRate;
      wa.add( (uint32_t)now(), weight, ws, WsClientID);
    }
  }
}
