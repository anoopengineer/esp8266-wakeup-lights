#ifndef WEBCONFIG_H
#define WEBCONFIG_H

#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

#include <vector>

class WebConfig {
   public:
    struct ConfigField {
        String name;         // Name of the configuration field
        String type;         // Data type (e.g., "string", "int", "float")
        String value;        // Current value of the field
        String description;  // Description of the field (for the form)
    };

    // Constructor
    WebConfig(uint16_t port = 80);

    // Add a new configuration field
    void addField(const String &name, const String &type,
                  const String &defaultValue, const String &description);

    // Begin the configuration manager with specified (or default) API endpoint
    // and port
    void begin(const char *apiEndpoint = "/");

    // Handle client requests (should be called in loop)
    void handleClient();

    // Get the value of a configuration field by name
    String getValue(const String &name) const;

   private:
    ESP8266WebServer server;
    std::vector<ConfigField> configFields;
    const char *configFilePath = "/config.json";
    String apiPath;

    void loadConfig();
    bool saveConfig();
    void setupWebServer();
    void handleGetConfig();
    void handleSaveConfig();
    void handleSuccess();
    void handleFailed();
};

#endif  // WEBCONFIG_H
