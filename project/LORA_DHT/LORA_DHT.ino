#include <DHT.h>
#include <RTCZero.h>
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

float SENSOR = 8;
float HUMEDAD;
float TEMPERATURA;
DHT dht(SENSOR,DHT11);
char cmd[200];
uint8_t LoRaWANPayload[4];


/*
 * COSAS PARA EL BAJO CONSUMO
*/
/* Change these values to set the current initial time */
const byte seconds = 0;
const byte minutes = 40;
const byte hours = 12;

/* Change these values to set the current initial date */
const byte day = 9;
const byte month = 9;
const byte year = 22;

int matchSS;
RTCZero rtc;



void allOutputs()
{
  // Configura los pines como salidas o entradas con pull-up
  // o estado definido, porque si quedan flotando aumenta el consumo
  pinMode(0,OUTPUT);
  pinMode(1,OUTPUT);
  pinMode(2,OUTPUT);
  pinMode(3,OUTPUT);
  pinMode(4,OUTPUT);
  pinMode(5,OUTPUT);
  pinMode(6,OUTPUT);
  pinMode(7,OUTPUT);
//  pinMode(8,OUTPUT); //EL 8 NO LO TOCA PORQUE ES EL QUE USA PARA EL SENSADO
  pinMode(9,OUTPUT);
  pinMode(10,OUTPUT);
  pinMode(11,OUTPUT);
  pinMode(12,OUTPUT);
//  pinMode(13,OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP); 
  pinMode(14,OUTPUT);
  pinMode(15,OUTPUT);
  pinMode(16,OUTPUT);
  pinMode(17,OUTPUT);
  pinMode(18,OUTPUT);
  pinMode(19,OUTPUT);
  pinMode(20,OUTPUT);
  pinMode(21,OUTPUT);
  pinMode(A0,OUTPUT);
  pinMode(A1,OUTPUT);
  pinMode(A2,OUTPUT);
  pinMode(A3,OUTPUT);
}
/*
 ===========================
*/





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
  allOutputs(); //LORA BAJO CONSUMO============
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

//BAJO CONSUMO================
    // Arranca el RTC
  rtc.begin();
  rtc.setTime(hours, minutes, seconds);
  rtc.setDate(day, month, year);

  // Inicia un match para que interrumpa a los 10"
  matchSS = 18;
  rtc.setAlarmTime(matchSS, 00, 00);
  rtc.enableAlarm(rtc.MATCH_SS);
  rtc.attachInterrupt(alarmMatch);
//============================
}

//====================BAJO CONSUMO=============
bool doLowPower()
{
  // pone al modulo LoRa en bajo consumo
  //loraSendCommand("sys sleep 120000");
 
  // Entra en bajo consumo hasta que interrumpa el RTC
  digitalWrite(PIN_LED, HIGH);  // Apaga el LED
  USBDevice.detach();           // apaga el USB
  rtc.standbyMode();            // Sleep until next alarm match

  // Al despertar retoma el USB
  USBDevice.init();
  USBDevice.attach();           // recupera el USB
  
  delay(500);
  loraSendCommand("radio get snr");
  delay(1000);
  loraSendCommand("radio get snr");

  if(!response[0]) {
    digitalWrite(loraReset, LOW);
    delay(1000);
    digitalWrite(loraReset, HIGH);
    loraWaitResponse(2000);
    return false;
  }

  return true;
}
//==========================================================

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
        debugSerial.println("cargando datos para envíar");
        LoRaWANPayload[0] = GET_INTEGER_PART_UNSIGNED_MAX255(TEMPERATURA);
        LoRaWANPayload[1] = GET_FRACTIONAL_PART_MAX255(TEMPERATURA);
        LoRaWANPayload[2] = GET_INTEGER_PART_UNSIGNED_MAX255(HUMEDAD);
        LoRaWANPayload[3] = GET_FRACTIONAL_PART_MAX255(HUMEDAD);
        loraSendData(1, LoRaWANPayload, 4);
        debugSerial.println("supuestamente envió datos al nodered");
        digitalWrite(PIN_LED, HIGH);
        
        while(!strstr(response, "mac_tx_ok")) {
          debugSerial.println("esperando respuesta");
        }
        debugSerial.println("response was ok");
        delay(200);
        digitalWrite(PIN_LED, LOW);
        delay(200);
        digitalWrite(PIN_LED, HIGH);
        i = 0;
        //BAJO CONSUMO===================
         if (!doLowPower())
          state = INIT;
        //===============================
        delay(200);
        digitalWrite(PIN_LED, LOW);
      }
      else
        state = INIT;
      break;
  }
}
//================BAJO CONSUMO===================
void alarmMatch()
{
  // Larga de nuevo la alarma 20" adelante
  matchSS = (matchSS + 2) % 24; 
  rtc.setAlarmTime(matchSS, 00, 00);
}
//================================================
