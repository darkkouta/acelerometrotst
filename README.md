Acelerômetro - Monitoramento e Cálculo de Vibração
Este projeto tem como objetivo monitorar e analisar dados de vibração capturados por um acelerômetro MPU6050, exibindo as informações em tempo real através de uma interface web. O sistema foi desenvolvido para calcular e alertar sobre condições de vibração perigosas, tanto para mãos e braços (HAV) quanto para corpo inteiro (WBV). Abaixo estão as principais funcionalidades e o status atual do desenvolvimento.

Funcionalidades
Monitoramento em Tempo Real:

Leitura contínua das acelerações nos eixos X, Y e Z.
Cálculo de Aceleração Resultante de Exposição (ARE):

Para mãos e braços (HAV).
Aceleração Resultante de Exposição Normalizada (AREN):

Para mãos e braços (HAV), calculada com base na aceleração medida e no tempo de exposição ajustável pelo usuário. O cálculo de AREN para corpo inteiro (WBV) está em desenvolvimento.
Fator Dy:

Estima o risco de desenvolvimento da síndrome de dedos brancos (causada por vibrações nas mãos e braços).
Cálculo de Médias:

Média das acelerações em cada eixo ao longo de um minuto.
Alertas de Segurança:

Alertas se os níveis de exposição vibratória ultrapassarem os limites recomendados pela norma NHO-10.
Interface Web:

Interface amigável para controle e visualização dos dados capturados. O usuário pode iniciar/parar medições e ajustar o tempo de exposição diretamente pela página web.
Funcionalidades em Desenvolvimento
Exposição para Corpo Inteiro (WBV):

O cálculo de aceleração resultante para exposição de corpo inteiro está em desenvolvimento e será incluído em futuras atualizações.
Filtros de Ruído:

Implementação de filtros digitais para melhorar a precisão das leituras e eliminar ruídos indesejados nas medições.
Como Utilizar
Configurar o Projeto:

Clone o repositório para o seu ambiente local.
Instale as bibliotecas necessárias, como Adafruit_MPU6050 e WiFi para ESP32.
Compilar e Fazer Upload:

Compile o código e faça o upload para um ESP32 com um MPU6050 conectado.
Conecte-se à Rede:

Após carregar o código, o ESP32 iniciará como ponto de acesso com o nome Acelerometro_AP. Conecte-se a essa rede para acessar a interface web.
Acessar a Interface Web:

Abra um navegador e acesse o IP exibido no monitor serial para visualizar os dados e controlar o sistema.
Parâmetros Ajustáveis
Tmexp (Tempo de Exposição):
O usuário pode ajustar o tempo de medição diretamente pela interface web. O valor padrão é de 1 hora, e pode ser alterado para atender aos requisitos específicos da medição.
Alerta de Segurança
O sistema exibirá alertas de segurança quando as medições excederem os limites recomendados para a exposição vibratória:
HAV (Mãos e Braços): Limite de AREN de 5 m/s².
WBV (Corpo Inteiro): Limite de AREN de 1,15 m/s² (em desenvolvimento).
Tecnologias Utilizadas
ESP32: Microcontrolador utilizado para processar os dados e hospedar a interface web.
MPU6050: Acelerômetro de 6 eixos para capturar as leituras de aceleração.
WiFi: Conexão sem fio para permitir o acesso à interface web.
HTML, CSS, JavaScript: Interface web responsiva para visualização em tempo real.

Licença
Este projeto é de código aberto sob a licença MIT. Veja o arquivo LICENSE para mais detalhes.
