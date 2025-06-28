# Controle BitDogLab Raspberry Pi Pico do Botões, Joystick e Temperatura - BJT

## Detalhes Deste Projeto
Para resolver a Questão Desafio que tem o intuito de fazer o Raspberry Pi Pico W se conectar com a nuvem, optei por usar o Google Cloud. Então, primeiro criei uma conta gratuita para realizar os testes, depois dentro da plataforma configurei uma VM (máquina virtual) e instalei o Mosquitto que é um broker MQTT. Assim, ele é o responsável por receber e enviar para a nuvem as informações dos botões, joystick e temperatura da placa.

## Passo a Passo

- A mudança que teve no Cmakelist foi a definição das credenciais de WiFi e MQTT (SSID, senha, servidor IP externo da VW na nuvem, usuário e senha da mesma).


 ###  **CMAKELIST**
```c
target_compile_definitions(Controle_BJL_VERSAO2 PRIVATE
    WIFI_SSID="Rede"#nome do rede wifi
    WIFI_PASSWORD="senha"#senha da rede wifi
    MQTT_SERVER="XX.XX.XXX.XXX"#ip externo do vm do google cloud
    MQTT_USERNAME="meu_usuario"#usuario definido
    MQTT_PASSWORD="passwd"#senha definida
)
````
 
- Então para utilizar este código é só substituir essas credenciais e utilize o arquivo instalador_freertos.sh para instalar o pacote de freertos, pelo terminal bash do vscode, com:
```c
chmod +x instalador_freertos.sh
````

```c
./instalador_freertos.sh
````
- Inclusão e aprimoramento das bibliotecas lwip denominadas como: lwipopts.h e e lwipopts-examples-common.h.
  
No arquivo pincipal .c foram feitas as seguintes modificações com as inclusão de novas bibliotecas:
- pico/unique-d.h: Permite o acesso de um identificador único;
- lwip/apps/mqtt.h: Inclui funções e estruturas para usar o protocolo MQTT com
a biblioteca LWIP;
- lwip/dns.h: Relacionada a resolução de nomes de domínio DNS;
- lwip/altcp-tls.h: Adicona suporte a TLS, usado para conexões MQTT seguras;
- lwip/tcp.h: Interface da pilha TCP da LWIP.

A seguir, define a porta padrão MQTT 1883 e o tópico onde será publicado as informações de temperatura, botões e joystick da placa BitDogLab.

```c
#define MQTT_PORT 1883//Porta MQTT
#define MQTT_TOPIC "bitdoglab/status"//Topico de pubicacao
````
O código principal (.c) foi modificado para incluir bibliotecas para identificador único do Pico, protocolo MQTT, resolução DNS e suporte a TLS para comunicação segura, assim, o Pico W lê continuamente os dados dos botões, joystick e sensor de temperatura. Esses dados são formatados em JSON e são publicados no tópico bitdoglab/status do broker MQTT a cada segundo e a conexão Wi-Fi é mantida ativa por uma tarefa dedicada do FreeRTOS.

###  **Resultado**
A solução permite que o Raspberry Pi Pico W envie dados dos sensores e do joystick para a nuvem em tempo real, com as informações sendo exibidas no monitor serial e enviadas para o Google Cloud via MQTT.
###  **Outras demais funções e aspectos do projeto**

### **Principais Funcionalidades**
- Conexão com rede Wi-Fi
- Leitura da temperatura
- Monitoramento do estado dos botões (pressionado/liberado)
- Exibição das coordenadas (X e Y) e direção do joystick
- Comunicação com o Google Cloud via MQTT
  
### **Stack Tecnológica**
- Linguagem C
- FreeRTOS
- Biblioteca lwIP (TCP/IP)
- Biblioteca GPIO e ADC do Raspberry Pi Pico
