#include <LiquidCrystal_I2C.h>  // Import Library LCD I2C
#include "DHT.h"                // Import Library Sensor DHT
#include <ESP8266WiFi.h>        // Import library mikrokontroler esp8266
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <MQ135.h>        // Import library sensor mq2
#include <PZEM004Tv30.h>  // Import Library sensor PZEM

// Konfigurasi SSID dan Password yang digunakan
#define WIFI_SSID "Iskandar 5 Atas"
#define WIFI_PASSWORD "setiawan"

// konfigurasi Broker dan Port yang digunakan
#define MQTT_HOST "test.mosquitto.org"
#define MQTT_PORT 1883

// Konfigurasi Topik MQTT
#define MQTT_PUB_TEMP "esp/dht/temperature/19102138"
#define MQTT_PUB_SMOKE "esp/mq2/smoke/19102138"
#define MQTT_PUB_ENERGY "esp/pzem/energy/19102138"
#define MQTT_PUB_PZEM_VOLTAGE "esp/pzem/voltage/19102138"
#define MQTT_PUB_PZEM_CURRENT "esp/pzem/current/19102138"
#define MQTT_PUB_PZEM_POWER "esp/pzem/power/19102138"
#define MQTT_PUB_STATUS "esp/alarm/status/19102138"

// Inisiasi pin digital dan analog yang digunakan
#define DHTPIN D3      // Inisiasi sensor DHT ke pin D3
#define DHTTYPE DHT22  // Inisasi Type Sensor DHT22
DHT dht(DHTPIN, DHTTYPE);
MQ135 mq135(A0);                     // Inisiasi Sensor MQ2 Asap
PZEM004Tv30 pzem(D7, D6);            // Inisiasi  Sensor PZEM RX, TX
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Inisiasi LCD I2C 16x2
#define BUZZERPIN 2                  // Inisiasi Buzzer Pada Pin D4

// Deklarasi variabel untuk pembacaan sensor
float temp, smoke, voltage, current, power, energy;
int counter = 0;

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

unsigned long previousMillis = 0;  // Mendifinisikan variabel yang mengatur niali 0 ke broker
const long interval = 5000;     // Mendefinisikan interval waktu untuk pengiriman data sensor.

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach();  // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttPublish(uint16_t packetId) {
  Serial.print("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  dht.begin();

  lcd.begin();
  lcd.setCursor(0, 0);
  lcd.print("Tugas Akhir");
  lcd.setCursor(0, 1);
  lcd.print("Monitoring Daya");

  pinMode(BUZZERPIN, OUTPUT);

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  //mqttClient.onSubscribe(onMqttSubscribe);
  //mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  // If your broker requires authentication (username and password), set them below
  //mqttClient.setCredentials("REPlACE_WITH_YOUR_USER", "REPLACE_WITH_YOUR_PASSWORD");
  connectToWifi();
}

void loop() {
  unsigned long currentMillis = millis();

  temp = dht.readTemperature();
  smoke = mq135.getPPM();
  voltage = pzem.voltage();
  current = pzem.current();
  power = pzem.power();    // Pemanggilan sensor PZEM parameter Watt
  energy = pzem.energy();  // Pemanggilan sensor PZEM parameter kwh

  if (currentMillis - previousMillis >= 6000) {
    //  Menampilkan data sensor di LCD I2C
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(temp);

    lcd.setCursor(9, 0);
    lcd.print("W:");
    lcd.print(power);

    lcd.setCursor(0, 1);
    lcd.print("V:");
    lcd.print(voltage);

    lcd.setCursor(9, 1);
    lcd.print("A:");
    lcd.print(current);
    delay(5000);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MQ2:");
    lcd.print(smoke);

    String psn;
    // Kondisi pada modul buzzer
    if (smoke > 200) {
      psn = "Ada Asap";
      digitalWrite(BUZZERPIN, HIGH);
      lcd.setCursor(0, 1);
      lcd.print("Ada Asap");
      uint16_t packetIdPub1 = mqttClient.publish(MQTT_PUB_STATUS, 1, true, psn.c_str());
      delay(5000);
    } else if (power > 300) {
      psn = "Overload";
      digitalWrite(BUZZERPIN, HIGH);
      lcd.setCursor(0, 9);
      lcd.print("Overload");
      uint16_t packetIdPub2 = mqttClient.publish(MQTT_PUB_STATUS, 1, true, psn.c_str());
      delay(5000);
    } else {
      psn = "Aman";
      digitalWrite(BUZZERPIN, LOW);
      lcd.setCursor(0, 1);
      lcd.print("Aman");
      uint16_t packetIdPub3 = mqttClient.publish(MQTT_PUB_STATUS, 1, true, psn.c_str());
      delay(5000);
    }
  }

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;  // Simpan waktu sekarang

    int hasil = counter++ - 1;
    float lastKwh = energy * hasil;
    Serial.println(lastKwh);
    Serial.println(hasil);
    delay(5000);

    //  Menampilkan data KWH
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("KWH Count => ");
    lcd.print(lastKwh);
    delay(5000);

    // Publish ke MQTT message dengan topik esp/dht/temperature
    uint16_t packetIdPub4 = mqttClient.publish(MQTT_PUB_TEMP, 1, true, String(temp).c_str());
    Serial.printf("Publishing on topic %s at QoS 1, packetId: %i ", MQTT_PUB_TEMP, packetIdPub4);
    Serial.printf("Message: %.2f \n", temp);

    // Publish ke MQTT message dengan topik esp/mq2/smoke
    uint16_t packetIdPub5 = mqttClient.publish(MQTT_PUB_SMOKE, 1, true, String(smoke).c_str());
    Serial.printf("Publishing on topic %s at QoS 1, packetId %i: ", MQTT_PUB_SMOKE, packetIdPub5);
    Serial.printf("Message: %.2f \n", smoke);

    // Publish ke MQTT message dengan topik esp/pzem/energy
    uint16_t packetIdPub6 = mqttClient.publish(MQTT_PUB_ENERGY, 1, true, String(energy).c_str());
    Serial.printf("Publishing on topic %s at QoS 1, packetId: %i ", MQTT_PUB_ENERGY, packetIdPub6);
    Serial.printf("Message: %.2f \n", energy);

    // Publish ke MQTT message dengan topik esp/pzem/voltage
    uint16_t packetIdPub7 = mqttClient.publish(MQTT_PUB_PZEM_VOLTAGE, 1, true, String(voltage).c_str());
    Serial.printf("Publishing on topic %s at QoS 1, packetId %i: ", MQTT_PUB_PZEM_VOLTAGE, packetIdPub7);
    Serial.printf("Message: %.2f \n", voltage);

    // Publish ke MQTT message dengan topik esp/pzem/current
    uint16_t packetIdPub8 = mqttClient.publish(MQTT_PUB_PZEM_CURRENT, 1, true, String(current).c_str());
    Serial.printf("Publishing on topic %s at QoS 1, packetId %i: ", MQTT_PUB_PZEM_CURRENT, packetIdPub8);
    Serial.printf("Message: %.2f \n", current);

    // Publish ke MQTT message dengan topik esp/pzem/power
    uint16_t packetIdPub9 = mqttClient.publish(MQTT_PUB_PZEM_POWER, 1, true, String(power).c_str());
    Serial.printf("Publishing on topic %s at QoS 1, packetId %i: ", MQTT_PUB_PZEM_POWER, packetIdPub9);
    Serial.printf("Message: %.2f \n", power);
  }
}