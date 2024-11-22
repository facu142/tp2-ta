#include "ThingSpeak.h"
#include "WiFi.h"
#include "DHT.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>


// Pines
#define PIN_DHT22 33
#define PIN_POTENCIOMETRO 32
#define PIN_BOTON 19

// Credenciales de red WiFi
const char *ssid = "PALOMACLARO";
const char *password = "R87POKCIMD";

// Configuración de ThingSpeak
unsigned long channelID = 2747874;
const char *WriteAPIKey = "W2ST8KR2EOSUDP2O";
const char *serverName = "api.thingspeak.com";

// Cliente WiFi
WiFiClient cliente;

// Web server en el ESP32
WiFiServer server(80);

// Sensor DHT22
DHT dht2(PIN_DHT22, DHT22);

// Variables para los datos
int contadorPulsaciones = 0;
float ultimaTemperatura = NAN;
float ultimaHumedad = NAN;
float ultimaPosicionPot = NAN;

// Temporizador
unsigned long tiempoUltimoEnvio = 0;
const unsigned long intervaloEnvio = 20000;  // 20 segundos

// Variables para debounce
volatile unsigned long ultimaInterrupcion = 0;
const unsigned long debounceTiempo = 200;  // Tiempo de debounce en ms

// Para manejar solicitudes web
String header;

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

  // Iniciar servidor web
  server.begin();
}

void loop() {


  // Enviar datos a ThingSpeak cada 20 segundos
  if (millis() - tiempoUltimoEnvio >= intervaloEnvio) {
    // Leer sensores
    Serial.println("IP: " + WiFi.localIP().toString());
    leerSensores();
    enviarDatosThingSpeak();
    tiempoUltimoEnvio = millis();
  }

  // Manejar solicitudes web
  manejarSolicitudesWeb();
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
  String contadorPulsacionesStr = String(contadorPulsaciones);

  ThingSpeak.setStatus(contadorPulsacionesStr);

  // Enviar datos
  if (ThingSpeak.writeFields(channelID, WriteAPIKey)) {
    Serial.println("Datos enviados correctamente a ThingSpeak.");
  } else {
    Serial.println("Error al enviar datos a ThingSpeak.");
  }
}

void manejarSolicitudesWeb() {
  WiFiClient client = server.available();  // Escuchar nuevos clientes
  if (client) {
    Serial.println("Nuevo cliente conectado.");
    String currentLine = "";  // Cadena para almacenar la solicitud HTTP
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();  // Leer byte del cliente
        Serial.write(c);         // Imprimir en consola
        header += c;
        if (c == '\n') {
          if (currentLine.length() == 0) {
            // Responder con una página web
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            obtenerDatosDeThingSpeakPublico(client);

            obtenerDatosDeThingSpeakPrivado(client);

            // Salir del bucle
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    header = "";
    client.stop();
    Serial.println("Cliente desconectado.");
  }
}

void obtenerDatosDeThingSpeakPrivado(WiFiClient &client) {
  HTTPClient http;

  // URL para obtener los feeds (datos de fields)
  String url = "https://api.thingspeak.com/channels/2747874/feeds.json?api_key=W2ST8KR2EOSUDP2O&results=1";

  // Solicitar los datos de los feeds
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode == 200) {
    // Si la solicitud fue exitosa, obtener los datos JSON de los feeds
   String payload = http.getString();
    // Crear un documento JSON
    StaticJsonDocument<1024> doc;

    // Deserializar el JSON recibido
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      Serial.print("Error al analizar JSON: ");
      Serial.println(error.c_str());
      return;
    }

    // Extraer datos del JSON
    JsonObject channel = doc["channel"];
    JsonObject feed = doc["feeds"][0];

    String canalNombre = channel["name"].as<String>();
    String canalDescripcion = channel["description"].as<String>();
    String temp = feed["field1"].as<String>();
    String hum = feed["field2"].as<String>();
    String pote = feed["field3"].as<String>();
    String puls = feed["field4"].isNull() ? "N/A" : feed["field4"].as<String>();

    client.println("<h1>Datos Canal" + canalNombre + "</h1>");
    client.println("<h2>Descripcion del Canal" + canalDescripcion + "</h2>");
    client.println("<table>");
    client.println("<tr><th>Temperatura (&deg;C)</th><th>Humedad (%)</th><th>Potenciómetro (%)</th><th>Pulsaciones</th></tr>");

    // Agregar datos dinámicos
    client.println("<tr>");
    client.println("<td>" + temp + "</td>");
    client.println("<td>" + hum + "</td>");
    client.println("<td>" + pote + "</td>");
    client.println("<td>" + puls + "</td>");
    client.println("</tr>");
    client.println("</table>");

  }
  http.end();

  // URL para obtener el status (pulsaciones)
  String statusUrl = "https://api.thingspeak.com/channels/2747874/status.json?api_key=W2ST8KR2EOSUDP2O&results=1";

  // Solicitar los datos del status
  http.begin(statusUrl);
  httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    client.println("<h1>Datos Canal 2747874 (Status - Pulsador)</h1>");
    client.println("<br>");
    client.println(payload);                        // Enviar al cliente conectado
    client.println("<br>");                         // Enviar al cliente conectado
    client.println("///////////////////////////");  // Enviar al cliente conectado
    client.println("<br>");                         // Enviar al cliente conectado
    client.println("</body>");
    client.println("</html>");
  }
  http.end();
}

void obtenerDatosDeThingSpeakPublico(WiFiClient &client) {
  HTTPClient http;

  // URL del canal público
  String url = "https://api.thingspeak.com/channels/2738000/feeds.json?api_key=YOUR_READ_API_KEY&results=1";

  // Solicitar los datos del canal público
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    client.println("<!DOCTYPE html>");
    client.println("<html lang='en'>");
    client.println("<head>");
    client.println("<meta charset='UTF-8'>");
    client.println("<meta name='viewport' content='width=device-width, initial-scale=1.0'>");
    client.println("<title>Datos ESP32</title>");
    client.println("<style>");
    client.println("body { font-family: Arial, sans-serif; text-align: center; margin: 20px; }");
    client.println("table { margin: 0 auto; border-collapse: collapse; width: 80%; }");
    client.println("th, td { border: 1px solid #ddd; padding: 8px; }");
    client.println("th { background-color: #f4f4f4; }");
    client.println("h1 { color: #333; }");
    client.println("</style>");
    client.println("</head>");
    client.println("<body>");
    
    client.println("<h1>Datos Canal 2738000</h1>");
    client.println("<br>");
    client.println(payload);                        // Enviar al cliente conectado
    client.println("<br>");                         // Enviar al cliente conectado
    client.println("///////////////////////////");  // Enviar al cliente conectado
    client.println("<br>");                         // Enviar al cliente conectado
  }
  http.end();
}
