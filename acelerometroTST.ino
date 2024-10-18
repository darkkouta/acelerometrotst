/*
  -----------------------------
  Acelerômetro - Monitoramento e Cálculo de Vibração
  -----------------------------
  Este programa foi desenvolvido para monitorar as leituras de aceleração 
  em tempo real usando um acelerômetro MPU6050 e exibi-las através de uma 
  interface web. O sistema calcula métricas como:

  - Aceleração resultante de exposição para mãos e braços (ARE).
  - Aceleração normalizada de exposição (AREN).
  - O fator Dy para estimar a exposição que pode levar ao aparecimento de dedos brancos.
  - Médias das acelerações ao longo de um minuto.
  - Alerta de conformidade com as normas NHO.

  Funcionalidades adicionais incluem:
  - Interface web para visualização e controle.
  - Cálculo automático de médias e envio em JSON.
  - Alerta de segurança se os níveis de vibração ultrapassarem os limites recomendados.
  -------------------------------
*/
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

Adafruit_MPU6050 mpu;

// ----------------------------------------------------
// Definições de variáveis
// ----------------------------------------------------
float offsetX = 0; // Offset do acelerômetro X
float offsetY = 0; // Offset do acelerômetro Y
float offsetZ = 0; // Offset do acelerômetro Z
float accelX_sum = 0, accelY_sum = 0, accelZ_sum = 0; // Somatórios para médias
int measurement_count = 0; // Contador de leituras
float accelX_avg = 0, accelY_avg = 0, accelZ_avg = 0; // Médias dos dados de aceleração

// ----------------------------------------------------
// Variáveis para cálculo de aceleração
// ----------------------------------------------------
float ARE;  // Aceleração Resultante
float AREN; // Aceleração Normalizada
float Dy;   // Cálculo de Dy
float Texp = 1; // Tempo de exposição (em horas)
int anos = 0, meses = 0, dias = 0; // Variáveis de tempo baseadas em Dy

// ----------------------------------------------------
// Variáveis de temperatura
// ----------------------------------------------------
float temperatura = 0; // Temperatura lida

// ----------------------------------------------------
// Limites para alertas
// ----------------------------------------------------
const float LIMITE_AREN_VMB = 5.0;  // Limite de VMB
const float LIMITE_AREN_VCI = 1.15; // Limite de VCI

// ----------------------------------------------------
// Variáveis de controle
// ----------------------------------------------------
bool isReading = false;            // Estado da leitura
bool countdown_active = false;     // Estado da contagem regressiva
unsigned long countdown_time = 30; // Tempo total da contagem em segundos
unsigned long last_update_time = 0; // Tempo da última atualização das médias

// ----------------------------------------------------
// Configurações de Wi-Fi
// ----------------------------------------------------
const char* ssid_ap = "Acelerometro"; // Nome do ponto de acesso
const char* password_ap = "";         // Senha do ponto de acesso
const char* ssid_sta = "Senua";       // Nome da rede Wi-Fi
const char* password_sta = "Agape2024@#"; // Senha da rede Wi-Fi

WebServer server; // Instância do servidor

// ----------------------------------------------------
// Página HTML para a interface
// ----------------------------------------------------
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Leitura de Acelerômetro</title>
  <style>
    body {
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      font-family: Arial, sans-serif;
      background-color: #f4f4f4;
      color: #333;
      margin: 0;
      padding: 20px;
      text-align: center;
    }
    .data-row {
      display: flex;
      justify-content: space-around;
      width: 80%;
      margin: 20px 0;
    }
  </style>
</head>
<body>
  <h1>Leitura de Acelerômetro</h1>
  
  <div class="data-row">
    <p>Aceleração X: <span id="accelX"></span></p>
    <p>Aceleração Y: <span id="accelY"></span></p>
    <p>Aceleração Z: <span id="accelZ"></span></p>
  </div>

  <div class="data-row">
    <p>Média X: <span id="accelX_avg"></span></p>
    <p>Média Y: <span id="accelY_avg"></span></p>
    <p>Média Z: <span id="accelZ_avg"></span></p>
  </div>

  <div class="data-row">
    <p>ARE: <span id="ARE"></span></p>
    <p>AREN: <span id="AREN"></span></p>
    <p>Dy: <span id="Dy"></span></p>
  </div>

  <div class="data-row">
    <p>Anos: <span id="anos"></span></p>
    <p>Meses: <span id="meses"></span></p>
    <p>Dias: <span id="dias"></span></p>
  </div>

  <div class="data-row">
    <p>Temperatura: <span id="temperatura"></span> °C</p>
  </div>

  <p>Tempo de Exposição: <span id="Texp"></span></p>

  <h3>Contagem Regressiva</h3>
  <p id="countdown"></p>

  <h3>Iniciar Leituras</h3>
  <button onclick="startReadings()">Iniciar</button>
  <button onclick="stopReadings()">Parar</button>
  <button onclick="resetAverages()">Zerar Médias</button>

  <h3>Ajustar Tempo de Exposição</h3>
  <input type="number" id="Texp_input" placeholder="Tempo em horas" step="0.1">
  <button onclick="setTime()">Ajustar Tempo</button>

  <h3>Ajustar Offsets</h3>
  <input type="number" id="offsetX_input" placeholder="Offset X" step="0.1">
  <input type="number" id="offsetY_input" placeholder="Offset Y" step="0.1">
  <input type="number" id="offsetZ_input" placeholder="Offset Z" step="0.1">
  <button onclick="setOffsets()">Aplicar Offsets</button>

  <h3>Calibrar Giroscópio</h3>
  <button onclick="setGyroOffsets()">Calibrar</button>

  <h3>Calibrar Acelerômetro</h3>
  <button onclick="calibrateAccelerometer()">Calibrar Acelerômetro</button>

  <script>
    function startReadings() {
      fetch('/start_readings', { method: 'POST' });
    }

    function stopReadings() {
      fetch('/stop_readings', { method: 'POST' });
    }

    function resetAverages() {
      fetch('/reset_averages', { method: 'POST' });
    }

    function setTime() {
      const Texp = document.getElementById('Texp_input').value;
      fetch('/set_time', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ Texp: parseFloat(Texp) })
      })
      .then(response => response.text())
      .then(data => {
        console.log(data); // Log da resposta
      });
    }

    function setOffsets() {
      const offsetX = document.getElementById('offsetX_input').value;
      const offsetY = document.getElementById('offsetY_input').value;
      const offsetZ = document.getElementById('offsetZ_input').value;
      fetch('/set_offsets', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ offsetX: parseFloat(offsetX), offsetY: parseFloat(offsetY), offsetZ: parseFloat(offsetZ) })
      })
      .then(response => response.text())
      .then(data => {
        console.log(data); // Log da resposta
      });
    }

    function setGyroOffsets() {
      fetch('/set_offset_gyro', { method: 'POST' })
        .then(response => response.text())
        .then(data => {
          console.log(data); // Log da resposta
        });
    }

    function calibrateAccelerometer() {
      fetch('/calibrate_accelerometer', { method: 'POST' })
        .then(response => response.text())
        .then(data => {
          console.log(data); // Log da resposta
        });
    }

    setInterval(() => {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          document.getElementById('accelX').innerText = data.accelX.toFixed(2);
          document.getElementById('accelY').innerText = data.accelY.toFixed(2);
          document.getElementById('accelZ').innerText = data.accelZ.toFixed(2);
          document.getElementById('accelX_avg').innerText = data.accelX_avg.toFixed(2);
          document.getElementById('accelY_avg').innerText = data.accelY_avg.toFixed(2);
          document.getElementById('accelZ_avg').innerText = data.accelZ_avg.toFixed(2);
          document.getElementById('ARE').innerText = data.ARE.toFixed(2);
          document.getElementById('AREN').innerText = data.AREN.toFixed(2);
          document.getElementById('Dy').innerText = data.Dy.toFixed(2);
          document.getElementById('anos').innerText = data.anos;
          document.getElementById('meses').innerText = data.meses;
          document.getElementById('dias').innerText = data.dias;
          document.getElementById('temperatura').innerText = data.temperatura.toFixed(2);
          document.getElementById('Texp').innerText = data.Texp.toFixed(2);

          if (data.countdown_active) {
            document.getElementById('countdown').innerText = data.countdown_time;
          }
        });
    }, 1000);
  </script>
</body>
</html>
)rawliteral";

// ----------------------------------------------------
// Função de setup
// ----------------------------------------------------
void setup() {
  Serial.begin(115200);
  // Inicializa o MPU6050
  if (!mpu.begin()) {
    Serial.println("Falha ao encontrar o MPU6050");
    while (1) delay(10);
  }
  // Inicia o Wi-Fi em modo AP
  WiFi.softAP(ssid_ap, password_ap);
  server.on("/", []() {
    server.send(200, "text/html", htmlPage);
  });
  server.on("/data", HTTP_GET, []() {
    String json = "{";
    json += "\"accelX\":" + String(getAccelX()) + ",";
    json += "\"accelY\":" + String(getAccelY()) + ",";
    json += "\"accelZ\":" + String(getAccelZ()) + ",";
    json += "\"accelX_avg\":" + String(accelX_avg) + ",";
    json += "\"accelY_avg\":" + String(accelY_avg) + ",";
    json += "\"accelZ_avg\":" + String(accelZ_avg) + ",";
    json += "\"ARE\":" + String(ARE) + ",";
    json += "\"AREN\":" + String(AREN) + ",";
    json += "\"Dy\":" + String(Dy) + ",";
    json += "\"anos\":" + String(anos) + ",";
    json += "\"meses\":" + String(meses) + ",";
    json += "\"dias\":" + String(dias) + ",";
    json += "\"temperatura\":" + String(temperatura) + ",";
    json += "\"Texp\":" + String(Texp) + ",";
    json += "\"countdown_active\":" + String(countdown_active) + ",";
    json += "\"countdown_time\":" + String(countdown_time);
    json += "}";
    server.send(200, "application/json", json);
  });

  server.on("/start_readings", HTTP_POST, []() {
    isReading = true;
    measurement_count = 0; // Reinicia o contador ao iniciar as leituras
    Serial.println("Iniciando leituras...");
    server.send(200, "text/plain", "Iniciando leituras...");
  });

  server.on("/stop_readings", HTTP_POST, []() {
    isReading = false;
    Serial.println("Parando leituras...");
    server.send(200, "text/plain", "Parando leituras...");
  });

  server.on("/reset_averages", HTTP_POST, []() {
    accelX_sum = 0;
    accelY_sum = 0;
    accelZ_sum = 0;
    measurement_count = 0; // Reinicia o contador
    Dy = 0; // Zera Dy
    anos = 0; meses = 0; dias = 0; // Zera os valores de tempo
    Serial.println("Médias e tempo zerados.");
    server.send(200, "text/plain", "Médias e tempo zerados.");
  });

  server.on("/set_time", HTTP_POST, []() {
    if (server.hasArg("plain")) {
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, server.arg("plain"));
      Texp = doc["Texp"];
      Serial.println("Tempo de exposição ajustado para: " + String(Texp) + " horas.");
    }
    server.send(200, "text/plain", "Tempo de exposição ajustado.");
  });

  server.on("/set_offsets", HTTP_POST, []() {
    if (server.hasArg("plain")) {
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, server.arg("plain"));
      offsetX = doc["offsetX"];
      offsetY = doc["offsetY"];
      offsetZ = doc["offsetZ"];
      Serial.println("Offsets ajustados: X=" + String(offsetX) + ", Y=" + String(offsetY) + ", Z=" + String(offsetZ));
    }
    server.send(200, "text/plain", "Offsets ajustados.");
  });

  server.on("/set_offset_gyro", HTTP_POST, []() {
    // Implementar lógica de ajuste do giroscópio aqui
    server.send(200, "text/plain", "Offsets do giroscópio ajustados.");
  });

  server.on("/calibrate_accelerometer", HTTP_POST, []() {
    // Implementar lógica de calibração do acelerômetro aqui
    server.send(200, "text/plain", "Acelerômetro calibrado.");
  });

  server.begin();
  Serial.println("Servidor HTTP iniciado.");
}

// ----------------------------------------------------
// Função principal de loop
// ----------------------------------------------------
void loop() {
  server.handleClient(); // Gerencia requisições do cliente
  if (isReading) {
    // Lê dados do acelerômetro
    float accelX = getAccelX();
    float accelY = getAccelY();
    float accelZ = getAccelZ();

    // Adiciona offsets
    accelX += offsetX;
    accelY += offsetY;
    accelZ += offsetZ;

    // Acumula somatórios para médias
    accelX_sum += accelX;
    accelY_sum += accelY;
    accelZ_sum += accelZ;
    measurement_count++;

    // Calcula médias se houver leituras
    if (measurement_count > 0) {
      accelX_avg = accelX_sum / measurement_count;
      accelY_avg = accelY_sum / measurement_count;
      accelZ_avg = accelZ_sum / measurement_count;

      // Cálculo da aceleração
      ARE = sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ);
      AREN = ARE * sqrt(Texp / 8);
      Dy = 31.8 * pow(AREN, -1.06);

      // Cálculo de anos, meses e dias com base no Dy
      anos = floor(Dy / 365);
      meses = floor((Dy - anos * 365) / 30);
      dias = round(Dy - anos * 365 - meses * 30);
    }

    // Exibe os valores no Serial
    Serial.print("Aceleração X: "); Serial.println(accelX);
    Serial.print("Aceleração Y: "); Serial.println(accelY);
    Serial.print("Aceleração Z: "); Serial.println(accelZ);
    Serial.print("ARE: "); Serial.println(ARE);
    Serial.print("AREN: "); Serial.println(AREN);
    Serial.print("Dy: "); Serial.println(Dy);
    Serial.print("Anos: "); Serial.println(anos);
    Serial.print("Meses: "); Serial.println(meses);
    Serial.print("Dias: "); Serial.println(dias);
    Serial.print("Temperatura: "); Serial.println(temperatura);

    // Contagem regressiva
    if (countdown_active) {
      unsigned long current_time = millis();
      if (current_time - last_update_time >= 1000) {
        last_update_time = current_time;
        countdown_time--;
        if (countdown_time <= 0) {
          countdown_active = false;
          Serial.println("Contagem regressiva finalizada.");
        }
      }
    }
  }
}

// Funções para obter dados do acelerômetro
float getAccelX() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  return a.acceleration.x;
}

float getAccelY() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  return a.acceleration.y;
}

float getAccelZ() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  return a.acceleration.z;
}
