#include "sat_payload.h"

bool get_binary_payload(Ecoset dataset,char* sat_payload){
  DataPacket packet;
  //1. Initialization (very important for padding)
  memset(packet.raw, 0, 16);
  //set default values out of range
  packet.fields.temp       = 0;     //nb: offset 2000
  packet.fields.humidity   = 1023;
  packet.fields.patm       = 2047;
  packet.fields.rain       = 511;
  packet.fields.wind_dir   = 511;
  packet.fields.wind_speed = 2047;
  packet.fields.wind_max   = 2047;
  packet.fields.ray        = 32767;

  //2. compute datetime to epoch offset format
  tmElements_t tm;
  tm.Year = CalendarYrToTm(dataset.year); // Convert 2026 in years since 1970
  tm.Month = dataset.month;
  tm.Day = dataset.day;
  tm.Hour = dataset.hour;
  tm.Minute = dataset.minute;
  tm.Second = dataset.second;
  // Compute Epoch time
  time_t epoch = makeTime(tm);

  uint32_t currentEpoch = (unsigned long)epoch; // actual datetime (2026-04-21)
  uint32_t epochOffset = 1767225600; // 2026-01-01
  uint32_t dateValue = currentEpoch - epochOffset; // Compute to 29 bits 
  //Serial.print("epochstored : ");Serial.println(dateValue);

  //3. Chargement du DataPacket
  packet.fields.marker = 0b001; // The value "1" on 3 bits
  packet.fields.date = dateValue;
  #if THP_CAPTEUR_COUNT > 0
    packet.fields.temp = (int32_t)round(dataset.MHTemp*10) + 2000;  //1/10 °C precision offset 200
    packet.fields.humidity = (uint32_t)round(dataset.MHHum*10);     //1/10 % precision
  #endif
  #if MOD_BME280
    packet.fields.patm = (uint32_t)round((dataset.MHPatm-900)*10);  //1/10 mBar precision
  #endif
  #if MOD_PLUIE
    packet.fields.rain = (uint32_t)round(dataset.MHPluie*10);       //1/10 mm precision
  #endif
  #if MOD_VENT
    packet.fields.wind_dir = (uint32_t)round(dataset.MHDir);        //1 ° precision
    packet.fields.wind_speed = (uint32_t)round(dataset.MHVit*10);   //1/10 km/h precision
    packet.fields.wind_max = (uint32_t)round(dataset.MHVmx*10);     //1/10 km/h precision
  #endif
  #if MOD_PYRANO
    packet.fields.ray = (uint32_t)round(dataset.MHPDavis*10);       //1/10 W/M² precision
  #endif
  //Serial.print("Taille totale : ");
  //Serial.println(sizeof(DataPacket)); // Doit afficher 16

  // We iterate through the raw array to send the hexadecimal representation
  for(int i = 0; i < 16; i++) {
    // sprintf writes directly to the correct part of the array
    // i * 2 allows shifting by 2 characters at each iteration
    sprintf(&sat_payload[i * 2], "%02X", packet.raw[i]);
  }
  sat_payload[32] = '\0'; // End char array with \0

  return true;
}