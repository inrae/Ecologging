/*
functions to use KIM2 module for satelite transmission
Created by Philippe CHAUMEIL, June 2026
Assisted by Gemini AI for code generation and refactoring.
Main goal is to compress measures datas in a compact binary format
Try to aggregate datas in 32 bit blocks
*/
#ifndef SATELLITE_H
#define SATELLITE_H

#include <TimeLib.h>
#include "config.h"           //configuration file
#include "buffer_pile.h"

union DataPacket {
  struct __attribute__((packed)) {
    uint32_t marker : 3; 
    uint32_t date : 29;
    //32 bits
    uint32_t patm : 11;
    uint32_t wind_speed : 11;
    uint32_t humidity : 10;
    //32 bits
    uint32_t temp : 12;
    uint32_t wind_dir : 9;
    uint32_t wind_max : 11;
    //32 bits
    uint32_t rain : 9;
    uint32_t ray : 15;
    //24 bits
    //uint32_t reserved : 8;
    uint8_t  padding[1];
  } fields;
  uint8_t raw[16];           // same data but as char array
};


//FORMAT encoding #1 0b001 => wheather datas
bool get_binary_payload(Ecoset dataset,char* sat_payload);

#endif