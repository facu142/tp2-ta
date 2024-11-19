#include "ThingSpeak.h"
#include "WiFi.h"
#include "DHT.h"

// Pines
#define PIN_DHT22 33
#define PIN_POTENCIOMETRO 32
#define PIN_BOTON 19

// Credenciales de red WiFi
const char *ssid = "ACNET2";
const char *password = "";

// Configuración de ThingSpeak
unsigned long channelID = 2747874;
const char *WriteAPIKey = "W2ST8KR2EOSUDP2O";
const char *serverName = "api.thingspeak.com";

// Cliente WiFi
WiFiClient cliente;

// Sensor DHT22
DHT dht2(PIN_DHT22, DHT22);

// Variables para los datos
int contadorPulsaciones = 0;
float ultimaTemperatura = NAN;
float ultimaHumedad = NAN;
float ultimaPosicionPot = NAN;

// Temporizador
unsigned long tiempoUltimoEnvio = 0;
const unsigned long intervaloEnvio = 20000; // 20 segundos

// Variables para debounce
volatile unsigned long ultimaInterrupcion = 0;
const unsigned long debounceTiempo = 200; // Tiempo de debounce en ms

void IRAM_ATTR contarPulsacion() {
  unsigned long tiempoActual = millis();
  if (tiempoActual - ultimaInterrupcion > debounceTiempo) {
    contadorPulsaciones++;
    ultimaInterrupcion = tiempoActual;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando...");

  // Configurar pines
  pinMode(PIN_BOTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_BOTON), contarPulsacion, FALLING);

  // Conexión WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.println("IP: " + WiFi.localIP().toString());

  // Inicializar sensores y ThingSpeak
  dht2.begin();
  ThingSpeak.begin(cliente);
}

void loop() {
  // Leer sensores
  leerSensores();

  // Enviar datos a ThingSpeak cada 20 segundos
  if (millis() - tiempoUltimoEnvio >= intervaloEnvio) {
    enviarDatosThingSpeak();
    tiempoUltimoEnvio = millis();
  }
}

void leerSensores() {
  // Leer potenciómetro
  int lecturaPot = analogRead(PIN_POTENCIOMETRO);
  ultimaPosicionPot = (lecturaPot / 4095.0) * 100;
  Serial.print("Posición del potenciómetro: ");
  Serial.print(ultimaPosicionPot);
  Serial.println(" %.");

  // Leer temperatura y humedad
  ultimaTemperatura = dht2.readTemperature();
  ultimaHumedad = dht2.readHumidity();

  if (isnan(ultimaTemperatura) || isnan(ultimaHumedad)) {
    Serial.println("Error al leer el sensor DHT22");
  } else {
    Serial.print("Temperatura: ");
    Serial.print(ultimaTemperatura);
    Serial.println(" °C");
    Serial.print("Humedad: ");
    Serial.print(ultimaHumedad);
    Serial.println(" %");
  }

  // Imprimir contador de pulsaciones
  Serial.print("Pulsaciones detectadas: ");
  Serial.println(contadorPulsaciones);
}

void enviarDatosThingSpeak() {
  // Configurar campos en ThingSpeak
  ThingSpeak.setField(1, ultimaTemperatura);
  ThingSpeak.setField(2, ultimaHumedad);
  ThingSpeak.setField(3, ultimaPosicionPot);
  ThingSpeak.setField(4, contadorPulsaciones);

  // Enviar datos
  if (ThingSpeak.writeFields(channelID, WriteAPIKey)) {
    Serial.println("Datos enviados correctamente a ThingSpeak.");
  } else {
    Serial.println("Error al enviar datos a ThingSpeak.");
  }
}
