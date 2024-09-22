#include <Adafruit_Sensor.h>
#include "painlessMesh.h"
#include <Arduino_JSON.h>
#include "DHT.h"

// MESH Details
#define   MESH_PREFIX     "MeshEveOscMar" // name for your MESH
#define   MESH_PASSWORD   "123456789" // password for your MESH
#define   MESH_PORT       5555 // default port

#define DHTPIN 27     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11   // DHT 22  (AM2302), AM2321

DHT dht(DHTPIN, DHTTYPE);

// Number for this node
int nodeNumber = 1;

// String to send to other nodes with sensor readings
String readings;

Scheduler userScheduler; // to control your personal task
painlessMesh mesh;

// User stub
void sendMessage(); // Prototype so PlatformIO doesn't complain
String getReadings(); // Prototype for sending sensor readings
String getFormattedTime(); // Prototype for getting local time

// Create tasks: to send messages and get readings;
Task taskSendMessage(TASK_SECOND * 5, TASK_FOREVER, &sendMessage);

// Variables para el monitoreo
bool monitoring = false; // Estado del monitoreo
unsigned long lastToggleTime = 0; // Tiempo de la última alternancia

// Función para obtener la hora local formateada con AM/PM
String getFormattedTime() {
  uint32_t nodeTime = mesh.getNodeTime() / 1000000; // Convertir de microsegundos a segundos
  uint32_t hours = (nodeTime / 3600) % 24; // Obtener las horas
  uint32_t minutes = (nodeTime / 60) % 60; // Obtener los minutos
  uint32_t seconds = nodeTime % 60; // Obtener los segundos
  
  String period = "AM";  // Inicialmente es AM

  // Convertir a formato de 12 horas
  if (hours >= 12) {
    period = "PM";  // Si es después del mediodía, es PM
    if (hours > 12) {
      hours -= 12;  // Convertir horas de 24 a 12 horas
    }
  } else if (hours == 0) {
    hours = 12;  // Convertir 00:xx a 12:xx AM
  }

  // Formatear la hora en formato hh:mm:ss AM/PM
  char timeBuffer[12];
  sprintf(timeBuffer, "%02u:%02u:%02u %s", hours, minutes, seconds, period.c_str());

  return String(timeBuffer);
}

String getReadings() {
  JSONVar jsonReadings;
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  String currentTime = getFormattedTime();

  Serial.print(F("Humidity: "));
  Serial.print(humidity, 1);
  Serial.print(F("%  Temperature: "));
  Serial.print(temperature, 1);
  Serial.println(F("°C "));
  Serial.print(F("%  Hora: "));
  Serial.print(currentTime);
  // Incluir los datos en el JSON
  jsonReadings["node"] = nodeNumber;
  jsonReadings["temp"] = temperature,1;
  jsonReadings["hum"] = humidity,1;
  jsonReadings["time"] = currentTime; // Incluir la hora local en el mensaje
  jsonReadings["monitoring"] = monitoring ? "Encendido" : "Apagado"; // Estado de monitoreo

  readings = JSON.stringify(jsonReadings);
  return readings;
}

void sendMessage() {
  if (monitoring) {
    String msg = getReadings();
    Serial.println(msg);
    mesh.sendBroadcast(msg);
  }
}

// Needed for painless library
void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("Mensaje recibido desde %u msg=%s\n", from, msg.c_str());

  // Parsear el mensaje recibido
  JSONVar myObject = JSON.parse(msg.c_str());

  if (JSON.typeof(myObject) == "undefined") {
    Serial.println("Error al parsear JSON");
    return;
  }

  // Extraer los datos del nodo, color y hora
  int node = myObject["node"];
  String color = (const char*)myObject["color"]; // Extraer el color como string
  String timeReceived = (const char*)myObject["time"]; // Extraer la hora como string

  // Mostrar el nodo, el color y la hora recibida
  Serial.print("Node: ");
  Serial.println(node);
  Serial.print("Led color: ");
  Serial.println(color); // Muestra el color en formato de texto
  Serial.print("Hora local: ");
  Serial.println(timeReceived); // Mostrar la hora local en formato legible
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("Nueva conexión, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.printf("Cambios en las conexiones\n");
}

void nodeTimeAdjustedCallback(int32_t offset) {
  Serial.printf("Hora ajustada %u. Offset = %d\n", mesh.getNodeTime(), offset);
}

void setup() {
  Serial.begin(115200);
  
  dht.begin();

  mesh.setDebugMsgTypes(ERROR | STARTUP); // set before init() so that you can see startup messages

  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();
}

void loop() {
  mesh.update();

  // Alternar el estado de monitoreo cada 10 segundos
  unsigned long currentMillis = millis();
  if (currentMillis - lastToggleTime >= 20000) { // Cada 10 segundos
    monitoring = !monitoring; // Cambiar el estado de monitoreo
    lastToggleTime = currentMillis; // Actualizar el tiempo de la última alternancia
    Serial.print("Monitoreo: ");
    Serial.println(monitoring ? "Encendido" : "Apagado");
  }
}
