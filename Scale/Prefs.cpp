#include "Prefs.h"

void Prefs::init()
{
  pfs.begin("scale", false);
  pfs.getBytes("settings", this + offsetof(Prefs, size), EESIZE );
}

void Prefs::update() // write the settings if changed
{
  uint16_t old_sum = sum;
  sum = 0;
  sum = Fletcher16((uint8_t*)this + offsetof(Prefs, size), EESIZE);

  if(old_sum == sum)
    return; // Nothing has changed?

  pfs.putBytes("settings", this + offsetof(Prefs, size), EESIZE );
}

uint16_t Prefs::Fletcher16( uint8_t* data, int count)
{
  uint16_t sum1 = 0;
  uint16_t sum2 = 0;
  
  for( int index = 0; index < count; ++index )
  {
    sum1 = (sum1 + data[index]) % 255;
    sum2 = (sum2 + sum1) % 255;
  }
  
  return (sum2 << 8) | sum1;
}
