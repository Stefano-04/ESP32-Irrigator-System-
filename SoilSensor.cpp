#include "SoilSensor.h"
#include <Arduino.h>
// Implementazione del costruttore
SoilSensor::SoilSensor(int p, String n, int aria, int acqua) {
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
  for (int i = 0; i < 5; i++) {
    somma += analogRead(pin);
    delay(50);
  }
  media = somma / 5.0;
  return media;
}

int SoilSensor::getPercentuale() {
  int percentuale = map(media, valoreSecco, valoreUmido, 0, 100);
  return constrain(percentuale, 0, 100);
}

String SoilSensor::getNome() {
  return nomeVaso;
}