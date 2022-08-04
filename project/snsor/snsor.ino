#include <DHT.h>    // importa la Librerias DHT
#include <DHT_U.h>
#define debugSerial SerialUSB
int SENSOR = 8;     // pin DATA de DHT22 a pin digital 2
int TEMPERATURA;
int HUMEDAD;

DHT dht(SENSOR, DHT22);   // creacion del objeto, cambiar segundo parametro
        // por DHT11 si se utiliza en lugar del DHT22
void setup(){
  debugSerial.begin(9600);   // inicializacion de monitor serial
  dht.begin();      // inicializacion de sensor
}

void loop(){
  debugSerial.print("start");
  delay(500);
  TEMPERATURA = dht.readTemperature();  // obtencion de valor de temperatura
  debugSerial.print("se leyó la temperatura");
  HUMEDAD = dht.readHumidity();   // obtencion de valor de humedad
  debugSerial.print("Temperatura: ");  // escritura en monitor serial de los valores
  debugSerial.print(TEMPERATURA);
  debugSerial.print(" Humedad: ");
  debugSerial.println(HUMEDAD);
  delay(500);
}
