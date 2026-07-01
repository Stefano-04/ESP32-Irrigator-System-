#pragma once
#include <Arduino.h>

class SoilSensor{
  private:
  int pin;
  String nomeVaso;
  float media;
  int valoreSecco;
  int valoreUmido;
  public:
  SoilSensor(int p, String n, int aria = 4095, int acqua = 1500); //costruttore

  void begin();              // Imposta il pinMode
  float leggi();              // Esegue la lettura multipla e fa la media
  int getPercentuale();      // Calcola e restituisce il valore 0-100%
  String getNome();
};