/************************************************
This is a library to add a buffer "pile" of data.
The aim is to allow mqtt interruption for a while
without loss of datas
It is a buffer "first in is first out"
If buffer is full, oldest data is replaced by new one
You must define Ecoset Structure to match datas you want to store and modify read and add function
P.Chaumeil 2024
************************************************/

#ifndef BUFFER_PILE_h
#define BUFFER_PILE_h

#define NUM_BUFFERED 25     //number of possible datas stored in buffer for transmission. One item is reserved empty for storing current value.
#define LIMIT_BUFFERED 6    //warning limit if buffer is too full

#include <Arduino.h>
#include "RTClib.h" //bibliotheque RTC pour DS3231

//WARNING: the struct members must be sorted in a descending order to minimize memory footprint

#define BUFFER_DEBUG 1

struct Ecoset
{
  public:
    byte day; //1 octet
    byte month;
    int year; //2 octets
    byte hour;
    byte minute;
    byte second;
    int UTCoffset;
    float MHTemp; //4 octets
    float MHHum;
    float MHPatm;
    float MHRay;
    float MHPDavis;
    float MHTempWater;
    float MHVit;
    float MHDir;
    float MHPluie;
    //float MHWaterVolt;
    //float MHWaterColonne;
    float MHWaterHauteur;

    // Constructeur par défaut qui met tout à zéro
    Ecoset() :
      day(0), month(0), year(0), hour(0), minute(0), second(0), UTCoffset(0),
      MHTemp(0), MHHum(0), MHPatm(0), MHRay(0), MHPDavis(0),
      MHTempWater(0),MHVit(0), MHDir(0), MHPluie(0),
      //MHWaterVolt(0), MHWaterColonne(0),
      MHWaterHauteur(0) {}
};

class BUFFER_PILE {
  public:
    BUFFER_PILE();
    void add_pile(const Ecoset& mydataset);
    bool read_pile(Ecoset& mydataset);
    void next_pile();
    byte available_pile();
    byte alert_full_pile();
    void debug_cpt();
    void get_formatted_datetime_bufferpile(const Ecoset& dataset, char* datetime);
    void get_formatted_UTC_bufferpile(const Ecoset& dataset, char* utc_datetime);

  private:

    byte buf_CR; // reading counter index
    byte buf_CI; // insertion counter index
    Ecoset ecobuffer[NUM_BUFFERED];
    void inc_CI();
    void inc_CR();

};
#endif //BUFFER_PILE_H