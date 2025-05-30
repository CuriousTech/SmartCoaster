#ifndef MEDIA_H
#define MEDIA_H

#include <Arduino.h>
#include <FS.h>
#include <FFat.h>

#define INTERNAL_FS FFat // Make it the same as the partition scheme uploaded (SPIFFS, LittleFS, FFat)

class Media
{
public:
  Media(void);
  void init(void);

  char *currFs(void);
  void deleteFile(char *pszFilename);
  fs::File createFile(String sFilename); //only ESP32 needs this namespace decl
  void createDir(char *pszName);
  void setPath(char *szPath);
  uint32_t freeSpace(void);
  bool loadFile(const char *name, uint8_t *pData, uint32_t size);
  bool saveFile(const char *name, uint8_t *pData, uint32_t size);
};

extern Media media;

#endif // MEDIA_H
