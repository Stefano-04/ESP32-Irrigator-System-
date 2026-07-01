#include "AirSensor.h"
#include <Arduino.h>
AirSensor::AirSensor(int p, uint8_t type) : dht(p, type){
  pin = p;
  nome = "Sensore di umidita' e di temperatura dell'aria ";
  temp=0.0;
  humi=0.0;
}
void AirSensor::begin(){
  dht.begin();
}

float AirSensor::leggiTemp(){
  return  dht.readTemperature();
}
float AirSensor::leggiHumi(){
  return dht.readHumidity();
}

String AirSensor::getNome() {
  return nome;
}