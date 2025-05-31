#ifndef TEMPARRAY_H
#define TEMPARRAY_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

union mbits
{
  uint32_t u;
  struct
  {
    int32_t tmdiff:12;
    int32_t weight:24;
  };
};

struct weightArr
{
  mbits m;
};

#define LOG_CNT 1440 // 1 day at 1 minute

class WeightArray
{
public:
  WeightArray(){};
  void init(void);
  void saveData(void);
  void newDay(void);
  uint8_t localHour(void);
  void add(uint32_t date, int32_t value, AsyncWebSocket &ws, int WsClientID);
  void historyDump(bool bStart, AsyncWebSocket &ws, int WsClientID);

  int32_t flOzAccum;

protected:
  bool get(int &pidx, int n);
  void sendNew(uint32_t date, int32_t Value, AsyncWebSocket &ws, int WsClientID);
  uint16_t getSum(void);

  weightArr m_log[LOG_CNT];
  int16_t m_idx;
  uint32_t m_lastDate;
  int32_t m_lastVal;
  uint8_t m_nSending;
  int32_t dayTotal[32];
};

#endif
