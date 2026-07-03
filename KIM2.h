/*
Library to use KIM2 module for satelite transmission
Created by Malek Charrad, May 2026.
Assisted by Gemini AI for code generation and refactoring.
Modified by Philippe CHAUMEIL, June 2026
*/
#ifndef KIM2_h
#define KIM2_h
#include <Arduino.h>

enum ResetState {
  RESET_IDLE,
  RESET_OFF,
  RESET_WAIT_OFF,
  RESET_ON,
  RESET_WAIT_ON
};

// Job Status
enum KIM2Status {
  KIM2_IDLE,
  KIM2_BUSY,
  KIM2_OK,
  KIM2_ERROR,
  KIM2_TIMEOUT
};

// Running mode
enum KIM2Mode {
  KIM2_BOOTING,
  KIM2_INIT,
  KIM2_RUN
};


class KIM2 {
public:
   // Constructor
  KIM2(Stream& serial, uint8_t powerPin, uint8_t relayPin);

  // Module initialization
  void initKim2(const char* rconfToken);
  
  // Power on module
  void powerOn();

  //listen periodically
  void update(); 

  //send datas to sat
  bool sendPayload(const char* payload);
 
  //get module status
  KIM2Status getStatus() const;

private:

  // HARDWARE 
  Stream* _serial;
  uint8_t _powerPin;
  uint8_t _relayPin;

  // State
  ResetState _resetState;
  KIM2Status _status;
  KIM2Mode _mode;

  //build messages
  char _buffer[64];
  size_t _bufferIndex;

  // pointer to dynamic token
  const char* _rconfToken;

  // timeout
  unsigned long _startTime;
  unsigned long _timeout;

  // Error management & index
  int _failCount;
  int _currentIndex;

  // COMMANDS INIT
  static const int MAX_CMD = 3;
  
// =====================
// Internal functions
// =====================

void start(); // init + radio config
void sendCurrentCommand();
void readSerial();
void parseLine(const char* line);
void flushInput();
void hardwareReset();
};
#endif