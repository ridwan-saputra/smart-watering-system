
#define BLYNK_PRINT Serial

// --- LIBRARIES ---
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>

// --- KONFIGURASI WIFI ---
const char* ssid = "";
const char* password = "";

// --- KONFIGURASI PIN ---
const int PIN_SOIL       = 34;
const int PIN_DHT        = 23;
const int PIN_PUMP       = 18;
const int PIN_BUZZER     = 5;
const int PIN_MASTER_BTN = 19;  // <--- PIN BARU: Tombol Fisik (Sambung ke GND)

// --- VIRTUAL PINS BLYNK ---
#define VPIN_SOIL_DISPLAY V0
#define VPIN_TEMP_DISPLAY V1
#define VPIN_PUMP_BUTTON  V3
#define VPIN_MODE_BUTTON  V4

// --- KONFIGURASI LOGIKA ---
#define DHT_TYPE DHT22
const int LIMIT_DRY = 60; 
const int LIMIT_WET = 70; 
const long RECONNECT_INTERVAL = 10000; 

// --- OBJECTS ---
DHT dht(PIN_DHT, DHT_TYPE);
BlynkTimer timer;

// --- GLOBAL VARIABLES (STATE) ---
int soilMoisture = 0;
float temperature = 0.0;
bool isAutoMode = true;
unsigned long lastConnectionAttempt = 0;

// STATUS UTAMA SISTEM (Default False = Mati saat dicolok)
bool isSystemOn = false; 

// ==========================================
// 0. FUNGSI UTILITY (BUZZER)
// ==========================================
void beep(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(100); 
    digitalWrite(PIN_BUZZER, LOW);
    delay(100);
  }
}

// ==========================================
// 1. FUNGSI HARDWARE & LOGIKA
// ==========================================

void readSensors() {
  int rawValue = analogRead(PIN_SOIL);
  soilMoisture = map(rawValue, 4095, 0, 0, 100);
  soilMoisture = constrain(soilMoisture, 0, 100);
  temperature = dht.readTemperature();
}

void setPumpState(bool turnOn) {
  // PENGAMAN: Jika sistem OFF, pompa DILARANG nyala
  if (!isSystemOn) {
    digitalWrite(PIN_PUMP, LOW);
    digitalWrite(PIN_BUZZER, LOW);
    return;
  }

  bool currentStatus = digitalRead(PIN_PUMP);
  
  if (turnOn && !currentStatus) {
    digitalWrite(PIN_PUMP, HIGH);
    digitalWrite(PIN_BUZZER, HIGH); 
    Serial.println(">> POMPA: MENYALA (ON) + BUZZER ON");
    if (Blynk.connected()) Blynk.virtualWrite(VPIN_PUMP_BUTTON, 1);
  } 
  else if (!turnOn && currentStatus) {
    digitalWrite(PIN_PUMP, LOW);
    digitalWrite(PIN_BUZZER, LOW);  
    Serial.println(">> POMPA: MATI (OFF) + BUZZER OFF");
    if (Blynk.connected()) Blynk.virtualWrite(VPIN_PUMP_BUTTON, 0);
  }
}

void runAutoLogic() {
  if (!isAutoMode) return; 

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
// 2. FUNGSI BARU: MASTER BUTTON
// ==========================================
void checkMasterButton() {
  // Baca tombol (Active LOW karena INPUT_PULLUP)
  // Jika ditekan, nilainya LOW (0)
  static int lastState = HIGH;
  int currentState = digitalRead(PIN_MASTER_BTN);

  if (lastState == HIGH && currentState == LOW) {
    // Tombol baru saja ditekan
    delay(50); // Debounce sederhana (tunggu getaran tombol hilang)
    
    if (digitalRead(PIN_MASTER_BTN) == LOW) {
      // Toggle status (ON jadi OFF, OFF jadi ON)
      isSystemOn = !isSystemOn;

      if (isSystemOn) {
        Serial.println("\n>>> SYSTEM ENABLED (SIAP KERJA) <<<");
        beep(1); // Bunyi panjang 1x tanda AKTIF
      } else {
        Serial.println("\n>>> SYSTEM DISABLED (STANDBY) <<<");
        
        // MATIKAN PAKSA SEMUA SAAT OFF
        digitalWrite(PIN_PUMP, LOW);
        digitalWrite(PIN_BUZZER, LOW);
        if (Blynk.connected()) Blynk.virtualWrite(VPIN_PUMP_BUTTON, 0);
        
        beep(3); // Bunyi pendek 3x tanda MATI
      }
    }
  }
  lastState = currentState;
}

// ==========================================
// 3. FUNGSI UTAMA (MAIN TASK)
// ==========================================

void sendDataToCloud() {
  // Kirim data hanya jika terhubung
  if (Blynk.connected()) {
    Blynk.virtualWrite(VPIN_SOIL_DISPLAY, soilMoisture);
    Blynk.virtualWrite(VPIN_TEMP_DISPLAY, temperature);
  }
}

void mainTask() {
  // JIKA SYSTEM OFF, TIDAK LAKUKAN APAPUN
  if (!isSystemOn) {
    Serial.println("[STANDBY] Menunggu Tombol Master ditekan...");
    return;
  }

  readSensors();     
  runAutoLogic();    
  sendDataToCloud(); 
}

void handleConnection() {
  // Tetap izinkan WiFi connect di background agar siap dipakai
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastConnectionAttempt >= RECONNECT_INTERVAL) {
      Serial.println("WiFi Putus! Reconnecting...");
      WiFi.disconnect();
      WiFi.reconnect();
      lastConnectionAttempt = millis();
    }
    return; 
  }

  if (!Blynk.connected()) {
    if (millis() - lastConnectionAttempt >= RECONNECT_INTERVAL) {
      Serial.println("Reconnecting Blynk...");
      if (Blynk.connect()) {
        Serial.println("Blynk Connected!");
        beep(2);
        // Jangan beep disini agar tidak berisik saat standby
      }
      lastConnectionAttempt = millis();
    }
  } else {
    Blynk.run();
  }
}

void updateAppUI() {
  if (isAutoMode) {
    Blynk.setProperty(VPIN_PUMP_BUTTON, "isDisabled", true);
    Blynk.setProperty(VPIN_PUMP_BUTTON, "color", "#7f8c8d"); 
    Blynk.setProperty(VPIN_PUMP_BUTTON, "label", "AUTO (Terkunci)");
  } else {
    Blynk.setProperty(VPIN_PUMP_BUTTON, "isDisabled", false);
    Blynk.setProperty(VPIN_PUMP_BUTTON, "color", "#23C48E"); 
    Blynk.setProperty(VPIN_PUMP_BUTTON, "label", "MANUAL POMPA");
  }
}

// ==========================================
// 4. BLYNK CALLBACKS
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
  if (!isSystemOn) return; // Cegah kontrol Blynk jika sistem OFF
  if (isAutoMode) return; 

  int manualState = param.asInt();
  setPumpState(manualState == 1);
}

// ==========================================
// 5. MAIN SETUP & LOOP
// ==========================================

void setup() {
  Serial.begin(9600);
  
  // Setup Hardware
  pinMode(PIN_PUMP, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_MASTER_BTN, INPUT_PULLUP); // PENTING: PULLUP AKTIF
  
  digitalWrite(PIN_PUMP, LOW);
  digitalWrite(PIN_BUZZER, LOW);
  
  dht.begin();

  Serial.println("--- SYSTEM BOOTING (STANDBY MODE) ---");
  Serial.println("Tekan tombol fisik di PIN 5 untuk memulai.");
  
  // Kita HAPUS beep awal, supaya benar-benar hening saat dicolok
  // beep(1); 

  // Setup Koneksi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Blynk.config(BLYNK_AUTH_TOKEN);

  timer.setInterval(2000L, mainTask);
}

void loop() {
  // 1. Cek tombol fisik terus menerus
  checkMasterButton();

  // 2. Jalankan timer hanya jika timer butuh jalan
  // (Timer di dalam mainTask sudah kita kasih "if (!isSystemOn) return")
  timer.run();        
  
  // 3. Handle Koneksi WiFi/Blynk
  // Kita biarkan WiFi tetap konek di background supaya saat tombol ditekan, 
  // dia sudah online dan tidak perlu nunggu connecting lagi.
  handleConnection(); 
}