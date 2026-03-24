# Ecologging_meteo

## Description
Le projet ECOLOGGING a vu le jour pour proposer une solution alternative aux stations commerciales déjà existantes. La station ECOLOGGING est moins chère, et permet de relever plusieurs paramètres météorologiques tels que la température, l’humidité relative, la vitesse du vent, son orientation, le rayonnement, et le cumul de pluie. Tout en gardant un rapport qualité coût de la donnée très intéressant. Cette station est modulable et déplaçable.
La station utilise comme microcontrolleur un ARDUINO MEGA 2560 associé à une carte SD pour le stockage local des données et un HAT SIM 7600G 4G pour la transmission 4G.
La transmission utilise le protocole MQTT avec support SSL et authentification login/password.
Un tampon mémoire paramétrable est utilisé pour stocker les données devant être transmises en cas de perte temporaire du réseau 4G.

The ECOLOGGING project was created to offer an alternative to existing commercial weather stations. The ECOLOGGING station is less expensive and allows for the recording of several meteorological parameters such as temperature, relative humidity, wind speed, wind direction, solar radiation, and rainfall totals, all while maintaining a very attractive data quality-to-cost ratio. This station is modular and portable.
The station uses an Arduino Mega 2560 microcontroller, an SD card for local data storage, and a 7600G 4G HAT SIM for 4G transmission.
Transmission uses the MQTT protocol with SSL support and login/password authentication.
A configurable memory buffer is used to store data to be transmitted in case of a temporary loss of the 4G network.

## Badges
![Demo Image](images/station_meteo_DIY.png)

## Installation
Le code peut être chargé sur l'arduino Mega avec l'IDE Arduino.
NOTE: Compte tenu des ressources mémoires nécessaires, le choix d'un Arduino Mega est requis.
Pour la communication MQTT avec SSL il est nécessaire de charger au préalable les clés de cryptage dans le module SIM 7600G depuis un serveur FTP.
Les documents relatifs à la réalisation matérielle de la station sont accessible sur hal, notamment dans le document [Tutoriel](https://hal.science/hal-05162126v2)

The code can be uploaded to the Arduino Mega using the Arduino IDE.
NOTE: Due to the memory requirements, an Arduino Mega is mandatory.
For MQTT communication with SSL, the encryption keys must first be uploaded to the SIM 7600G module from an FTP server.
Documents relating to the hardware implementation of the station are available on HAL, particularly in the [Tutorial](https://hal.science/hal-05162126v2) document.


## Usage
Il est nécessaire de paramétrer un certain nombre de valeurs dans les fichiers suivants :

_ config.h > selection des capteurs installés, de la périodicité des acquisitions / moyennes, du nom du topic si envoi en 4G
_ SIM7600MQTTparam.h > parmétrage des informations de connexion au serveur

_ buffer_pile.h > nombre de données gardées en mémoire tampon (NUM_BUFFERED)
_ capteurs_meteo.h > paramétrage des capteurs (GPIO utilisés)


Several values ​​need to be configured in the following files:

_ config.h > selection of installed sensors, acquisition frequency/averages, topic name if sending via 4G
_ SIM7600MQTTparam.h > server connection information settings

_ buffer_pile.h > number of data points kept in buffer memory (NUM_BUFFERED)
_ sensor_meteo.h > sensor settings (GPIO used)

## Support
pierre.bordenave@inrae.fr

## Roadmap
Support for Lorawan connectivity
Support for Satellite connectivity

## Authors and acknowledgment
Pierre Bordenave (UEFP - INRAe Cestas Pierroton) pierre.bordenave@inrae.fr
Philippe Chaumeil (Biogeco - INRAe Cestas Pierroton) philippe.chaumeil@inrae.fr


## License
GNU GPL

## Project status
Développement et test en cours.
