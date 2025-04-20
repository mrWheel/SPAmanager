#ifndef SETTINGS_CLASS_H
#define SETTINGS_CLASS_H

#include <string>
#include <vector>
#include <map>
#include "LittleFS.h"
#include "Arduino.h"
#include <ArduinoJson.h>

// Field descriptor structure
struct FieldDescriptor 
{
  std::string fieldName;      // Name of the field in the settings struct
  std::string fieldPrompt;    // Display name for the field
  std::string fieldType;      // "s" for string, "n" for numeric
  
  // For string fields
  size_t fieldLen;            // Maximum length for string fields (only used if fieldType is "s")
  
  // For numeric fields (only used if fieldType is "n")
  int fieldMin;               // Minimum value
  int fieldMax;               // Maximum value
  float fieldStep;            // Step value
  
  // Pointer to the actual data
  void* fieldValue;           // Pointer to the actual field value
};

// Settings container class
class SettingsContainer 
{
private:
  std::string settingsName;
  std::string settingsFile;
  std::string settingsTarget;
  std::vector<FieldDescriptor> fields;

public:
  SettingsContainer() {}
  
  SettingsContainer(const std::string& name, const std::string& file, const std::string& target)
    : settingsName(name), settingsFile(file), settingsTarget(target) {}
    
  void addField(const FieldDescriptor& field) 
  {
    fields.push_back(field);
  }

  const std::string& getName() const { return settingsName; }
  const std::string& getFile() const { return settingsFile; }
  const std::string& getTarget() const { return settingsTarget; }
  const std::vector<FieldDescriptor>& getFields() const { return fields; }
};

class SettingsClass 
{
private:
  // Settings containers
  std::map<std::string, SettingsContainer> settingsContainers;
  Stream* debug = nullptr;
  
  // Helper method to get a string value from a void pointer
  std::string getStringValue(void* ptr) 
  {
    std::string* strPtr = static_cast<std::string*>(ptr);
    std::string value = *strPtr;
    
    // Verify the string was retrieved correctly
    if (debug && doDebug) 
    {
      debug->printf("getStringValue(): Retrieved value: [%s], length: %d\n", 
                  value.c_str(), value.length());
    }
    
    return value;
  }
  
  // Helper method to get a numeric value from a void pointer
  int getNumericValue(void* ptr) 
  {
    return *static_cast<uint8_t*>(ptr);
  }
  
  // Helper method to set a string value
  void setStringValue(void* ptr, const std::string& value) 
  {
    std::string* strPtr = static_cast<std::string*>(ptr);
    *strPtr = value;
    
    // Verify the string was set correctly
    if (debug && doDebug) 
    {
      debug->printf("setStringValue(): Set value: [%s], actual stored: [%s]\n", 
                  value.c_str(), strPtr->c_str());
    }
}
  
  // Helper method to set a numeric value
  void setNumericValue(void* ptr, int value) 
  {
    *static_cast<uint8_t*>(ptr) = static_cast<uint8_t>(value);
  }

public:
  // Device settings data
  std::string hostname;
  uint8_t scrollSnelheid;
  uint8_t LDRMinWaarde;
  uint8_t LDRMaxWaarde;
  uint8_t maxIntensiteitLeds;
  std::string skipItems;
  uint8_t tickerSpeed;
  
  // Weerlive settings data
  std::string weerliveAuthToken;
  std::string weerlivePlaats;
  uint8_t weerliveRequestInterval;
  
  // Mediastack settings data
  std::string mediastackAuthToken;
  uint8_t mediastackMaxMessages;
  uint8_t mediastackRequestInterval;
  uint8_t mediastackOnlyDuringDay;
  
  // Parola settings data
  uint8_t parolaHardwareType;
  uint8_t parolaNumDevices;
  uint8_t parolaNumZones;
  uint8_t parolaPinDIN;
  uint8_t parolaPinCS;
  uint8_t parolaPinCLK;
  
  // rssfeed settings data
  uint8_t requestInterval = 60; // Default request interval in minutes
  std::string domain0 = {};
  std::string path0  = {};
  uint8_t maxFeeds0 = 0;
  std::string domain1 = {};
  std::string path1  = {};
  uint8_t maxFeeds1 = 0;
  std::string domain2 = {};
  std::string path2  = {};
  uint8_t maxFeeds2 = 0;
  std::string domain3 = {};
  std::string path3  = {};
  uint8_t maxFeeds3 = 0;
  std::string domain4 = {};
  std::string path4  = {};
  uint8_t maxFeeds4 = 0;
  std::string domain5 = {};
  std::string path5  = {};
  uint8_t maxFeeds5 = 0;
  std::string domain6 = {};
  std::string path6  = {};
  uint8_t maxFeeds6 = 0;
  std::string domain7 = {};
  std::string path7  = {};
  uint8_t maxFeeds7 = 0;

  SettingsClass();
  void setDebug(Stream* debugPort);
  
  // Initialize all settings containers
  void initializeSettingsContainers();
  const SettingsContainer* getSettingsContainer(const std::string& settingsType) const;
  
  // Generic methods
  std::string buildJsonFieldsString(const std::string& settingsType);
  void readSettingFields(const std::string& settingsType);
  void writeSettingFields(const std::string& settingsType);
  
  // Generic method to save settings based on target
  void saveSettings(const std::string& target);
#ifdef SETTINGS_DEBUG
  bool doDebug = true;
#else
  bool doDebug = false;
#endif
};

#endif // SETTINGS_CLASS_H
