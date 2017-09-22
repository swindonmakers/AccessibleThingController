# AccessibleThingController
Firmware for our NodeMCU-based access and machine (things) controllers


Hardware Setup
==============

* NodeMCU
* PN532 RFID module connected over I2C
* Relay module to switch door/machine power
* Neopixel LED or Ring for status feedback
* Buttons - door exit or machine time control
* Door sensor (magnetic reed switch)
* Alarm output (to buzzer), via relay or transistor
* Power sense line - to detect when running off battery backup

Projects
========
* doorController - code running an Arduino Pro Mini for the door.
* thingWifi - code running on an ESP8266-01 for the door.
* thingController - "abandoned" door controller code for a NodeMCU (not reliable / stable enough)
* machineController - code for a NodeMCU for controlling access to machines [see](https://github.com/swindonmakers/AccessibleThingController/wiki/Machine-Access-Controller)

Custom Libraries
================
* AccessSystem - wrapper for api calls to the AccessSystem Pi
* TokenCache - an EEPROM based cache for access tokens
