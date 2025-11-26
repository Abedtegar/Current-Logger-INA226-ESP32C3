#include "INA226.h"
#include <Arduino.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <Wire.h>

INA226 INA(0x40);

// BLE UUIDs
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
bool restartRequested = false;

// put function declarations here:
#define Buzz 2
#define Led 1
#define Alert 10
#define SDA_PIN 8
#define SCL_PIN 9
#define volt_threshold 15.5
#define curr_threshold 6.0
unsigned long previousMillis = 0;
const long interval = 200;
bool ledState = LOW;
/* KALIBRASI INA226 MENGGUNAKAN setMaxCurrentShunt()
 * Metode ini lebih sederhana - library otomatis menghitung parameter optimal
 * Parameter:
 * 1. maxCurrent - Maksimal arus yang diharapkan dalam Ampere
 * 2. shunt - Resistansi shunt dalam Ohm
 * 3. normalized - true = LSB dinormalisasi, false = LSB presisi maksimal
 */

/* USER SET VALUES - Sesuaikan dengan setup Anda */
float maxCurrent = 20;                 // Maksimal arus yang diukur dalam Ampere
float shunt = 0.003977142857142857142; // Shunt resistance dalam Ohm
bool normalized = true; // true = LSB dinormalisasi untuk kompatibilitas
float bus_voltage_offset =
    0.0; // Offset tegangan bus dalam Volt (untuk kalibrasi)
float bus_voltage_multiplier = 1.0;
float g_bus_voltage_offset = 0.0;
float g_bus_voltage_multiplier = 1.0;

void readINA226();
void initialout();
void calibrateINA226();
void initBLE();
String timestamp();
void blinkLed();

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    Serial.println("BLE Client Connected");
  };
  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    Serial.println("BLE Client Disconnected");
    restartRequested = true; // request restart to allow fresh pairing
  }
};

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println(__FILE__);
  Serial.print("INA226_LIB_VERSION: ");
  Serial.println(INA226_LIB_VERSION);
  Serial.println();

  Wire.begin(SDA_PIN, SCL_PIN);
  if (!INA.begin()) {
    Serial.println("could not connect. Fix and Reboot");
  }

  initBLE();

  
      1.0; // Multiplier tegangan bus (untuk kalibrasi)

  Serial.println("\n***** INA226 KALIBRASI *****");
  int result = INA.setMaxCurrentShunt(maxCurrent, shunt, normalized);

  if (result == 0) {
    Serial.println("Kalibrasi arus berhasil!");
  } else {
    Serial.print("Kalibrasi arus gagal! Error code: ");
    Serial.println(result);
  }

  Serial.print("Shunt:\t\t");
  Serial.print(INA.getShunt(), 4);
  Serial.println(" Ohm");
  Serial.print("Current LSB:\t");
  Serial.print(INA.getCurrentLSB(), 10);
  Serial.println(" A/bit");
  Serial.print("Current LSB:\t");
  Serial.print(INA.getCurrentLSB_uA(), 3);
  Serial.println(" uA/bit");
  Serial.print("Max Current:\t");
  Serial.print(INA.getMaxCurrent(), 3);
  Serial.println(" A");
  Serial.print("Normalized:\t");
  Serial.println(normalized ? "true" : "false");
  Serial.print("Bus V Offset:\t");
  Serial.print(bus_voltage_offset, 3);
  Serial.println(" V");
  Serial.print("Bus V Multi:\t");
  Serial.println(bus_voltage_multiplier, 3);
  Serial.println();

  // Simpan parameter kalibrasi tegangan ke variabel global
  g_bus_voltage_offset = bus_voltage_offset;
  g_bus_voltage_multiplier = bus_voltage_multiplier;

  initialout();
  calibrateINA226();
}

void loop() {
  readINA226();

  if (INA.getBusVoltage() < volt_threshold) {
    blinkLed();
  }else 
  // BLE connection handling: restart advertising when client disconnects
  if (!deviceConnected && oldDeviceConnected) {
    // brief pause to allow stack cleanup
    delay(500);
    if (restartRequested) {
      Serial.println("Restarting BLE advertising to allow re-pairing...");
      // stop advertising if running
      if (BLEDevice::getAdvertising()) {
        BLEDevice::getAdvertising()->stop();
      }
      delay(200);
      BLEDevice::startAdvertising();
      Serial.println("BLE advertising restarted");
      restartRequested = false;
    } else {
      // simple advertising restart
      BLEDevice::startAdvertising();
      Serial.println("BLE advertising restarted");
    }
    oldDeviceConnected = deviceConnected;
  }

  // detect new connection
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
}

String timestamp() {
  unsigned long ms = millis();
  unsigned long micro = ms % 1000;
  unsigned long seconds = ms / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  seconds = seconds % 60;
  minutes = minutes % 60;
  char buffer[20];
  sprintf(buffer, "%02lu:%02lu:%02lu", hours, minutes, seconds);
  return String(buffer);
}

void readINA226() {
  // Serial.print("\nTIME\t\tBUS\tSHUNT\tCURRENT\tPOWER");

  // Baca tegangan bus dan terapkan kalibrasi
  float busVoltage = INA.getBusVoltage();
  float calibratedBusVoltage =
      (busVoltage * g_bus_voltage_multiplier) + g_bus_voltage_offset;

  String data = String(timestamp()) + "," + String(calibratedBusVoltage, 3) +
                "," + String(INA.getShuntVoltage_mV(), 3) + "," +
                String(INA.getCurrent_mA(), 3) + "," +
                String(INA.getPower_mW(), 3);

  Serial.println(data);

  if (deviceConnected) {
    pCharacteristic->setValue(data.c_str());
    pCharacteristic->notify();
  }
  delay(10);
}

void calibrateINA226() {
  Serial.println("***** VERIFIKASI KALIBRASI *****");
  Serial.println("Mengambil 10 sample untuk verifikasi...\n");

  float bv = 0, cu = 0;
  for (int i = 0; i < 10; i++) {
    bv += INA.getBusVoltage();
    cu += INA.getCurrent_mA();
    delay(150);
  }
  bv /= 10;
  cu /= 10;

  Serial.println("Rata-rata 10 pembacaan:");
  Serial.print("Bus Voltage:\t");
  Serial.print(bv, 3);
  Serial.println(" V");
  Serial.print("Current:\t");
  Serial.print(cu, 3);
  Serial.println(" mA");

  Serial.println("\n***** PANDUAN KALIBRASI MANUAL *****");
  Serial.println("Jika hasil tidak akurat, lakukan kalibrasi manual:");

  Serial.println("\n1. KALIBRASI SHUNT RESISTANCE:");
  Serial.println("   - Hubungkan beban ~50-100mA");
  Serial.println("   - Ukur arus dengan multimeter (DMM)");
  Serial.print("   - Hitung: shunt_baru = ");
  Serial.print(INA.getShunt(), 4);
  Serial.print(" * ");
  Serial.print(cu, 3);
  Serial.println(" / (Arus_DMM_dalam_mA)");
  Serial.println("   - Update nilai shunt di setup()");

  if (cu < 40) {
    Serial.println("\n   ⚠️  PERINGATAN: Arus terlalu rendah (<50mA)");
    Serial.println(
        "       Gunakan beban yang lebih besar untuk kalibrasi shunt!");
  }

  Serial.println("\n2. KALIBRASI BUS VOLTAGE:");
  Serial.println("   - Ukur tegangan bus dengan multimeter (DMM)");
  Serial.print("   - Tegangan INA226 terbaca: ");
  Serial.print(bv, 3);
  Serial.println(" V");
  Serial.println("   - Hitung faktor kalibrasi:");
  Serial.print("     bus_voltage_multiplier = Tegangan_DMM / ");
  Serial.println(bv, 3);
  Serial.println("   - Contoh: Jika DMM=5.12V, maka multiplier = 5.12 / " +
                 String(bv, 3));
  Serial.println("   - Update nilai bus_voltage_multiplier di setup()");

  Serial.println("\n3. KALIBRASI MAX CURRENT:");
  Serial.println(
      "   - Jika mengukur arus >1A, tingkatkan maxCurrent di setup()");
  Serial.println("   - Contoh: maxCurrent = 3.0 untuk pengukuran sampai 3A");

  Serial.println("\n***** MULAI PENGUKURAN REALTIME *****");
  Serial.println("TIME\t\tBUS(V)\tSHUNT(mV) CURRENT(mA) POWER(mW)");
  delay(1000);
}

void initialout() {
  pinMode(Buzz, OUTPUT);
  pinMode(Led, OUTPUT);
  pinMode(Alert, INPUT);

  digitalWrite(Buzz, LOW);
  digitalWrite(Led, LOW);
  delay(500);
  digitalWrite(Buzz, HIGH);
  digitalWrite(Led, HIGH);
  delay(200);
  digitalWrite(Buzz, LOW);
  digitalWrite(Led, LOW);
  delay(200);
  digitalWrite(Buzz, HIGH);
  digitalWrite(Led, HIGH);
  delay(200);
  digitalWrite(Buzz, LOW);
  digitalWrite(Led, LOW);
  delay(200);
}

void initBLE() {
  BLEDevice::init("Current_Logger_ESP32C3");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setValue("INA226 Data");
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();
  Serial.println("BLE Initialized. Waiting for connections...");
}

// Global kalibrasi tegangan



void blinkLed() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    ledState = !ledState;
    digitalWrite(Led, ledState);
    digitalWrite(Buzz, ledState);
  }
}
