#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <string>
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

void appComm();                         //comunicacao com App
void updateScreen();                    //Refresh da tela
void quantitativos();                   //Leitura dos sensores
void checkGearTimer();                  //icone de notificao
void rotinaAlimentacao();               //Rotina de despejo
boolean agendamentoCheck();             //Monitoramento da hora
void playStartupAnimation();            //Animacao de inicializacao
String formatNumber(int num);           //Formatacao de numeros para 4 digitos
void triggerGear(uint32_t duration_ms); //icone de notificao

// ---- DISPLAY 128x64 ----
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1 // Sem botao de reset
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- WIFI & DATA ---
const char *ssid = "Lobato";
const char *password = "A00dc729@";
const char *ntpServer = "pool.ntp.org";
const int daylightOffset_sec = 0;  // Sem horario de verão(3600 para DLS)
const long gmtOffset_sec = -10800; // GMT -3 Brasilia -> -3*60*60 = -108000

/* ---- PINAGEM ----
====================
| 02 AZUL          |
| 04 ROXO          |
| 12 CINZA         |
| 13 BRANCO        |
| 19 CINZA         |
| 25 AMARELO       |
| 26 LARANJA       |
| 27 MARROM        |
| 33 VERDE         |
====================
*/
// const int hxSckPin      =  4;
// const int hxDtPin      = 16;
const int buzzerPin =  2;
const int int3Pin   = 12;
const int int4Pin   = 13;
const int int1Pin   = 25;
const int int2Pin   = 26;
const int ledPin    = 27;
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

state tupdate = {-1, -1, -1, -1}; // Difinicao de estados impossiveis

// --- Variaveis do Display(Azul) ---
String line2Text = "Rcao";
int line2Number = 1234;

String line3Text = "Agua";
String line3Number = "-";

// Targets(MAX 10 Agendamentos)
int hAgen[10] = {16, 21, 21, -1, -1, -1, -1, -1, -1, -1};
int mAgen[10] = {55, 38, 37, -1, -1, -1, -1, -1, -1, -1};

int racao_atual = 50.0;   //Valor atual
int racao_target = 150.0; //Valor objetivo
int taxaPeso = 50;        //miligrama/segundo

int agua_atual = 20;      //Porcentagens 20%
int agua_target = 80;     //80%
int taxaAgua = 580;       //mililitro/segundo

long tempo_motor = 0;
long tempo_bomba = 0;
long tempo_giro = 2000;   //TEMPO DE ABERTURA/FECHAMENTO

const int cFactor = 0;    //FATOR DE CALIBRACAO DA CELULAR DE CARGA

// --- Sistema de notificacao ---
bool isGearVisible = false;
unsigned long gearHideTime = 0;

// --- Variaveis de tempo (millis) ---
unsigned long lastScreenUpdate = 0;
const unsigned long SCREEN_UPDATE_INTERVAL = 500;

// Icones =======================================
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

void setup()
{
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("Alocação de SSD1306 falhou"));
    for (;;)
      ;
  }

  playStartupAnimation();
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20){ // 10s tentando conectar ao WiFi
    delay(500);
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED){
    Serial.println("Connected to: " + WiFi.SSID() + " with IP Address: " + WiFi.localIP());
  }

  // Inicializacao NTP data
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Inicializacao da celula de carga
  /*
  scale.begin(hxSckPin, hxDtPin);
  scale.set_scale(calibrationFactor);
  scale.tare();
  */
  //---- PINAGEM ----
  pinMode(int1Pin, OUTPUT);
  pinMode(int2Pin, OUTPUT);
  pinMode(int3Pin, OUTPUT);
  pinMode(int4Pin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  digitalWrite(ledPin, LOW);
  digitalWrite(int1Pin, LOW);
  digitalWrite(int2Pin, LOW);
  digitalWrite(int3Pin, LOW);
  digitalWrite(int4Pin, LOW);
  digitalWrite(buzzerPin, LOW);
}

void loop(){
  unsigned long currentMillis = millis();

  // Sensores
  quantitativos();

  line2Number = racao_atual;
  line3Number = String(agua_atual) + "%";

  // Atualizacao da tela 500ms com millis
  if (currentMillis - lastScreenUpdate >= SCREEN_UPDATE_INTERVAL){
    lastScreenUpdate = currentMillis;
    updateScreen();
  }

  // Icone de notificacao
  checkGearTimer();

  // Despejo agendado
  if (agendamentoCheck() && !tupdate.flag){ // Flag impede mais de um despejo por minuto(Validar entradas para evitar erros)
    rotinaAlimentacao();
  }
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

// Atualizacao da tela =====================================
void updateScreen(){
  display.clearDisplay();

  // --- ZONA AMARELA ---
  //HORA (Esquerda POS X=0)
  String timeString = "--:--";
  if (getLocalTime(&timeinfo)){
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
  }else{
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

// STARTUP ===========================
void playStartupAnimation(){
  for (int x = -16; x < 132; x += 4){
    display.clearDisplay();

    if ((x / 4) % 2 == 0){ // POS X par frame 1/impar frame2
      display.drawBitmap(x, 24, dog_frame1, 16, 16, SSD1306_WHITE);
    }else{
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

  if (tupdate.sensorAgua != waterLevelPercentage){
    tupdate.sensorAgua = waterLevelPercentage;

    Serial.println("Leitura do sensor(%): " + String(tupdate.sensorAgua) + "%");
  }
  agua_atual = waterLevelPercentage;

  // Leitura balanca
  /*
  int scaleValue = scale.get_value(5);
  Serial.println("Leitura da balanca: "+ String(scaleValue) + "g");

  racao_atual = scaleValue;
  */
}

void rotinaAlimentacao(){
  Serial.println("Iniciando rotina de alimentacao");
  // Quantitativos pt2
  if (racao_target > racao_atual){
    float diferenca_racao = racao_target - racao_atual;
    tempo_motor = diferenca_racao * taxaPeso; // Conversão para ms
  }else{
    tempo_motor = 0;
  }

  if (agua_target > agua_atual){
    float diferenca_agua = agua_target - agua_atual;
    tempo_bomba = diferenca_agua * taxaAgua; // Conversão para ms
  }else{
    tempo_bomba = 0;
  }
  Serial.println("Tempo bomba: " + String(tempo_bomba / 1000) + "s |" + " Tempo motor: " + String(tempo_motor / 1000) + "s");
  Serial.println("Total: " + String((tempo_bomba+tempo_motor+tempo_giro*2) / 1000) + "s");

  // ACIONAMENTO MOTOR
  if (tempo_motor > 0){
    // ALERTA SONORO & VISUAL
    triggerGear(4000);
    digitalWrite(buzzerPin, HIGH);
    digitalWrite(ledPin, HIGH);
    delay(200);
    digitalWrite(buzzerPin, LOW);

    // ABRE
    digitalWrite(int1Pin, HIGH);
    digitalWrite(int2Pin, LOW);
    delay(tempo_giro);
    // PERMANECE ABERTO
    digitalWrite(int1Pin, LOW);
    digitalWrite(int2Pin, LOW);
    delay(tempo_motor);
    // FECHA
    digitalWrite(int1Pin, LOW);
    digitalWrite(int2Pin, HIGH);
    delay(tempo_giro);
    digitalWrite(int1Pin, LOW);
    digitalWrite(int2Pin, LOW);
    digitalWrite(ledPin, LOW);
  }

  if (tempo_bomba > 0){
    // ALERTA SONORO & VISUAL
    triggerGear(4000);
    digitalWrite(buzzerPin, HIGH);
    digitalWrite(ledPin, HIGH);
    delay(200);
    digitalWrite(buzzerPin, LOW);

    // LIGA
    digitalWrite(int3Pin, HIGH);
    digitalWrite(int4Pin, LOW);
    delay(tempo_bomba);
    // DESLIGA
    digitalWrite(int3Pin, LOW);
    digitalWrite(int4Pin, LOW);
    digitalWrite(ledPin, LOW);
  }

  appComm();
  tupdate.flag = true;
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
  }else{
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

void appComm(){
  Serial.println("AppComm mock up...");
}

String formatNumber(int num){
  char buff[5];
  snprintf(buff, sizeof(buff), "%04d", num);
  return String(buff);
}