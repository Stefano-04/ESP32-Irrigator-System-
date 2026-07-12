#include "SoilSensor.h"
#include <Arduino.h>

const int N_LETTURE=5;  //numero letture per Soil Moisture Sensor
// Implementazione del costruttore
SoilSensor::SoilSensor(int p, const char* n, int aria, int acqua) {
  pin = p;
  nomeVaso = n;
  valoreSecco = aria;
  valoreUmido = acqua;
  media = 0.0;
}
void SoilSensor::begin(){
  pinMode(pin, INPUT);
}

float SoilSensor::leggi(){
  long somma = 0;
  for (int i = 0; i < N_LETTURE; i++) {
    somma += analogRead(pin);
    delay(50);
  }
  media = (float)somma / N_LETTURE;
  return media;
}

int SoilSensor::getPercentuale() {
  int percentuale = map(media, valoreSecco, valoreUmido, 0, 100);
  if (percentuale<0){
    percentuale=0;
  }
  else if (percentuale>100){
    percentuale=100;
  }
  return percentuale;
}

const char* SoilSensor::getNome() {
  return nomeVaso;
}