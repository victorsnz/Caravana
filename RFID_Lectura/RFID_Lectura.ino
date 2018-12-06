/* Typical pin layout used:
 * --------------------------------------------------------------------------------------- ---------
 *             MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino     NodeMCU
 *             Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro   Pro Micro   
 * Signal      Pin          Pin           Pin       Pin        Pin              Pin         Pin
 * --------------------------------------------------------------------------------------- ---------
 * RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST         D3
 * SPI SS      SDA(SS)      10            53        D10        10               10          D4
 * SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16          D7
 * SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14          D6
 * SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15          D5
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN  D4 //10 
#define RST_PIN D3 //9

const char* ssid = ""; // Nombre de la conexión wifi
const char* password = "";
const char* mqtt_server = "";
int mqtt_port = 2000;

WiFiClient espClient;
PubSubClient client_MQTT (espClient);

/*
 * RFID
 */
MFRC522 rfid(SS_PIN, RST_PIN); // Instancia de la clase
MFRC522::MIFARE_Key key; 

// Inicializamos array que almacena nuevo NUID 
byte nuidPICC[4];

//Inicializamos contador
int cont = 0;

void setup() 
{ 
  Serial.begin(9600);

  setup_wifi(); 
  client_MQTT.setServer(mqtt_server, mqtt_port);
  client_MQTT.setCallback(callback);
  
  SPI.begin(); // Iniciamos SPI bus
  rfid.PCD_Init(); // Iniciamos MFRC522 

  for (byte i = 0; i < 6; i++) 
  {
    key.keyByte[i] = 0xFF;
  }
  
  Serial.println("Acerque la tarjeta al lector.");
}
 
void loop() 
{
  if (!client_MQTT.connected()) 
  {
    reconnect(); /// reconection MQTT
  }
  client_MQTT.loop();
 
  //client_MQTT.subscribe("output"); /// usar para suscribirse a un topic
  
    // Se buscan nuevas tarjetas
  if ( ! rfid.PICC_IsNewCardPresent())
    return;

  // Se verifica si el NUID ha sido leído
  if ( ! rfid.PICC_ReadCardSerial())
    return;

  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  Serial.println(rfid.PICC_GetTypeName(piccType));

  // Verifica el PICC del tipo MIFARE clásico
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&  
      piccType != MFRC522::PICC_TYPE_MIFARE_1K   &&
      piccType != MFRC522::PICC_TYPE_MIFARE_4K ) 
  {
    Serial.println(F("Este tag no es del tipo MIFARE Classic."));
    return;
  }

  if (rfid.uid.uidByte[0] != nuidPICC[0] || 
      rfid.uid.uidByte[1] != nuidPICC[1] || 
      rfid.uid.uidByte[2] != nuidPICC[2] || 
      rfid.uid.uidByte[3] != nuidPICC[3] ) 
  {
    Serial.println(F("Se ha detectado una nueva tarjeta. "));

    // Se almacena NUID en el arreglo nuidPICC
    for (byte i = 0; i < 4; i++) 
    {
      nuidPICC[i] = rfid.uid.uidByte[i];
    }
   
    Serial.println(F("The NUID tag is: "));
    
    Serial.print(F("En hexa: "));
    printHex(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();
    Serial.print(F("En dec: "));
    printDec(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();
    
    cont ++;
    
    char code[4]; /// array char
    String value = String((char*)nuidPICC); //// input to string
    String (value).toCharArray(code, 4); /// String to array char
    client_MQTT.publish("TagNumber", code); //// Array char mensaje MQTT /// state input - estado entrada
  }
  else Serial.println(F("Tarjeta leída previamente."));

  Serial.print("Cantidad: ");
  Serial.println(cont);

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}


/**
 * Helper routine to dump a byte array as hex values to Serial. 
 */
void printHex(byte *buffer, byte bufferSize) 
{
  for (byte i = 0; i < bufferSize; i++) 
  {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

/**
 * Helper routine to dump a byte array as dec values to Serial.
 */
void printDec(byte *buffer, byte bufferSize) 
{
  for (byte i = 0; i < bufferSize; i++) 
  {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
  }
}


void setup_wifi() 
{
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Conectando a");
  Serial.println(ssid); 
  
  WiFi.begin(ssid, password);
  Serial.println(".");
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Conectado ");
  Serial.print("MQTT Server ");
  Serial.print(mqtt_server);
  Serial.print(":");
  Serial.println(String(mqtt_port));
  Serial.print("ESP8266 IP ");
  Serial.println(WiFi.localIP());
}

/* funtion callback
 * 
 * Esta funcion realiza la recepcion de los topic suscritos
 * This function performs the reception of subscribed topics
 * 
 */
void callback(char* topic, byte* payload, unsigned int length) 
{
  String string;
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) 
  {
    //Serial.print((char)payload[i]);
    string+=((char)payload[i]); 
  }
  Serial.println(string);
}

 /*
 * 
 * Funcion que realiza la reconexion de cliente MQTT
 * Function that performs MQTT client reconnection
 * 
 * enable/habilita client_MQTT.subscribe("event");
 */ 
void reconnect() 
{
  // Loop until we're reconnected
  while (!client_MQTT.connected()) 
  { 
    if (client_MQTT.connect("ESP8266Client")) 
    {} 
    else 
    {
      Serial.println("No se pudo conectar al servidor MQTT.");
      Serial.println("rc= ");
      Serial.println(client_MQTT.state());
    }
  }
}
