#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <string>
#include <HX711.h>
#include <Arduino.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

void updateScreen();                    // Refresh da tela
void quantitativos();                   // Leitura dos sensores
void checkGearTimer();                  // icone de notificao
boolean agendamentoCheck();             // Monitoramento da hora
void playStartupAnimation();            // Animacao de inicializacao
String formatNumber(int num);           // Formatacao de numeros para 4 digitos
void startRotinaAlimentacao();          // Rotina de despejo
void triggerGear(uint32_t duration_ms); // icone de notificao

// ---- DISPLAY 128x64 ----
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1 // Sem botao de reset
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ----- CONFIGURACOES WIFI, DATA ETC -----
const char *ssid = "Lobato";
const char *password = "A00dc729@";
const char *ntpServer = "pool.ntp.org";           // ?
const char *MDNS_HOSTNAME = "feedpet";            // Nome de acesso amigavel: http://feedpet.local ( Inconsistente )
const int daylightOffset_sec = 0;                 // Sem horario de verao(3600 para DLS)
const long gmtOffset_sec = -10800;                // GMT -3 Brasilia -> -3*60*60 = -108000

String line2Text = "Rcao";                        // Rotulo exibido no display
String line3Text = "Agua";

const int max_schedules = 10;                     // Maximo de agendamentos
const int min_time = 300;                         // tempo minimo de abertura
const int max_time = 80000;                       // trava de seguranca
const int default_grams = 40;                     // usado se o app nao enviar quantidade(g)
const int bowlCapacity = 300;                     // gramas
const int food_factor = 300;                      // miligrama/segundo
const int water_factor = 180;                     // mililitro/segundo
const long cycle_time = 2000;                     // TEMPO DE ABERTURA/FECHAMENTO(ms)
const int cFactor = 2100;                         // FATOR DE CALIBRACAO DA CELULAR DE CARGA
const size_t SCHEDULES_JSON_CAPACITY = 4096;      // tamanho do json
const unsigned long SCREEN_UPDATE_INTERVAL = 500; // Taxa de atualizacao da tela(ms)

/* ---- PINAGEM ----
====================
| 02 BLUE          |
| 04 PURPLE        |
| 12 GRAY          |
| 13 WHITE         |
| 19 GRRAY         |
| 25 YELLOW        |
| 26 ORANGE        |
| 27 BROWN         |
| 33 GREEN         |
--------------------
| RED   VCC        |
| BLACK GND        |
| WHITE A-         |
| GREEN A+         |
==================*/
const int buzzerPin = 2;
const int SCKPin = 18;
const int DTPin = 19;
const int INT3 = 12;
const int INT4 = 13;
const int INT1 = 25;
const int INT2 = 26;
const int ledPin = 27;
const int sensorPin = 33;

// Estrutura de estado atual de variaveis relevantes para atualizacao do terminal(Prototipagem)
struct tm timeinfo;
struct state{
  int hora;
  int minuto;
  int sensorPeso;
  int sensorAgua;
  boolean flag;
};

state tupdate = {-1, -1, -1, -1}; // Definicao de estados impossiveis
HX711 scale;
String lastTriggerKey = "";       // redundancia
String deviceIP = "";
WebServer server(80);             //porta do webserver
Preferences prefs;

// --- Variaveis do Display(Azul) ---
int line2Number = 1234;
String line3Number = "-";

// Targets(MAX 10 Agendamentos)
int hAgen[max_schedules] = {16, 18, 18, -1, -1, -1, -1, -1, -1, -1};
int mAgen[max_schedules] = {55, 43, 42, -1, -1, -1, -1, -1, -1, -1};

// ----- Variaveis mecanicas -----
int food_current = 0;     // Valor atual
int food_target = 150;    // Valor objetivo
int water_current = 20;   // Porcentagens 20%
int water_target = 80;    // 80%
long motor_time = 0;
long pump_time = 0;

// ---------- non‑blocking feeding state machine ----------
enum FeedState {
  FEED_IDLE,            // Nda acontecendo
  FEED_MOTOR_ALERT,     // buzzer+LED ON
  FEED_MOTOR_OPEN,      // Abertura (cycle_time)
  FEED_MOTOR_DISPENSE,  // Motor parado, Comida Caindo (motor_time)
  FEED_MOTOR_CLOSE,     // Fechamento (cycle_time)
  FEED_PUMP_ALERT,      // buzzer+LED(200ms)
  FEED_PUMP_RUN,        // BOMBA Ativa (pump_time)
  FEED_DONE             // Execucao completa, set flag
};

FeedState feedState = FEED_IDLE;
unsigned long feedPhaseStart = 0;    // Guarda quando o millis do inicio da fase
unsigned long feedMotorTime = 0;     // Computa o tempo do motor
unsigned long feedPumpTime = 0;      // Computa o tempo da bomba
unsigned long feedCycleTime = 0;     // cycle_time (copia)
bool feedLedOn = false;              // Lembra se o led esta ligado
// -----------------------------------------------

// --- Sistema de notificacao ---
bool isGearVisible = false;
unsigned long gearHideTime = 0;

// --- Variaveis de tempo (millis) ---
unsigned long lastScreenUpdate = 0;

// Icones ==========================================
// 8x8 notif
const unsigned char gear_bmp[] PROGMEM = {
    0x18, 0x3C, 0x7E, 0xDB, 0xDB, 0x7E, 0x3C, 0x18};

// 8x8 WiFi
const unsigned char wifi_on_bmp[] PROGMEM = {
    0x3C, 0x42, 0x99, 0x24, 0x5A, 0x24, 0x18, 0x18};
const unsigned char wifi_off_bmp[] PROGMEM = {
    0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81};

// 16x16 Startup anim
const unsigned char dog_frame1[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80,
    0x03, 0xC0, 0x03, 0xE0, 0x3F, 0xF0, 0x7F, 0xF8,
    0x7F, 0xF8, 0x7F, 0xF0, 0x3F, 0xE0, 0x19, 0xC0,
    0x18, 0x80, 0x18, 0x80, 0x00, 0x00, 0x00, 0x00};
const unsigned char dog_frame2[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80,
    0x03, 0xC0, 0x03, 0xE0, 0x3F, 0xF0, 0x7F, 0xF8,
    0x7F, 0xF8, 0x7F, 0xF0, 0x3F, 0xE0, 0x0B, 0xA0,
    0x11, 0x10, 0x20, 0x08, 0x00, 0x00, 0x00, 0x00};


int readLevel(int pin){ // 0 100 agua
  int raw = analogRead(pin);
  int level = map(raw, 0, 2050, 0, 100);
  return constrain(level, 0, 100);
}

int readGram(){ // 0 100 racao
    // 5 media leituras
    float weight = scale.get_units(5);
    int level = map(weight, -1, bowlCapacity, 0, 100);
    
    Serial.print("Peso: ");
    Serial.print(weight);
    Serial.println(" g");

    return constrain(level, 0, 100);
    Serial.println("HX711 not found.");
}

// Conexao -----------------------------------------
// Formato do JSON [{"id":"...", "type":"food", "hour":8, "minute":30, "enabled":true}, ...]
void addCorsHeaders(){
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// Formato salvo: array JSON de objetos
//   [{"id":"...", "type":"food", "hour":8, "minute":30, "enabled":true, "grams":40, "days":[1,5]},
bool loadSchedules(JsonDocument &doc){
  prefs.begin("feedpet", true); // somente leitura
  String raw = prefs.getString("schedules", "[]");
  prefs.end();

  DeserializationError err = deserializeJson(doc, raw);
  if (err){
    doc.clear();
    doc.to<JsonArray>();
    return false;
  }
  if (!doc.is<JsonArray>()){
    doc.clear();
    doc.to<JsonArray>();
  }
  return true;
}

//Salvamento do json
void saveSchedules(JsonDocument &doc){
  String out;
  serializeJson(doc, out);
  prefs.begin("feedpet", false); // leitura/escrita
  prefs.putString("schedules", out);
  prefs.end();
}

// Insere ou atualiza (por id) um agendamento
void upsertSchedule(const JsonObject &incoming){
  DynamicJsonDocument doc(SCHEDULES_JSON_CAPACITY);
  loadSchedules(doc);
  JsonArray arr = doc.as<JsonArray>();

  String id = incoming["id"].as<String>();
  bool found = false;

  for (JsonObject item : arr){
    if (item["id"].as<String>() == id)
    {
      item["type"] = incoming["type"];
      item["hour"] = incoming["hour"];
      item["minute"] = incoming["minute"];
      item["enabled"] = incoming["enabled"];
      item["grams"] = incoming["grams"] | default_grams;
      item.remove("days");
      JsonArray days = item.createNestedArray("days");
      if (incoming.containsKey("days")){
        for (JsonVariant d : incoming["days"].as<JsonArray>())
          days.add(d.as<int>());
      }
      found = true;
      break;
    }
  }

  if (!found)
  {
    if (arr.size() >= max_schedules){
      // Remove o mais antigo (primeiro) para abrir espaço
      arr.remove(0);
    }
    JsonObject newItem = arr.createNestedObject();
    newItem["id"] = id;
    newItem["type"] = incoming["type"];
    newItem["hour"] = incoming["hour"];
    newItem["minute"] = incoming["minute"];
    newItem["enabled"] = incoming["enabled"];
    newItem["grams"] = incoming["grams"] | default_grams;
    JsonArray days = newItem.createNestedArray("days");
    if (incoming.containsKey("days"))
    {
      for (JsonVariant d : incoming["days"].as<JsonArray>())
        days.add(d.as<int>());
    }
  }

  saveSchedules(doc);
}

bool deleteScheduleById(const String &id){
  DynamicJsonDocument doc(SCHEDULES_JSON_CAPACITY);
  loadSchedules(doc);
  JsonArray arr = doc.as<JsonArray>();

  for (size_t i = 0; i < arr.size(); i++)
  {
    if (arr[i]["id"].as<String>() == id)
    {
      arr.remove(i);
      saveSchedules(doc);
      return true;
    }
  }
  return false;
}

// Rota: GET /status
// Retorna JSON com níveis de ração e água
void handleStatus(){
  addCorsHeaders();

  StaticJsonDocument<160> doc;
  doc["food_level"] = readGram();
  doc["water_level"] = readLevel(sensorPin);
  doc["device"] = "Feedpet-ESP32";
  doc["ip"] = deviceIP;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// Rota: GET /food?g=NN
// Dispensa ração imediatamente. "g" é opcional (gramas).
void handleFood()
{
  addCorsHeaders();
  int food_target = default_grams;
  if (server.hasArg("g"))
  { // verifica se o app mandou a quantidade
    food_target = server.arg("g").toInt();
  }
  startRotinaAlimentacao();
  server.send(200, "text/plain", "OK");
  Serial.println("Ração dispensada via app!");
}

// Rota: GET /schedule
// Lista todos os agendamentos salvos
void handleGetSchedules(){
  addCorsHeaders();
  DynamicJsonDocument doc(SCHEDULES_JSON_CAPACITY);
  loadSchedules(doc);
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// Rota: POST /schedule
// Recebe um agendamento {id,type,hour,minute,enabled,grams,days} e salva (insere ou atualiza) na memória NVS
void handlePostSchedule(){
  addCorsHeaders();

  if (!server.hasArg("plain")){
    server.send(400, "text/plain", "Corpo vazio");
    return;
  }

  String body = server.arg("plain");
  DynamicJsonDocument incoming(512);
  DeserializationError err = deserializeJson(incoming, body);

  if (err || !incoming.containsKey("id") || !incoming.containsKey("hour") || !incoming.containsKey("minute")){
    server.send(400, "text/plain", "JSON inválido");
    return;
  }

  if (!incoming.containsKey("type"))
    incoming["type"] = "food";
  if (!incoming.containsKey("enabled"))
    incoming["enabled"] = true;
  if (!incoming.containsKey("grams"))
    incoming["grams"] = default_grams;

  upsertSchedule(incoming.as<JsonObject>());

  Serial.print("Agendamento salvo: ");
  Serial.println(body);

  server.send(200, "text/plain", "OK");
}

// ─────────────────────────────────────────
// Rota: DELETE /schedule?id=xxxx
// Remove um agendamento salvo
// ─────────────────────────────────────────
void handleDeleteSchedule(){
  addCorsHeaders();

  if (!server.hasArg("id"))
  {
    server.send(400, "text/plain", "Faltando parâmetro id");
    return;
  }

  bool removed = deleteScheduleById(server.arg("id"));
  server.send(removed ? 200 : 404, "text/plain", removed ? "OK" : "Não encontrado");
}

// Rota: OPTIONS (preflight CORS)
void handleOptions(){
  addCorsHeaders();
  server.send(204);
}

// Rota: GET / e GET /qr
// Página simples mostrando o IP em destaque e um QR code (feedpet://IP:80) para escanear direto no app.
void handleRoot(){
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
  html += "<h1> Feedpet</h1>";
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

// Verifica a cada chamada do loop() se algum
// agendamento ativo bate com a hora atual (e o dia da semana)
void checkSchedules(){
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)){
    return; // hora ainda não sincronizada
  }

  char key[16];
  snprintf(key, sizeof(key), "%d-%02d-%02d", timeinfo.tm_yday,
           timeinfo.tm_hour, timeinfo.tm_min);
  String currentKey = String(key);

  if (currentKey == lastTriggerKey){
    return; // já verificamos neste minuto
  }

  // tm_wday: 0=domingo ... 6=sábado.
  int today = timeinfo.tm_wday == 0 ? 7 : timeinfo.tm_wday;

  DynamicJsonDocument doc(SCHEDULES_JSON_CAPACITY);
  loadSchedules(doc);
  JsonArray arr = doc.as<JsonArray>();

  for (JsonObject item : arr){
    bool enabled = item["enabled"] | false;
    int hour = item["hour"] | -1;
    int minute = item["minute"] | -1;
    food_target = item["grams"] | default_grams;
    //Serial.println(item["grams"]);

    if (!enabled || hour != timeinfo.tm_hour || minute != timeinfo.tm_min)
      continue; // se agenda estiver desativado ou hora diferente do atual ou min dif do atual ja encerra

    // verifica se hoje esta na lista de dias (lista vazia = todos os dias)
    bool dayMatches = true;
    if (item.containsKey("days")){
      JsonArray days = item["days"].as<JsonArray>();
      if (days.size() > 0){
        dayMatches = false;
        for (JsonVariant d : days){
          if (d.as<int>() == today)
          {
            dayMatches = true;
            break;
          }
        }
      }
    }

    if (dayMatches){
      Serial.println("Horário agendado bateu — dispensando ração automaticamente!");
      startRotinaAlimentacao();
      break; // dispensa uma vez mesmo se houver mais de um match no minuto
    }
  }

  lastTriggerKey = currentKey;
}

// Mostra o IP de forma bem visível no Serial Monitor
void printIPBanner(){
  Serial.println();
  Serial.println("========================================");
  Serial.println("   FEEDPET CONECTADO COM SUCESSO");
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

// Atualizacao da tela =====================================
void updateScreen(){
  display.clearDisplay();

  // --- ZONA AMARELA ---
  // HORA (Esquerda POS X=0)
  String timeString = "--:--";
  if (getLocalTime(&timeinfo))
  {
    char timeBuff[10];
    strftime(timeBuff, sizeof(timeBuff), "%H:%M", &timeinfo);
    timeString = String(timeBuff);
  }
  display.setTextSize(1);
  display.setCursor(0, 4);
  display.print(timeString);

  // Notif icone (POS X=60)
  if (isGearVisible){
    display.drawBitmap(60, 4, gear_bmp, 8, 8, SSD1306_WHITE);
  }

  // WiFi icone (POS X=120)
  if (WiFi.status() == WL_CONNECTED){
    display.drawBitmap(120, 4, wifi_on_bmp, 8, 8, SSD1306_WHITE);
  }
  else{
    display.drawBitmap(120, 4, wifi_off_bmp, 8, 8, SSD1306_WHITE);
  }

  display.drawLine(0, 15, 127, 15, SSD1306_WHITE);

  // --- ZONA AZUL ---
  display.setTextSize(2);

  display.setCursor(0, 24);
  display.print(line2Text);
  display.print(": ");
  display.print(formatNumber(line2Number));

  display.setCursor(0, 46);
  display.print(line3Text);
  display.print(": ");
  display.print(line3Number); // Nao formatar agua

  display.display();
}

// Sistema de Notif =================================
void triggerGear(uint32_t duration_ms){
  isGearVisible = true;
  gearHideTime = millis() + duration_ms;

  // Gear notif( independente do 500ms refresh rate)
  display.drawBitmap(60, 4, gear_bmp, 8, 8, SSD1306_WHITE);
  display.display();
}

// Check notificacao
void checkGearTimer(){
  if (isGearVisible && millis() > gearHideTime)
  {
    isGearVisible = false;

    // Apaga somente a notif
    display.fillRect(60, 4, 8, 8, SSD1306_BLACK);
    display.display();
  }
}


// STARTUP ===========================
void playStartupAnimation(){
  for (int x = -16; x < 132; x += 4)
  {
    display.clearDisplay();

    if ((x / 4) % 2 == 0)
    { // POS X par frame 1/impar frame2
      display.drawBitmap(x, 24, dog_frame1, 16, 16, SSD1306_WHITE);
    }
    else
    {
      display.drawBitmap(x, 24, dog_frame2, 16, 16, SSD1306_WHITE);
    }

    // NAO FUNCIONA --------------
    display.setTextSize(1);
    display.setCursor(30, 4);
    display.print("Starting...");
    //---------------------------
    display.display();
    delay(50);
  }
}

// UTILITIES =================================
void quantitativos(){
  // Leitura sensor de agua
  int sensorValue = analogRead(sensorPin);
  int waterLevelPercentage = (sensorValue / 4095.0) * 100; // Conversao para %

  if (tupdate.sensorAgua != waterLevelPercentage)
  {
    tupdate.sensorAgua = waterLevelPercentage;

    Serial.println("Leitura do sensor(%): " + String(tupdate.sensorAgua) + "%");
  }
  water_current = waterLevelPercentage;

  // Leitura balanca
  /*
  int scaleValue = scale.get_value(5);
  Serial.println("Leitura da balanca: "+ String(scaleValue) + "g");

  food_current = scaleValue;
  */
}

// Call this ONCE to start a feeding routine
void startRotinaAlimentacao() {
  Serial.println("Iniciando rotina de alimentacao");

  // ----- Calculate durations (same as before) -----
  unsigned long motor_time = 0;
  unsigned long pump_time = 0;

  Serial.println("Comida atual: " + String(food_current) + ", Comida target: " + String(food_target));
  if (food_target > food_current) {
    motor_time = (food_target - food_current) * food_factor;
  }
  if (water_target > water_current) {
    pump_time = (water_target - water_current) * water_factor;
  }
  feedCycleTime = cycle_time;   // store for later use

  Serial.println("Tempo bomba: " + String(pump_time / 1000) + "s |" +
                 " Tempo motor: " + String(motor_time / 1000) + "s");
  Serial.println("Total: " + String((pump_time + motor_time + cycle_time * 2) / 1000) + "s");

  // Safety checks
  if (motor_time > max_time) {
    Serial.println("Motor time exceeds max – aborted");
    return;
  }
  if (pump_time > max_time) {
    Serial.println("Pump time exceeds max – aborted");
    return;
  }

  feedMotorTime = motor_time;
  feedPumpTime  = pump_time;
  feedLedOn = false;

  // Decide first state: motor first (if needed), otherwise pump, else done
  if (feedMotorTime > min_time) {
    feedState = FEED_MOTOR_ALERT;
    feedPhaseStart = millis();
    // Immediately call triggerGear and turn on buzzer+LED (alert start)
    triggerGear(feedMotorTime);
    digitalWrite(buzzerPin, HIGH);
    digitalWrite(ledPin, HIGH);
    feedLedOn = true;
  } else if (feedPumpTime > min_time) {
    // skip motor, go to pump alert
    feedState = FEED_PUMP_ALERT;
    feedPhaseStart = millis();
    triggerGear(feedPumpTime);
    digitalWrite(buzzerPin, HIGH);
    digitalWrite(ledPin, HIGH);
    feedLedOn = true;
  } else {
    // nothing to do – finish immediately
    feedState = FEED_DONE;
  }
}

// -------------------------------------------------------------
// Call this in loop() repeatedly – it advances the feeding steps
// -------------------------------------------------------------
void updateRotinaAlimentacao(){
  if (feedState == FEED_IDLE) return;   // Estado vazio nada para fazer

  unsigned long now = millis();

  switch (feedState) {
    // ---------- Alerta do motor (buzzer+LED) ----------
    case FEED_MOTOR_ALERT:
      if (now - feedPhaseStart >= 200) {
        digitalWrite(buzzerPin, LOW);   // turn off buzzer after 200ms
        // move to motor opening
        digitalWrite(INT1, HIGH);
        digitalWrite(INT2, LOW);
        feedState = FEED_MOTOR_OPEN;
        feedPhaseStart = now;
      }
      break;

    // ---------- MOTOR OPENING (cycle_time) ----------
    case FEED_MOTOR_OPEN:
      if (now - feedPhaseStart >= feedCycleTime) {
        // stop motor, begin dispensing
        digitalWrite(INT1, LOW);
        digitalWrite(INT2, LOW);
        feedState = FEED_MOTOR_DISPENSE;
        feedPhaseStart = now;
      }
      break;

    // ---------- MOTOR DISPENSING (motor_time) ----------
    case FEED_MOTOR_DISPENSE:
      if (now - feedPhaseStart >= feedMotorTime) {
        // start closing
        digitalWrite(INT1, LOW);
        digitalWrite(INT2, HIGH);
        feedState = FEED_MOTOR_CLOSE;
        feedPhaseStart = now;
      }
      break;

    // ---------- MOTOR CLOSING (cycle_time) ----------
    case FEED_MOTOR_CLOSE:
      if (now - feedPhaseStart >= feedCycleTime) {
        // motor off, turn LED off
        digitalWrite(INT1, LOW);
        digitalWrite(INT2, LOW);
        digitalWrite(ledPin, LOW);
        feedLedOn = false;
        // now check pump
        if (feedPumpTime > min_time) {
          feedState = FEED_PUMP_ALERT;
          feedPhaseStart = now;
          triggerGear(4000);
          digitalWrite(buzzerPin, HIGH);
          digitalWrite(ledPin, HIGH);
          feedLedOn = true;
        } else {
          feedState = FEED_DONE;
        }
      }
      break;

    // ---------- Alerta da bomba (buzzer+LED) ----------
    case FEED_PUMP_ALERT:
      if (now - feedPhaseStart >= 200) {
        digitalWrite(buzzerPin, LOW);
        // start pump
        digitalWrite(INT3, HIGH);
        digitalWrite(INT4, LOW);
        feedState = FEED_PUMP_RUN;
        feedPhaseStart = now;
      }
      break;

    // ---------- PUMP RUNNING (pump_time) ----------
    case FEED_PUMP_RUN:
      if (now - feedPhaseStart >= feedPumpTime) {
        digitalWrite(INT3, LOW);
        digitalWrite(INT4, LOW);
        digitalWrite(ledPin, LOW);
        feedLedOn = false;
        feedState = FEED_DONE;
      }
      break;

    // ---------- FINISH ----------
    case FEED_DONE:
      tupdate.flag = true;          // original final flag
      feedState = FEED_IDLE;        // back to idle
      Serial.println("Rotina concluida");
      break;

    default:
      break;
  }
}

boolean agendamentoCheck(){
  /* getLocalTime(&timeinfo) -> Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    %A dia da semana
    %B Mes
    %d Dia
    %Y Ano
    %H Hora
    %M Minuto
    %S Segundo
  */
  if (!getLocalTime(&timeinfo)){
    Serial.println("Erro: Obtencao da data atual falhou");
    return false;
  }
  else{
    if (tupdate.minuto != timeinfo.tm_min || tupdate.hora != timeinfo.tm_hour){
      tupdate.hora = timeinfo.tm_hour;
      tupdate.minuto = timeinfo.tm_min;
      tupdate.flag = false;

      Serial.println("Hora: " + String(tupdate.hora) + ":" + String(tupdate.minuto));
    }
    // Check data atual == data agendada
    for (int i = 0; i < 10; i++){ // MAX 10, return na primeira verdade(Validar entradas para previnir erros)
      // Serial.println("Hora: " + String(tupdate.hora) + ":" + String(tupdate.minuto) + " | Hora marcada[" + String(i) + "]: " + String(horaAgendada[i]) + ":" + String(minutoAgendado[i]));
      if (timeinfo.tm_hour == hAgen[i] && timeinfo.tm_min == mAgen[i])
        return true;
    }
    return false;
  }
}

String formatNumber(int num){
  if (num < 0) num = 0;
  char buff[5];
  snprintf(buff, sizeof(buff), "%04d", num);
  return String(buff);
}

void setup(){
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)){
    Serial.println(F("Alocação de SSD1306 falhou"));
  }

  playStartupAnimation();
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20){
    Serial.print(".");
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED){
    // IPAddress ip = WiFi.localIP();
    deviceIP = WiFi.localIP().toString();
    printIPBanner();

    // mDNS: permite acessar via http://feedpet.local em vez do IP puro
    if (MDNS.begin(MDNS_HOSTNAME)){
      MDNS.addService("http", "tcp", 80);
      Serial.println("mDNS ativo: http://" + String(MDNS_HOSTNAME) + ".local");
    }
    else{
      Serial.println("Falha ao iniciar mDNS (o IP numérico continua funcionando).");
    }

    // Sincroniza hora via NTP (necessário para o agendamento automático)
    configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
    Serial.print("Sincronizando hora via NTP");
    struct tm timeinfo;
    int ntpAttempts = 0;
    while (!getLocalTime(&timeinfo, 1000) && ntpAttempts < 10){
      Serial.print(".");
      ntpAttempts++;
    }
    if (getLocalTime(&timeinfo)){
      Serial.println("\nHora sincronizada: " + String(asctime(&timeinfo)));
    }
    else{
      Serial.println("\nFalha ao sincronizar hora — agendamento automático não vai funcionar até reconectar.");
    }
  }
  else{
    Serial.println("\nFalha na conexão WiFi.");
  }

  // Rotas do servidor HTTP
  server.on("/", HTTP_GET, handleRoot);
  server.on("/qr", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/food", HTTP_GET, handleFood);
  server.on("/schedule", HTTP_GET, handleGetSchedules);
  server.on("/schedule", HTTP_POST, handlePostSchedule);
  server.on("/schedule", HTTP_DELETE, handleDeleteSchedule);
  server.on("/status", HTTP_OPTIONS, handleOptions);
  server.on("/food", HTTP_OPTIONS, handleOptions);
  server.on("/schedule", HTTP_OPTIONS, handleOptions);

  server.begin();
  Serial.println("Servidor HTTP iniciado na porta 80");
  Serial.println("----------------------------------\n");

  // Inicializacao da celula de carga
  Serial.println("HX711 Iniciando...");
  
  scale.begin(DTPin, SCKPin);
  scale.set_scale(cFactor);
  scale.tare(); //manda pra zero - gera erro se algo estiver na balanca

  //---- PINAGEM ----
  pinMode(INT1, OUTPUT);
  pinMode(INT2, OUTPUT);
  pinMode(INT3, OUTPUT);
  pinMode(INT4, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  digitalWrite(INT1, LOW);
  digitalWrite(INT2, LOW);
  digitalWrite(INT3, LOW);
  digitalWrite(INT4, LOW);
  digitalWrite(ledPin, LOW);
  digitalWrite(buzzerPin, LOW);
}

void loop(){
  unsigned long currentMillis = millis();

  updateRotinaAlimentacao();
  server.handleClient();
  checkSchedules();
  // Sensores
  //quantitativos();

  line2Number = scale.get_units(5);
  //else Serial.println("HX711 not found.");
  line3Number = String(readLevel(sensorPin)) + "%";

  // Atualizacao da tela 500ms com millis
  if (currentMillis - lastScreenUpdate >= SCREEN_UPDATE_INTERVAL)
  {
    lastScreenUpdate = currentMillis;
    updateScreen();
  }

  // Icone de notificacao
  checkGearTimer();
  checkSchedules();

  if (WiFi.status() == WL_CONNECTED)
  { // Caos a conexao mude
    String currentIP = WiFi.localIP().toString();
    if (currentIP != deviceIP && currentIP != "0.0.0.0")
    {
      deviceIP = currentIP;
      printIPBanner();
    }
  }
}