#ifndef SIM7600MQTTparam_h
#define SIM7600MQTTparam_h

  #define CLIENT_ID "st_name"                       // Indentifiant du client pour le protocole MQTT
  #define SERVER_URL "mqtt.your.server.fr"
  #define PORT "8883"
  #define LOGIN "login"
  #define PASSWD "password"

  #define RELAY_SIM_PIN 5
  
  //AT+IPREX=57600  set baudrate permanently after reboot

  const bool SSLMODE = 1;

  #define DEFAULT_TZ 1 //offset to UTC in hours at time the program is compiled and uploaded. Can be positive or negative.
  #define SYNC_NTP_DATETIME 1 //request NTP server to synchronize date & time else use gsm operator NITZ infos (! quick but not always available or exact !)

  //AT commands related to TimeZone
  //AT+CTZU=1 enable and disable automatic time and time zone update via NITZ  (nonvolatile)
  //AT+CTZR=1 time zone change event reporting on (volatile)
  //AT+CCLK=“08/11/28,12:30:33+32” last 2 digits

  //NTP (volatile)
  //AT+CNTP="fr.pool.ntp.org",0   get time from NTP server in UTC
  //AT+CNTP   return OK and +CNTP:0 or other number if error

#endif
