#include <DHT.h>

// Macros
#define debugSerial SerialUSB
#define loraSerial Serial1
#define loraReset 4
#define RESPONSE_LEN  100

//CLAVES ABP

#define DEVEUIABP     "0004A30B002171FB"
#define DEV_NWKSKEY   "CAFECAFECAFECAFECAFECAFECAFECAFE"
#define DEV_APPSKEY   "CAFECAFECAFECAFECAFECAFECAFECAFE"
#define DEV_DEVADDR   "99887766"

//MACROS
/**
 * @brief Retorna la parte entera de un número de punto flotante positivo. Si el num es > a 255, retorna 255
 * @return uint8_t (max 255)
 * 
 */
#define GET_INTEGER_PART_UNSIGNED_MAX255(num)(((num) > 255) ? (255) : ((num < 0) ? 0 : ((uint8_t)(num))))        

/**
 * @brief Retorna la parte fraccional de un número de punto flotante positivo. Si el num es > a 255, retorna 0
 * @return uint8_t 
 * 
 * 
 */
#define GET_FRACTIONAL_PART_MAX255(num)((fabs(num) > 255) ? 0 : ((num < 0) ? 0 : ((uint8_t)((fabs(num) - (uint8_t)(fabs(num)) ) * 100))))


// Variables
static const char hex[] = { "0123456789ABCDEF" };
static char response[RESPONSE_LEN];

static enum {
  INIT = 0,
  JOIN, JOINED
} state;

float SENSOR = 9;
float SENSOR2 = 8;
float HUMEDAD;
float TEMPERATURA;
float HUMEDAD2;
float TEMPERATURA2;
DHT dht(SENSOR,DHT22);
DHT dht2(SENSOR2,DHT11);
char cmd[200];
uint8_t LoRaWANPayload[8];



/*
 * Vacia el buffer de recepcion desde el modulo LoRa
 */
void loraClearReadBuffer()
{
  while(loraSerial.available())
    loraSerial.read();
}

/*
 * Espera la respuesta del modulo LoRa y la imprime en el puerto de debug
 */
void loraWaitResponse(int timeout)
{
  size_t read = 0;

  loraSerial.setTimeout(timeout);
  
  read = loraSerial.readBytesUntil('\n', response, RESPONSE_LEN);
  if (read > 0) {
    response[read - 1] = '\0'; // set \r to \0
    debugSerial.println(response);
  }
  else {
    debugSerial.println("Response timeout");
    response[0] = '\0';
  }
}

/*
 * Envia un comando al modulo LoRa y espera la respuesta
 */
void loraSendCommand(char *command)
{
  loraClearReadBuffer();
  debugSerial.println(command);
  loraSerial.println(command);
  loraWaitResponse(1000);
}

/*
 * Transmite un mensaje de datos por LoRa
 */
void loraSendData(int port, uint8_t *data, uint8_t dataSize)
{
  char cmd[200];
  char *p;
  int i;
  uint8_t c;

  sprintf(cmd, "mac tx cnf %d ", port);

  p = cmd + strlen(cmd);
  
  for (i=0; i<dataSize; i++) {
    c = *data++;
    *p++ = hex[c>>4];
    *p++ = hex[c&0x0f];
  }
  *p++ = 0;

  loraSendCommand(cmd);
  if (strstr(response, "ok"))
    loraWaitResponse(10000);
}

/*
 * Funcion de inicializacion
 */
void setup() {
  pinMode(PIN_LED, OUTPUT);
  pinMode(loraReset, OUTPUT);
  digitalWrite(PIN_LED, HIGH);
  
  loraSerial.begin(57600);
  debugSerial.begin(9600);

  delay(2000);
  loraWaitResponse(1000);
  digitalWrite(loraReset, LOW);
  delay(1000);
  digitalWrite(loraReset, HIGH);
  loraWaitResponse(10000);
  digitalWrite(PIN_LED, LOW);
  loraSendCommand("sys reset");
  loraSendCommand("sys get hweui");
  dht.begin();
  dht2.begin();
  
   
}

void loop() {
  static int i, dr;
  delay(4000); //ESPERA 1 MINUTO. CAMBIAR POR MILLIS QUE ES NO BLOQUEANTE
  HUMEDAD = dht.readHumidity();
  debugSerial.print("HUMEDAD: ");
  debugSerial.println(HUMEDAD);
  delay(2000);
  TEMPERATURA = dht.readTemperature();
  debugSerial.print("TEMPERATURA: ");
  debugSerial.println(TEMPERATURA);
  delay(4000); //ESPERA 1 MINUTO. CAMBIAR POR MILLIS QUE ES NO BLOQUEANTE
  HUMEDAD2 = dht2.readHumidity();
  debugSerial.print("HUMEDAD2: ");
  debugSerial.println(HUMEDAD2);
  delay(2000);
  TEMPERATURA2 = dht2.readTemperature();
  debugSerial.print("TEMPERATURA2: ");
  debugSerial.println(TEMPERATURA2);
  switch (state) {
    case INIT: 
 //     loraSendCommand("sys factoryRESET");
 //     loraWaitResponse(10000);
      loraSendCommand("sys reset");
      delay(200);
      sprintf(cmd, "mac set deveui %s\0", DEVEUIABP);
      loraSendCommand(cmd); 
      sprintf(cmd, "mac set devaddr %s\0", DEV_DEVADDR);
      loraSendCommand(cmd); 
      sprintf(cmd, "mac set appskey %s\0", DEV_APPSKEY);
      loraSendCommand(cmd); 
      sprintf(cmd, "mac set nwkskey %s\0", DEV_NWKSKEY);
      loraSendCommand(cmd); 
      loraSendCommand("mac set adr on");
      state = JOIN;
      break;
    case JOIN:
      loraSendCommand("mac join abp");
      loraWaitResponse(10000);
      if (strstr(response, "accepted")) {
        loraSendCommand("mac get adr");
        i = 0;
        state = JOINED;
      }
      break;
    case JOINED:
      if (i++ < 100) {
        LoRaWANPayload[0] = GET_INTEGER_PART_UNSIGNED_MAX255(TEMPERATURA);
        LoRaWANPayload[1] = GET_FRACTIONAL_PART_MAX255(TEMPERATURA);
        LoRaWANPayload[2] = GET_INTEGER_PART_UNSIGNED_MAX255(HUMEDAD);
        LoRaWANPayload[3] = GET_FRACTIONAL_PART_MAX255(HUMEDAD);
        LoRaWANPayload[4] = GET_INTEGER_PART_UNSIGNED_MAX255(TEMPERATURA2);
        LoRaWANPayload[5] = GET_FRACTIONAL_PART_MAX255(TEMPERATURA2);
        LoRaWANPayload[6] = GET_INTEGER_PART_UNSIGNED_MAX255(HUMEDAD2);
        LoRaWANPayload[7] = GET_FRACTIONAL_PART_MAX255(HUMEDAD2);
        loraSendData(1, LoRaWANPayload, 8);
        digitalWrite(PIN_LED, HIGH);
        if (strstr(response, "mac_tx_ok")) {
          delay(200);
          digitalWrite(PIN_LED, LOW);
          delay(200);
          digitalWrite(PIN_LED, HIGH);
          i = 0;
        }
        delay(200);
        digitalWrite(PIN_LED, LOW);
      }
      else
        state = INIT;
      break;
  }
}
