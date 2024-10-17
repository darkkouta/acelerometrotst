Acelerômetro - Monitoramento e Cálculo de Vibração
Este programa foi desenvolvido para monitorar as leituras de aceleração em tempo real usando um acelerômetro MPU6050 e exibi-las através de uma interface web. O sistema calcula métricas como:

Aceleração resultante de exposição para mãos e braços (ARE).
Aceleração normalizada de exposição (AREN).
O fator 
𝐷
𝑦
Dy para estimar a exposição que pode levar ao aparecimento de dedos brancos.
Médias das acelerações ao longo de um minuto.
Alerta de conformidade com as normas NHO.
O usuário pode ajustar o tempo de exposição (Texp) via interface web, além de iniciar e parar a coleta de dados manualmente.

Funcionalidades
Interface web para visualização e controle.
Cálculo automático de médias e envio em JSON.
Alerta de segurança se os níveis de vibração ultrapassarem os limites recomendados.
Dependências
Para compilar este código, você precisará das seguintes bibliotecas:

Wire: Para comunicação I2C.
Adafruit_MPU6050: Biblioteca para o acelerômetro MPU6050.
WiFi: Para a conectividade sem fio.
WebServer: Para criar um servidor web.
Configuração de Wi-Fi
O programa cria uma rede Wi-Fi (ponto de acesso) sem senha, permitindo que dispositivos se conectem e acessem a interface web. O nome da rede e a senha podem ser definidos nas constantes ap_ssid e ap_password.

Uso
Conexão: Conecte seu dispositivo à rede Wi-Fi criada pelo programa.
Acesso à interface: Abra um navegador e acesse o IP exibido no monitor serial (normalmente 192.168.4.1).
Ajuste o Tempo de Exposição: Use a interface para definir o tempo de exposição (Texp) em horas.
Iniciar Parar Leituras: Inicie ou pare as leituras de aceleração através da interface.
Fórmulas
Aceleração Resultante de Exposição (ARE): Representa a aceleração medida do acelerômetro.

Aceleração Normalizada de Exposição (AREN):

AREN é a aceleração normalizada de exposição.



Observações
A funcionalidade de cálculo da aceleração normalizada de exposição para corpo inteiro está em desenvolvimento.
Filtros adicionais para melhorar a precisão das medições estão sendo planejados.
Este projeto é licenciado sob a Licença MIT. Veja o arquivo LICENSE para mais detalhes.

## Contato

Para mais informações, consulte o repositório do projeto no GitHub https://github.com/darkkouta/acelerometrotst ou entre em contato.


