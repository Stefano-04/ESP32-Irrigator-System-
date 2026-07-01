#pragma once //evita che file diversi carichino questa libreria più volte

#include <Arduino.h>
#include <DHT.h>

class AirSensor{
  private:
  DHT dht;
  int pin;
  float temp;
  float humi;
  String nome;
  public:
  AirSensor(int p, uint8_t type); //costruttore
  void begin();
  float leggiTemp();
  float leggiHumi();
  String getNome();
};