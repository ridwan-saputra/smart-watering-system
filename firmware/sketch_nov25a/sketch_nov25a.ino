#define BLYNK_PRINT Serial

// --- LIBRARIES ---
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>

// --- KONFIGURASI WIFI ---
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// --- KONFIGURASI PIN ---
const int PIN_SOIL  = 34;
const int PIN_DHT   = 21;
const int PIN_PUMP  = 18;

// --- VIRTUAL PINS BLYNK ---
#define VPIN_SOIL_DISPLAY V0
#define VPIN_TEMP_DISPLAY V1
#define VPIN_PUMP_BUTTON  V3
#define VPIN_MODE_BUTTON  V4

// --- KONFIGURASI LOGIKA ---
#define DHT_TYPE DHT22
const int LIMIT_DRY = 60; // Batas kering (Nyalakan pompa)
const int LIMIT_WET = 70; // Batas basah (Matikan pompa)
const long RECONNECT_INTERVAL = 10000; // 10 Detik

// --- OBJECTS ---
DHT dht(PIN_DHT, DHT_TYPE);
BlynkTimer timer;

// --- GLOBAL VARIABLES (STATE) ---
int soilMoisture = 0;
float temperature = 0.0;
bool isAutoMode = true;
unsigned long lastConnectionAttempt = 0;

// ==========================================
// 1. FUNGSI HARDWARE (BACA & KONTROL FISIK)
// ==========================================

void readSensors() {
  // Baca Soil Moisture
  int rawValue = analogRead(PIN_SOIL);
  soilMoisture = map(rawValue, 4095, 0, 0, 100);
  soilMoisture = constrain(soilMoisture, 0, 100);

  // Baca Suhu
  temperature = dht.readTemperature();
}

void setPumpState(bool turnOn) {
  // Cek status fisik saat ini agar tidak write berulang-ulang
  bool currentStatus = digitalRead(PIN_PUMP);
  
  if (turnOn && !currentStatus) {
    digitalWrite(PIN_PUMP, HIGH);
    Serial.println(">> POMPA: MENYALA (ON)");
    if (Blynk.connected()) Blynk.virtualWrite(VPIN_PUMP_BUTTON, 1);
  } 
  else if (!turnOn && currentStatus) {
    digitalWrite(PIN_PUMP, LOW);
    Serial.println(">> POMPA: MATI (OFF)");
    if (Blynk.connected()) Blynk.virtualWrite(VPIN_PUMP_BUTTON, 0);
  }
}

// ==========================================
// 2. FUNGSI LOGIKA (OTOMATISASI)
// ==========================================

void runAutoLogic() {
  if (!isAutoMode) return; // Jika manual, keluar dari fungsi ini

  if (soilMoisture < LIMIT_DRY) {
    Serial.println("[LOGIC] Tanah Kering -> Perintah ON");
    setPumpState(true);
  } 
  else if (soilMoisture > LIMIT_WET) {
    Serial.println("[LOGIC] Tanah Basah -> Perintah OFF");
    setPumpState(false);
  }
}

// ==========================================
// 3. FUNGSI KOMUNIKASI (WIFI & BLYNK)
// ==========================================

void sendDataToCloud() {
  if (Blynk.connected()) {
    Blynk.virtualWrite(VPIN_SOIL_DISPLAY, soilMoisture);
    Blynk.virtualWrite(VPIN_TEMP_DISPLAY, temperature);
  } else {
    Serial.printf("[OFFLINE] Soil: %d%% | Temp: %.1fC\n", soilMoisture, temperature);
  }
}

// Fungsi gabungan yang dipanggil Timer setiap 2 detik
void mainTask() {
  readSensors();     // 1. Baca sensor dulu
  runAutoLogic();    // 2. Tentukan nasib pompa
  sendDataToCloud(); // 3. Lapor ke internet (kalau ada)
}

void handleConnection() {
  // Jika WiFi putus, coba reconnect
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastConnectionAttempt >= RECONNECT_INTERVAL) {
      Serial.println("WiFi Putus! Mencoba reconnect WiFi...");
      WiFi.disconnect();
      WiFi.reconnect();
      lastConnectionAttempt = millis();
    }
    return; // Keluar, jangan lanjut cek Blynk dulu
  }

  // Jika WiFi aman tapi Blynk putus
  if (!Blynk.connected()) {
    if (millis() - lastConnectionAttempt >= RECONNECT_INTERVAL) {
      Serial.println("Mencoba konek ke Blynk...");
      if (Blynk.connect()) {
        Serial.println("Blynk Terhubung Kembali!");
      }
      lastConnectionAttempt = millis();
    }
  } else {
    // Jika semua aman, jalankan rutin Blynk
    Blynk.run();
  }
}

void updateAppUI() {
  // Update tampilan tombol di Aplikasi berdasarkan Mode
  if (isAutoMode) {
    Blynk.setProperty(VPIN_PUMP_BUTTON, "isDisabled", true);
    Blynk.setProperty(VPIN_PUMP_BUTTON, "color", "#7f8c8d"); // Abu-abu
    Blynk.setProperty(VPIN_PUMP_BUTTON, "label", "AUTO (Terkunci)");
    Serial.println("Mode: OTOMATIS");
  } else {
    Blynk.setProperty(VPIN_PUMP_BUTTON, "isDisabled", false);
    Blynk.setProperty(VPIN_PUMP_BUTTON, "color", "#23C48E"); // Hijau
    Blynk.setProperty(VPIN_PUMP_BUTTON, "label", "MANUAL POMPA");
    Serial.println("Mode: MANUAL");
  }
}

// ==========================================
// 4. BLYNK CALLBACKS (INPUT DARI HP)
// ==========================================

BLYNK_CONNECTED() {
  Blynk.syncVirtual(VPIN_MODE_BUTTON);
  Blynk.syncVirtual(VPIN_PUMP_BUTTON);
}

BLYNK_WRITE(VPIN_MODE_BUTTON) {
  isAutoMode = (param.asInt() == 1);
  updateAppUI();
}

BLYNK_WRITE(VPIN_PUMP_BUTTON) {
  if (isAutoMode) return; // Abaikan jika mode Auto

  int manualState = param.asInt();
  if (manualState == 1) {
    Serial.println("[MANUAL] Tombol ditekan -> ON");
    setPumpState(true);
  } else {
    Serial.println("[MANUAL] Tombol dilepas -> OFF");
    setPumpState(false);
  }
}

// ==========================================
// 5. MAIN SETUP & LOOP
// ==========================================

void setup() {
  Serial.begin(9600);
  
  // Setup Hardware
  pinMode(PIN_PUMP, OUTPUT);
  digitalWrite(PIN_PUMP, LOW);
  dht.begin();

  // Setup Koneksi (Non-Blocking)
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Blynk.config(BLYNK_AUTH_TOKEN);

  // Setup Timer
  timer.setInterval(2000L, mainTask);
  
  Serial.println("--- SISTEM SMART WATERING DIMULAI ---");
}

void loop() {
  timer.run();        // Menjalankan tugas sensor & logika (Priority 1)
  handleConnection(); // Mengurus koneksi internet (Priority 2)
}