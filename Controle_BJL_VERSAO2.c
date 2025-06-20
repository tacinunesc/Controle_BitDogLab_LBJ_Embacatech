#include "pico/cyw43_arch.h"//Modulo WIFI
#include "pico/stdlib.h"//Bibloteca padrao das Raspberry Pi Pico
#include "hardware/adc.h"//Leitura (ADC)
#include "hardware/gpio.h"//Controle de pinos
#include "lwip/tcp.h"//Rede TCP/IP
#include "FreeRTOS.h"
#include "task.h"//Gereciamento de tarefas
#include <stdio.h>//Entrada/Saida
#include <string.h>

// ==== Definições dos pinos ====
#define PINO_BOTAO_A 5
#define PINO_BOTAO_B 6
#define PINO_JOYSTICK_X 27
#define PINO_JOYSTICK_Y 26

// ==== Definicoes da Rede Wi-Fi ====
#define NOME_REDE "Rede"
#define SENHA_REDE "Senha"

// ==== Variaveis globais ====
char mensagem_botaoA[20] = "liberado";
char mensagem_botaoB[20] = "liberado";
int joystick_x = 0;
int joystick_y = 0;
char direcao_joystick[20] = "Centro";
float temperatura_c = 0.0;
bool led_ligado = false;

char resposta_http_html[4096];
char resposta_http_json[512];

// ==== Funcao para calcular a direcao do joystick ====
void calcular_direcao_joystick(int x, int y, char *direcao) {
    const int zona_morta = 200;//Define uma "zona morta" para o centro do joystick
    bool cima = y < (2048 - zona_morta);
    bool baixo = y > (2048 + zona_morta);
    bool esquerda = x < (2048 - zona_morta);
    bool direita = x > (2048 + zona_morta);

    if (!cima && !baixo && !esquerda && !direita) strcpy(direcao, "Centro"); //Centro
    else if (cima && !esquerda && !direita) strcpy(direcao, "Norte");//Cima
    else if (baixo && !esquerda && !direita) strcpy(direcao, "Sul");//Baixo
    else if (esquerda && !cima && !baixo) strcpy(direcao, "Oeste");//Esquerda
    else if (direita && !cima && !baixo) strcpy(direcao, "Leste");//Direita
    else if (cima && direita) strcpy(direcao, "Nordeste");//Cima e direita
    else if (cima && esquerda) strcpy(direcao, "Noroeste");//Cima e esquerda
    else if (baixo && direita) strcpy(direcao, "Sudeste");//Baixo e direita
    else if (baixo && esquerda) strcpy(direcao, "Sudoeste");//Baixo e esquerda
    else strcpy(direcao, "Centro"); //Retorna ao centro
}

// ==== Funcao para ler temperatura ====
float ler_temperatura() {
    adc_select_input(4); // Canal do sensor interno
    uint16_t leitura = adc_read();
    const float conversao = 3.3f / (1 << 12); // 12 bits
    float tensao = leitura * conversao;
    return 27.0f - (tensao - 0.706f) / 0.001721f;
}

// ==== Funcao para criar a reposta JSON ====
void criar_resposta_json() {
    snprintf(resposta_http_json, sizeof(resposta_http_json),
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
        "{"
        "\"botaoA\":\"%s\","
        "\"botaoB\":\"%s\","
        "\"joystick\":{\"x\":%d,\"y\":%d,\"direcao\":\"%s\"},"
        "\"temperatura\":%.2f,"
        "\"led\":%s"
        "}",
        mensagem_botaoA,
        mensagem_botaoB,
        joystick_x,
        joystick_y,
        direcao_joystick,
        temperatura_c,
        led_ligado ? "true" : "false"
    );
}

// ==== Funcao para criar resposta HTML ====
void criar_resposta_http_html() {
    snprintf(resposta_http_html, sizeof(resposta_http_html),
        "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
        "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
        "<title>Controle BitDogLab LBJ Embacatech</title><meta name='viewport' content='width=device-width, initial-scale=1'>"
        //Estilo CSS
        "<style>"
        "body{font-family:Arial;background:#f0f0f0;text-align:center;padding:20px;}"
        "header{background:#007BFF;color:white;padding:20px;border-radius:15px;}"
        "h1{margin:0;font-size:2.5em;}"
        "button{padding:10px 30px;margin:10px;border:none;border-radius:8px;background:#007BFF;color:white;"
        "font-size:1em;cursor:pointer;}button:hover{background:#0056b3;}"
        "table{margin:20px auto;background:white;border-collapse:collapse;box-shadow:0 0 10px rgba(0,0,0,0.1);}"
        "th,td{padding:15px 25px;border:1px solid #ccc;}"
        "th{background:#007BFF;color:white;}"
        ".rosa{display:grid;grid-template-columns:repeat(3,1fr);gap:5px;max-width:300px;margin:30px auto;"
        "background:#333;padding:20px;border-radius:15px;color:pink;font-weight:bold;}"
        ".rosa div{display:flex;align-items:center;justify-content:center;padding:10px;border-radius:8px;"
        "background:#444;transition:0.3s;}"
        ".ativo{background:yellow;color:black;}"
        "</style></head><body>"
        //Conteudo da pagina HTML
        "<header><h1>Controle BitDogLab LBJ Embacatech</h1></header>"
        "<div><button onclick='ligarLED()'>Ligar LED</button>"
        "<button onclick='desligarLED()'>Desligar LED</button></div>"
        "<h2>Estado do LED: <span id='ledStatus'>Desconhecido</span></h2>"

        "<table>"//Tabela para exibir os dados dos sensores
        "<tr><th>Botões</th><td id='statusBotoes'></td></tr>"
        "<tr><th>Joystick</th><td id='statusJoystick'></td></tr>"
        "<tr><th>Direção e Temperatura</th><td id='statusDirecao'></td></tr>"
        "</table>"

        "<div class='rosa'>"
        "<div id='Norte'>N</div><div id='Nordeste'>NE</div><div id='Leste'>L</div>"
        "<div id='Noroeste'>NO</div><div id='Centro'>&bull;</div><div id='Sudeste'>SE</div>"
        "<div id='Oeste'>O</div><div id='Sudoeste'>SO</div><div id='Sul'>S</div>"
        "</div>"
        //Parte JavaScript para interatividade e atualizacao
        "<script>"
        "function atualizarRosa(d){"
        "['Norte','Nordeste','Leste','Sudeste','Sul','Sudoeste','Oeste','Noroeste','Centro'].forEach(e=>{"
        "let el=document.getElementById(e);"
        "if(el){el.classList.remove('ativo');if(e==d)el.classList.add('ativo');}"
        "});}"
        "function atualizarStatus(){"
        "fetch('/api/status').then(r=>r.json()).then(data=>{"
        "document.getElementById('statusBotoes').innerText=`A: ${data.botaoA} | B: ${data.botaoB}`;"
        "document.getElementById('statusJoystick').innerText=`X: ${data.joystick.x}, Y: ${data.joystick.y}`;"
        "document.getElementById('statusDirecao').innerText=`${data.joystick.direcao} | ${data.temperatura.toFixed(1)}°C`;"
        "document.getElementById('ledStatus').innerText=data.ledAzul?'Ligado':'Desligado';"
        "atualizarRosa(data.joystick.direcao);"
        "});}"
        "function ligarLED(){fetch('/led/on');}"
        "function desligarLED(){fetch('/led/off');}"
        "setInterval(atualizarStatus,1000);atualizarStatus();"
        "</script>"

        "</body></html>"
    );
}

// ==== Funcoes do Servidor TCP  ====
//Funcao chamada quando um requisicao HTTP e recebida
static err_t tratar_requisicao(void *arg, struct tcp_pcb *pcb, struct pbuf *buf, err_t err) {
    if (buf == NULL) {tcp_close(pcb); return ERR_OK;}

    char *requisicao = (char *)buf->payload;
    char url[64];
    //Extrai a URL da linha GET da requisicao HTTP
    if (sscanf(requisicao, "GET %63s HTTP", url) == 1) {
        if (strcmp(url, "/led/on") == 0) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
            led_ligado = true;
            const char *resp = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
            tcp_write(pcb, resp, strlen(resp), TCP_WRITE_FLAG_COPY);
        } else if (strcmp(url, "/led/off") == 0) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            led_ligado = false;
            const char *resp = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
            tcp_write(pcb, resp, strlen(resp), TCP_WRITE_FLAG_COPY);
        } else if (strcmp(url, "/api/status") == 0) {
            criar_resposta_json();
            tcp_write(pcb, resposta_http_json, strlen(resposta_http_json), TCP_WRITE_FLAG_COPY);
        } else {
            criar_resposta_http_html();
            tcp_write(pcb, resposta_http_html, strlen(resposta_http_html), TCP_WRITE_FLAG_COPY);
        }
    }

    tcp_recved(pcb, buf->tot_len);//Notifica a pilha TCP que os dados foram recebidos
    pbuf_free(buf);//Libera o buffer
    tcp_close(pcb);//Fecha a conexao TCP
    return ERR_OK;
}
//Funcao chamada quando uma nova conexao TCP e aceita
static err_t tratar_conexao(void *arg, struct tcp_pcb *pcb_nova, err_t err) {
    tcp_recv(pcb_nova, tratar_requisicao);
    return ERR_OK;
}
//Funcao para iniciar o servidor HTTP
static void iniciar_servidor_http() {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) return;
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) return;
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, tratar_conexao);
}

// ==== Tarefa WIFI (FreeRTOS Task) ====
//Mantem a pilha do wifi funcionando e processando eventos
void tarefa_wifi(void *params) {
    while (true) {
        cyw43_arch_poll();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ==== Task Leitura (FreeRTOS Task) ====
//Le continuamente o estado dos botoes, joystick e temperatura
void tarefa_leitura(void *params) {
    while (true) {
        bool estadoA = !gpio_get(PINO_BOTAO_A);
        bool estadoB = !gpio_get(PINO_BOTAO_B);
        
        //Atualiza as mensagens dos botoes com base no estado
        snprintf(mensagem_botaoA, sizeof(mensagem_botaoA), estadoA ? "pressionado" : "liberado");
        snprintf(mensagem_botaoB, sizeof(mensagem_botaoB), estadoB ? "pressionado" : "liberado");
        
        //Leitura do joystick
        adc_select_input(0);
        joystick_x = adc_read();
        adc_select_input(1);
        joystick_y = adc_read();
        
        //Calcula a direcao do joystick com base nas leituras
        calcular_direcao_joystick(joystick_x, joystick_y, direcao_joystick);

        temperatura_c = ler_temperatura();

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


// ==== Funcao Principal (Main) ====
//Ponto inicial do programa: inicializa hardware, WiFi, servidor e tarefas
int main() {
    stdio_init_all(); // Inicializa todas as saídas padrão (incluindo serial)
    sleep_ms(2000);   // Pequena pausa para garantir a inicialização serial

    // Inicializa o módulo Wi-Fi CYW43
    if (cyw43_arch_init()) return 1; // Retorna erro se a inicialização falhar
    cyw43_arch_enable_sta_mode();    // Habilita o modo estação (cliente) do Wi-Fi

    // Tenta conectar à rede Wi-Fi especificada
    printf("Conectando ao Wi-Fi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(NOME_REDE, SENHA_REDE, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Falha na conexão Wi-Fi!\n"); // Mensagem de erro se a conexão falhar
        return 1;
    }
    printf("Conectado!\nIP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list))); // Exibe o IP da placa

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0); // Garante que o LED do Wi-Fi comece desligado

    // Configuração dos pinos dos botões
    gpio_init(PINO_BOTAO_A); gpio_set_dir(PINO_BOTAO_A, GPIO_IN); gpio_pull_up(PINO_BOTAO_A);
    gpio_init(PINO_BOTAO_B); gpio_set_dir(PINO_BOTAO_B, GPIO_IN); gpio_pull_up(PINO_BOTAO_B);

    // Inicialização do ADC para o joystick e sensor de temperatura
    adc_init(); // Inicializa o controlador ADC
    adc_gpio_init(PINO_JOYSTICK_X); // Habilita o pino X do joystick para ADC
    adc_gpio_init(PINO_JOYSTICK_Y); // Habilita o pino Y do joystick para ADC
    adc_set_temp_sensor_enabled(true); // Ativa o sensor de temperatura interno

    iniciar_servidor_http(); // Inicia o servidor HTTP

    // Cria as tarefas do FreeRTOS
    // xTaskCreate(funcao_da_tarefa, "Nome da Tarefa", Tamanho da Pilha, Parametros, Prioridade, Handle da Tarefa)
    xTaskCreate(tarefa_wifi, "WiFiPoll", 512, NULL, 1, NULL);   // Tarefa para manter o Wi-Fi ativo
    xTaskCreate(tarefa_leitura, "Leitura", 512, NULL, 1, NULL); // Tarefa para ler os sensores

    vTaskStartScheduler(); // Inicia o agendador do FreeRTOS (o controle de execução passa para as tarefas)

    while (true); // Loop infinito para manter o programa em execução após o agendador
    return 0; // O programa nunca deve chegar aqui se o agendador iniciar corretamente
}