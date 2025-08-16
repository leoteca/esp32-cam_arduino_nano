
#include <DHT.h>
#include <Stepper.h>

// ---------- CONFIGURACIÓN DHT11 ----------
#define DHTPIN 2          // Pin de datos del DHT11
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ---------- CONFIGURACIÓN MOTOR DE PASOS ----------
const int stepsPerRevolution = 2048;  // 28BYJ-48
Stepper stepper(stepsPerRevolution, 8, 10, 9, 11); // IN1, IN2, IN3, IN4

// ---------- VARIABLES ----------
unsigned long lastSendTime = 0;
unsigned long interval = 2000; // Cada 2 segundos

void setup() {
  Serial.begin(9600);     // Comunicación UART con ESP32
  dht.begin();

  // Inicializar motor de pasos
  stepper.setSpeed(10);  // RPM del motor
}

void loop() {
  // Leer temperatura y humedad
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // Validar lectura del DHT11
  if (isnan(h) || isnan(t)) {
    Serial.println("Error leyendo DHT11");
    return;
  }

  // Enviar al ESP32 cada X segundos
  if (millis() - lastSendTime >= interval) {
    lastSendTime = millis();
    
    Serial.print("T:");
    Serial.print(t, 1);  // 1 decimal
    Serial.print(" H:");
    Serial.println(h, 1);
  }

  // CONTROL DEL MOTOR DE PASOS
  // (Ejemplo simple: gira 1 vuelta hacia adelante y se detiene)
  static bool yaGiro = false;
  if (!yaGiro) {
    stepper.step(stepsPerRevolution);  // Una vuelta completa
    delay(500);
    yaGiro = true;
  }
}