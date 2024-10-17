/*
  -----------------------------
  Acelerômetro - Monitoramento e Cálculo de Vibração
  -----------------------------
  Este programa foi desenvolvido para monitorar as leituras de aceleração 
  em tempo real usando um acelerômetro MPU6050 e exibi-las através de uma 
  interface web. O sistema calcula métricas como:

  - Aceleração resultante de exposição para mãos e braços (ARE).
  - Aceleração normalizada de exposição para corpo inteiro em desenvolvimento.
  - O fator Dy para estimar a exposição que pode levar ao aparecimento de dedos brancos.
  - Médias das acelerações ao longo de um minuto.
  - Alerta de conformidade com as normas NHO.

  O usuário pode ajustar o tempo de medição (Tmexp) via interface web, 
  além de iniciar e parar a coleta de dados manualmente.

  Funcionalidades adicionais incluem:
  - Interface web para visualização e controle.
  - Cálculo automático de médias e envio em JSON.
  - Alerta de segurança se os níveis de vibração ultrapassarem os limites recomendados.

  Github: *Link do Projeto no GitHub (opcional)*
  -------------------------------
*/
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <WiFi.h>
#include <WebServer.h>
#include <math.h>

// Definições do ponto de acesso
const char* ap_ssid = "Acelerometro_AP"; // Nome da rede
const char* ap_password = ""; // Rede sem senha (livre)

// Inicializa o servidor web
WebServer server(80);

// Cria um objeto para o acelerômetro
Adafruit_MPU6050 mpu;

// Variáveis para o cálculo de vibração
float ARE = 0.0;  // Aceleração resultante de exposição 
float AREN = 0.0; // Aceleração resultante de exposição normalizada
const float T_0 = 8.0; // Tempo de referência de 8 horas
float Tmed = 1.0;  // Valor inicial de Tmed em horas (pode ser alterado via web)

// Variáveis para armazenar os dados lidos do acelerômetro
float accelX = 0.0;
float accelY = 0.0;
float accelZ = 0.0;
float accelX_sum = 0.0;
float accelY_sum = 0.0;
float accelZ_sum = 0.0;
float accelX_avg = 0.0;
float accelY_avg = 0.0;
float accelZ_avg = 0.0;
int measurement_count = 0;
unsigned long measurement_start_time = 0;
bool isReading = false; // Estado da leitura

// Temporizador
unsigned long countdown_time = 60;  // Tempo de medição em segundos (1 minuto)
unsigned long countdown_start_time = 0;
bool countdown_active = false;

// Variáveis para Dy
float Dy = 0.0; // Cálculo de Dy com expoente -1.06

// Limites de NHO-10
const float LIMITE_AREN_HAV = 5.0;  // Limite de AREN para mãos e braços (HAV)
const float LIMITE_AREN_WBV = 1.15; // Limite de AREN para corpo inteiro (WBV)

// HTML da página web com formulário para alterar o Tmed
const char* htmlPage = R"rawliteral(
<!DOCTYPE HTML>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <title>Acelerômetro</title>
  <style>
    body { text-align: center; font-family: Arial, sans-serif; }
    table { margin: 0 auto; border-collapse: collapse; }
    table, th, td { border: 1px solid black; padding: 10px; }
    input[type="number"] { padding: 5px; }
  </style>
  <script>
    function updateData() {
      fetch('/data')
      .then(response => response.json())
      .then(data => {
        document.getElementById('accelX').innerText = data.accelX.toFixed(2) + " m/s²";
        document.getElementById('accelY').innerText = data.accelY.toFixed(2) + " m/s²";
        document.getElementById('accelZ').innerText = data.accelZ.toFixed(2) + " m/s²";
        document.getElementById('accelX_avg').innerText = data.accelX_avg.toFixed(2) + " m/s² (Média X)";
        document.getElementById('accelY_avg').innerText = data.accelY_avg.toFixed(2) + " m/s² (Média Y)";
        document.getElementById('accelZ_avg').innerText = data.accelZ_avg.toFixed(2) + " m/s² (Média Z)";
        document.getElementById('ARE').innerText = data.ARE.toFixed(2) + " m/s² ";
        document.getElementById('AREN').innerText = data.AREN.toFixed(2) + " m/s² ";
        document.getElementById('Dy').innerText = data.Dy.toFixed(2) + " (Dy - Aparecimento de Dedos Brancos anos)";
        document.getElementById('Tmed').innerText = data.Tmed.toFixed(2) + " horas";
        document.getElementById('alertaHAV').innerText = data.alertaHAV;
        document.getElementById('alertaWBV').innerText = data.alertaWBV;
        document.getElementById('countdown').innerText = data.countdown + " segundos restantes";
      });
    }

    setInterval(updateData, 1000); // Atualiza a cada 1000ms
  </script>
</head>
<body>
  <h1>Dados do Acelerômetro</h1>

  <table>
    <tr><th>Eixo</th><th>Aceleração (m/s²)</th></tr>
    <tr><td>X</td><td id="accelX">0.00</td></tr>
    <tr><td>Y</td><td id="accelY">0.00</td></tr>
    <tr><td>Z</td><td id="accelZ">0.00</td></tr>
  </table>

  <p>Média X: <span id="accelX_avg">0.00 m/s²</span></p>
  <p>Média Y: <span id="accelY_avg">0.00 m/s²</span></p>
  <p>Média Z: <span id="accelZ_avg">0.00 m/s²</span></p>

  <p>ARE : <span id="ARE">0.00 m/s²</span></p>
  <p>AREN : <span id="AREN">0.00 m/s²</span></p>
  <p>Dy: <span id="Dy">0.00</span></p>
  <p>Tmed: <span id="Tmed">0.00 horas</span></p>

  <p id="alertaHAV" style="color:red;"></p>
  <p id="alertaWBV" style="color:red;"></p>
  
  <p id="countdown" style="font-weight:bold;">Tempo restante: 60 segundos</p>

  <form action="/setTmed" method="POST">
    <label for="tmed">Definir Tempo de exposição (horas):</label><br>
    <input type="number" id="tmed" name="tmed" value="1" min="0.01" step="0.01"><br><br>
    <input type="submit" value="Atualizar Texp">
  </form>

  <form action="/toggle" method="POST">
    <input type="submit" value="Iniciar/Parar Leituras">
  </form>

</body>
</html>
)rawliteral";

// Declaração das funções antes da função setup
void handleRoot();
void handleData();
void handleSetTmed();
void handleToggle();

void setup() {
  Serial.begin(115200);  // Inicializa o Monitor Serial
  
  // Configura o modo de ponto de acesso (sem senha)
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid);  // Sem senha, rede aberta
  
  // Exibe o IP do AP no Monitor Serial
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Inicializa o acelerômetro
  Wire.begin();
  if (!mpu.begin()) {
    Serial.println("Falha ao inicializar MPU6050");
    while (1);
  }

  // Configura a sensibilidade para 2g
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);

  // Manipuladores de requisição web
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/setTmed", HTTP_POST, handleSetTmed); // Manipulador para atualizar o Tmed
  server.on("/toggle", HTTP_POST, handleToggle); // Manipulador para iniciar/parar leituras
  server.begin();

  Serial.println("Servidor iniciado. Acesse a página pelo IP mostrado acima.");
}

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleData() {
  if (!isReading) {
    server.send(200, "application/json", "{}");
    return;
  }

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Acelerações em m/s²
  accelX = a.acceleration.x;
  accelY = a.acceleration.y;
  accelZ = a.acceleration.z;

  // Soma as acelerações para calcular a média
  accelX_sum += accelX;
  accelY_sum += accelY;
  accelZ_sum += accelZ;
  measurement_count++;

  // Calcula a média das acelerações
  accelX_avg = accelX_sum / measurement_count;
  accelY_avg = accelY_sum / measurement_count;
  accelZ_avg = accelZ_sum / measurement_count;

  // Calcula o RMS (Raiz quadrada da média dos quadrados) da aceleração para mãos e braços
  ARE = sqrt(pow(accelX, 2) + pow(accelY, 2) + pow(accelZ, 2));
  AREN = ARE * sqrt(Tmed / T_0);

  // Calcula o Dy (dedos brancos)
  Dy = pow(ARE / AREN, -1.06);

  // Verifica se as leituras estão acima dos limites da norma NHO-10
  String alertaHAV = (AREN > LIMITE_AREN_HAV) ? "Alerta! Exposição VMB acima do limite!" : "Exposição VMB dentro do limite.";
  String alertaWBV = (AREN > LIMITE_AREN_WBV) ? "Alerta! Exposição VCI acima do limite!" : "Exposição VCI dentro do limite.";

  // Calcula o tempo restante
  unsigned long elapsed_time = (millis() - countdown_start_time) / 1000;
  unsigned long countdown = countdown_time - elapsed_time;
  if (countdown <= 0) {
    countdown = 0;
    isReading = false; // Para a leitura ao final do tempo
  }

  // Envia os dados em formato JSON para a página
  String jsonData = "{";
  jsonData += "\"accelX\":" + String(accelX) + ",";
  jsonData += "\"accelY\":" + String(accelY) + ",";
  jsonData += "\"accelZ\":" + String(accelZ) + ",";
  jsonData += "\"accelX_avg\":" + String(accelX_avg) + ",";
  jsonData += "\"accelY_avg\":" + String(accelY_avg) + ",";
  jsonData += "\"accelZ_avg\":" + String(accelZ_avg) + ",";
  jsonData += "\"ARE\":" + String(ARE) + ",";
  jsonData += "\"AREN\":" + String(AREN) + ",";
  jsonData += "\"Dy\":" + String(Dy) + ",";
  jsonData += "\"Tmed\":" + String(Tmed) + ",";
  jsonData += "\"alertaHAV\":\"" + alertaHAV + "\",";
  jsonData += "\"alertaWBV\":\"" + alertaWBV + "\",";
  jsonData += "\"countdown\":" + String(countdown);
  jsonData += "}";

  server.send(200, "application/json", jsonData);
}

void handleSetTmed() {
  if (server.hasArg("tmed")) {
    Tmed = server.arg("tmed").toFloat();
    Serial.print("Tmed atualizado para ");
    Serial.println(Tmed);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleToggle() {
  isReading = !isReading;
  if (isReading) {
    // Reinicia os valores de somatório e contador
    accelX_sum = 0.0;
    accelY_sum = 0.0;
    accelZ_sum = 0.0;
    measurement_count = 0;
    
    // Inicia o temporizador de contagem regressiva
    countdown_start_time = millis();
    Serial.println("Leitura iniciada.");
  } else {
    Serial.println("Leitura parada.");
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void loop() {
  server.handleClient();  // Processa as requisições
}
