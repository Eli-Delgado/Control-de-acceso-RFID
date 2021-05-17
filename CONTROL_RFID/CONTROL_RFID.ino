#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Ethernet.h>
#include <MFRC522.h>


// Object for Servo
Servo miservo;
//Object for Display
LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C address 0x27, 16 column and 2 rows

//Variables for RFID
#define RST_PIN  9      // constante para referenciar pin de reset
#define SS_PIN  4      // constante para referenciar pin de slave select

MFRC522 mfrc522(SS_PIN, RST_PIN); // crea objeto mfrc522 enviando pines de slave select y reset


byte LecturaUID[4];         //Array for UID read.



// replace the MAC address below by the MAC address printed on a sticker on the Arduino Shield 2
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

EthernetClient client;

int    HTTP_PORT   = 9096;
char   HOST_NAME[] = "192.168.100.6";


struct clientData {
  String id;
  String nombre;
  bool habilitado;
};

void setup() {
  //Initializing Setial Monitor.
  Serial.begin(9600); 
  //Initializing servo
  miservo.attach(3);
  miservo.write(90);
  //Initializing display
  lcd.init(); 
  lcd.backlight();
  //Initializing RFID
  SPI.begin();        //SPI BUS
  mfrc522.PCD_Init();     //RFID Lector

  // initialize the Ethernet shield using DHCP:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to obtaining an IP address using DHCP");
    while(true);
  }
  
  Serial.println("Ready!");
  printDisplay("Bienvenido", "Pase Tarjeta");
}

void loop() {

  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }
  
  //Save Recieved UID
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    LecturaUID[i] = mfrc522.uid.uidByte[i];
  }
  //RFID LEIDO
  char str[32] = "";
  array_to_string(LecturaUID, 4, str);
  String rfid = String(str);

  struct clientData empleado = httpGET(rfid);
  Serial.println(empleado.habilitado);
  Serial.println(empleado.id);
  if(empleado.habilitado) {
    printDisplay("Pase", empleado.nombre);
    openDoor();
    printDisplay("Bienvenido", "Pase Tarjeta");

    httpPOST(empleado.id);
  } else {
    printDisplay("Alto", "Sin registro");
    delay(5000);
    printDisplay("Bienvenido", "Pase Tarjeta");
  }
  
  mfrc522.PICC_HaltA();     // detiene comunicacion con tarjeta
  
}

void openDoor() {
  int pos = 0; 
  Serial.println("opening");
  for(pos = 90; pos < 170; pos += 1) {
    miservo.write(pos);
    delay(15);
  }
  delay(5000);
  Serial.println("closing");
  for(pos = 170; pos >= 90; pos -= 1) {
    miservo.write(pos);
    delay(15);
  } 
}

void printDisplay(String row1, String row2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(row1);
  lcd.setCursor(0, 1);
  lcd.print(row2);
}

void array_to_string(byte array[], unsigned int len, char buffer[]) {
    for (unsigned int i = 0; i < len; i++) {
        byte nib1 = (array[i] >> 4) & 0x0F;
        byte nib2 = (array[i] >> 0) & 0x0F;
        buffer[i*2+0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
        buffer[i*2+1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
    }
    buffer[len*2] = '\0';
}

struct clientData httpGET(String rfid) {
  struct clientData c;
  c.id = "null";
  c.nombre = "null";
  c.habilitado = false;
  
  Serial.println(rfid);

  
  Serial.println("Connecting...");

  // Connect to HTTP server
  EthernetClient client;
  client.setTimeout(10000);
  if (!client.connect(HOST_NAME, HTTP_PORT)) {
    Serial.println("Connection failed");
    return c;
  }

  Serial.println(F("Connected!"));

  // Send HTTP request
  client.println("GET /api/empleado/byRfid/" + rfid + " HTTP/1.0");
  client.println("Host: " + String(HOST_NAME));
  client.println("Connection: close");
  if (client.println() == 0) {
    Serial.println("Failed to send request");
    client.stop();
    return c;
  }

  // Check HTTP status
  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
    Serial.print("Unexpected response: ");
    Serial.println(status);
    client.stop();
    return c;
  }

  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders)) {
    Serial.println("Invalid response");
    client.stop();
    return c;
  }

  // Allocate the JSON document
  // Use arduinojson.org/v6/assistant to compute the capacity.
  const size_t capacity = JSON_OBJECT_SIZE(3) + 78;
  DynamicJsonDocument doc(capacity);

  // Parse JSON object
  DeserializationError error = deserializeJson(doc, client);
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.f_str());
    client.stop();
    return c;
  }

  // Extract values
  Serial.println("Response:");
  
  int id = doc["id"];
  Serial.println(id);
  c.id = String(id);
  
  char* n = doc["nombre"];
  Serial.println(n);
  c.nombre = String(n);

  bool hab = doc["habilitado"].as<bool>();
  Serial.println(hab);
  c.habilitado = hab;
  
  
  // Disconnect
  client.stop();
  return c;
}

void httpPOST(String id) {
  String JSON1 = "{\"EmpleadoId\":"; 
  String JSON2 = "}";

  String JSON = JSON1 + id + JSON2;
  // connect to web server on port 80:
  if(client.connect(HOST_NAME, HTTP_PORT)) {
    // if connected:
    Serial.println("Connected to server");
    // make a HTTP request:
    // send HTTP header
    client.println("POST /api/ingresos HTTP/1.1");
    client.println("Host: " + String(HOST_NAME));
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.print("Content-Length: ");
    client.println(JSON.length());
    client.println();
    client.println(JSON);
    client.println(); // end HTTP header

    // send HTTP body
    //client.println(queryString);

    while(client.connected()) {
      if(client.available()){
        // read an incoming byte from the server and print it to serial monitor:
        char c = client.read();
        Serial.print(c);
      }
    }

    // the server's disconnected, stop the client:
    client.stop();
    Serial.println();
    Serial.println("disconnected");
  } else {// if not connected:
    Serial.println("connection failed");
  }
}
