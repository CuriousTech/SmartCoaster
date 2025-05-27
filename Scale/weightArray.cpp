#include "weightArray.h"
#include "Prefs.h"
#include "jsonstring.h"
#include <TimeLib.h>
#include <FS.h>
#include <FFat.h>

extern void WsSend(String s);

const char fileName[] = "/dayTotal";

void WeightArray::init()
{
  FFat.begin(true);
}

void WeightArray::saveData()
{
  if(year() < 2025) // time not set
    return;

  File file;
  String sName = fileName;
  sName += year();
  sName += ".bin";

  static bool bLoaded = false; // save will be called when time changes
  if(bLoaded == false)  // load if not done yet
  {
    bLoaded = true;
    file = FFat.open(sName, "r");
  
    if(!file)
      return;

    file.read((byte*)dayTotal, sizeof(dayTotal));
    file.close();
    flOzAccum = dayTotal[0]; // use day 0 for the current day accumulation
    return;
  }

  file = FFat.open(sName, "w");

  if(!file)
    return;

  dayTotal[0] = flOzAccum;
  file.write((byte*)dayTotal, sizeof(dayTotal));
  file.close();
}

void WeightArray::newDay(int8_t day)
{
  if(day == 0) return;
  dayTotal[day] = flOzAccum;
  flOzAccum = 0;
}

void WeightArray::add(uint32_t date, int32_t value, AsyncWebSocket &ws, int WsClientID)
{
  weightArr *p = &m_log[m_idx];

  if(m_lastDate == 0)
    m_lastDate = date;
  p->m.tmdiff = constrain(date - m_lastDate, 0, 0xFFF);
  m_lastDate = date;

  p->m.weight = value;

  sendNew(date, value, ws, WsClientID);
  m_lastVal = value;

  if(++m_idx >= LOG_CNT)
    m_idx = 0;
  m_log[m_idx].m.u = 0; // mark as invalid data/end
}

bool WeightArray::get(int &pidx, int n)
{
  if(n < 0 || n > LOG_CNT-1) // convert 0-(LOG_CNT-1) to reverse index circular buffer
    return false;
  int idx = m_idx - 1 - n; // 0 = last entry
  if(idx < 0) idx += LOG_CNT;
  if(m_log[idx].m.u == 0) // invalid data/end
    return false;
  pidx = idx;
  return true;
}

#define CHUNK_SIZE 800

// send the log in chucks of CHUNK_SIZE
void WeightArray::historyDump(bool bStart, AsyncWebSocket &ws, int WsClientID)
{
  static bool bSending;
  static int entryIdx;

  if(ws.availableForWrite(WsClientID) == false)
    return;

  int aidx;
  if(bStart)
  {
    m_nSending = 1;
    entryIdx = 0;

    jsonString js("ref");
    js.Var("tb"  , m_lastDate); // date of first entry
    ws.text(WsClientID, js.Close());

    if( get(aidx, 0) == false)
    {
      bSending = false;
      return;
    }
  }

  if(m_nSending == 0)
    return;

  // minute
  String out;
  out.reserve(CHUNK_SIZE + 100);

  out = "{\"cmd\":\"data\",\"d\":[";

  bool bC = false;

  for(; entryIdx < LOG_CNT - 1 && out.length() < CHUNK_SIZE && get(aidx, entryIdx); entryIdx++)
  {
    int len = out.length();
    if(bC) out += ",";
    bC = true;
    out += "[";         // [seconds, temp, rh],
    out += m_log[aidx].m.tmdiff; // seconds differential from next entry
    out += ",";
    out += m_log[aidx].m.weight;
    out += "]";
    if( out.length() == len) // memory full
      break;
  }
  out += "]}";
  if(bC == false) // done
    m_nSending = 0;
  else
    ws.text(WsClientID, out);
}

void WeightArray::sendNew(uint32_t date, int32_t Value, AsyncWebSocket &ws, int WsClientID)
{
  // Update lower bars
  dayTotal[0] = flOzAccum;
  jsonString js2("days");
  js2.Array("days", dayTotal, 32);
  ws.text(WsClientID, js2.Close());

  // Add to chart
  String out = "{\"cmd\":\"data2\",\"d\":[[";

  out += m_lastDate;
  out += ",";
  out += Value;
  out += "]]}";
  ws.text(WsClientID, out);
}
