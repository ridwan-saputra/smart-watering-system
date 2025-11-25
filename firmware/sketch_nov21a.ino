/*
 * Project: Smart Watering System (Soil + DHT22)
 * Author: Ridwan Saputra
 * Description: Sistem penyiraman otomatis dengan monitoring
   Kelembapan tanah, Suhu dan Kelembapan Udara
 */

// Pin Mapping
// #define PIN_SOIL_SENSOR 34
// #define PIN_DHT 18
// #define PIN_RELAY_PUMP 19

// PIN SIMULASI PROTEUS
#define PIN_SOIL_SENSOR A0
#define PIN_DHT 3
#define PIN_RELAY_PUMP 2


// Konfigurasi Sensor
#define DHTTYPE DHT22
const int DRY_THRESHOLD = 60;
const int WET_THRESHOLD = 70;

// Libraries
#include <DHT.h>

// Global Object
DHT dht(PIN_DHT, DHTTYPE);

// Soil Moisture Sensor and Logic
void soilMoistureSensor(){
  // Soil Moisture
  int rawValue = analogRead(PIN_SOIL_SENSOR);
  
  // int soilMoisture = map(rawValue, 4095, 0, 0, 100);
  // soilMoisture = constrain(soilMoisture, 0, 100);

  // SENSOR SIMULASI PROTEUS
  int soilMoisture = map(rawValue, 1023, 0, 0, 100);

  Serial.print("Kelembapan Tanah: ");
  Serial.print(soilMoisture);
  Serial.println("%");

  // Logic
  if (soilMoisture < DRY_THRESHOLD){
    digitalWrite(PIN_RELAY_PUMP, HIGH);
  }
  else if (soilMoisture > WET_THRESHOLD){
    digitalWrite(PIN_RELAY_PUMP, LOW);
  }
}

// DHT22 Sensor
void DHTsensor(){
  int temperature = dht.readTemperature();
  int humidity = dht.readHumidity();
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Gagal membaca sensor DHT!");
    return; // Keluar dari fungsi agar tidak mengirim data sampah
  }
  Serial.print("Kelembapan Udara: ");
  Serial.print(humidity);
  Serial.println("%");
  Serial.print("Suhu: ");
  Serial.print(temperature);
  Serial.println("°C");
}

// Start Setup
void setup(){
  Serial.begin(9600);

  pinMode(PIN_RELAY_PUMP, OUTPUT);
  digitalWrite(PIN_RELAY_PUMP, LOW);

  dht.begin();

}

// Main Program
void loop(){
  soilMoistureSensor();
  DHTsensor();
  delay(500);
}