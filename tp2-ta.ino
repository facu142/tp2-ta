#include "ThingSpeak.h"
#include "WiFi.h"
#include "DHT.h"
#include <HTTPClient.h>

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
const unsigned long intervaloEnvio = 20000; // 20 segundos

// Variables para debounce
volatile unsigned long ultimaInterrupcion = 0;
const unsigned long debounceTiempo = 200; // Tiempo de debounce en ms

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
  WiFiClient client = server.available(); // Escuchar nuevos clientes
  if (client) {
    Serial.println("Nuevo cliente conectado.");
    String currentLine = ""; // Cadena para almacenar la solicitud HTTP
    while (client.connected()) {
      if (client.available()) {
        char c = client.read(); // Leer byte del cliente
        Serial.write(c);        // Imprimir en consola
        header += c;
        if (c == '\n') {
          if (currentLine.length() == 0) {
            // Responder con una página web
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // Contenido de la página
            client.println("<!DOCTYPE html>");
            client.println("<html>");
            client.println("<head><title>Datos ESP32</title></head>");
            client.println("<body>");
            client.println("<h1>Datos Recopilados por el ESP32</h1>");
            client.println("<table border='1' style='width:100%; text-align:center;'>");
            client.println("<tr><th>Temperatura (&deg;C)</th><th>Humedadf (%)</th><th>Potenciómetro (%)</th><th>Pulsaciones</th></tr>");

            // Agregar datos dinámicos
            client.println("<tr>");
            client.println("<td>" + String(ultimaTemperatura) + "</td>");
            client.println("<td>" + String(ultimaHumedad) + "</td>");
            client.println("<td>" + String(ultimaPosicionPot) + "</td>");
            client.println("<td>" + String(contadorPulsaciones) + "</td>");
            client.println("</tr>");
            client.println("</table>");

            client.println("<h1>Datos desde ThingSpeak (Canal 2738000)</h1>");
            client.println("<table border='1' style='width:100%; text-align:center;'>");
            client.println("<tr><th>Latitud</th><th>Longitud</th><th>Tensión</th></tr>");

            // Leer datos de ThingSpeak
            String jsonData = obtenerDatosDeThingSpeak();
            if (jsonData != "") {
              client.println(jsonData); // Mostrar datos reales
            } else {
              client.println("<tr><td colspan='3'>No se pudo obtener datos de ThingSpeak</td></tr>");
            }

            client.println("</table>");
            client.println("</body>");
            client.println("</html>");

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


// Función para obtener datos del canal de ThingSpeak
String obtenerDatosDeThingSpeakPrivado() {
  HTTPClient http;
  String jsonData = "";
  
  // URL para obtener los feeds (datos de fields)
  String url = "https://api.thingspeak.com/channels/2747874/feeds.json?api_key=W2ST8KR2EOSUDP2O&results=1"; 
  
  // Solicitar los datos de los fields
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode == 200) {
    // Si la solicitud fue exitosa, obtener los datos JSON de los feeds
    String payload = http.getString();
    
    // Extraer los datos de los fields
    int startIndex = payload.indexOf("\"field1\":\"");
    int endIndex = payload.indexOf("\"", startIndex + 9);
    String field1 = payload.substring(startIndex + 9, endIndex);

    startIndex = payload.indexOf("\"field2\":\"", endIndex);
    endIndex = payload.indexOf("\"", startIndex + 9);
    String field2 = payload.substring(startIndex + 9, endIndex);

    startIndex = payload.indexOf("\"field3\":\"", endIndex);
    endIndex = payload.indexOf("\"", startIndex + 9);
    String field3 = payload.substring(startIndex + 9, endIndex);

    // Construir fila HTML con los datos de los fields
    jsonData += "<tr>";
    jsonData += "<td>" + field1 + "</td>"; // Temperatura u otro dato
    jsonData += "<td>" + field2 + "</td>"; // Humedad u otro dato
    jsonData += "<td>" + field3 + "</td>"; // Potenciómetro u otro dato
    jsonData += "</tr>";
  } else {
    Serial.println("Error en la solicitud HTTP de fields: " + String(httpCode));
    jsonData = "<p>No se pudo obtener datos de ThingSpeak (fields)</p>";
  }

  // URL para obtener el status (pulsaciones)
  String statusUrl = "https://api.thingspeak.com/channels/2747874/status.json?api_key=W2ST8KR2EOSUDP2O"; 
  
  // Solicitar los datos del status
  http.begin(statusUrl);
  httpCode = http.GET();
  if (httpCode == 200) {
    // Si la solicitud fue exitosa, obtener los datos JSON del status
    String payload = http.getString();
    
    // Extraer el valor de las pulsaciones desde el status
    int startIndex = payload.indexOf("\"status\":\"");
    int endIndex = payload.indexOf("\"", startIndex + 10);
    String pulsaciones = payload.substring(startIndex + 10, endIndex);

    // Construir fila HTML con el valor de las pulsaciones
    jsonData += "<tr>";
    jsonData += "<td colspan='3'>Pulsaciones: " + pulsaciones + "</td>"; // Pulsaciones
    jsonData += "</tr>";
  } else {
    Serial.println("Error en la solicitud HTTP de status: " + String(httpCode));
    jsonData += "<tr><td colspan='4'>No se pudo obtener datos del status de ThingSpeak</td></tr>";
  }

  http.end();
  return jsonData;
}


// Función para obtener datos del canal 2738000 de ThingSpeak
String obtenerDatosDeThingSpeak() {
  HTTPClient http;
  String jsonData = "";
  String url = "https://api.thingspeak.com/channels/2738000/feeds.json?api_key=YOUR_READ_API_KEY&results=1";
  
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode == 200) {
    // Si la solicitud fue exitosa, obtener los datos JSON
    String payload = http.getString();
    
    // Buscar el primer "feed" dentro del JSON
    int startIndex = payload.indexOf("\"feeds\":");
    int endIndex = payload.indexOf("]}", startIndex);
    
    if (startIndex != -1 && endIndex != -1) {
      // Extraemos solo la parte de los feeds
      String feedsData = payload.substring(startIndex + 9, endIndex);
      
      // Procesamos cada uno de los feeds y extraemos los valores
      int latitudeIndex = feedsData.indexOf("\"field1\":\"");
      int longitudeIndex = feedsData.indexOf("\"field2\":\"");
      int field4Index = feedsData.indexOf("\"field4\":\"");
      int field5Index = feedsData.indexOf("\"field5\":\"");

      // Crear los encabezados de la tabla con los nombres de los campos
      jsonData += "<table><tr><th>Latitude</th><th>Longitude</th><th>Tensão (V)</th></tr>";
      
      // Verificamos que los campos estén presentes y extraemos los valores
      if (latitudeIndex != -1) {
        String latitude = feedsData.substring(latitudeIndex + 10, feedsData.indexOf("\"", latitudeIndex + 10));
        jsonData += "<tr><td>" + latitude + "</td>";
      } else {
        jsonData += "<tr><td>-</td>";
      }
      
      if (longitudeIndex != -1) {
        String longitude = feedsData.substring(longitudeIndex + 10, feedsData.indexOf("\"", longitudeIndex + 10));
        jsonData += "<td>" + longitude + "</td>";
      } else {
        jsonData += "<td>-</td>";
      }

      // Campo Tensão (V)
      if (field4Index != -1) {
        String field4 = feedsData.substring(field4Index + 10, feedsData.indexOf("\"", field4Index + 10));
        jsonData += "<td>" + field4 + "</td></tr>";
      } else {
        jsonData += "<td>-</td></tr>";
      }

      jsonData += "</table>"; // Cerrar la tabla
    }
  } else {
    Serial.println("Error en la solicitud HTTP: " + String(httpCode));
    jsonData = "<p>No se pudo obtener datos de ThingSpeak</p>";
  }
  http.end();
  return jsonData;
}


