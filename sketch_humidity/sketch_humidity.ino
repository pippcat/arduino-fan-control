// Lüftersteuerung zur Kellerentlüftung mit Innen- und Außensensor
// Written by tommy.schoenherr@posteo.de , public domain

// Bugs:
// - Steuerung kackt regelmäßig ab: Display zeigt nix an, Steuerung bleibt hängen
// - Sensoren werden öfter für ~1 Sek nicht erkannt

// ToDos:
// - Stabilitätsprobleme lösen

#include <SPI.h> // für die SD Karte
#include <SD.h> // für die SD Karte
#include <LiquidCrystal_I2C.h> // Für den LCD - https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library/blob/master/LiquidCrystal_I2C.h
LiquidCrystal_I2C lcd(0x27, 20, 4); // ??, Spalten, Zeilen
#include "RTClib.h" // Für die Uhr - https://www.elecrow.com/wiki/index.php?title=File:RTC.zip
RTC_DS1307 RTC;
#include <math.h> // Für Berechnungen
// DHT Library:
#include <SimpleDHT.h>
int dataPinSensor1 = 2;  // innen
int dataPinSensor2 = 3;  // aussen
SimpleDHT22 dht22;

// alte DHT Library:
//#include "DHT.h" // Für die Sensoren
//#define DHT1PIN 2     // PIN innen
//#define DHT2PIN 3     // PIN aussen
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//DHT dht1(DHT1PIN, DHTTYPE); // innen
//DHT dht2(DHT2PIN, DHTTYPE); // aussen

// Variablen für die SD Karte:
Sd2Card card;
SdVolume volume;
SdFile root;
const int chipSelect = 4;
const int relaisPin = 8; // PIN zum schalten des Relais
int lastwrite = 0; // für Schreibcheck SD Karte
boolean Luefter = false; // Lüfter an=true oder aus=false
int anzeigedauer = 5000; // Anzeigedauer der 3 LCD Screens in ms

////////////////////////////////////
// Setup
/////////////////////////////////////
void setup() {
  // db("a");
  lcd.begin();
  pinMode(relaisPin, OUTPUT); // Relaispin als Output setzen
  //dht1.begin();
  //dht2.begin();
  RTC.begin();
  if (! RTC.isrunning()) {
    //Serial.println("RTC is NOT running!\n"); // alle serials auskommentiert, sonst gibts RAM Probleme
    RTC.adjust(DateTime(__DATE__, __TIME__)); // RTC auf Compilezeit setzen
  }
  //Serial.begin(9600); // Serielle Kommunikation starten
  while (!Serial) {
    ; // warten, bis serieller Port bereit ist
  }

  //Serial.print("SD Karte wird initialisiert.\n");

  // überprüfen, ob SD Karte verfügbar ist
  if (!SD.begin(chipSelect)) {
    //Serial.println("Initialisierung der SD Karte fehlgeschlagen.\n");
    return;
  }
  //Serial.println("SD Karte bereit.\n");
  // db("b");
}

void loop() {  
  //Serial.print("//// BEGIN LOOP /// \n");
  ///////////////////////////////////////////////////////////////
  // Auslesen der Sensoren // DB: c -> d
  ///////////////////////////////////////////////////////////////
  lcd.setCursor(19, 3);
  lcd.print("c");
  //Serial.print("Sensoren werden ausgelesen\n");
   // Lese Sensor 1:
  ////Serial.println("Lese Sensor 1");
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
  // alte library
  //float h1 = dht1.readHumidity(); // innen
  //float t1 = dht1.readTemperature();
  //float h2 = dht2.readHumidity(); // außen
  //float t2 = dht2.readTemperature();
  lcd.setCursor(19, 3);
  lcd.print("d");
  
  ///////////////////////////////////////////////////////////////
  // Die Sensoren geben manchmal keinen Messwert zurück,
  // was dazu führt, dass das Logfile korrumpiert wird.
  // Deshalb wird der komplette Loop nur ausgeführt
  // wenn alle Messwerte erkannt wurden:
  // DB: e -> f
  lcd.print("e");
  if (isnan(h1) || isnan(t1) || isnan(h2) || isnan(t2)) {
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("SENSORFEHLER");
    lcd.setCursor(19, 3);
    lcd.print("e");
    //Serial.println("SENSORFEHLER!\n");
    // Die Sensoren sind öfters für ~1 Sekunde nicht erreichbar, aber
    // falls das Problem länger bestehen sollte: Lüfter ausschalten! 
    Luefter = false; 
    digitalWrite(relaisPin, LOW);
    delay(anzeigedauer); // Wartezeit vor neuem Versuch 
    lcd.print("f");
    //return; // was ist der Unterschied mit oder ohne?
  }
  else {
    
    ///////////////////////////////////////////////////////////////////////////////
    // Berechnung der Luftfeuchte nach http://www.wetterochs.de/wetter/feuchte.html
    // DB: g -> h
    ///////////////////////////////////////////////////////////////////////////////
    lcd.clear();
    lcd.setCursor(19, 3);
    lcd.print("g");
    //Serial.print("Berechnung der Luftfeuchte\n");
    float a1, a2 = 7.5;
    float b1, b2 = 237.3;
    // a und b für T>=0°C:
    if (t1 > 0) {
      a1 = 7.5;
      b1 = 237.3;
    }
    // a und b für T<0°C:
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
    // SDD = Sättigungsdampfdruck in hPa; DaDr = Dampfdruck in hPa;
    // TD = Taupunkttemp. in °C; AF = abs. Feuchte in g H20 pro m^3 Luft
    float SDD1 = 6.1078 * pow(10, ((a1 * t1) / (b1 + t1)));
    float DaDr1 = h1 / 100 * SDD1;
    float v1 = log10(DaDr1 / 6.1078);
    float TD1 = b1 * v1 / (a1 - v1);
    float AF1 = pow(10, 5) * 18.016 / 8314.3 * DaDr1 / (t1 + 273.15);
    float SDD2 = 6.1078 * pow(10, ((a2 * t2) / (b2 + t2)));
    float DaDr2 = h2 / 100 * SDD2;
    float v2 = log10(DaDr2 / 6.1078);
    float TD2 = b2 * v2 / (a2 - v2);
    float AF2 = pow(10, 5) * 18.016 / 8314.3 * DaDr2 / (t2 + 273.15);
    float dif_t = t1 - t2;
    float dif_h = h1 - h2;
    ////Serial.print("Sättigungsdampfdruck innen = "); //Serial.print(SDD1); //Serial.print("hPa; ");
    ////Serial.print("Sättigungsdampfdruck aussen = "); //Serial.print(SDD2); //Serial.print("hPa; ");
    ////Serial.print("Taupunkttemperatur innen = "); //Serial.print(TD1); //Serial.print("°C; ");
    ////Serial.print("Taupunkttemperatur aussen = "); //Serial.print(TD2); //Serial.print("°C; ");
    ////Serial.print("Absolute Feuchte innen = "); //Serial.print(AF1); //Serial.print("g/m³; ");
    ////Serial.print("Absolute Feuchte aussen = "); //Serial.print(AF2); //Serial.print("g/m³; ");
    ////Serial.print("Temperaturunterschied = "); //Serial.print(dif_t); //Serial.print("°C; ");
    ////Serial.print("Luftfeuchtenunterschied = "); //Serial.print(dif_t); //Serial.print("%\n");
    
    lcd.setCursor(19, 3);
    lcd.print("h");
    
    ///////////////////////////////////////////////////////////////////////
    // Lueftersteuerung // DB: i -> j
    ///////////////////////////////////////////////////////////////////////
    lcd.setCursor(19, 3);
    lcd.print("i");
    if (AF2 > AF1) { // abs. LF außen größer -> Lüfter aus
      //Serial.print("Absolute Luftfeuchte außen größer -> Lüfter aus!\n");
      Luefter = false;
      digitalWrite(relaisPin, LOW);
    }
    else { // abs. LF außen kleiner
      //Serial.print("Absolute Luftfeuchte innen größer.\n");
      Luefter = false;
      digitalWrit  e(relaisPin, LOW);
      if ( t2 > t1 ) { // aussen waermer als drinnen -> Lüfter an
        //Serial.print("Außen wärmer als innen -> Lüfter an!\n");
        Luefter = true;
        digitalWrite(relaisPin, HIGH);
      }
      else { // aussen kaelter als innen
        //Serial.print("Außen kälter als innen.\n");
        if ( t1 > TD1 ) { // Temp. im Keller über Taupunkt?
          //Serial.print("Temperatur im Keller über Taupunkt -> Lüfter an!\n");
          Luefter = true;
          digitalWrite(relaisPin, HIGH);
        }
      }
    }
    lcd.setCursor(19, 3);
    lcd.print("j");
  
    //////////////////////////////////////////////////////////////////////
    // Display // DB: k, l, m
    ///////////////////////////////////////////////////////////////////////
    // LCD Bild 1: INNEN
    //Serial.print("Schreibe LCD Bild 1: innen\n");
    lcd.clear();
    lcd.setCursor(19, 3);
    lcd.print("k");
    lcd.setCursor(7, 0); // Spalte,Zeile
    lcd.print("INNEN ");
    lcd.setCursor(0, 1);
    lcd.print(" T=");
    lcd.print(t1);
    lcd.print("C  LF=");
    lcd.print(h1);
    lcd.print("%");
    lcd.setCursor(0, 2);
    lcd.print("TP=");
    lcd.print(TD1);
    lcd.print("C  AF=");
    lcd.print(AF1);
    lcd.setCursor(0, 3);
    lcd.print("Luefter:   ");
    if (Luefter == false) {
      lcd.print("AUS");
    }
    else {
      lcd.print("AN");
    }
    delay(anzeigedauer);
    
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
    lcd.print(TD2);
    lcd.print("C  AF=");
    lcd.print(AF2);
    lcd.setCursor(0, 3);
    lcd.print("Luefter:   ");
    if (Luefter == false) {
      lcd.print("AUS");
    }
    else {
      lcd.print("AN");
    }
    delay(anzeigedauer);
    
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
    lcd.print("Luefter:   ");
    if (Luefter == false) {
      lcd.print("AUS");
    }
    else {
      lcd.print("AN");
    }
    delay(anzeigedauer);
  
    
    ///////////////////////////////////////////////////////////////
    // SD Card logging // DB: n, o, p
    //////////////////////////////////////////////////////////////
    // nur schreiben, wenn mehr als eine Minute vergangen ist:
    lcd.setCursor(19, 3);
    lcd.print("n");
    if (lastwrite != now.minute()) {
      String dataString = ""; // String für Datalogging auf SD Karte
      // Format: unixtime;t1;h1;TD1;AF1;t2;h2;TD2;AF2;Luefter
      // für korrektes Datum in LibreofficeCalc: =A1/86400+25569
      dataString += String(now.unixtime());
      dataString += ";";
      dataString += String(t1);
      dataString += ";";
      dataString += String(h1);
      dataString += ";";
      dataString += String(TD1);
      dataString += ";";
      dataString += String(AF1);
      dataString += ";";
      dataString += String(t2);
      dataString += ";";
      dataString += String(h2);
      dataString += ";";
      dataString += String(TD2);
      dataString += ";";
      dataString += String(AF2);
      dataString += ";";
      dataString += String(Luefter);
      
      ///////////////////////////////////////////////////////////////
      // Schreiben auf SD Karte
      
      lcd.setCursor(19, 3);
      lcd.print("o");
      // Datei öffnen. Achtung: es kann immer nur eine Datei geöffnet,
      // sein, also Datei am Ende immer wieder schließen!
      File dataFile = SD.open("log.txt", FILE_WRITE);
      // Schreiben, wenn die Datei verfügbar ist:
      if (dataFile) {
        dataFile.println(dataString);
        dataFile.close();
        //Serial.println("Schreibe auf SD-Karte:\n");
        //Serial.println(dataString);
        lcd.clear();
        lcd.setCursor(0, 1);
        lcd.print("Schreibe auf SD!");
        delay(anzeigedauer);
      }
      // Errormeldung, wenn nicht verfügbar:
      else {
        //Serial.println("Kann datalog.txt nicht öffnen!");
        lcd.clear();
        lcd.setCursor(0, 1);
        lcd.print("SD Karten Fehler");
        delay(anzeigedauer);
      }
      lastwrite = now.minute(); // lastwrite auf aktuellen Minutenwert setzen
      lcd.setCursor(19, 3);
      lcd.print("p");
    }
    lcd.setCursor(19, 3);
    lcd.print("q");
  }
  //Serial.print("///// END LOOP ///// \n"); 
}

// erzeugt auch RAM Probleme:
// void db(char Buchstabe) {
//   lcd.setCursor(19, 3);
//   lcd.print(buchstabe);
//   return; 
//}

