#include <Arduino.h>
#include <FastLED.h>
#include <LedControl.h>
#include <WiFi.h>
#include <DHT.h>
#include "time.h"

const char* ssid = "your-SSID";
const char* password = "your-Passwort";

#define LED_PIN     13      
#define NUM_LEDS    111     
#define LDR_PIN     34      
#define DHTPIN      4       
#define PIRQ_PIN    27      
#define DHTTYPE     DHT11   

// MAX7219 7-Segment-Display (DIN=23, CLK=18, CS=5, 1 Modul mit 8 Ziffern)
LedControl lc = LedControl(23, 18, 5, 1);
DHT dht(DHTPIN, DHTTYPE);

CRGB leds[NUM_LEDS];
#define CLOCK_COLOR CRGB(0, 200, 255) 

unsigned long lastMotionTime = 0;
const unsigned long TIMEOUT_DURATION = 15UL * 60UL * 1000UL; // 15 Minuten
bool clockIsSleeping = false;

int lastSyncDay = -1;

struct Time12H {
  int hours;
  int minutes;
};

class TimeManager {
public:
  void configureTime() {
    // 3600 Sek (MEZ) + 3600 Sek (Sommerzeit-Offset) = Korrekte deutsche Zeit
    configTime(3600, 3600, "pool.ntp.org", "time.nist.gov");
  }

  Time12H getCurrentTime() {
    struct tm timeinfo;
    Time12H currentTime = {12, 0};
    if (getLocalTime(&timeinfo)) {
      currentTime.hours = timeinfo.tm_hour;
      currentTime.minutes = timeinfo.tm_min;
    }
    return currentTime;
  }

  void getDate(int &day, int &month, int &year) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      day = timeinfo.tm_mday;
      month = timeinfo.tm_mon + 1;
      year = timeinfo.tm_year + 1900;
    }
  }
};

TimeManager timeManager;

const int Es[] = {0, 1};
const int Ist[] = {3, 4, 5};
const int Fuenf_Min[] = {7, 8, 9, 10};       
const int Zehn_Min[] = {18, 19, 20, 21};     
const int Zwanzig[] = {11, 12, 13, 14, 15, 16, 17}; 
const int Dreiviertel[] = {22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
const int Viertel[] = {26, 27, 28, 29, 30, 31, 32};
const int Nach[] = {38, 39, 40, 41};
const int Vor[] = {35, 36, 37};
const int Halb[] = {44, 45, 46, 47};
const int Zwoelf[] = {49, 50, 51, 52, 53, 54};
const int Zwei[] = {62, 63, 64, 65};
const int Eins[] = {60, 61, 62, 63};
const int Ein[] = {61, 62, 63};
const int Sieben[] = {55, 56, 57, 58, 59, 60};
const int Drei[] = {67, 68, 69, 70};
const int Fuenf_Std[] = {73, 74, 75, 76};     
const int Elf[] = {85, 86, 87};
const int Neun[] = {81, 82, 83, 84};
const int Vier[] = {77, 78, 79, 80};
const int Acht[] = {89, 90, 91, 92};
const int Zehn_Std[] = {93, 94, 95, 96};     
const int Sechs[] = {104, 105, 106, 107, 108};
const int Uhr[] = {100, 101, 102};

const int WLAN_ANIMATION_PART1[] = {82};
const int WLAN_ANIMATION_PART2[] = {82, 73, 59, 60, 61, 69};
const int WLAN_ANIMATION_PART3[] = {82, 73, 59, 60, 61, 69, 56, 53, 35, 29, 28, 27, 26, 25, 41, 45, 64};

void lightWord(const int word[], int size, CRGB color) {
  for (int i = 0; i < size; i++) {
    if (word[i] < NUM_LEDS) {
      leds[word[i]] = color;
    }
  }
}

void syncNTP() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    FastLED.clear();
    int phase = (attempts / 2) % 3;
    if (phase == 0) lightWord(WLAN_ANIMATION_PART1, 1, CLOCK_COLOR);
    else if (phase == 1) lightWord(WLAN_ANIMATION_PART2, 6, CLOCK_COLOR);
    else lightWord(WLAN_ANIMATION_PART3, 17, CLOCK_COLOR);
    FastLED.show();
    
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWLAN verbunden, frage NTP-Zeit ab...");
    timeManager.configureTime();
    
    struct tm timeinfo;
    int ntpTries = 0;
    while (!getLocalTime(&timeinfo) && ntpTries < 20) {
      delay(500);
      ntpTries++;
    }

    if (ntpTries < 20) {
      Serial.println("NTP-Zeit erfolgreich synchronisiert!");
      FastLED.clear();
      lightWord(WLAN_ANIMATION_PART3, 17, CRGB::Green);
      FastLED.show();
      delay(1000);
    } else {
      Serial.println("NTP-Timeout! Konnte Zeit nicht laden.");
    }

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  } else {
    Serial.println("\nWLAN-Verbindung fehlgeschlagen!");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PIRQ_PIN, INPUT);

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.clear();
  FastLED.show();

  lc.shutdown(0, false);
  lc.setIntensity(0, 10);
  lc.clearDisplay(0);

  dht.begin();

  syncNTP();

  lastMotionTime = millis(); 
}

unsigned long previousMillis = 0;
bool showTemp = true; 

void loop() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    if (timeinfo.tm_hour == 12 && timeinfo.tm_min == 0 && timeinfo.tm_mday != lastSyncDay) {
      Serial.println("Es ist 12:00 Uhr mittags - Starte automatischen Zeitsync...");
      syncNTP();
      lastSyncDay = timeinfo.tm_mday; 
    }
  }

  // --- 1. Bewegung prüfen ---
  if (digitalRead(PIRQ_PIN) == HIGH) {
    lastMotionTime = millis(); 
    if (clockIsSleeping) {
      clockIsSleeping = false; 
      Serial.println("Bewegung erkannt! Uhr wacht auf.");
    }
  }

  if (millis() - lastMotionTime > TIMEOUT_DURATION) {
    clockIsSleeping = true;
  }

  if (clockIsSleeping) {
    FastLED.setBrightness(0);
    lc.setIntensity(0, 0);   
    FastLED.clear();
    FastLED.show();
    lc.clearDisplay(0);
    delay(500); 
    return; 
  }

  // --- 2. LDR Nachtabschaltung ---
  int ldrValue = analogRead(LDR_PIN);
  if (ldrValue < 300) {
    FastLED.setBrightness(0);
    lc.setIntensity(0, 0);   
    FastLED.clear();
    FastLED.show();
    lc.clearDisplay(0);
    delay(1000);
    return; 
  } else {
    FastLED.setBrightness(255); 
    lc.setIntensity(0, 10);     
  }

  // --- 3. Zeit holen und Matrix steuern ---
  Time12H currentTime = timeManager.getCurrentTime();
  int minutes = currentTime.minutes;
  int hours = currentTime.hours;

  int m5 = (minutes + 2) / 5; 
  int displayHour = hours % 12;

  if (m5 >= 5) displayHour = (displayHour + 1) % 12;
  if (displayHour == 0) displayHour = 12;

  FastLED.clear();

  lightWord(Es, 2, CLOCK_COLOR);
  lightWord(Ist, 3, CLOCK_COLOR);

  switch (m5) {
    case 0: break;
    case 1: lightWord(Fuenf_Min, 4, CLOCK_COLOR); lightWord(Nach, 4, CLOCK_COLOR); break;
    case 2: lightWord(Zehn_Min, 4, CLOCK_COLOR); lightWord(Nach, 4, CLOCK_COLOR); break;
    case 3: lightWord(Viertel, 7, CLOCK_COLOR); lightWord(Nach, 4, CLOCK_COLOR); break;
    case 4: lightWord(Zwanzig, 7, CLOCK_COLOR); lightWord(Nach, 4, CLOCK_COLOR); break;
    case 5: lightWord(Fuenf_Min, 4, CLOCK_COLOR); lightWord(Vor, 3, CLOCK_COLOR); lightWord(Halb, 4, CLOCK_COLOR); break;
    case 6: lightWord(Halb, 4, CLOCK_COLOR); break;
    case 7: lightWord(Fuenf_Min, 4, CLOCK_COLOR); lightWord(Nach, 4, CLOCK_COLOR); lightWord(Halb, 4, CLOCK_COLOR); break;
    case 8: lightWord(Zehn_Min, 4, CLOCK_COLOR); lightWord(Vor, 3, CLOCK_COLOR); lightWord(Halb, 4, CLOCK_COLOR); break;
    case 9: lightWord(Viertel, 7, CLOCK_COLOR); lightWord(Vor, 3, CLOCK_COLOR); break;
    case 10: lightWord(Zehn_Min, 4, CLOCK_COLOR); lightWord(Vor, 3, CLOCK_COLOR); break;
    case 11: lightWord(Fuenf_Min, 4, CLOCK_COLOR); lightWord(Vor, 3, CLOCK_COLOR); break;
  }

  if (m5 == 0) {
    if (displayHour == 1) lightWord(Ein, 3, CLOCK_COLOR);
    else if (displayHour == 2) lightWord(Zwei, 4, CLOCK_COLOR);
    else if (displayHour == 3) lightWord(Drei, 4, CLOCK_COLOR);
    else if (displayHour == 4) lightWord(Vier, 4, CLOCK_COLOR);
    else if (displayHour == 5) lightWord(Fuenf_Std, 4, CLOCK_COLOR);
    else if (displayHour == 6) lightWord(Sechs, 5, CLOCK_COLOR);
    else if (displayHour == 7) lightWord(Sieben, 6, CLOCK_COLOR);
    else if (displayHour == 8) lightWord(Acht, 4, CLOCK_COLOR);
    else if (displayHour == 9) lightWord(Neun, 4, CLOCK_COLOR);
    else if (displayHour == 10) lightWord(Zehn_Std, 4, CLOCK_COLOR);
    else if (displayHour == 11) lightWord(Elf, 3, CLOCK_COLOR);
    else if (displayHour == 12) lightWord(Zwoelf, 6, CLOCK_COLOR);

    lightWord(Uhr, 3, CLOCK_COLOR); 
  } else {
    if (displayHour == 1) lightWord(Eins, 4, CLOCK_COLOR);
    else if (displayHour == 2) lightWord(Zwei, 4, CLOCK_COLOR);
    else if (displayHour == 3) lightWord(Drei, 4, CLOCK_COLOR);
    else if (displayHour == 4) lightWord(Vier, 4, CLOCK_COLOR);
    else if (displayHour == 5) lightWord(Fuenf_Std, 4, CLOCK_COLOR);
    else if (displayHour == 6) lightWord(Sechs, 5, CLOCK_COLOR);
    else if (displayHour == 7) lightWord(Sieben, 6, CLOCK_COLOR);
    else if (displayHour == 8) lightWord(Acht, 4, CLOCK_COLOR);
    else if (displayHour == 9) lightWord(Neun, 4, CLOCK_COLOR);
    else if (displayHour == 10) lightWord(Zehn_Std, 4, CLOCK_COLOR);
    else if (displayHour == 11) lightWord(Elf, 3, CLOCK_COLOR);
    else if (displayHour == 12) lightWord(Zwoelf, 6, CLOCK_COLOR);
  }

  FastLED.show();

  // --- 4. 7-Segment-Display Wechsel ---
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= 5000) { 
    previousMillis = currentMillis;
    showTemp = !showTemp; 
  }

  lc.clearDisplay(0);
  if (showTemp) {
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    
    int tInt = (int)temp; 
    int hInt = (int)hum;

    lc.setDigit(0, 7, tInt / 10, false);
    lc.setDigit(0, 6, tInt % 10, true);   
    lc.setChar(0,  5, 'C', false);          
    lc.setRow(0, 4, B00110000); // Sauberes 'I'

    lc.setDigit(0, 2, hInt / 10, false);
    lc.setDigit(0, 1, hInt % 10, false);
    lc.setChar(0,  0, 'F', false);          
  } else {
    int d, m, y;
    timeManager.getDate(d, m, y);

    lc.setDigit(0, 7, d / 10, false);
    lc.setDigit(0, 6, d % 10, true);   
    lc.setDigit(0, 5, m / 10, false);
    lc.setDigit(0, 4, m % 10, true);   
    lc.setDigit(0, 3, (y / 1000) % 10, false); 
    lc.setDigit(0, 2, (y / 100) % 10, false);  
    lc.setDigit(0, 1, (y / 10) % 10, false);   
    lc.setDigit(0, 0, y % 10, false);          
  }

  delay(1000);
}