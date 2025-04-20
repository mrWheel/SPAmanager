#include "ParolaManager.h"

ParolaManager::ParolaManager(uint8_t csPin) : csPin(csPin) {}

void ParolaManager::begin(const PAROLA &config)
{
    MD_MAX72XX::moduleType_t hardware;
    switch (config.HARDWARE_TYPE)
    {
    case 0: hardware = MD_MAX72XX::PAROLA_HW; break;
    case 1: hardware = MD_MAX72XX::FC16_HW; break;
    case 2: hardware = MD_MAX72XX::GENERIC_HW; break;
    default: hardware = MD_MAX72XX::FC16_HW; break;
    }

    parola = new MD_Parola(hardware, csPin, config.MAX_DEVICES);
    parola->begin(config.MAX_ZONES);
    parola->setSpeed(config.MAX_SPEED);
}

void ParolaManager::setRandomEffects(const std::vector<uint8_t> &effects)
{
    effectList = effects;
}

void ParolaManager::setCallback(std::function<void(const String&)> callback)
{
    onFinished = callback;
}
void ParolaManager::sendNextText(const String &text)
{
    currentText = text;
    
    // Cast the random effect to textEffect_t
    textEffect_t effectIn = static_cast<textEffect_t>(effectList[random(0, effectList.size())]);
    textEffect_t effectOut = static_cast<textEffect_t>(effectList[random(0, effectList.size())]);
    
    // The displayText function expects:
    // (text, alignment, speed, pause, effectIn, effectOut)
    parola->displayText(
        (char *)currentText.c_str(),
        PA_CENTER,  // Alignment
        50,         // Speed (you might want to use a config value here)
        1000,       // Pause time in milliseconds
        effectIn,   // Entry effect
        effectOut   // Exit effect
    );
} // sendNextText()

void ParolaManager::loop()
{
    if (parola->displayAnimate())
    {
        if (onFinished)
            onFinished(currentText);
    }
}