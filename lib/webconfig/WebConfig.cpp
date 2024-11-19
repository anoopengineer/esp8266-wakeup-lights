#include "WebConfig.h"

// Constructor
WebConfig::WebConfig(uint16_t port) : server(port) {}
// Add a new configuration field
void WebConfig::addField(const String &name, const String &type,
                         const String &defaultValue,
                         const String &description) {
    ConfigField field = {name, type, defaultValue, description};
    configFields.push_back(field);
}

// Initialize the configuration manager with optional API endpoint and port
void WebConfig::begin(const char *apiEndpoint) {
    apiPath = apiEndpoint;  // Set API path

    if (!LittleFS.begin()) {
        Serial.println("Failed to mount LittleFS");
        return;
    }

    loadConfig();
    setupWebServer();
    server.begin();
    Serial.printf("Web server started on port %d with API path: %s\n",
                  server.getServer().port(), apiPath.c_str());
}

// Handle client requests
void WebConfig::handleClient() { server.handleClient(); }

// Load configuration from LittleFS
void WebConfig::loadConfig() {
    if (LittleFS.exists(configFilePath)) {
        File file = LittleFS.open(configFilePath, "r");
        if (file) {
            StaticJsonDocument<512> doc;
            DeserializationError error = deserializeJson(doc, file);
            if (!error) {
                for (auto &field : configFields) {
                    if (doc[field.name].is<String>()) {
                        field.value = doc[field.name].as<String>();
                    }
                }
            }
            file.close();
        }
    }
}

// Save configuration to LittleFS
bool WebConfig::saveConfig() {
    File file = LittleFS.open(configFilePath, "w");
    if (file) {
        StaticJsonDocument<512> doc;
        for (auto &field : configFields) {
            doc[field.name] = field.value;
        }
        serializeJson(doc, file);
        file.close();
        return true;
    } else {
        return false;
    }
}

// Set up web server routes
void WebConfig::setupWebServer() {
    server.on(apiPath.c_str(), HTTP_GET, [this]() { handleGetConfig(); });
    server.on(apiPath.c_str(), HTTP_POST, [this]() { handleSaveConfig(); });
    server.on((apiPath + "/success").c_str(), HTTP_GET,
              [this]() { handleSuccess(); });
    server.on((apiPath + "/failed").c_str(), HTTP_GET,
              [this]() { handleFailed(); });
}

// Handle GET request to serve HTML form with configuration data
void WebConfig::handleGetConfig() {
    // String html = "<html><body><h1>Configuration</h1><form action='" +
    // apiPath +
    //               "' method='POST'>";
    // for (auto &field : configFields) {
    //     html += "<label>" + field.description + "</label><br>";
    //     html += "<input type='text' name='" + field.name + "' value='" +
    //             field.value + "'><br><br>";
    // }
    // html += "<input type='submit' value='Save'></form></body></html>";

    String html =
        "<html><head><meta name='viewport' content='width=device-width, "
        "initial-scale=1'>"
        "<link rel='stylesheet' "
        "href='https://stackpath.bootstrapcdn.com/bootstrap/4.5.2/css/"
        "bootstrap.min.css'>"
        "<style>"
        "body { padding: 10px; }"
        ".container { max-width: 100%; padding: 10px; }"
        "h1 { font-size: 1.5rem; margin-bottom: 1rem; }"
        ".form-group label { font-size: 0.9rem; }"
        ".btn { width: 100%; font-size: 1rem; padding: 0.75rem; }"
        "</style></head><body>";

    html += "<div class='container'>";
    html += "<h1 class='text-center'>Configuration</h1>";
    html += "<form action='" + apiPath + "' method='POST'>";

    for (auto &field : configFields) {
        html += "<div class='form-group'>";
        html +=
            "<label for='" + field.name + "'>" + field.description + "</label>";
        html += "<input type='text' id='" + field.name + "' name='" +
                field.name + "' value='" + field.value +
                "' class='form-control'>";
        html += "</div>";
    }

    html += "<button type='submit' class='btn btn-primary mt-4'>Save</button>";
    html += "</form></div></body></html>";

    server.send(200, "text/html", html);
}

// Handle POST request to save configuration data
void WebConfig::handleSaveConfig() {
    if (server.args() == 0) {
        server.sendHeader("Location", apiPath + "/failed");
        server.send(303);
        return;
    }

    for (auto &field : configFields) {
        if (server.hasArg(field.name)) {
            field.value = server.arg(field.name);
        }
    }

    if (saveConfig()) {
        server.sendHeader("Location", apiPath + "/success");
        server.send(303);
    } else {
        server.sendHeader("Location", apiPath + "/failed");
        server.send(303);
    }
}

// Handle success endpoint
void WebConfig::handleSuccess() {
    String html = "<html><body><h1>Configuration Saved Successfully</h1>";
    html += "<p>Your configuration has been saved.</p>";
    html += "<a href='" + apiPath +
            "'>Return to Configuration Page</a></body></html>";
    server.send(200, "text/html", html);
}

// Handle failed endpoint
void WebConfig::handleFailed() {
    String html = "<html><body><h1>Configuration Save Failed</h1>";
    html +=
        "<p>There was an error saving your configuration. Please try "
        "again.</p>";
    html += "<a href='" + apiPath +
            "'>Return to Configuration Page</a></body></html>";
    server.send(500, "text/html", html);
}

// Get the value of a configuration field by name
String WebConfig::getValue(const String &name) const {
    for (const auto &field : configFields) {
        if (field.name == name) {
            return field.value;
        }
    }
    return String();  // Return empty string if field not found
}
