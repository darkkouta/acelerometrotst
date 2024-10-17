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
float AREN = 0.0; // Aceleração normalizada de exposição
const float T_0 = 8.0; // Tempo de referência de 8 horas
float Texp = 1.0;  // Valor inicial de Texp em horas (pode ser alterado via web)

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

// HTML da página web com formulário para alterar o Texp
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
    input[type="number"], input[type="submit"] { padding: 5px; }
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
        document.getElementById('Texp').innerText = data.Texp.toFixed(2) + " horas";
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

  <p>ARE: <span id="ARE">0.00 m/s²</span></p>
  <p>AREN: <span id="AREN">0.00 m/s²</span></p>
  <p>Dy: <span id="Dy">0.00</span></p>
  <p>Texp: <span id="Texp">0.00 horas</span></p>
  
  <p id="alertaHAV"></p>
  <p id="alertaWBV"></p>
  
  <h3>Ajuste do Tempo de Exposição (Texp)</h3>
  <form action="/set_time" method="POST">
    <input type="number" name="Texp" min="0.1" max="24.0" step="0.1" value="1.0">
    <input type="submit" value="Ajustar">
  </form>

  <h3>Controle de Leitura</h3>
  <form action="/start_stop" method="POST">
    <input type="submit" value="Iniciar/Parar Leituras">
  </form>

  <h3>Contagem Regressiva</h3>
  <p id="countdown">60 segundos restantes</p>
</body>
</html>
)rawliteral";

// Funções para inicialização e loop
void setup() {
  Serial.begin(115200);
  // Inicialização do acelerômetro
  if (!mpu.begin()) {
    Serial.println("Falha ao inicializar MPU6050");
    while (1);
  }
  
  // Inicia a rede Wi-Fi
  WiFi.softAP(ap_ssid, ap_password);
  Serial.println("Rede Wi-Fi iniciada");
  
  // Define rotas para o servidor web
  server.on("/", []() {
    server.send(200, "text/html", htmlPage);
  });
  
  server.on("/data", []() {
    server.send(200, "application/json", String("{\"accelX\":") + accelX + ",\"accelY\":" + accelY + ",\"accelZ\":" + accelZ +
                 ",\"accelX_avg\":" + accelX_avg + ",\"accelY_avg\":" + accelY_avg + ",\"accelZ_avg\":" + accelZ_avg +
                 ",\"ARE\":" + ARE + ",\"AREN\":" + AREN + ",\"Dy\":" + Dy +
                 ",\"Texp\":" + Texp + ",\"countdown\":" + (countdown_active ? (countdown_time - (millis() - countdown_start_time) / 1000) : countdown_time) +
                 ",\"alertaHAV\":\"" + (AREN > LIMITE_AREN_HAV ? "ALERTA: Aceleração para Mãos e Braços ultrapassou o limite!" : "") +
                 "\",\"alertaWBV\":\"" + (AREN > LIMITE_AREN_WBV ? "ALERTA: Aceleração para Corpo Inteiro ultrapassou o limite!" : "") + "\"}");
  });
  
  server.on("/set_time", HTTP_POST, []() {
    if (server.hasArg("Texp")) {
      Texp = server.arg("Texp").toFloat();
      Serial.print("Tempo de exposição ajustado: ");
      Serial.println(Texp);
    }
    server.send(200, "text/html", htmlPage); // Redireciona de volta para a página
  });

  server.on("/start_stop", HTTP_POST, []() {
    isReading = !isReading; // Alterna o estado de leitura
    if (isReading) {
      Serial.println("Leitura iniciada");
      measurement_start_time = millis();
      measurement_count = 0; // Reseta a contagem
      accelX_sum = 0.0;
      accelY_sum = 0.0;
      accelZ_sum = 0.0;
      countdown_start_time = millis(); // Inicia a contagem
      countdown_active = true; // Ativa a contagem regressiva
    } else {
      Serial.println("Leitura parada");
      countdown_active = false; // Para a contagem regressiva
    }
    server.send(200, "text/html", htmlPage); // Redireciona de volta para a página
  });
  
  server.begin();
  Serial.println("Servidor web iniciado");
}

void loop() {
  server.handleClient();

  if (isReading) {
    // Lê dados do acelerômetro
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    accelX = a.acceleration.x;
    accelY = a.acceleration.y;
    accelZ = a.acceleration.z;

    // Acumula as leituras
    accelX_sum += accelX;
    accelY_sum += accelY;
    accelZ_sum += accelZ;
    measurement_count++;

    // Cálculo da média
    if (measurement_count > 0) {
      accelX_avg = accelX_sum / measurement_count;
      accelY_avg = accelY_sum / measurement_count;
      accelZ_avg = accelZ_sum / measurement_count;
    }

    // Cálculo de ARE e AREN
    ARE = sqrt(accelX_avg * accelX_avg + accelY_avg * accelY_avg + accelZ_avg * accelZ_avg);
    AREN = ARE / T_0;  // Normaliza pela referência de tempo T_0

    // Cálculo de Dy
    Dy = AREN * pow(Texp, -1.06); // Cálculo conforme a norma

    // Verifica se os limites foram ultrapassados
    if (AREN > LIMITE_AREN_HAV) {
      Serial.println("ALERTA: AREN para Mãos e Braços ultrapassou o limite!");
    }
    if (AREN > LIMITE_AREN_WBV) {
      Serial.println("ALERTA: AREN para Corpo Inteiro ultrapassou o limite!");
    }
    
    // Reset de contagem após um minuto
    if (countdown_active && (millis() - countdown_start_time) / 1000 >= countdown_time) {
      isReading = false; // Para a leitura
      countdown_active = false; // Para a contagem regressiva
      Serial.println("Contagem regressiva finalizada, leitura parada.");
    }
  }
}
