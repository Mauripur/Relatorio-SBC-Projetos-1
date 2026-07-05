# Pet Feeder App 🐾

App Flutter para controle de comedouro automático via WiFi local com ESP32.

## Estrutura do projeto

```
lib/
├── main.dart                    # Entrada do app e navegação
├── screens/
│   ├── home_screen.dart         # Dashboard principal
│   ├── schedule_screen.dart     # Agendamentos de ração
│   ├── history_screen.dart      # Histórico de alimentações
│   └── settings_screen.dart     # Configuração do IP do ESP32
├── services/
│   ├── device_service.dart      # Comunicação HTTP com o ESP32
│   └── history_service.dart     # Salvar histórico localmente
└── models/
    └── schedule.dart            # Modelo e serviço de agendamentos

esp32_pet_feeder.ino             # Código para gravar no ESP32
```

## Como rodar o app

### 1. Instalar dependências
```bash
flutter pub get
```

### 2. Rodar no Chrome (para testar)
```bash
flutter run -d chrome
```

### 3. Gerar APK para Android
```bash
flutter build apk --release
```
O APK estará em `build/app/outputs/flutter-apk/app-release.apk`

## Configurar o ESP32

1. Abra o arquivo `esp32_pet_feeder.ino` no Arduino IDE
2. Edite as linhas com sua rede WiFi:
```cpp
const char* WIFI_SSID = "NOME_DA_SUA_REDE";
const char* WIFI_PASS = "SENHA_DA_SUA_REDE";
```
3. Instale as bibliotecas no Arduino IDE:
   - **ESP32Servo** (by Kevin Harrington)
   - **ArduinoJson** (by Benoit Blanchon)
4. Grave no ESP32
5. Abra o Monitor Serial (115200 baud) e copie o IP que aparecer
6. No app, vá em Configurações e cole o IP

## Pinos do ESP32

| Pino  | Função                        |
|-------|-------------------------------|
| GPIO 13 | Servo do comedouro (ração)  |
| GPIO 34 | Sensor de nível de ração    |
| GPIO 35 | Sensor de nível de água     |

## Endpoints da API (ESP32)

| Rota         | Método | Descrição                        |
|--------------|--------|----------------------------------|
| `/status`    | GET    | Retorna níveis de ração e água   |
| `/food`      | GET    | Dispensa ração imediatamente     |
| `/schedule`  | POST   | Salva agendamento no dispositivo |
