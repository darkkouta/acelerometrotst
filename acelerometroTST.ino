/*
  ------------------------------------------------------------
  Projeto: Sistema de Monitoramento de Vibração com MPU6050 e ESP32
  Autor: Wellington Barros Rosa
  Data de Criação: 24/10/2024
  Versão: 1.4
  Descrição:
  Este projeto implementa um sistema de monitoramento de vibração utilizando o sensor MPU6050 e
  um microcontrolador ESP32 para a leitura de dados de aceleração e giroscópio, além de uma interface
  web para visualização e ajuste de parâmetros de medição. O sistema permite a configuração de filtros
  de ponderação e oferece funcionalidades para ajuste de tempo de exposição (Texp), que influencia o
  cálculo de parâmetros como ARE, AREN, e Dy, baseados nas normas de vibração.

  Funcionalidades:
  - Leitura e processamento de dados de aceleração do MPU6050.
  - Interface web para visualização dos dados de vibração em tempo real.
  - Ajuste automático de offsets e calibração inicial do giroscópio.
  - Ajuste configurável do tempo de exposição (Texp) pela interface web.
  - Cálculos de parâmetros de vibração como ARE, AREN, AM, AMR, AREP, VDVR, CF, AMJ, PICO e VDVEXP.
  - Filtros de ponderação WB, WC, WD, WE, WF, WH, WJ, WK e WM para análise de vibração em corpo inteiro e mão/braço.
  - Configuração de unidade de medida entre m/s² e g.

  Bibliotecas Utilizadas:
  - Wire.h: Para comunicação I2C.
  - Adafruit_ADXL345_U.h: Para controle do sensor ADXL345.
  - WiFi.h: Para configuração da rede Wi-Fi.
  - WebServer.h: Para criação do servidor web.

  Agradecimentos:
  Gostaria de expressar minha profunda gratidão aos professores Fábio Miranda e Mauro de Mendonça Costa 
  pelo apoio e orientação durante o desenvolvimento deste projeto.
*/

#include <Wire.h>
#include <Adafruit_ADXL345_U.h>  
#include <WiFi.h>
#include <WebServer.h>
#include <cmath> 
#include <arduinoFFT.h>  


// --------------------- CONSTANTES E DEFINIÇÕES PARA FFT ---------------------
#define SAMPLES 128                 // Número de amostras para a FFT
#define SAMPLING_FREQUENCY 1000     // Frequência de amostragem em Hz para coleta de dados

double vReal[SAMPLES];
double vImag[SAMPLES];

// Especificação explícita do tipo `double` para ArduinoFFT
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, (double)SAMPLING_FREQUENCY);

// --------------------- DEFINIÇÕES GLOBAIS ---------------------
float offsetX = 0.0; // Valor inicial para offsetX

#define UNIDADE_MS2 true // true para m/s², false para g
#define BANDA_OITAVA 1   // 1 para 1/1 oitava, 3 para 1/3 oitava

// Variável Texp
float Texp = 1.0;  // Valor inicial de Texp em segundos

// Filtros de Ponderação
enum FiltroPonderacao {WB, WC, WD, WE, WF, WH, WJ, WK, WM};
FiltroPonderacao filtroAtual = WB;

// Estruturas para Envio de Dados
struct DadosVibracao {
    float am;
    float amr;
    float are;
    float aren;
    float arep;
    float vdvr;
    float cf;
    float amj;
    float pico;
    float vdvexp;
    float frequencia;
};

DadosVibracao dados;

// --------------------- OBJETOS E INSTÂNCIAS ---------------------
Adafruit_ADXL345_Unified adxl = Adafruit_ADXL345_Unified(12345);
WebServer server(80);

// --------------------- PROTÓTIPOS DE FUNÇÕES ---------------------
float calcularAREN(float are, float texp);
float calcularAM(float x, float y, float z);
float aplicarFiltroPonderacao(float valor, FiltroPonderacao filtro);
float calcularAMR(float x, float y, float z);
float calcularARE(float x, float y, float z);
float calcularAREP(float x, float y, float z);
float calcularVDVR(float x, float offset);
float calcularCF(float am, float are);
float calcularAMJ(float x, float y, float z);
float calcularPICO(float x, float y, float z);
float calcularVDVEXP(float x, float vdvr);
float calcularFrequencia(float x, float y, float z);
float calcularFrequenciaFFT();

// --------------------- CONFIGURAÇÃO ---------------------
void setup() {
    Serial.begin(115200);
    inicializarSensor();
    configurarWiFi();
    configurarServidor();
    Serial.println("Servidor HTTP iniciado.");
}

// Função para inicializar o sensor
void inicializarSensor() {
    if (!adxl.begin()) {
        Serial.println("Erro ao inicializar ADXL345");
        while (1);
    }
    adxl.setRange(ADXL345_RANGE_16_G);
}

// Função para configurar Wi-Fi
void configurarWiFi() {
    const char* ssid_ap = "Acelerometro";
    const char* password_ap = "";

    const char* ssid_sta = "suarede";
    const char* password_sta = "suarede";

    // Inicializa o Wi-Fi no modo AP
    WiFi.softAP(ssid_ap, password_ap);
    Serial.println("Ponto de acesso iniciado.");

    // Conecta à rede Wi-Fi (station mode)
    WiFi.begin(ssid_sta, password_sta);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("Conectado à rede Wi-Fi.");
}

// Função para configurar o servidor
void configurarServidor() {
    server.on("/", []() {
        server.send(200, "text/html", 
            "<html><head><meta charset='utf-8'><title>Monitor de Vibração</title>"
            "<style>"
            ".tab { display: none; }"
            ".tab-button { cursor: pointer; padding: 10px; background-color: #f1f1f1; border: 1px solid #ccc; display: inline-block; }"
            ".active { background-color: #ccc; }"
            "</style>"
            "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.5.1/jquery.min.js'></script>"
            "<script>"
            "function showTab(tabId) {"
            "  $('.tab').hide();"
            "  $('#' + tabId).show();"
            "  $('.tab-button').removeClass('active');"
            "  $('#btn-' + tabId).addClass('active');"
            "} "
            "function updateData() {"
            "$.getJSON('/data', function(data) {"
            "$('#am').text(data.am.toFixed(2));"
            "$('#amr').text(data.amr.toFixed(2));"
            "$('#are').text(data.are.toFixed(2));"
            "$('#aren').text(data.aren.toFixed(2));"
            "$('#arep').text(data.arep.toFixed(2));"
            "$('#vdvr').text(data.vdvr.toFixed(2));"
            "$('#cf').text(data.cf.toFixed(2));"
            "$('#amj').text(data.amj.toFixed(2));"
            "$('#pico').text(data.pico.toFixed(2));"
            "$('#vdvexp').text(data.vdvexp.toFixed(2));"
            "$('#frequencia').text(data.frequencia.toFixed(2));"
            "});"
            "} "
            "function setTexp() {"
            "  var texp = parseFloat(document.getElementById('texp').value);"
            "  $.post('/setTexp', { texp: texp });"
            "} "
            "setInterval(updateData, 1000);"
            "showTab('dados');" // Mostra a aba de dados por padrão
            "</script></head>"
            "<body><h1>Monitor de Vibração</h1>"
            "<div>"
            "<div class='tab-button' id='btn-dados' onclick='showTab(\"dados\")'>Dados de Vibração</div>"
            "<div class='tab-button' id='btn-config' onclick='showTab(\"config\")'>Configurações</div>"
            "</div>"
            "<div id='dados' class='tab'>"
            "<p>Aceleração Média (AM): <span id='am'>0.00</span> m/s²</p>"
            "<p>Aceleração Média Retificada (AMR): <span id='amr'>0.00</span> m/s²</p>"
            "<p>Aceleração Resultante Efetiva (ARE): <span id='are'>0.00</span> m/s²</p>"
            "<p>Aceleração Resultante Normalizada (AREN): <span id='aren'>0.00</span> m/s²</p>"
            "<p>Aceleração Resultante de Pico (AREP): <span id='arep'>0.00</span> m/s²</p>"
            "<p>Variação Dinâmica da Velocidade Resultante (VDVR): <span id='vdvr'>0.00</span> m/s²</p>"
            "<p>Fator de Crista (CF): <span id='cf'>0.00</span></p>"
            "<p>Aceleração Máxima Instantânea (AMJ): <span id='amj'>0.00</span> m/s²</p>"
            "<p>Pico Absoluto (PICO): <span id='pico'>0.00</span> m/s²</p>"
            "<p>Valor Dose de Vibração Exponencial (VDVEXP): <span id='vdvexp'>0.00</span></p>"
            "<p>Frequência Média: <span id='frequencia'>0.00</span> Hz</p>"
            "</div>"
            "<div id='config' class='tab'>"
            "<p>Tempo de Exposição (Texp): <input type='number' id='texp' value='1.0' step='0.1'>"
            "<button onclick='setTexp()'>Atualizar</button></p>"
            "</div>"
            "</body></html>");
    });


}

    server.on("/data", []() {
        String json = "{\"am\":" + String(dados.am) + 
                      ",\"amr\":" + String(dados.amr) + 
                      ",\"are\":" + String(dados.are) + 
                      ",\"aren\":" + String(dados.aren) + 
                      ",\"arep\":" + String(dados.arep) + 
                      ",\"vdvr\":" + String(dados.vdvr) + 
                      ",\"cf\":" + String(dados.cf) + 
                      ",\"amj\":" + String(dados.amj) + 
                      ",\"pico\":" + String(dados.pico) + 
                      ",\"vdvexp\":" + String(dados.vdvexp) + 
                      ",\"frequencia\":" + String(dados.frequencia) + "}";
        server.send(200, "application/json", json);
    });

    server.on("/setTexp", HTTP_POST, []() {
        if (server.hasArg("texp")) {
            Texp = server.arg("texp").toFloat();
        }
        server.send(200, "text/plain", "Texp atualizado");
    });

    server.begin();
}

// --------------------- LOOP PRINCIPAL ---------------------
void loop() {
    server.handleClient();
    sensors_event_t event;
    adxl.getEvent(&event);

    // Calcule os dados de vibração com os novos valores de aceleração
    dados.am = aplicarFiltroPonderacao(calcularAM(event.acceleration.x, event.acceleration.y, event.acceleration.z), filtroAtual);
    dados.amr = aplicarFiltroPonderacao(calcularAMR(event.acceleration.x, event.acceleration.y, event.acceleration.z), filtroAtual);
    dados.are = aplicarFiltroPonderacao(calcularARE(event.acceleration.x, event.acceleration.y, event.acceleration.z), filtroAtual);
    dados.aren = calcularAREN(dados.are, Texp);
    dados.arep = aplicarFiltroPonderacao(calcularAREP(event.acceleration.x, event.acceleration.y, event.acceleration.z), filtroAtual);
    dados.vdvr = calcularVDVR(event.acceleration.x, offsetX);
    dados.cf = calcularCF(dados.am, dados.are);
    dados.amj = aplicarFiltroPonderacao(calcularAMJ(event.acceleration.x, event.acceleration.y, event.acceleration.z), filtroAtual);
    dados.pico = calcularPICO(event.acceleration.x, event.acceleration.y, event.acceleration.z);
    dados.vdvexp = calcularVDVEXP(event.acceleration.x, dados.vdvr);

    // Calcula a frequência média usando FFT
    dados.frequencia = calcularFrequenciaFFT();
}

// --------------------- FUNÇÕES NOVAS ---------------------
float calcularFrequenciaFFT() {
    for (int i = 0; i < SAMPLES; i++) {
        sensors_event_t event;
        adxl.getEvent(&event);
        vReal[i] = calcularAM(event.acceleration.x, event.acceleration.y, event.acceleration.z);
        vImag[i] = 0;
        delayMicroseconds(1000000 / SAMPLING_FREQUENCY);
    }

    FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.compute(FFT_FORWARD);
    FFT.complexToMagnitude();

    double peakFrequency = FFT.majorPeak();
    return peakFrequency;
}

float calcularAREN(float are, float texp) {
    return are / texp;
}


float calcularAM(float x, float y, float z) {
    // Implementar o cálculo de AM
    return sqrt(x * x + y * y + z * z);
}

float aplicarFiltroPonderacao(float valor, FiltroPonderacao filtro) {
    // Implementar o filtro de ponderação
    return valor; // Retorna o valor sem filtro, como exemplo
}

float calcularAMR(float x, float y, float z) {
    // Implementar o cálculo de AMR
    return fabs(x) + fabs(y) + fabs(z);
}

float calcularARE(float x, float y, float z) {
    // Implementar o cálculo de ARE
    return sqrt(x * x + y * y + z * z) / sqrt(3);
}

float calcularAREP(float x, float y, float z) {
    // Implementar o cálculo de AREP
    return fmax(fabs(x), fmax(fabs(y), fabs(z)));
}

float calcularVDVR(float x, float offset) {
    // Implementar o cálculo de VDVR
    return x - offset;
}

float calcularCF(float am, float are) {
    // Implementar o cálculo de CF
    return am / are;
}

float calcularAMJ(float x, float y, float z) {
    // Implementar o cálculo de AMJ
    return fmax(fabs(x), fmax(fabs(y), fabs(z)));
}

float calcularPICO(float x, float y, float z) {
    // Implementar o cálculo de PICO
    return fmax(fabs(x), fmax(fabs(y), fabs(z)));
}

float calcularVDVEXP(float x, float vdvr) {
    // Implementar o cálculo de VDVEXP
    return x + vdvr;
}
