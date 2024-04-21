#include <Fuzzy.h>
#include <SSD1306.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <TimeLib.h>

#define saidaControle 16                  // Define o pino 14 do ESP32 como saída de controle
#define BUZZ 2                            // Define o pino 2 do ESP32 como o buzzer
#define tempAD 14                         // Define o pino 13 do ESP32 como entrada do sensor de temperatura
#define batAD 39                          // Define o pino 39 do ESP32 como entrada do sensor de bateria
#define INTERVALO_ALIMENTACAO_FILA 15000  // Intervalo de 15 segundos em milissegundos

// Tamanho do display OLED (em pixels)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define ssid "mary"
#define password "renato37"

#define DHTTYPE DHT11

DHT dht(tempAD, DHTTYPE);

WiFiServer server(80);

Fuzzy *fuzzy = new Fuzzy();

QueueHandle_t filaMonitoramento;

TimerHandle_t timerAlimentacaoFila;

struct DadosMonitoramento {
  float temperatura;
  float umidade;
  String created_at;
};

float tempMuitoBaixa, tempBaixa, temperaturaInicial, tempAlta, tempMuitoAlta, porcentagemBat, temperatura, humidity;
bool isConectado = false;

// Conjuntos fuzzy para temperatura e velocidade do ventilador
FuzzySet *baixaTemp = new FuzzySet(tempMuitoBaixa, tempBaixa, tempBaixa, temperaturaInicial);     // baixa: será definida dinamicamente
FuzzySet *mediaTemp = new FuzzySet(tempBaixa, temperaturaInicial, temperaturaInicial, tempAlta);  // media: será definida dinamicamente
FuzzySet *altaTemp = new FuzzySet(temperaturaInicial, tempAlta, tempAlta, tempMuitoAlta);         // alta: será definida dinamicamente
FuzzySet *baixaVelocidade = new FuzzySet(0, 30, 30, 50);                                          // baixa: 30%
FuzzySet *mediaVelocidade = new FuzzySet(40, 60, 60, 80);                                         // média: 60%
FuzzySet *altaVelocidade = new FuzzySet(70, 90, 90, 100);                                         // alta: 90%

// Inicialização do objeto SSD1306
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  pinMode(saidaControle, OUTPUT);
  pinMode(BUZZ, OUTPUT);
  pinMode(tempAD, INPUT);
  pinMode(batAD, INPUT);

  dht.begin();

  Wire.begin(5, 4);
  // Inicialização do display OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  //Connect to Wi-Fi
  WiFi.begin(ssid, password);

  // Start the HTTP server
  server.begin();

  // Definição das regras fuzzy
  definirRegras();

  filaMonitoramento = xQueueCreate(10, sizeof(JSONVar));

  // Iniciar o timer para alimentar a fila a cada 15 segundos
  timerAlimentacaoFila = xTimerCreate("timerAlimentacaoFila", pdMS_TO_TICKS(INTERVALO_ALIMENTACAO_FILA), pdTRUE, 0, alimentaFila);
  xTimerStart(timerAlimentacaoFila, 0);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    isConectado = false;
  } else if (WiFi.status() == WL_CONNECTED) {
    isConectado = true;
    processarObjetoJSON();
    processarFila();
  }

  temperatura = dht.readTemperature();
  humidity = dht.readHumidity();

  float tensaoBat = (analogRead(batAD) / 4095.0) * 3.3;
  // porcentagemBat = (tensaoBat / 12.0) * 100;  // Calcula a porcentagem da bateria de 12V
  porcentagemBat = (tensaoBat) / 3.3 * 100;  // Calcula a porcentagem da bateria de 3.3V



  // Atualiza o valor de entrada da temperatura no sistema fuzzy
  fuzzy->setInput(1, temperatura);
  // Executa a fuzzificação
  fuzzy->fuzzify();
  // Executa a defuzzificação
  float velocidade = fuzzy->defuzzify(1);

  // Define a velocidade do ventilador
  analogWrite(saidaControle, velocidade);

  exibirDadosDisplay();

  delay(200);  // Tempo entre medições do sensor (500ms)
};

void definirConjuntosFuzzy(float temperaturaInicial, float offset) {
  // Define os limites dos conjuntos fuzzy com base na temperatura inicial
  tempMuitoBaixa = temperaturaInicial - offset;
  tempBaixa = temperaturaInicial - (offset / 2);
  tempAlta = temperaturaInicial + (offset / 2);
  tempMuitoAlta = temperaturaInicial + offset;
};

void exibirDadosDisplay() {
  // Exibe a porcentagem da bateria, a temperatura e o status do WiFi no display OLED
  display.clearDisplay();

  display.setTextSize(1);  // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Temperatura: " + String(temperatura, 1) + "C");

  display.setTextSize(1);  // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println("Bateria: " + String(porcentagemBat) + "%");

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 50);
  display.println("WiFi: " + String(isConectado ? "Conectado" : "Nao Conectado"));



  display.display();  // Show initial text
  delay(100);
}

void definirRegras() {
  // Regra: Se temperatura é baixa, então velocidade é baixa
  FuzzyRuleAntecedent *ifBaixaTemp = new FuzzyRuleAntecedent();
  ifBaixaTemp->joinSingle(baixaTemp);
  FuzzyRuleConsequent *thenBaixaVelocidade = new FuzzyRuleConsequent();
  thenBaixaVelocidade->addOutput(baixaVelocidade);
  FuzzyRule *fuzzyRule1 = new FuzzyRule(1, ifBaixaTemp, thenBaixaVelocidade);
  fuzzy->addFuzzyRule(fuzzyRule1);

  // Regra: Se temperatura é média, então velocidade é média
  FuzzyRuleAntecedent *ifMediaTemp = new FuzzyRuleAntecedent();
  ifMediaTemp->joinSingle(mediaTemp);
  FuzzyRuleConsequent *thenMediaVelocidade = new FuzzyRuleConsequent();
  thenMediaVelocidade->addOutput(mediaVelocidade);
  FuzzyRule *fuzzyRule2 = new FuzzyRule(2, ifMediaTemp, thenMediaVelocidade);
  fuzzy->addFuzzyRule(fuzzyRule2);

  // Regra: Se temperatura é alta, então velocidade é alta
  FuzzyRuleAntecedent *ifAltaTemp = new FuzzyRuleAntecedent();
  ifAltaTemp->joinSingle(altaTemp);
  FuzzyRuleConsequent *thenAltaVelocidade = new FuzzyRuleConsequent();
  thenAltaVelocidade->addOutput(altaVelocidade);
  FuzzyRule *fuzzyRule3 = new FuzzyRule(3, ifAltaTemp, thenAltaVelocidade);
  fuzzy->addFuzzyRule(fuzzyRule3);
}

void processarObjetoJSON() {
  HTTPClient http;
  WiFiClient client;

  if (http.begin(client, "http://localhost3000/configs/2")) {  // URL do servidor e endpoint
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();  // Obtém a resposta JSON

      // Decodifica o JSON
      JSONVar payloadRecebido = JSON.parse(payload);

      // Obtém os valores do JSON
      float temperatura = atof(JSON.stringify(payloadRecebido["temperatura"]).c_str());  // Converte de String para float
      float offset = atof(JSON.stringify(payloadRecebido["offset"]).c_str());            // Converte de String para float

      definirConjuntosFuzzy(temperatura, offset);
    }

    http.end();
  }
}

void alimentaFila(TimerHandle_t timer) {
  // Obter os dados de temperatura e umidade
  temperatura = dht.readTemperature();
  humidity = dht.readHumidity();

  // Obter a data e hora atual
  time_t now = time(nullptr);  // Obter o tempo atual
  struct tm *timeinfo;
  timeinfo = localtime(&now);
  String created_at = String(year()) + "-" + String(month()) + "-" + String(day()) + " " + String(hour()) + ":" + String(minute()) + ":" + String(second());

  // Criar um objeto JSON com os dados de monitoramento
  JSONVar dadosJSON;
  dadosJSON["temperatura"] = temperatura;
  dadosJSON["umidade"] = humidity;
  dadosJSON["data"] = created_at;

  // Enviar os dados para a fila
  xQueueSendToBack(filaMonitoramento, &dadosJSON, portMAX_DELAY);
}

void processarFila() {
  // Verificar se há conexão Wi-Fi
  if (WiFi.status() == WL_CONNECTED) {
    // Loop para processar todos os itens na fila
    while (uxQueueMessagesWaiting(filaMonitoramento) > 0) {
      // Obter o próximo item da fila
      DadosMonitoramento dados;
      xQueueReceive(filaMonitoramento, &dados, portMAX_DELAY);

      // Criar um objeto JSON com os dados de monitoramento
      JSONVar dadosJSON;
      dadosJSON["temperatura"] = dados.temperatura;
      dadosJSON["umidade"] = dados.umidade;
      dadosJSON["data"] = dados.created_at;

      // Enviar os dados via POST
      HTTPClient http;
      http.begin("http://localhost:3000/registries");
      http.addHeader("Content-Type", "application/json");
      int httpCode = http.POST(JSON.stringify(dadosJSON));

      // Liberar recursos
      http.end();
    }
  }
}
