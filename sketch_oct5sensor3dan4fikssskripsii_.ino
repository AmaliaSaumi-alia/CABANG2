#define BLYNK_TEMPLATE_ID "TMPL63elZ23Za"
#define BLYNK_TEMPLATE_NAME "Pengukuran dan analisis debit air"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>

// Konfigurasi Blynk
char auth[] = "unUI6LbkExTDugMbk6eJFCuz94Mh7N_N";
char ssid[] = "Ndk Modal";
char pass[] = "12345678";

// Pin untuk Sensor Aliran Air
#define FLOW_SENSOR_PIN3 D5  // Sensor 3
#define FLOW_SENSOR_PIN4 D6  // Sensor 4

// Inisialisasi LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Alamat I2C LCD mungkin berbeda

volatile int flowPulseCount3 = 0;
volatile int flowPulseCount4 = 0;

// Koefisien kalibrasi sensor aliran
const float calibrationFactor = 7.5;  // Pulsa per liter

unsigned long previousMillis = 0; // Variabel untuk menghitung waktu
float totalDebitSensor4 = 0.0;    // Variabel untuk menyimpan total debit sensor 4
float totalBiaya = 0.0;           // Variabel untuk menyimpan total biaya

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200, 60000);  // Waktu UTC+7 (25200 detik offset)

// ISR untuk menangkap pulsa dari sensor flow
void ICACHE_RAM_ATTR flowPulseISR3() { flowPulseCount3++; }
void ICACHE_RAM_ATTR flowPulseISR4() { flowPulseCount4++; }

void setup() {
  Serial.begin(9600);
  delay(100);

  Serial.println("Mulai setup...");
  Blynk.begin(auth, ssid, pass);

  lcd.init();
  lcd.backlight();
  Serial.println("LCD diinisialisasi...");

  pinMode(FLOW_SENSOR_PIN3, INPUT);
  pinMode(FLOW_SENSOR_PIN4, INPUT);

  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN3), flowPulseISR3, RISING);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN4), flowPulseISR4, RISING);

  Serial.println("Interrupts diinisialisasi...");

  timeClient.begin();  // Mulai client NTP
}

// Fungsi untuk menghitung biaya berdasarkan debit air
float hitungBiaya(float totalDebitSensor4) {
  float biaya = 0;

  if (totalDebitSensor4 <= 10) {
    biaya = totalDebitSensor4 * 21;  // per m³
  } else if (totalDebitSensor4 <= 20) {
    biaya = (10 * 21) + ((totalDebitSensor4 - 10) * 31);
  } else if (totalDebitSensor4 <= 30) {
    biaya = (10 * 21) + (10 * 31) + ((totalDebitSensor4 - 20) * 45);
  } else {
    biaya = (10 * 21) + (10 * 31) + (10 * 45) + ((totalDebitSensor4 - 30) * 63);
  }
  return biaya;
}

// Fungsi untuk menghitung dan menampilkan status kebocoran
void tampilkanStatusKebocoran(float flowRate3, float flowRate4) {
  // Toleransi 5% untuk perbedaan debit
  float toleransi = 0.05 * flowRate3;

  // Hitung selisih debit
  float selisihDebit = abs(flowRate3 - flowRate4);
  
  String statusKebocoran = "Aman";
  String tingkatKebocoran = "Tidak ada";

  // Tentukan status kebocoran berdasarkan selisih
  if (selisihDebit > toleransi) {
    if (selisihDebit >= 0.01) {
      tingkatKebocoran = "Besar";
    } else if (selisihDebit >= 0.006) {
      tingkatKebocoran = "Sedang";
    } else if (selisihDebit >= 0.005) {
      tingkatKebocoran = "Kecil";
    }
    statusKebocoran = "Bocor";
  }

  // Tampilkan status kebocoran di Serial Monitor
  Serial.print("Status Kebocoran: ");
  Serial.println(statusKebocoran);
  Serial.print("Tingkat Kebocoran: ");
  Serial.println(tingkatKebocoran);

  // Kirim status kebocoran ke Blynk
  Blynk.virtualWrite(V3, statusKebocoran);
  Blynk.virtualWrite(V4, tingkatKebocoran);

  // Tampilkan status kebocoran di LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sts: ");
  lcd.print(statusKebocoran);
  lcd.setCursor(0, 1);
  lcd.print("Tk: ");
  lcd.print(tingkatKebocoran);
  delay(3000);
}

void loop() {
  Blynk.run();

  unsigned long currentMillis = millis();
  unsigned long interval = 1000;  // 1 detik

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Hentikan interrupt saat membaca pulsa
    noInterrupts();
    int pulseCount3 = flowPulseCount3;
    int pulseCount4 = flowPulseCount4;
    flowPulseCount3 = 0;
    flowPulseCount4 = 0;
    interrupts();

    // Hitung debit aliran air dalam m³/menit
    float flowRate3_m3min = (pulseCount3 / calibrationFactor) / 1000;  // Konversi ke m³/menit
    float flowRate4_m3min = (pulseCount4 / calibrationFactor) / 1000;  // Konversi ke m³/menit

    // Konversi debit ke liter
    float volume3_liters = pulseCount3 / calibrationFactor;
    float volume4_liters = pulseCount4 / calibrationFactor;

    // Jika ada aliran air, tambahkan ke total debit
    if (flowRate4_m3min > 0) {
      totalDebitSensor4 += flowRate4_m3min * (interval / 60000.0);  // Tambah debit ke total
    }

    // Hitung biaya berdasarkan total debit air yang terpakai
    totalBiaya = hitungBiaya(totalDebitSensor4);

    // Kirim data ke Blynk
    Blynk.virtualWrite(V0, flowRate3_m3min); // Sensor 3
    Blynk.virtualWrite(V1, flowRate4_m3min); // Sensor 4
    Blynk.virtualWrite(V2, totalBiaya);      // Biaya total

    // Tampilkan volume dalam liter dan debit dalam m³/menit di Serial Monitor
    Serial.print("Sensor 3: Debit = ");
    Serial.print(flowRate3_m3min, 3);
    Serial.println(" m³/min");
    Serial.print("Volume = ");
    Serial.print(volume3_liters);
    Serial.println(" liter");

    Serial.print("Sensor 4: Debit = ");
    Serial.print(flowRate4_m3min, 3);
    Serial.println(" m³/min");
    Serial.print("Volume = ");
    Serial.print(volume4_liters);
    Serial.println(" liter");

    Serial.print("Biaya Total: ");
    Serial.print(totalBiaya);
    Serial.println(" Rupiah");

    // Tampilkan di LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Biaya: ");
    lcd.print(totalBiaya);
    lcd.print(" Rp");
    delay(3000);

    // Tampilkan data sensor di LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("DB3: ");
    lcd.print(flowRate3_m3min, 3); // Tampilkan dalam m³
    lcd.setCursor(0, 1);
    lcd.print("DB4: ");
    lcd.print(flowRate4_m3min, 3);
    delay(3000);

    // Tampilkan status kebocoran
    tampilkanStatusKebocoran(flowRate3_m3min, flowRate4_m3min);

    // Tampilkan waktu di Serial Monitor
    timeClient.update();  // Update waktu dari NTP
    Serial.print("Waktu sekarang: ");
    Serial.print(timeClient.getHours()); Serial.print(":");
    Serial.print(timeClient.getMinutes()); Serial.print(":");
    Serial.println(timeClient.getSeconds());
  }
}
