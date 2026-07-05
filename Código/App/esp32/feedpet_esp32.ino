/*
  Feedpet ESP32 - Comedouro automático (com agendamento real)

  Pinos utilizados:
  - GPIO 13: Servo do comedouro (ração)
  - GPIO 34: Sensor de nível de ração (analógico)
  - GPIO 35: Sensor de nível de água (analógico)

  Instale as bibliotecas (Gerenciador de Bibliotecas do Arduino IDE):
  - ESP32Servo      (by Kevin Harrington)
  - ArduinoJson     (by Benoit Blanchon)
  - Preferences     (já vem com o core do ESP32, não precisa instalar)
  - ESPmDNS         (já vem com o core do ESP32, não precisa instalar)

  NOVIDADES desta versão:
  - Quantidade em gramas: /food agora aceita ?g=NN e os agendamentos
    salvam quanto liberar em cada horário. É convertido em tempo de
    abertura do servo usando MS_PER_GRAM (veja CALIBRAÇÃO abaixo).
  - Dias da semana: cada agendamento pode ter uma lista "days" (1=Seg
    ... 7=Dom). Lista vazia = todos os dias.
  - Página de status em "/" e "/qr" que mostra o IP do dispositivo de
    forma bem visível no navegador e um QR code pra escanear pelo app
    (formato feedpet://IP:PORTA).
  - Hostname mDNS "feedpet.local" — em várias redes dá pra acessar o
    ESP32 sem nem digitar o IP, só http://feedpet.local
  - O IP também é impresso bem destacado no Serial Monitor ao conectar.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <time.h>

// ============ CONFIGURAÇÕES - EDITE AQUI ============
const char* WIFI_SSID = "NOME_DA_SUA_REDE";
const char* WIFI_PASS = "SENHA_DA_SUA_REDE";

// Nome de acesso amigável: http://feedpet.local (nem todo roteador/
// celular suporta mDNS, por isso o IP numérico continua funcionando)
const char* MDNS_HOSTNAME = "feedpet";

// Fuso horário do Brasil (UTC-3). Ajuste se necessário.
const long GMT_OFFSET_SEC = -3 * 3600;
const int DAYLIGHT_OFFSET_SEC = 0;
// ====================================================

// ============ CALIBRAÇÃO DA QUANTIDADE ============
// Quanto tempo (em ms) o servo/rosca fica aberto para liberar 1 grama
// de ração. Isso MUDA de acordo com o formato da ração e o mecanismo.
//
// Como calibrar:
//   1) Rode dispenseFood(100) uma vez (ex: chame GET /food?g=100).
//   2) Pese quantos gramas realmente caíram.
//   3) MS_PER_GRAM = tempo_usado_ms / gramas_reais_que_caíram
//   4) Ajuste o valor abaixo e repita até bater com o peso real.
const float MS_PER_GRAM = 18.0;   // valor inicial, PRECISA calibrar
const int MIN_DISPENSE_MS = 150;  // tempo mínimo de abertura (evita "tremer" sem soltar nada)
const int MAX_DISPENSE_MS = 8000; // trava de segurança (evita motor preso ligado indefinidamente)
const int DEFAULT_GRAMS = 40;     // usado se o app não enviar quantidade
// ====================================================

// Pinos
const int SERVO_FOOD_PIN = 13;
const int SENSOR_FOOD_PIN = 34;
const int SENSOR_WATER_PIN = 35;

// Configurações do servo
const int SERVO_OPEN_ANGLE = 90;   // Ângulo para abrir
const int SERVO_CLOSE_ANGLE = 0;   // Ângulo para fechar

// Limites de armazenamento
const int MAX_SCHEDULES = 20;
const size_t SCHEDULES_JSON_CAPACITY = 6144;

WebServer server(80);
Servo foodServo;
Preferences prefs;

// Evita disparar o mesmo horário várias vezes dentro do mesmo minuto
String lastTriggerKey = "";
String deviceIP = "";

// ─────────────────────────────────────────
// Lê o nível do sensor (0 a 4095 → 0 a 100%)
// Ajuste os valores min/max conforme seu sensor
// ─────────────────────────────────────────
int readLevel(int pin) {
  int raw = analogRead(pin);
  int level = map(raw, 0, 4095, 0, 100);
  return constrain(level, 0, 100);
}

// ─────────────────────────────────────────
// Dispensa ração. [grams] é convertido em tempo de abertura
// do servo/rosca usando a calibração MS_PER_GRAM.
// ─────────────────────────────────────────
void dispenseFood(int grams) {
  if (grams <= 0) grams = DEFAULT_GRAMS;

  int ms = (int)(grams * MS_PER_GRAM);
  ms = constrain(ms, MIN_DISPENSE_MS, MAX_DISPENSE_MS);

  Serial.printf("Dispensando ~%dg (%dms de abertura)\n", grams, ms);

  foodServo.write(SERVO_OPEN_ANGLE);
  delay(ms);
  foodServo.write(SERVO_CLOSE_ANGLE);
}

// ─────────────────────────────────────────
// CORS headers para requisições do app
// ─────────────────────────────────────────
void addCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// ─────────────────────────────────────────
// Persistência dos agendamentos (NVS)
// Formato salvo: array JSON de objetos
//   [{"id":"...", "type":"food", "hour":8, "minute":30, "enabled":true,
//     "grams":40, "days":[1,5]}, ...]
// ─────────────────────────────────────────
bool loadSchedules(JsonDocument& doc) {
  prefs.begin("feedpet", true); // somente leitura
  String raw = prefs.getString("schedules", "[]");
  prefs.end();

  DeserializationError err = deserializeJson(doc, raw);
  if (err) {
    doc.clear();
    doc.to<JsonArray>();
    return false;
  }
  if (!doc.is<JsonArray>()) {
    doc.clear();
    doc.to<JsonArray>();
  }
  return true;
}

void saveSchedules(JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  prefs.begin("feedpet", false); // leitura/escrita
  prefs.putString("schedules", out);
  prefs.end();
}

// Insere ou atualiza (por id) um agendamento
void upsertSchedule(const JsonObject& incoming) {
  DynamicJsonDocument doc(SCHEDULES_JSON_CAPACITY);
  loadSchedules(doc);
  JsonArray arr = doc.as<JsonArray>();

  String id = incoming["id"].as<String>();
  bool found = false;

  for (JsonObject item : arr) {
    if (item["id"].as<String>() == id) {
      item["type"]    = incoming["type"];
      item["hour"]    = incoming["hour"];
      item["minute"]  = incoming["minute"];
      item["enabled"] = incoming["enabled"];
      item["grams"]   = incoming["grams"] | DEFAULT_GRAMS;
      item.remove("days");
      JsonArray days = item.createNestedArray("days");
      if (incoming.containsKey("days")) {
        for (JsonVariant d : incoming["days"].as<JsonArray>()) days.add(d.as<int>());
      }
      found = true;
      break;
    }
  }

  if (!found) {
    if (arr.size() >= MAX_SCHEDULES) {
      // Remove o mais antigo (primeiro) para abrir espaço
      arr.remove(0);
    }
    JsonObject newItem = arr.createNestedObject();
    newItem["id"]      = id;
    newItem["type"]    = incoming["type"];
    newItem["hour"]    = incoming["hour"];
    newItem["minute"]  = incoming["minute"];
    newItem["enabled"] = incoming["enabled"];
    newItem["grams"]   = incoming["grams"] | DEFAULT_GRAMS;
    JsonArray days = newItem.createNestedArray("days");
    if (incoming.containsKey("days")) {
      for (JsonVariant d : incoming["days"].as<JsonArray>()) days.add(d.as<int>());
    }
  }

  saveSchedules(doc);
}

bool deleteScheduleById(const String& id) {
  DynamicJsonDocument doc(SCHEDULES_JSON_CAPACITY);
  loadSchedules(doc);
  JsonArray arr = doc.as<JsonArray>();

  for (size_t i = 0; i < arr.size(); i++) {
    if (arr[i]["id"].as<String>() == id) {
      arr.remove(i);
      saveSchedules(doc);
      return true;
    }
  }
  return false;
}

// ─────────────────────────────────────────
// Rota: GET /status
// Retorna JSON com níveis de ração e água
// ─────────────────────────────────────────
void handleStatus() {
  addCorsHeaders();

  StaticJsonDocument<160> doc;
  doc["food_level"] = readLevel(SENSOR_FOOD_PIN);
  doc["water_level"] = readLevel(SENSOR_WATER_PIN);
  doc["device"] = "Feedpet-ESP32";
  doc["ip"] = deviceIP;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// ─────────────────────────────────────────
// Rota: GET /food?g=NN
// Dispensa ração imediatamente. "g" é opcional (gramas).
// ─────────────────────────────────────────
void handleFood() {
  addCorsHeaders();
  int grams = DEFAULT_GRAMS;
  if (server.hasArg("g")) {
    grams = server.arg("g").toInt();
  }
  dispenseFood(grams);
  server.send(200, "text/plain", "OK");
  Serial.println("Ração dispensada via app!");
}

// ─────────────────────────────────────────
// Rota: GET /schedule
// Lista todos os agendamentos salvos
// ─────────────────────────────────────────
void handleGetSchedules() {
  addCorsHeaders();
  DynamicJsonDocument doc(SCHEDULES_JSON_CAPACITY);
  loadSchedules(doc);
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// ─────────────────────────────────────────
// Rota: POST /schedule
// Recebe um agendamento {id,type,hour,minute,enabled,grams,days}
// e salva (insere ou atualiza) na memória NVS
// ─────────────────────────────────────────
void handlePostSchedule() {
  addCorsHeaders();

  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Corpo vazio");
    return;
  }

  String body = server.arg("plain");
  DynamicJsonDocument incoming(512);
  DeserializationError err = deserializeJson(incoming, body);

  if (err || !incoming.containsKey("id") || !incoming.containsKey("hour") ||
      !incoming.containsKey("minute")) {
    server.send(400, "text/plain", "JSON inválido");
    return;
  }

  if (!incoming.containsKey("type")) incoming["type"] = "food";
  if (!incoming.containsKey("enabled")) incoming["enabled"] = true;
  if (!incoming.containsKey("grams")) incoming["grams"] = DEFAULT_GRAMS;

  upsertSchedule(incoming.as<JsonObject>());

  Serial.print("Agendamento salvo: ");
  Serial.println(body);

  server.send(200, "text/plain", "OK");
}

// ─────────────────────────────────────────
// Rota: DELETE /schedule?id=xxxx
// Remove um agendamento salvo
// ─────────────────────────────────────────
void handleDeleteSchedule() {
  addCorsHeaders();

  if (!server.hasArg("id")) {
    server.send(400, "text/plain", "Faltando parâmetro id");
    return;
  }

  bool removed = deleteScheduleById(server.arg("id"));
  server.send(removed ? 200 : 404, "text/plain", removed ? "OK" : "Não encontrado");
}

// ─────────────────────────────────────────
// Rota: OPTIONS (preflight CORS)
// ─────────────────────────────────────────
void handleOptions() {
  addCorsHeaders();
  server.send(204);
}

// ─────────────────────────────────────────
// Rota: GET / e GET /qr
// Página simples mostrando o IP em destaque e um QR code
// (feedpet://IP:80) para escanear direto no app.
// ─────────────────────────────────────────
void handleRoot() {
  addCorsHeaders();

  String qrData = "feedpet://" + deviceIP + ":80";

  String html = "<!DOCTYPE html><html lang='pt-br'><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Feedpet</title><style>";
  html += "body{font-family:sans-serif;background:#0D0D0F;color:#F1F1F3;";
  html += "display:flex;flex-direction:column;align-items:center;justify-content:center;";
  html += "min-height:100vh;margin:0;padding:24px;text-align:center}";
  html += "h1{font-size:28px;margin-bottom:4px}";
  html += "p{color:#9090A0;margin-top:0}";
  html += ".ip{font-size:40px;font-weight:800;color:#4ADE80;letter-spacing:1px;";
  html += "background:#1A1A1F;padding:16px 28px;border-radius:16px;margin:20px 0}";
  html += ".card{background:#1A1A1F;padding:20px;border-radius:16px;margin-top:16px}";
  html += "</style></head><body>";
  html += "<h1>🐾 Feedpet</h1>";
  html += "<p>Use este IP nas Configurações do app, ou escaneie o QR code</p>";
  html += "<div class='ip'>" + deviceIP + "</div>";
  html += "<div class='card'>";
  html += "<img src='https://api.qrserver.com/v1/create-qr-code/?size=220x220&data=" + qrData + "' width='220' height='220' alt='QR code'>";
  html += "<p style='margin-top:10px'>Porta: 80</p>";
  html += "</div>";
  html += "<p style='margin-top:24px;font-size:12px'>Hostname alternativo: http://" + String(MDNS_HOSTNAME) + ".local</p>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

// ─────────────────────────────────────────
// Verifica a cada chamada do loop() se algum
// agendamento ativo bate com a hora atual (e o dia da semana)
// ─────────────────────────────────────────
void checkSchedules() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) {
    return; // hora ainda não sincronizada
  }

  char key[16];
  snprintf(key, sizeof(key), "%d-%02d-%02d", timeinfo.tm_yday,
           timeinfo.tm_hour, timeinfo.tm_min);
  String currentKey = String(key);

  if (currentKey == lastTriggerKey) {
    return; // já verificamos neste minuto
  }

  // tm_wday: 0=domingo ... 6=sábado. Convertendo para o padrão do app
  // (1=segunda ... 7=domingo):
  int today = timeinfo.tm_wday == 0 ? 7 : timeinfo.tm_wday;

  DynamicJsonDocument doc(SCHEDULES_JSON_CAPACITY);
  loadSchedules(doc);
  JsonArray arr = doc.as<JsonArray>();

  for (JsonObject item : arr) {
    bool enabled = item["enabled"] | false;
    int hour = item["hour"] | -1;
    int minute = item["minute"] | -1;
    int grams = item["grams"] | DEFAULT_GRAMS;

    if (!enabled || hour != timeinfo.tm_hour || minute != timeinfo.tm_min) continue;

    // Verifica se hoje está na lista de dias (lista vazia = todos os dias)
    bool dayMatches = true;
    if (item.containsKey("days")) {
      JsonArray days = item["days"].as<JsonArray>();
      if (days.size() > 0) {
        dayMatches = false;
        for (JsonVariant d : days) {
          if (d.as<int>() == today) { dayMatches = true; break; }
        }
      }
    }

    if (dayMatches) {
      Serial.println("Horário agendado bateu — dispensando ração automaticamente!");
      dispenseFood(grams);
      break; // dispensa uma vez mesmo se houver mais de um match no minuto
    }
  }

  lastTriggerKey = currentKey;
}

// ─────────────────────────────────────────
// Mostra o IP de forma bem visível no Serial Monitor
// ─────────────────────────────────────────
void printIPBanner() {
  Serial.println();
  Serial.println("========================================");
  Serial.println("   FEEDPET CONECTADO COM SUCESSO");
  Serial.println("========================================");
  Serial.print("   IP do dispositivo : ");
  Serial.println(deviceIP);
  Serial.print("   Endereço alternativo : http://");
  Serial.print(MDNS_HOSTNAME);
  Serial.println(".local");
  Serial.print("   Página com QR code   : http://");
  Serial.print(deviceIP);
  Serial.println("/qr");
  Serial.println("========================================");
  Serial.println("   Copie o IP acima para o app Feedpet");
  Serial.println("   (Configurações > inserir IP manualmente)");
  Serial.println("   ou escaneie o QR code na página /qr");
  Serial.println("========================================\n");
}

// ─────────────────────────────────────────
// Setup
// ─────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Feedpet ESP32 ===");

  // Servo
  ESP32PWM::allocateTimer(0);
  foodServo.setPeriodHertz(50);
  foodServo.attach(SERVO_FOOD_PIN, 500, 2400);
  foodServo.write(SERVO_CLOSE_ANGLE);

  // WiFi
  Serial.print("Conectando ao WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    deviceIP = WiFi.localIP().toString();
    printIPBanner();

    // mDNS: permite acessar via http://feedpet.local em vez do IP puro
    if (MDNS.begin(MDNS_HOSTNAME)) {
      MDNS.addService("http", "tcp", 80);
      Serial.println("mDNS ativo: http://" + String(MDNS_HOSTNAME) + ".local");
    } else {
      Serial.println("Falha ao iniciar mDNS (o IP numérico continua funcionando).");
    }

    // Sincroniza hora via NTP (necessário para o agendamento automático)
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");
    Serial.print("Sincronizando hora via NTP");
    struct tm timeinfo;
    int ntpAttempts = 0;
    while (!getLocalTime(&timeinfo, 1000) && ntpAttempts < 10) {
      Serial.print(".");
      ntpAttempts++;
    }
    if (getLocalTime(&timeinfo)) {
      Serial.println("\nHora sincronizada: " + String(asctime(&timeinfo)));
    } else {
      Serial.println("\nFalha ao sincronizar hora — agendamento automático não vai funcionar até reconectar.");
    }
  } else {
    Serial.println("\nFalha na conexão WiFi. Verifique as credenciais.");
  }

  // Rotas do servidor HTTP
  server.on("/",         HTTP_GET,     handleRoot);
  server.on("/qr",        HTTP_GET,     handleRoot);
  server.on("/status",   HTTP_GET,     handleStatus);
  server.on("/food",     HTTP_GET,     handleFood);
  server.on("/schedule", HTTP_GET,     handleGetSchedules);
  server.on("/schedule", HTTP_POST,    handlePostSchedule);
  server.on("/schedule", HTTP_DELETE,  handleDeleteSchedule);
  server.on("/status",   HTTP_OPTIONS, handleOptions);
  server.on("/food",     HTTP_OPTIONS, handleOptions);
  server.on("/schedule", HTTP_OPTIONS, handleOptions);

  server.begin();
  Serial.println("Servidor HTTP iniciado na porta 80");
  Serial.println("========================\n");
}

// ─────────────────────────────────────────
// Loop
// ─────────────────────────────────────────
void loop() {
  server.handleClient();
  checkSchedules();

  // Se o IP mudar (ex: reconexão de Wi-Fi com DHCP diferente), atualiza
  if (WiFi.status() == WL_CONNECTED) {
    String currentIP = WiFi.localIP().toString();
    if (currentIP != deviceIP && currentIP != "0.0.0.0") {
      deviceIP = currentIP;
      printIPBanner();
    }
  }
}
