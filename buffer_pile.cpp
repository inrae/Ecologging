/************************************************
This is a library to add a buffer "pile" of data.
The aim is to allow mqtt interruption for a while
without loss of datas
P.Chaumeil 2024
************************************************/

#include "buffer_pile.h"

/****************
 PUBLIC FUNCTIONS
 ****************/

BUFFER_PILE::BUFFER_PILE() {
  buf_CR = 0;
  buf_CI = 0;
  full_elements = 0;
}

//add a dataset to the buffer pile
void BUFFER_PILE::add_pile(const Ecoset& mydataset) {
  if (full_elements == NUM_BUFFERED) {
    #if BUFFER_DEBUG
      Serial.println(F("#_# BUFFER FULL! Overwriting oldest data."));
    #endif
    inc_CR(); // Offset to keep FIFO
    full_elements--; // virtually free one place
  }
  ecobuffer[buf_CI] = mydataset;
  inc_CI();
  full_elements++;
}

//read a dataset from the buffer pile
bool BUFFER_PILE::read_pile(Ecoset& mydataset) {
  if (full_elements > 0) {
    mydataset = ecobuffer[buf_CR];
    #if BUFFER_DEBUG
      Serial.println(F("#_# read dataset from pile done"));
    #endif
    return true;
  } else {
    return false;
  }
}

//set read index to next item in buffer pile (#item treatment done)
void BUFFER_PILE::next_pile() {
  if (full_elements > 0) {
    inc_CR();
    full_elements--;
  }
  #if BUFFER_DEBUG
    debug_cpt();
  #endif
}

//return number of available space to store datas
byte BUFFER_PILE::available_pile() {
  return NUM_BUFFERED - full_elements;
}

//return true if buffer is too full
byte BUFFER_PILE::alert_full_pile() {
  if (full_elements > LIMIT_BUFFERED ) {
    return true;
  }
  return false;
}

void BUFFER_PILE::debug_cpt() {
  Serial.print(F("#_# buf_CI:"));Serial.println(buf_CI);
  Serial.print(F("#_# buf_CR:"));Serial.println(buf_CR);
}

//formatting and loading date/time in stack/buffer compatible format
void BUFFER_PILE::get_formatted_datetime_bufferpile(const Ecoset& dataset, char* datetime) {
  const char template_datetime[] = "%02d/%02d/%02d %02d:%02d:%02d";
  char buffer_time[25] = "\0";
  snprintf(buffer_time, sizeof(buffer_time), template_datetime, dataset.year, dataset.month, dataset.day, dataset.hour, dataset.minute, dataset.second);
  strcpy(datetime, buffer_time);
}

//calculates and returns the UTC time based on the time stored in the dataset and the timezone offset
void BUFFER_PILE::get_formatted_UTC_bufferpile(const Ecoset& dataset, char* utc_datetime) {
  DateTime data_date(dataset.year,dataset.month,dataset.day,dataset.hour,dataset.minute,dataset.second);
  DateTime UTC_date;
  int DeltaUTC = abs(dataset.UTCoffset);
  if(dataset.UTCoffset < 0) {
    UTC_date = data_date + TimeSpan(DeltaUTC * 3600);
  } else {
    UTC_date = data_date - TimeSpan(DeltaUTC * 3600);
  }
  snprintf(utc_datetime, 20, "%04d-%02d-%02d %02d:%02d:%02d", UTC_date.year(), UTC_date.month(), UTC_date.day(), UTC_date.hour(), UTC_date.minute(), UTC_date.second());
}

/****************
PRIVATE FUNCTIONS
****************/
// buf_CI incrementation
void BUFFER_PILE::inc_CI() {
  buf_CI++;
  if(buf_CI >= NUM_BUFFERED) {buf_CI = 0;}
}

//buf_CR incrementation
void BUFFER_PILE::inc_CR() {
  buf_CR++;
  if(buf_CR >= NUM_BUFFERED) {buf_CR = 0;}
}