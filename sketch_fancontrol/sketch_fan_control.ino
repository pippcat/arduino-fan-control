// Arduino Fan Control
// Written by tommy.schoenherr@posteo.de , public domain

#include <SPI.h> // for SD card
#include <SD.h> // for SD card
#include <LiquidCrystal_I2C.h> // For the LCD: https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library/blob/master/LiquidCrystal_I2C.h
LiquidCrystal_I2C lcd(0x27, 20, 4); // ??, rows, columns
#include "RTClib.h" // for the Real Time Clock: https://www.elecrow.com/wiki/index.php?title=File:RTC.zip
RTC_DS1307 RTC;
#include <math.h> // for calculations
// DHT Library:
#include <SimpleDHT.h>
int dataPinSensor1 = 2;  // inside
int dataPinSensor2 = 3;  // outside
SimpleDHT22 dht22;

// variables for the SD card
Sd2Card card;
SdVolume volume;
SdFile root;
const int chipSelect = 4;
const int relaisPin = 8; // PIN to switch the relais
int lastwrite = 0; // writecheck for SD card
boolean fan = false; // fan on = true or off = false
int screentime = 5000; // screen time of the diffent pages in ms

////////////////////////////////////
// setup
/////////////////////////////////////
void setup() {
  // db("a"); //debug letters will be located in the corner of the display if enablead
  lcd.begin();
  pinMode(relaisPin, OUTPUT); // set relaisPin as output
  //dht1.begin();
  //dht2.begin();
  RTC.begin();
  if (! RTC.isrunning()) {
    // RAM issues if serial.println is enabled, therefore uncommented in whole sketch :((
    //Serial.println("RTC is NOT running!\n");
    RTC.adjust(DateTime(__DATE__, __TIME__)); // set RTC to system time
  }
  //Serial.begin(9600); // start serial communication
  while (!Serial) {
    ; // wait until serial port is ready
  }

  //Serial.print("SD Karte wird initialisiert.\n");

  // check avaliability of SD card
  if (!SD.begin(chipSelect)) {
    //Serial.println("Initialisierung der SD Karte fehlgeschlagen.\n");
    return;
  }
  //Serial.println("SD Karte bereit.\n");
  // db("b");
}

void loop() {  
  ///////////////////////////////////////////////////////////////
  // sensor readout // DB: c -> d
  ///////////////////////////////////////////////////////////////
  //Serial.print("//// BEGIN LOOP /// \n");
  lcd.setCursor(19, 3);
  lcd.print("c");
  //Serial.print("Sensoren werden ausgelesen\n");
  // read sensor 1:
  //Serial.println("Lese Sensor 1");
  byte temperature = 0;
  byte humidity = 0;
  int err = SimpleDHTErrSuccess;
  if ((err = dht22.read(dataPinSensor1, &temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
    //Serial.print("Fehler Sensor 1: "); //Serial.println(err);delay(1000);
    return;
  }
  //Serial.print("Daten Sensor 1: ");
  //Serial.print((int)temperature); //Serial.print(" °C, "); 
  //Serial.print((int)humidity); //Serial.println(" %");
  float t1 = (float)temperature;
  float h1 = (float)humidity;
  // Sensor 2
  ////Serial.println("Lese Sensor 2");
  byte temperature2 = 0;
  byte humidity2 = 0;
  if ((err = dht22.read(dataPinSensor2, &temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
    //Serial.print("Fehler Sensor 2: "); //Serial.println(err);delay(1000);
    return;
  }
  //Serial.print("Daten Sensor 2: ");
  //Serial.print((int)temperature); //Serial.print(" °C, "); 
  //Serial.print((int)humidity); //Serial.println(" %");
  float t2 = (float)temperature;
  float h2 = (float)humidity;
  lcd.setCursor(19, 3);
  lcd.print("d");
  
  ///////////////////////////////////////////////////////////////
  // Sometimes the sensors don't give back a proper value
  // which leads to a corrupted log file.
  // Therefore the loop is only executed if all values are valid.
  // DB: e -> f
  lcd.print("e");
  if (isnan(h1) || isnan(t1) || isnan(h2) || isnan(t2)) {
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("SENSORFEHLER");
    lcd.setCursor(19, 3);
    lcd.print("e");
    //Serial.println("SENSORFEHLER!\n");
    // Sometimes sensors loose connection for ~1 second
    // shut down fan if the problem persists.
    fan = false; 
    digitalWrite(relaisPin, LOW);
    delay(screentime); // wait before retrying
    lcd.print("f");
    return;
  }
  else {
    
    ///////////////////////////////////////////////////////////////////////////////
    // Calculations done after http://www.wetterochs.de/wetter/feuchte.html
    // DB: g -> h
    ///////////////////////////////////////////////////////////////////////////////
    lcd.clear();
    lcd.setCursor(19, 3);
    lcd.print("g");
    //Serial.print("Berechnung der Luftfeuchte\n");
    float a1, a2 = 7.5;
    float b1, b2 = 237.3;
    // a and b for T>=0°C:
    if (t1 > 0) {
      a1 = 7.5;
      b1 = 237.3;
    }
    // a and b for T<0°C:
    else {
      a1 = 7.6;
      b1 = 240.7;
    }
    if (t2 > 0) {
      a2 = 7.5;
      b2 = 237.3;
    }
    else {
      a2 = 7.6;
      b2 = 240.7;
    }
    // SVP = saturation vapor pressure in hPa; VaPr = vapor pressure in hPa;
    // TODP = temperature of dew point  in °C; AH = absolute humidity in g H20 per m^3 air
    float SVP1 = 6.1078 * pow(10, ((a1 * t1) / (b1 + t1)));
    float VaPr1 = h1 / 100 * SVP1;
    float v1 = log10(VaPr1 / 6.1078);
    float TODP1 = b1 * v1 / (a1 - v1);
    float AH1 = pow(10, 5) * 18.016 / 8314.3 * VaPr1 / (t1 + 273.15);
    float SVP2 = 6.1078 * pow(10, ((a2 * t2) / (b2 + t2)));
    float VaPr2 = h2 / 100 * SVP2;
    float v2 = log10(VaPr2 / 6.1078);
    float TODP2 = b2 * v2 / (a2 - v2);
    float AH2 = pow(10, 5) * 18.016 / 8314.3 * VaPr2 / (t2 + 273.15);
    float dif_t = t1 - t2;
    float dif_h = h1 - h2;
    ////Serial.print("Sättigungsdampfdruck innen = "); //Serial.print(SVP1); //Serial.print("hPa; ");
    ////Serial.print("Sättigungsdampfdruck aussen = "); //Serial.print(SVP2); //Serial.print("hPa; ");
    ////Serial.print("Taupunkttemperatur innen = "); //Serial.print(TODP1); //Serial.print("°C; ");
    ////Serial.print("Taupunkttemperatur aussen = "); //Serial.print(TODP2); //Serial.print("°C; ");
    ////Serial.print("Absolute Feuchte innen = "); //Serial.print(AH1); //Serial.print("g/m³; ");
    ////Serial.print("Absolute Feuchte aussen = "); //Serial.print(AH2); //Serial.print("g/m³; ");
    ////Serial.print("Temperaturunterschied = "); //Serial.print(dif_t); //Serial.print("°C; ");
    ////Serial.print("Luftfeuchtenunterschied = "); //Serial.print(dif_t); //Serial.print("%\n");
    
    lcd.setCursor(19, 3);
    lcd.print("h");
    
    ///////////////////////////////////////////////////////////////////////
    // fan control // DB: i -> j
    ///////////////////////////////////////////////////////////////////////
    lcd.setCursor(19, 3);
    lcd.print("i");
    if (AH2 > AH1) { // abs. hum. outside is bigger -> switch off fan
      //Serial.print("Absolute Luftfeuchte außen größer -> Lüfter aus!\n");
      fan = false;
      digitalWrite(relaisPin, LOW);
    }
    else { // abs. hum. outside smaller
      //Serial.print("Absolute Luftfeuchte innen größer.\n");
      fan = false;
      digitalWrit  e(relaisPin, LOW);
      if ( t2 > t1 ) { // outside is warmer than inside -> switch on fan
        //Serial.print("Außen wärmer als innen -> Lüfter an!\n");
        fan = true;
        digitalWrite(relaisPin, HIGH);
      }
      else { // outside is colder than inside
        //Serial.print("Außen kälter als innen.\n");
        if ( t1 > TODP1 ) { // is the temperature inside bigger than the dew point?
          //Serial.print("Temperatur im Keller über Taupunkt -> Lüfter an!\n");
          fan = true;
          digitalWrite(relaisPin, HIGH);
        }
      }
    }
    lcd.setCursor(19, 3);
    lcd.print("j");
  
    //////////////////////////////////////////////////////////////////////
    // Display // DB: k, l, m
    ///////////////////////////////////////////////////////////////////////
    // LCD screen 1: inside
    //Serial.print("Schreibe LCD Bild 1: innen\n");
    lcd.clear();
    lcd.setCursor(19, 3);
    lcd.print("k");
    lcd.setCursor(7, 0); // Column,Row
    lcd.print("INNEN ");
    lcd.setCursor(0, 1);
    lcd.print(" T=");
    lcd.print(t1);
    lcd.print("C  LF=");
    lcd.print(h1);
    lcd.print("%");
    lcd.setCursor(0, 2);
    lcd.print("TP=");
    lcd.print(TODP1);
    lcd.print("C  AF=");
    lcd.print(AH1);
    lcd.setCursor(0, 3);
    lcd.print("fan:   ");
    if (fan == false) {
      lcd.print("AUS");
    }
    else {
      lcd.print("AN");
    }
    delay(screentime);
    
    // LCD Bild 2: AUSSEN
    //Serial.print("Schreibe LCD Bild 2: aussen\n");
    lcd.clear();
    lcd.setCursor(19, 3);
    lcd.print("l");
    lcd.setCursor(7, 0); // Spalte,Zeile
    lcd.print("AUSSEN");
    lcd.setCursor(0, 1);
    lcd.print(" T=");
    lcd.print(t2);
    lcd.print("C  LF=");
    lcd.print(h2);
    lcd.print("%");
    lcd.setCursor(0, 2);
    lcd.print("TP=");
    lcd.print(TODP2);
    lcd.print("C  AF=");
    lcd.print(AH2);
    lcd.setCursor(0, 3);
    lcd.print("fan:   ");
    if (fan == false) {
      lcd.print("AUS");
    }
    else {
      lcd.print("AN");
    }
    delay(screentime);
    
    // LCD Bild 3: ZEIT UND DATUM
    //Serial.print("Schreibe LCD Bild 3: Status\n");
    DateTime now = RTC.now();
    lcd.clear();
    lcd.setCursor(19, 3);
    lcd.print("m");
    lcd.setCursor(7, 0);
    lcd.print("STATUS");
    lcd.setCursor(0, 1);
    lcd.print("Datum:    ");
    if (now.day() < 10) {  // Formatierung bei < 10
      lcd.print("0");
    }
    lcd.print(now.day(), DEC);
    lcd.print('.');
    if (now.month() < 10) {  // Formatierung bei < 10
      lcd.print("0");
    }
    lcd.print(now.month(), DEC);
    lcd.print('.');
    lcd.print(now.year(), DEC);
    lcd.setCursor(0, 2);
    lcd.print("Zeit:     ");
    if (now.hour() < 10) {  // Formatierung bei < 10
      lcd.print("0");
    }
    lcd.print(now.hour(), DEC);
    lcd.print(':');
    if (now.minute() < 10) {  // Formatierung bei < 10
      lcd.print("0");
    }
    lcd.print(now.minute(), DEC);
    lcd.print(':');
    if (now.second() < 10) {  // Formatierung bei < 10
      lcd.print("0");
    }
    lcd.print(now.second(), DEC);
    lcd.setCursor(0, 3);
    lcd.print("fan:   ");
    if (fan == false) {
      lcd.print("AUS");
    }
    else {
      lcd.print("AN");
    }
    delay(screentime);
  
    
    ///////////////////////////////////////////////////////////////
    // SD Card logging // DB: n, o, p
    //////////////////////////////////////////////////////////////
    // only write to SD card if more than a minute has passed
    lcd.setCursor(19, 3);
    lcd.print("n");
    if (lastwrite != now.minute()) {
      String dataString = ""; // string for datalog to SD
      // format: unixtime;t1;h1;TODP1;AH1;t2;h2;TODP2;AH2;fan
      // get correct date in LibreOfficeCalc: =A1/86400+25569
      dataString += String(now.unixtime());
      dataString += ";";
      dataString += String(t1);
      dataString += ";";
      dataString += String(h1);
      dataString += ";";
      dataString += String(TODP1);
      dataString += ";";
      dataString += String(AH1);
      dataString += ";";
      dataString += String(t2);
      dataString += ";";
      dataString += String(h2);
      dataString += ";";
      dataString += String(TODP2);
      dataString += ";";
      dataString += String(AH2);
      dataString += ";";
      dataString += String(fan);
      
      ///////////////////////////////////////////////////////////////
      // Write to SD card
      ///////////////////////////////////////////////////////////////
      
      lcd.setCursor(19, 3);
      lcd.print("o");
      // open file. don't forget to close it!
      File dataFile = SD.open("log.txt", FILE_WRITE);
      // write if file is available:
      if (dataFile) {
        dataFile.println(dataString);
        dataFile.close();
        //Serial.println("Schreibe auf SD-Karte:\n");
        //Serial.println(dataString);
        lcd.clear();
        lcd.setCursor(0, 1);
        lcd.print("Schreibe auf SD!");
        delay(screentime);
      }
      // error handling:
      else {
        //Serial.println("Kann datalog.txt nicht öffnen!");
        lcd.clear();
        lcd.setCursor(0, 1);
        lcd.print("SD Karten Fehler");
        delay(screentime);
      }
      lastwrite = now.minute(); // set lastwrite to now
      lcd.setCursor(19, 3);
      lcd.print("p");
    }
    lcd.setCursor(19, 3);
    lcd.print("q");
  }
  //Serial.print("///// END LOOP ///// \n"); 
}

// disabled due to RAM issues:
// void db(char Buchstabe) {
//   lcd.setCursor(19, 3);
//   lcd.print(buchstabe);
//   return; 
//}

