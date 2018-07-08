Arduino humidity fan control
============================

This Sketch is used to control an Arduino Nano which is used to get the wet air out of a basement. It'll 
- read two DHT22 sensors to get the temperature and humidity from the inside and outside world
- calculate the absolute humidity
- decide, if it is useful to blow the air out
- use a relais to switch on or off a fan
- display data on a 20x4 I2C-LCD
- log the measurements to an SD card, using a RTC module for timestamping

How it works
------------

![decision tree](images/fancontrol.img.jpg?raw=true "decision tree of program logic")

Bugs
----

- the sketch crashes after a random (hours .. days) time due to unknown reason
