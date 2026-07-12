#pragma once
#include <Arduino.h>
extern const int N_LETTURE;
class SoilSensor{
  private:
  int pin;
  const char* nomeVaso;
  float media;
  int valoreSecco;
  int valoreUmido;
  public:
  SoilSensor(int p, const char* n, int aria = 4095, int acqua = 1500); //costruttore

  void begin();              // Imposta il pinMode
  float leggi();              // Esegue la lettura multipla e fa la media
  int getPercentuale();      // Calcola e restituisce il valore 0-100%
  const char* getNome();
};