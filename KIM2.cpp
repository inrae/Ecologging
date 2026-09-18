#include "KIM2.h"

// CONSTRUCTOR
KIM2::KIM2(Stream& serial, uint8_t powerPin, uint8_t relayPin) {
  _serial = &serial;
  _powerPin = powerPin;
  _relayPin = relayPin;
}

// INITIALIZATION
void KIM2::initKim2(const char* rconfToken) {
  if (rconfToken != nullptr) {
    strncpy(_rconfToken, rconfToken, sizeof(_rconfToken) - 1);
    _rconfToken[sizeof(_rconfToken) - 1] = '\0';
  } else {
    _rconfToken[0] = '\0';
  }

  pinMode(_powerPin, OUTPUT);
  pinMode(_relayPin, OUTPUT);
  digitalWrite(_relayPin, LOW); // active relay (NC configuration)

  _status = KIM2_IDLE;
  _mode = KIM2_INIT;
  _failCount = 0;
  _initIndex = 0;
  _runIndex = 0;

  _bufferIndex = 0;
  _buffer[0] = '\0';
  _payloadBuffer[0] = '\0';
  _resetState = RESET_IDLE;
}

// Power on module
void KIM2::powerOn() {
  Serial.println("[KIM2] POWER ON");
  digitalWrite(_powerPin, HIGH);

  _mode = KIM2_BOOTING;
  _initIndex = 0;
  _runIndex = 0;
  _startTime = millis();
}

// run INIT sequence
void KIM2::start() {
  Serial.println(F("[KIM2] START INIT"));
  _initIndex = 0;
  _failCount = 0;
  _runIndex = 0;
  sendInitCommand();
}

// Send command (INIT Mode) - Symétrique avec sendNextRunCommand
void KIM2::sendInitCommand() {
  if (_initIndex >= MAX_INIT_CMD) {
    Serial.println(F("[KIM2] INIT FINISHED -> RUN MODE"));

    _mode = KIM2_RUN;
    _status = KIM2_IDLE;
    _runIndex = 0;
    return;
  }

  flushInput();

  if (_initIndex == 0) {
    _serial->println("AT+PING=?");
  } 
  else if (_initIndex == 1) {
    _serial->print(F("AT+RCONF="));
    _serial->println(_rconfToken); 
  } 
  else if (_initIndex == 2) {
    _serial->println(F("AT+KMAC=1"));
  }

  _status = KIM2_BUSY;
  _startTime = millis();
  _timeout = 3000;
}

// Send command (RUN Sequence: AT+KMAC=1 then AT+TX)
void KIM2::sendNextRunCommand() {
  flushInput();

  if (_runIndex == 0) {
    Serial.println(F("[KIM2] RUN SEQ [0] -> AT+KMAC=1"));
    _serial->println(F("AT+KMAC=1"));
    _timeout = 3000;
  } 
  else if (_runIndex == 1) {
    Serial.print(F("[KIM2] RUN SEQ [1] -> AT+TX="));
    Serial.println(_payloadBuffer);
    
    _serial->print(F("AT+TX="));
    _serial->print(_payloadBuffer);
    _serial->println(F(",00"));
    _timeout = 5000;
  }

  _status = KIM2_BUSY;
  _startTime = millis();
}

// Send PAYLOAD (Entry point for RUN sequence)
bool KIM2::sendPayload(const char* payload) {
  if (_mode != KIM2_RUN){
    Serial.println(F("[KIM2] SEND BLOCKED -> NOT RUN MODE"));
    return false;
  }
  if (_status == KIM2_BUSY){    
    Serial.println(F("[KIM2] SEND BLOCKED -> BUSY"));
    return false;                 
  }

  if (payload != nullptr) {
    strncpy(_payloadBuffer, payload, sizeof(_payloadBuffer) - 1);
    _payloadBuffer[sizeof(_payloadBuffer) - 1] = '\0';
  } else {
    _payloadBuffer[0] = '\0';
  }

  Serial.println(F("[KIM2] START RUN SEQUENCE"));
  _runIndex = 0;
  sendNextRunCommand();

  return true;
}

// UPDATE MAIN (Dispatcher)
void KIM2::update() {
  if (_resetState != RESET_IDLE) {
    updateResetState();
    return;
  }

  switch (_mode) {
    case KIM2_BOOTING:
      updateBootingMode();
      break;
    case KIM2_INIT:
      updateInitMode();
      break;
    case KIM2_RUN:
      updateRunMode();
      break;
  }
}

// --- UPDATE SUB_FUNCTIONS ---

void KIM2::updateResetState() {
  switch (_resetState) {
    case RESET_OFF:
      Serial.println(F("[KIM2] RESET -> POWER OFF"));
      digitalWrite(_relayPin, HIGH); 
      _startTime = millis();        
      _resetState = RESET_WAIT_OFF; 
      break;

    case RESET_WAIT_OFF:
      if (millis() - _startTime >= 3000) {
        _resetState = RESET_ON;       
      }
      break;

    case RESET_ON:
      digitalWrite(_relayPin, LOW); 
      _startTime = millis();         
      _resetState = RESET_WAIT_ON;   
      break;

    case RESET_WAIT_ON:
      if (millis() - _startTime >= 8000) { 
        Serial.println(F("[KIM2] RESET COMPLETE"));
        flushInput();            
        _resetState = RESET_IDLE; 
        _mode = KIM2_INIT;       
        _initIndex = 0;
        _runIndex = 0;
        start();                 
      }
      break;

    default:
      Serial.println(F("[KIM2] ERROR: Invalid reset state, recovering..."));
      _resetState = RESET_IDLE;
      break;
  }
}

void KIM2::updateBootingMode() {
  if (millis() - _startTime >= 8000) {
    Serial.println(F("[KIM2] BOOT FINISHED"));
    flushInput();             
    _mode = KIM2_INIT;        
    start();                  
  }
}

void KIM2::updateInitMode() {
  readSerial();

  if (_status == KIM2_BUSY && (millis() - _startTime >= _timeout)) {
    Serial.println(F("[KIM2] TIMEOUT IN INIT"));
    _failCount++;
    
    if (_failCount >= 3) {
      Serial.println(F("[KIM2] TOO MANY FAILS -> RESET"));
      _failCount = 0;
      hardwareReset(); 
    } else {
      _status = KIM2_TIMEOUT;
      _startTime = millis();
      _timeout = 1000;
    }
  }

  if (_status == KIM2_ERROR || _status == KIM2_TIMEOUT) {
    if (millis() - _startTime >= _timeout) {
      Serial.println(F("[KIM2] -> RETRYING INIT COMMAND"));
      sendInitCommand();
    }
  }
}

void KIM2::updateRunMode() {
  readSerial();               

  if (_status == KIM2_BUSY && millis() - _startTime >= _timeout) {
    Serial.println(F("[KIM2] TIMEOUT IN RUN"));
    _failCount++;
    Serial.print(F("[KIM2] FAIL COUNT = ")); Serial.println(_failCount);
    
    if (_failCount >= 3) {
      Serial.println(F("[KIM2] TOO MANY FAILS -> RESET"));
      _failCount = 0;
      hardwareReset();        
    } else {
       _status = KIM2_TIMEOUT;
    }
  }
  
  if (_status == KIM2_ERROR || _status == KIM2_TIMEOUT) {
    if (millis() - _startTime >= _timeout) {
      Serial.println(F("[KIM2] -> RETRYING CURRENT RUN COMMAND"));
      sendNextRunCommand();
    }
  }
}

// read module
void KIM2::readSerial() {
  while (_serial->available()) {
    char c = _serial->read();
    if (c == '\n') {
      _buffer[_bufferIndex] = '\0';
      parseLine(_buffer);
      _bufferIndex = 0;         
    }
    else if (c != '\r') {
      if (_bufferIndex < sizeof(_buffer) - 1) {
        _buffer[_bufferIndex++] = c;
      }
    }
  }
}

// Parse response
void KIM2::parseLine(const char* line) {
  if (line == nullptr || line[0] == '\0') return;

  Serial.print(F("[KIM2 RX] "));
  Serial.println(line);

  if (strncmp(line, "AT+FW", 5) == 0) return;

  // MODE INIT
  if (_mode == KIM2_INIT) {
    if (strstr(line, "+OK") != nullptr) {
      _failCount = 0;
      _initIndex++;
      sendInitCommand();
      return;
    }
  }

  // MODE RUN (indexed seq: _runIndex 0 -> KMAC, 1 -> TX)
  if (_mode == KIM2_RUN) {
    if (_runIndex == 0 && strstr(line, "+OK") != nullptr) {
      Serial.println(F("[KIM2] KMAC OK -> NEXT RUN COMMAND"));
      _failCount = 0;
      _runIndex++;
      sendNextRunCommand();
      return;
    }
    else if (_runIndex == 1 && strncmp(line, "+TX=", 4) == 0) {
      Serial.println(F("[KIM2] TX OK -> SEQUENCE FINISHED"));
      _status = KIM2_OK;
      _failCount = 0;
      _runIndex = 0;
      return;
    }
  }

  // Global Error received
  if (strstr(line, "ERROR") != nullptr) {
    Serial.println(F("[KIM2] ERROR"));
    _failCount++;

    if (_failCount >= 3) {
      Serial.println(F("[KIM2] TOO MANY ERRORS -> RESET"));
      _failCount = 0;
      hardwareReset();
    } else {
      _status = KIM2_ERROR;
      _startTime = millis();
      _timeout = 1000;
    }
  }
}

void KIM2::flushInput() {
  while (_serial->available()) {
    _serial->read();
  }
}

void KIM2::hardwareReset() {
  Serial.println(F("[KIM2] HARD RESET"));
  _status = KIM2_IDLE;
  _initIndex = 0;
  _runIndex = 0;
  _resetState = RESET_OFF;
}

KIM2Status KIM2::getStatus() const {
  return _status;
}