/*
Library to use TEL0157 module for GNSS and RTC DS3231
Created by Philippe CHAUMEIL, Sept 2026.
Assisted by Gemini AI for code generation and refactoring.
*/

#ifndef GNSS_RTC_H
#define GNSS_RTC_H

#include <Arduino.h>
#include <RTClib.h>
#include "config.h"

#if MOD_GPS
  #include <DFRobot_GNSS.h>
#endif

// Callback function te retrieve GSM DateTime 
typedef DateTime* (*GsmDateTimeCallback)();

class GNSS_RTC {
public:
    enum State {
        IDLE,
        POWERING_ON,
        WAITING_FIX,
        POWERING_OFF
    };

    GNSS_RTC(uint8_t relayPin = 3,
             GsmDateTimeCallback gsmCallback = nullptr,
             unsigned long syncIntervalMs = 604800000UL,    // 7 days default 604800000UL
             unsigned long retryIntervalMs = 600000UL); // 10 min default 600000UL

    bool begin();
    void update();      // To call in main loop()
    void triggerSync(); // Trigger a synchronization request based on available modules

    // usefull RTC methods
    DateTime getNow();
    void getFormattedDateTime(char* buffer, size_t size);
    void printFormattedDateTime();

    bool adjustWithGSM();

private:
    RTC_DS3231 _rtc;
    
    uint8_t _relayPin;
    GsmDateTimeCallback _getGsmDateTime; // Pointer to GSM function GSM supplied in parameter
    State _state;
    
    uint8_t _attemptCount;
    unsigned long _lastSyncTime;
    unsigned long _syncInterval;    //Nominal interval
    unsigned long _retryInterval;
    unsigned long _nextInterval;
    unsigned long _stateTimer;
    unsigned long _lastAttemptTimer;
    unsigned long _lastFixCheckTimer;

#if MOD_GPS
    DFRobot_GNSS_I2C _gnss;
#endif

    const unsigned long GNSS_TIMEOUT_MS = 120000UL;             // 2-minute timeout to acquire the GNSS fix
    const unsigned long WARMUP_DELAY_MS = 4000UL;               // 4-second delay after relay activation
    const unsigned long FIX_CHECK_INTERVAL_MS = 5000UL;         // Timer to space out calls

    void startGNSS();
    void stopGNSS();
    bool trySyncFromGNSS();
};

#endif // GNSS_RTC_H
