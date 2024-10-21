/*
  -----------------------------
  Acelerômetro - Monitoramento e Cálculo de Vibração
  Versão: 1.0.3
  Data: 20/10/2024
  Autor: Wellington Barros Rosa
  E-mail: wellington@bewaves.com.br
  Descrição: Este programa foi desenvolvido para monitorar as leituras de aceleração 
  em tempo real usando um acelerômetro MPU6050 e exibi-las através de uma 
  interface web. O sistema calcula métricas como:
  - Aceleração resultante de exposição para mãos e braços (ARE).
  - Aceleração normalizada de exposição (AREN).
  - O fator Dy para estimar a exposição que pode levar ao aparecimento de dedos brancos.
  - Ajuste de offsets em g para cada eixo.
  -------------------------------
  Atualizações:
  - 20/10/2024: Implementação de ajustes de offset em g, conversão de medidas para m/s², 
    e cálculo da Aceleração Resultante de Exposição (ARE), Aceleração Normalizada de Exposição (AREN)
    e o fator Dy com base nas leituras do acelerômetro.
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
float offsetX = 0; // Offset do acelerômetro X em g
float offsetY = 0; // Offset do acelerômetro Y em g
float offsetZ = 0; // Offset do acelerômetro Z em g

// ----------------------------------------------------
// Variáveis para cálculo de aceleração
// ----------------------------------------------------
float ARE;  // Aceleração Resultante
float AREN; // Aceleração Normalizada
float Dy;   // Cálculo de Dy
float Texp = 1; // Tempo de exposição (em horas)
int anos = 0, meses = 0, dias = 0; // Variáveis de tempo baseadas em Dy

// ----------------------------------------------------
// Limites para alertas
// ----------------------------------------------------
const float LIMITE_AREN_VMB = 5.0;  // Limite de VMB
const float LIMITE_AREN_VCI = 1.15; // Limite de VCI

// ----------------------------------------------------
// Variáveis de controle
// ----------------------------------------------------
bool isReading = false;            // Estado da leitura

// ----------------------------------------------------
// Configurações de Wi-Fi
// ----------------------------------------------------
const char* ssid_ap = "Acelerometro"; // Nome do ponto de acesso
const char* password_ap = "";         // Senha do ponto de acesso
const char* ssid_sta = "Suarede";       // Nome da rede Wi-Fi
const char* password_sta = "suasenha"; // Senha da rede Wi-Fi

WebServer server; // Instância do servidor

// ----------------------------------------------------
// Página HTML para a interface com footer modificado
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
            font-family: 'Arial', sans-serif;
            background-color: #000000;
            color: #000000;
            margin: 0;
            padding: 20px;
            text-align: center;
        }
        h1 {
            color: #9aaf4c;
            margin-bottom: 20px;
        }
        .data-row {
            display: flex;
            justify-content: space-around;
            width: 80%;
            margin: 10px 0;
            padding: 15px;
            border: 1px solid #ddd;
            border-radius: 8px;
            background-color: #fff;
            box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
        }
        button {
            margin: 5px;
            padding: 10px 20px;
            font-size: 16px;
            border: none;
            border-radius: 5px;
            background-color: #fdf903;
            color: #000000;
            cursor: pointer;
            transition: background-color 0.3s;
        }
        button:hover {
            background-color: #ffffff;
        }
        input {
            margin: 5px;
            padding: 10px;
            font-size: 16px;
            border: 1px solid #ddd;
            border-radius: 5px;
        }
        footer {
            margin-top: 20px;
            padding: 10px;
            border-top: 1px solid #ddd;
            width: 100%;
            text-align: center;
            background-color: #f0f0f0;
        }
        footer p {
            margin: 5px;
            font-size: 14px;
        }
    </style>
</head>
<body>
<h1>Leitura de Acelerômetro</h1>

<div class="data-row">
    <p>Aceleração X: <span id="accelX"></span> m/s²</p>
    <p>Aceleração Y: <span id="accelY"></span> m/s²</p>
    <p>Aceleração Z: <span id="accelZ"></span> m/s²</p>
</div>

<div class="data-row">
    <p>ARE: <span id="ARE"></span> m/s²</p>
    <p>AREN: <span id="AREN"></span> m/s²</p>
    <p>Dy: <span id="Dy"></span> Anos</p>
</div>

<h3>Iniciar Leituras</h3>
<button onclick="startReadings()">Iniciar</button>
<button onclick="stopReadings()">Parar</button>

<h3>Ajustar Tempo de Exposição</h3>
<input type="number" id="Texp_input" placeholder="Tempo em horas" step="0.1">
<button onclick="setTime()">Ajustar Tempo</button>

<h3>Ajustar Offsets</h3>
<input type="number" id="offsetX_input" placeholder="Offset X" step="0.1">
<input type="number" id="offsetY_input" placeholder="Offset Y" step="0.1">
<input type="number" id="offsetZ_input" placeholder="Offset Z" step="0.1">
<button onclick="setOffsets()">Aplicar Offsets</button>

<footer>
    <p>Offset X: <span id="offsetX_display"></span> g</p>
    <p>Offset Y: <span id="offsetY_display"></span> g</p>
    <p>Offset Z: <span id="offsetZ_display"></span> g</p>
    <p>Temperatura: <span id="temperature"></span> °C</p>
    <p>Desenvolvido por Wellington Barros Rosa</p>
</footer>

<script>
    // Função para iniciar as leituras
    function startReadings() {
        fetch('/start_readings', { method: 'POST' });
    }

    // Função para parar as leituras
    function stopReadings() {
        fetch('/stop_readings', { method: 'POST' });
    }

    // Função para ajustar o tempo de exposição
    function setTime() {
        const Texp = document.getElementById('Texp_input').value;
        fetch('/set_time?Texp=' + Texp, { method: 'POST' });
    }

    // Função para ajustar os offsets
    function setOffsets() {
        const offsetX = document.getElementById('offsetX_input').value;
        const offsetY = document.getElementById('offsetY_input').value;
        const offsetZ = document.getElementById('offsetZ_input').value;
        fetch(`/set_offsets?offsetX=${offsetX}&offsetY=${offsetY}&offsetZ=${offsetZ}`, { method: 'POST' });
    }

    // Atualiza as leituras na página
    function updateReadings() {
        fetch('/get_readings')
            .then(response => response.json())
            .then(data => {
                document.getElementById('accelX').innerText = data.accelX;
                document.getElementById('accelY').innerText = data.accelY;
                document.getElementById('accelZ').innerText = data.accelZ;
                document.getElementById('ARE').innerText = data.ARE;
                document.getElementById('AREN').innerText = data.AREN;
                document.getElementById('Dy').innerText = data.Dy;
                document.getElementById('offsetX_display').innerText = data.offsetX;
                document.getElementById('offsetY_display').innerText = data.offsetY;
                document.getElementById('offsetZ_display').innerText = data.offsetZ;
                document.getElementById('temperature').innerText = data.temperature;
            });
    }

    setInterval(updateReadings, 1000); // Atualiza a cada segundo
</script>
</body>
</html>
)rawliteral";

// ----------------------------------------------------
// Funções de inicialização
// ----------------------------------------------------
void setup() {
    Serial.begin(115200);
    // Conexão Wi-Fi em modo de ponto de acesso
    WiFi.softAP(ssid_ap, password_ap);
    Serial.println("Ponto de acesso iniciado");

    // Conexão ao Wi-Fi em modo STA
    WiFi.begin(ssid_sta, password_sta);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Conectando ao Wi-Fi...");
    }
    Serial.println("Conectado ao Wi-Fi");

    // Inicializa o sensor MPU6050
    if (!mpu.begin()) {
        Serial.println("Falha ao encontrar o MPU6050");
        while (1);
    }
    Serial.println("MPU6050 encontrado");

    // Inicia o servidor web
    server.on("/", []() {
        server.send(200, "text/html", htmlPage);
    });
    server.on("/start_readings", HTTP_POST, []() {
        isReading = true;
        server.send(200, "text/plain", "Leituras iniciadas");
    });
    server.on("/stop_readings", HTTP_POST, []() {
        isReading = false;
        server.send(200, "text/plain", "Leituras paradas");
    });
    server.on("/get_readings", HTTP_GET, []() {
        float accelX = 0, accelY = 0, accelZ = 0;
        if (isReading) {
            sensors_event_t a, g, temp;
            mpu.getEvent(&a, &g, &temp);

            // Aplica offsets e converte para m/s²
            accelX = (a.acceleration.x - offsetX * 9.81); // Conversão de g para m/s²
            accelY = (a.acceleration.y - offsetY * 9.81); // Conversão de g para m/s²
            accelZ = (a.acceleration.z - offsetZ * 9.81); // Conversão de g para m/s²

            // Cálculo de ARE e AREN
            ARE = sqrt(pow(accelX, 2) + pow(accelY, 2) + pow(accelZ, 2)); // Aceleração Resultante em m/s²
            AREN = ARE * sqrt(Texp / 8.0); // Cálculo de AREN
            Dy = 31.8 * pow(AREN, -1.06); // Cálculo de Dy

            // Conversão para g para a interface
            float accelX_g = accelX / 9.81; // Conversão para g
            float accelY_g = accelY / 9.81; // Conversão para g
            float accelZ_g = accelZ / 9.81; // Conversão para g

            // Envia as leituras de volta ao cliente
            String jsonResponse = "{";
            jsonResponse += "\"accelX\": " + String(accelX_g, 2) + ",";
            jsonResponse += "\"accelY\": " + String(accelY_g, 2) + ",";
            jsonResponse += "\"accelZ\": " + String(accelZ_g, 2) + ",";
            jsonResponse += "\"ARE\": " + String(ARE, 2) + ",";
            jsonResponse += "\"AREN\": " + String(AREN, 2) + ",";
            jsonResponse += "\"Dy\": " + String(Dy, 2) + ",";
            jsonResponse += "\"offsetX\": " + String(offsetX, 2) + ",";
            jsonResponse += "\"offsetY\": " + String(offsetY, 2) + ",";
            jsonResponse += "\"offsetZ\": " + String(offsetZ, 2) + ",";
            jsonResponse += "\"temperature\": " + String(temp.temperature, 2);
            jsonResponse += "}";
            server.send(200, "application/json", jsonResponse);
        } else {
            server.send(200, "application/json", "{\"status\":\"Leitura parada\"}");
        }
    });
    server.on("/set_time", HTTP_POST, []() {
        if (server.hasArg("Texp")) {
            Texp = server.arg("Texp").toFloat();
            server.send(200, "text/plain", "Tempo de exposição ajustado");
        } else {
            server.send(400, "text/plain", "Parâmetro Texp não fornecido");
        }
    });
    server.on("/set_offsets", HTTP_POST, []() {
        if (server.hasArg("offsetX") && server.hasArg("offsetY") && server.hasArg("offsetZ")) {
            offsetX = server.arg("offsetX").toFloat();
            offsetY = server.arg("offsetY").toFloat();
            offsetZ = server.arg("offsetZ").toFloat();
            server.send(200, "text/plain", "Offsets aplicados");
        } else {
            server.send(400, "text/plain", "Parâmetros de offset não fornecidos");
        }
    });

    server.begin();
}

// ----------------------------------------------------
// Loop principal
// ----------------------------------------------------
void loop() {
    server.handleClient();
    // Apenas para fins de depuração, você pode monitorar a conexão
    Serial.println("Servindo cliente...");
}

