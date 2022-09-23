#include <DHT.h>
#include <RTCZero.h>
//#include <ArduinoLowPower.h>
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
float SENSOR = 9;
float HUMEDAD;
float TEMPERATURA;
float HUMEDAD2;
float TEMPERATURA2;
DHT dht(SENSOR,DHT22);
DHT dht2(SENSOR2,DHT11);
char cmd[200];
uint8_t LoRaWANPayload[8];
int contador = 0; //para realizar los 5 minutos de lecturas, aprox cada envío toma 30 segundos

/* Change these values to set the current initial time */
const byte seconds = 0;
const byte minutes = 00;
const byte hours = 17;

/* Change these values to set the current initial date */
const byte day = 17;
const byte month = 11;
const byte year = 15;

int matchSS;

/* Create an rtc object */
RTCZero rtc;

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
//  pinMode(8,OUTPUT);
//  pinMode(9,OUTPUT);
  pinMode(10,OUTPUT);
  pinMode(11,OUTPUT);
  pinMode(12,OUTPUT);
  pinMode(13,OUTPUT);
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
  // Arranca el RTC
  rtc.begin();
  rtc.setTime(hours, minutes, seconds);
  rtc.setDate(day, month, year);

  // Inicia un match para que interrumpa a la hora
  matchSS = 18;
  rtc.setAlarmTime(matchSS, 00, 0);
  rtc.enableAlarm(rtc.MATCH_HHMMSS);
  rtc.attachInterrupt(alarmMatch);

  
  dht.begin();
  dht2.begin();
}

bool doLowPower()
{
  // pone al modulo LoRa en bajo consumo por una hora
  loraSendCommand("sys sleep 3000000");
  
  // Entra en bajo consumo hasta que interrumpa el RTC
  digitalWrite(PIN_LED, HIGH);  // Apaga el LED
  
  debugSerial.println("entrara en bajo consumo el cortex m0 por un minuto");
  USBDevice.detach();           // apaga el USB
  rtc.standbyMode();
//  LowPower.sleep(60000);
  
  debugSerial.println("regresa del bajo consumo");
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


void loop() {
  static int i, dr;
  delay(5000); //ESPERA 1 MINUTO. CAMBIAR POR MILLIS QUE ES NO BLOQUEANTE
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
        contador++;
        if(contador==10){
        contador = 0;
        if (!doLowPower()){
            state = INIT;
        }
        delay(200);
        digitalWrite(PIN_LED, LOW);
       }
      }else
        state = INIT;
      break;
  }
}
void alarmMatch()
  {
    // Larga de nuevo la alarma 1h adelante
    matchSS = (matchSS + 1) % 24; 
    rtc.setAlarmTime(matchSS, 00, 0);
  }
