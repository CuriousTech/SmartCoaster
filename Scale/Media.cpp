#include "Media.h"

extern void WsSend(String s);
extern void alert(String txt);

Media::Media()
{ 
}

char* Media::currFs()
{
#ifdef _FFAT_H_
  return "FFat";
#elif defined(_SPIFFS_H_)
  return "SPIFFS";
#elif defined(_LITTLEFS_H_)
  return "LittleFS";
#else
  return "Unknown";
#endif
}

void Media::init()
{
  INTERNAL_FS.begin(true); // started elsewhere
}

uint32_t Media::freeSpace()
{
  return INTERNAL_FS.totalBytes() - INTERNAL_FS.usedBytes();
}

void Media::setPath(char *szPath) // SPIFFS has no dirs
{
  File Path = INTERNAL_FS.open(szPath);
  if (!Path)
    return;

  uint16_t fileCount = 0;
  File file = Path.openNextFile();

  String s = "{\"cmd\":\"files\",\"list\":[";

  while (file)
  {
    if(fileCount)
      s += ",";
    s += "[\"";
    s += file.name();
    s += "\",";
    s += file.size();
    s += ",";
    s += file.isDirectory();
    s += ",";
    s += file.getLastWrite();
    s += "]";
    fileCount++;
    file = Path.openNextFile();
  }
  Path.close();
  s += "]}";
  WsSend(s);
}

void Media::deleteFile(char *pszFilename)
{
  INTERNAL_FS.remove(pszFilename);
}

File Media::createFile(String sFilename)
{
  return INTERNAL_FS.open(sFilename, "w");
}

void Media::createDir(char *pszName)
{
  if( !INTERNAL_FS.exists(pszName) )
    INTERNAL_FS.mkdir(pszName);
}

bool Media::loadFile(const char *name, uint8_t *pData, uint32_t size)
{
  File file = INTERNAL_FS.open(name, "r");
  
  if(!file)
  {
    alert("File open failed");
    return false;
  }
  file.read(pData, size);
  file.close();
  return true;
}

bool Media::saveFile(const char *name, uint8_t *pData, uint32_t size)
{
  File file = INTERNAL_FS.open(name, "w");

  if(!file)
  {
    alert("Data save failed");
    return false;
  }

  file.write(pData, size);
  file.close();
  return true;
}
