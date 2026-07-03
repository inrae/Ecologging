/************************************************
This is a library to add a buffer "pile" of data.
The aim is to allow mqtt interruption for a while
without loss of datas
It is a buffer "first in is first out"
If buffer is full, oldest data is replaced by new one
You must define Ecoset Structure to match datas you want to store and modify read and add function
P.Chaumeil 2024 modified 2026 (Assisted by Gemini AI for code generation and refactoring.)
************************************************/

#ifndef BUFFER_PILE_h
#define BUFFER_PILE_h

#include <Arduino.h>
#include "config.h" 
#include "RTClib.h"                     //bibliotheque RTC pour DS3231

#define NUM_BUFFERED 25                 //number of possible datas stored in buffer for transmission.
#define LIMIT_BUFFERED 6                //warning limit if buffer is too full

//WARNING: the struct members must be sorted in a descending order to minimize memory footprint

#define BUFFER_DEBUG 1

struct Ecoset {
  public:
    //floats first to reduce memory lost
    float MHTemp = -99.0;                   //4 octets
    float MHHum = -1.0;
    float MHPatm = 0.0;
    #if MOD_VEML7700
      float MHRay = -1.0;
    #endif
    #if MOD_PYRANO
      float MHPDavis = -1.0;
    #endif
    #if MOD_DS18B20
      float MHTempWater = -99.0;
    #endif
    #if MOD_VENT
      float MHVit = -1.0;
      float MHDir = -1.0;
      float MHVmx = -1.0;
    #endif
    #if MOD_PLUIE
      float MHPluie = -1.0;
    #endif
    #if MOD_ADS_KIT0139
      float MHWaterHauteur = -1.0;
      //float MHWaterVolt;
      //float MHWaterColonne;
    #endif
    #if NB_SEN0600_PROBE
      // Array to store probes values
      float MHTempSEN0600[NB_SEN0600_PROBE]; 
      float MHHumSEN0600[NB_SEN0600_PROBE];
    #endif

    //ints after floats to reduce memory lost
    int UTCoffset = 0;
    int year = 0;                       //2 octets

    //bytes at last position to reduce memory lost
    byte day = 0;                       //1 octet
    byte month = 0;
    byte hour = 0;
    byte minute = 0;
    byte second = 0;

    Ecoset() {
      #if NB_SEN0600_PROBE
        for(int i=0; i<NB_SEN0600_PROBE; i++) {
          MHTempSEN0600[i] = -999.0;
          MHHumSEN0600[i] = -999.0;
        }
      #endif
    }
};

class BUFFER_PILE {
  public:
    BUFFER_PILE();
    void add_pile(const Ecoset& mydataset);
    bool read_pile(Ecoset& mydataset);
    void next_pile();                   //should be called just BEFORE add_pile() if only 1 last value is desired and not buffer functionality!
    byte available_pile();
    byte alert_full_pile();
    void debug_cpt();
    void get_formatted_datetime_bufferpile(const Ecoset& dataset, char* datetime);
    void get_formatted_UTC_bufferpile(const Ecoset& dataset, char* utc_datetime);

  private:

    byte buf_CR;                        // reading counter index
    byte buf_CI;                        // insertion counter index
    byte full_elements;                 // nb pending elements in buffer
    Ecoset ecobuffer[NUM_BUFFERED];
    void inc_CI();
    void inc_CR();

};
#endif //BUFFER_PILE_H