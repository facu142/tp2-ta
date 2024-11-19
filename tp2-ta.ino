#include "ThingSpeak.h"
#include "WiFi.h"
#include "DHT.h"

// Pin del DHT22
#define pin2 33

// Credenciales de red WiFi
const char *ssid = "ACNET2";  // Nombre de la red
const char *password = "";    // Contraseña de la red

// Configuración de ThingSpeak
unsigned long channelID = 2747874;           // ID del canal
const char *WriteAPIKey = "W2ST8KR2EOSUDP2O"; // API Key de escritura
const char *ReadAPIKey = "R2KTBQZG3LWGLCYA";  // API Key de lectura
const char *serverName = "api.thingspeak.com"; // Servidor ThingSpeak

// Cliente y servidor
WiFiClient cliente;
WiFiServer server(80);

// Sensor DHT22
DHT dht2(pin2, DHT22);

// Variables para almacenar los datos obtenidos
String datosThingSpeak = "";

void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando...");

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

  // Iniciar el servidor web
  server.begin();
}

void loop() {
  // Enviar datos a ThingSpeak
  enviarDatosThingSpeak();

  // Manejar solicitudes del servidor web
  WiFiClient client = server.available();
  if (client) {
    String currentLine = "";
    String header = "";

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        header += c;

        if (c == '\n' && currentLine.length() == 0) {
          // Enviar respuesta HTTP
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println("Connection: close");
          client.println();

          // Obtener datos de ThingSpeak y generar página HTML
          obtenerDatosThingSpeak();
          String pagina = generarPaginaHTML();
          client.println(pagina);
          break;
        } else if (c == '\n') {
          currentLine = "";
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    client.stop();
  }

  // Pausa para evitar saturación
  delay(14000);
}

void enviarDatosThingSpeak() {
  float t2 = dht2.readTemperature();
  float h2 = dht2.readHumidity();

  while (isnan(t2) || isnan(h2)) {
    Serial.println("Lectura fallida en el sensor DHT22, repitiendo...");
    delay(2000);
    t2 = dht2.readTemperature();
    h2 = dht2.readHumidity();
  }

  Serial.print("Temperatura: ");
  Serial.println(t2);
  Serial.print("Humedad: ");
  Serial.println(h2);

  ThingSpeak.setField(1, t2);
  ThingSpeak.setField(2, h2);
  ThingSpeak.writeFields(channelID, WriteAPIKey);
  Serial.println("Datos enviados a ThingSpeak.");
}

void obtenerDatosThingSpeak() {
  if (cliente.connect(serverName, 80)) {
    cliente.print(String("GET /channels/") + channelID + "/feeds.json?api_key=" + ReadAPIKey + "&results=1 HTTP/1.1\r\n" +
                  "Host: " + serverName + "\r\n" +
                  "Connection: close\r\n\r\n");

    while (cliente.connected() || cliente.available()) {
      if (cliente.available()) {
        String linea = cliente.readStringUntil('\n');
        if (linea.startsWith("{")) { // Buscar JSON en la respuesta
          datosThingSpeak = linea;
        }
      }
    }
    cliente.stop();
  } else {
    Serial.println("Error al conectar con ThingSpeak.");
  }
}

String generarPaginaHTML() {
  // Variables para almacenar los datos extraídos del JSON
  String temperatura = "N/A";
  String humedad = "N/A";
  String timestamp = "N/A";

  // Extraer los datos del JSON
  if (datosThingSpeak.length() > 0) {
    int indexFeeds = datosThingSpeak.indexOf("\"feeds\":");
    if (indexFeeds != -1) {
      int indexTemp = datosThingSpeak.indexOf("\"field1\":", indexFeeds);
      int indexHum = datosThingSpeak.indexOf("\"field2\":", indexFeeds);
      int indexCreated = datosThingSpeak.indexOf("\"created_at\":", indexFeeds);

      if (indexTemp != -1) {
        temperatura = datosThingSpeak.substring(indexTemp + 9, datosThingSpeak.indexOf(",", indexTemp));
      }
      if (indexHum != -1) {
        humedad = datosThingSpeak.substring(indexHum + 9, datosThingSpeak.indexOf(",", indexHum));
      }
      if (indexCreated != -1) {
        timestamp = datosThingSpeak.substring(indexCreated + 14, datosThingSpeak.indexOf("\"", indexCreated + 14));
      }
    }
  }

  // Generar HTML
  return "<!DOCTYPE html>"
         "<html>"
         "<head>"
         "<title>Datos de ThingSpeak</title>"
         "<style>"
         "table { width: 50%; border-collapse: collapse; margin: 20px auto; text-align: center; }"
         "th, td { border: 1px solid #ddd; padding: 8px; }"
         "th { background-color: #f4f4f4; font-weight: bold; }"
         "</style>"
         "</head>"
         "<body>"
         "<h1 style='text-align: center;'>Datos desde ThingSpeak</h1>"
         "<table>"
         "<tr><th>Campo</th><th>Valor</th></tr>"
         "<tr><td>Temperatura</td><td>" + temperatura + "°C</td></tr>"
         "<tr><td>Humedad</td><td>" + humedad + "%</td></tr>"
         "<tr><td>Timestamp</td><td>" + timestamp + "</td></tr>"
         "</table>"
         "</body>"
         "</html>";
}
