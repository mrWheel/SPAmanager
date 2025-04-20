#pragma once
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <vector>

struct PAROLA
{
    uint8_t HARDWARE_TYPE;
    uint8_t MAX_DEVICES;
    uint8_t MAX_ZONES;
    uint8_t MAX_SPEED;
};

class ParolaManager
{
  public:
      ParolaManager(uint8_t csPin);
      void begin(const PAROLA &config);
      void setRandomEffects(const std::vector<uint8_t> &effects);
      void setCallback(std::function<void(const String&)> callback);    void sendNextText(const String &text);
      void loop();
  
  private:
      MD_Parola *parola = nullptr;
      uint8_t csPin;
      std::vector<uint8_t> effectList;
      String currentText = "";
      std::function<void(const String&)> onFinished = nullptr;  // Change from std::function<void()>
};