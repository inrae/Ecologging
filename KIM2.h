/*
Library to use KIM2 module for satelite transmission
Created by Malek Charrad, May 2026.
Assisted by Gemini AI for code generation and refactoring.
Modified by Philippe CHAUMEIL, Sept 2026
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

  // Initialization module
  void initKim2(const char* rconfToken);
  
  // Power on module
  void powerOn();

  // Listen periodically
  void update(); 

  // Send datas to sat (Sequence: AT+KMAC -> AT+TX)
  bool sendPayload(const char* payload);
 
  // Get module status
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

  // Buffers
  char _buffer[64];
  size_t _bufferIndex;
  char _rconfToken[36];
  char _payloadBuffer[50];

  // Timeout
  uint32_t _startTime;
  uint32_t _timeout;

  // Error management & index
  int _failCount;
  
  // Sequence indexes & limits
  int _initIndex;
  static const int MAX_INIT_CMD = 3;

  int _runIndex;
  static const int MAX_RUN_CMD = 2;

  // Private Helper Functions
  void start(); 
  void sendInitCommand();
  void sendNextRunCommand();
  void readSerial();
  void parseLine(const char* line);
  void flushInput();
  void hardwareReset();

  // Modular State Update Functions
  void updateResetState();
  void updateBootingMode();
  void updateInitMode();
  void updateRunMode();
};

#endif