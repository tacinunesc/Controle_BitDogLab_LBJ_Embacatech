#include "pico/cyw43_arch.h" //Comunicação sem fio
#include "pico/stdlib.h"     //Entrada e saída, manipulação de GPIOS, temprização e comunicação serial
#include "hardware/adc.h"   //ADC
#include "lwip/tcp.h"       //TCP/IP
#include "FreeRTOS.h"       //Controle de tarefas
#include "task.h"
#include <string.h>         //Manipulação de strings
#include <stdio.h>          //entrada e saída de dados

//define os pinos utilizados
#define PINO_LED_Azul 12 //define o pino 12 para controlar o LED AZUL
#define PINO_BOTAO_A 5 //define o pino 5 como entrada para o Botão A da placa
#define PINO_BOTAO_B 6//define o pino 6 como entrada para o Botão B da placa
#define PINO_JOYSTICK_X 26  // ADC0 define o pino 26 como entrada do eixo X do joystick
#define PINO_JOYSTICK_Y 27  // ADC1 define o pino 27 como entrada do eixo y do joystick


#define NOME_REDE "NomedaRede"//Nome da Rede
#define SENHA_REDE "SenhadaRede"//Senha da Rede

//define estados iniciais dos botões
char mensagem_botaoA[50] = "Liberado";
char mensagem_botaoB[50] = "Liberado";

// define variáveis para armazenar a posição do joystick
int joystick_x = 0;
int joystick_y = 0;
//define a posição inicial do joystick
char direcao_joystick[16] = "Centro";
//buffer para armazenar resposta HTTP do sistema
char resposta_http[2048];

//função para calcular direção do joystick
void calcular_direcao_joystick(int x, int y, char* direcao) {
   //define zona_morta como a posição central
    const int parada = 200;
    //determina se o joystick esta inclinada para cada direção
    bool cima = y < (2048 - parada);//inclinado para cima
    bool baixo = y > (2048 + parada);//inclinado para baixo
    bool esquerda = x < (2048 - parada);//inclinado para esquerda
    bool direita = x > (2048 + parada);//inclinado para direita
    //se ele não estiver inclinado está na posição central
    if (!cima && !baixo && !esquerda && !direita) {
        strcpy(direcao, "Centro");//define a direção como centro
        return;
    }
    //verifica direções principais
    if (cima && !esquerda && !direita) strcpy(direcao, "Norte");
    else if (baixo && !esquerda && !direita) strcpy(direcao, "Sul");
    else if (esquerda && !cima && !baixo) strcpy(direcao, "Oeste");
    else if (direita && !cima && !baixo) strcpy(direcao, "Leste");
    //verifica direções diagonais
    else if (cima && direita) strcpy(direcao, "Nordeste");
    else if (cima && esquerda) strcpy(direcao, "Noroeste");
    else if (baixo && direita) strcpy(direcao, "Sudeste");
    else if (baixo && esquerda) strcpy(direcao, "Sudoeste");
    else strcpy(direcao, "Centro");
}
//gera um html paa representar a rosa dos ventos
void criar_html_rosa_dos_ventos(const char* direcao, char* buffer, size_t tamanho_buffer) {
    snprintf(buffer, tamanho_buffer,
        "<pre style=\"font-family: monospace; font-size: 20px; line-height: 1; color: #FF00FF;\">"
        "       %s       \n"    //Linha superior referencia a direção Norte
        "     %s  |  %s    \n"  //Direções Noroeste e Nordeste
        " %s --- + --- %s \n"   //Direções Oeste e Leste, com simbolo de + indicando região central
        "     %s  |  %s    \n"  //Direções Sudoeste e Sudeste
        "       %s       \n"    //Linha inferior faz referencia a direção Sul
        "</pre>",
        //Compara a direção atual e destaca em negrito cor de rosa se for correspondente
        strcmp(direcao, "Norte") == 0 ? "<b>Norte</b>" : "Norte",
        strcmp(direcao, "Noroeste") == 0 ? "<b>Noroeste</b>" : "Noroeste",
        strcmp(direcao, "Nordeste") == 0 ? "<b>Nordeste</b>" : "Nordeste",
        strcmp(direcao, "Oeste") == 0 ? "<b>Oeste</b>" : "Oeste",
        strcmp(direcao, "Leste") == 0 ? "<b>Leste</b>" : "Leste",
        strcmp(direcao, "Sudoeste") == 0 ? "<b>Sudoeste</b>" : "Sudoeste",
        strcmp(direcao, "Sudeste") == 0 ? "<b>Sudeste</b>" : "Sudeste",
        strcmp(direcao, "Sul") == 0 ? "<b>Sul</b>" : "Sul"
    );
}

void criar_resposta_http() {
    //buffer para armazenar a rosa dos ventos
    char html_rosa_dos_ventos[512];
    //chama a funcao anterior com base na direcao do joystick
    criar_html_rosa_dos_ventos(direcao_joystick, html_rosa_dos_ventos, sizeof(html_rosa_dos_ventos));
    //gera a reposta http incluindo a pagina html completa
    snprintf(resposta_http, sizeof(resposta_http),
    //Cabecalho da resposta HTTP
        "HTTP/1.1 200 OK\r\n"//indica sucesso na requisicao
        "Content-Type: text/html; charset=UTF-8\r\n"//define tipo de conteúdo como HTML
        "Refresh: 5\r\n"//Atualiza a página a cada 5 segundos
        "\r\n"//separa 
        // Inicio do HTML
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset=\"UTF-8\">"
        "<title>Controle de LED, Botões e Joystick</title>" //titulo da pagina
        "</head>"
        "<body>"
        //Cabeçalho visual da pagina
        "<header style=\"border: 3px solid black; padding: 15px; text-align: center; border:none; border-radius:8px; background-color: #007BFF;\">"
        "<h1 style=\"color: white; font-size: 48px; margin: 0;\">Controle de LED, Botões e Joystick</h1>"
        "</header>"
        //Botoes para ligar/desligar o LED azul
        "<p><a href=\"/led/on\"><button style=\"font-size:1.5em; padding:15px 40px;text-align: center; background-color:#007BFF; color:white; border:none; border-radius:8px; cursor:pointer;\">Ligar LED Azul</button></a></p>"
        "<p><a href=\"/led/off\"><button style=\"font-size:1.5em; padding:15px 40px; text-align: center; background-color:#007BFF; color:white; border:none; border-radius:8px; cursor:pointer;\">Desligar LED Azul</button></a></p>"
        //Estrutura da tabela para exibir informcoes dos Botões A e B e do Joystick
        "<div style=\"display: flex; justify-content: center; margin-top: 40px;\">"
        "<table style=\"width: 90%%; border-collapse: collapse; font-size: 1.5em; text-align: left;\">"
        "  <tr>"
        "    <th style=\"padding: 15px; border: 1px solid #ccc; background-color: #f2f2f2;\">Estado dos Botões</th>"
        "    <th style=\"padding: 15px; border: 1px solid #ccc; background-color: #f2f2f2;\">Posição do Joystick</th>"
        "    <th style=\"padding: 15px; border: 1px solid #ccc; background-color: #f2f2f2;\">Direção</th>"
        "  </tr>"
        "  <tr>"
        "    <td style=\"padding: 15px; border: 1px solid #ccc;\">Botão A: %s<br>Botão B: %s</td>"// Estado dos botoes
        "    <td style=\"padding: 15px; border: 1px solid #ccc;\">X: %d<br>Y: %d</td>"//Posicao do Joystick
        "    <td style=\"padding: 15px; border: 1px solid #ccc; font-weight: bold; font-size: 32px;\">%s</td>"//Direcao do Joystick
        "  </tr>"
        "</table>"
        "</div>"
        //fim da tabela
        //Exibicao da rosa dos ventos gerada
        "<div style=\"text-align: center; margin-top: 40px;\">%s</div>"
        //fechamento do corpo da pagina e do html
        "</body>"
        "</html>\r\n",
        //preenchimento dos placeholders com os valores das variaveis
        mensagem_botaoA, mensagem_botaoB,//Estado dos botoes
        joystick_x, joystick_y,//posição do joystick
        direcao_joystick,//direção calculada do joystick
        html_rosa_dos_ventos//html da rosa dos ventos
    );
}

//Funcao para processar requisicoes recebidas via protocolo TCP/IP
static err_t tratar_requisicao(void *arg, struct tcp_pcb *pcb, struct pbuf *buf, err_t err) {
    //Verifica se o buffer (buf) esta vazio, indicando que nao ha dados na requisicao
    if (buf == NULL) {
        tcp_close(pcb);//Fecha a conexao TCP
        return ERR_OK;//Retorna sucesso, encerrando o processamento
    }
    //Converte  o playload recebido para uma string, permitindo sua manipulacao
    char *requisicao = (char *)buf->payload;
    if (strstr(requisicao, "GET /led/on")) gpio_put(PINO_LED_Azul, 1);//Liga o LED azul
    else if (strstr(requisicao, "GET /led/off")) gpio_put(PINO_LED_Azul, 0);//Desliga o LED azul
    //Gera a resposta HTTP com as informacoes atualizadas do sistema
    criar_resposta_http();
    //Envia a resposta ao usuario via protocolo TCP
    tcp_write(pcb, resposta_http, strlen(resposta_http), TCP_WRITE_FLAG_COPY);
    //Libera a memoria alocada pelo buffer da requisicao
    pbuf_free(buf);
    //Retorna sucesso, indicando que a requisicao foi processada corretamente
    return ERR_OK;
}
//Funcao que trata de uma nova conexao TCP 
static err_t tratar_conexao(void *arg, struct tcp_pcb *pcb_nova, err_t err) {
    //Define a funcao "tratar requisicao" como manipualdor de recebimento de dados
    //Isso significa que quando os dados forem recebidos nessa conexao, "tratar requisicao" será chamada
    tcp_recv(pcb_nova, tratar_requisicao);
    //Retorna sucesso se conexao for configurada corretamente
    return ERR_OK;
}
//Funcao que inicia o servidor HTTP
static void iniciar_servidor_http(void) {
    //Cria uma nova estrutura de controle para conexoes TCP
    struct tcp_pcb *pcb = tcp_new();
    //Se a criação do PCB falhar, encerra a funcao
    if (!pcb) return;
    //Associa o servidor a qualquer endereço IP disponivel (IP_ADDR_ANNY) na porta 80 (HTTP)
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) return;
    //Coloca o servidor TCP em modo de escuta para aceitar conexoes
    pcb = tcp_listen(pcb);
    //Define a funcao "tratar conexao" como o manipulador de conexoes recebidas
    //Quando um usuario se conectar ao servidor, essa funcao sera chamada
    tcp_accept(pcb, tratar_conexao);
}
//Funcao que mantem a conectividade ativa
void tarefa_wifi(void *params) {//Loop infinito para garantir a execucao continua
    while (1) {
        //Executa diversas verificacoes e manutencao do modulo WIFI CYW43
        cyw43_arch_poll();
        //Aguarda 10 milissegundos antes de repetir a operacao
        //Evita consumo excessivo da CPU e melhora a  eficiencia do sistema
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
//Funcao realiza a leitura do estado dos botoes e do joystick
void tarefa_leitura(void *params) {
    //Armazena o estado anterior dos botoes 
    static bool estado_anterior_botaoA = false;
    static bool estado_anterior_botaoB = false;
    while (1) {//Loop infinito para manter a tarefa em execução constante
        //Le o estado dos botoes fisicos(invertendo a logica se estiver ativo)
        bool estado_botaoA = !gpio_get(PINO_BOTAO_A);
        bool estado_botaoB = !gpio_get(PINO_BOTAO_B);
        //Verifica se o estado do botao A mudou desde a ultima leitura
        if (estado_botaoA != estado_anterior_botaoA) {
            estado_anterior_botaoA = estado_botaoA;//Atualiza o estado anterior
            snprintf(mensagem_botaoA, sizeof(mensagem_botaoA), estado_botaoA ? "Botão A pressionado" : "Botão A liberado");//Atualiza a mensagem
        }
        //Verifica se o estado do botao B mudou desde a ultima leitura
        if (estado_botaoB != estado_anterior_botaoB) {
            estado_anterior_botaoB = estado_botaoB;//Atualiza o estado anterior
            snprintf(mensagem_botaoB, sizeof(mensagem_botaoB), estado_botaoB ? "Botão B pressionado" : "Botão B liberado");//Atualiza a mensagem
        }
        //Seleciona e le os valores analogicos do joystick para X e Y
        adc_select_input(0);//Seleciona o eixo X
        joystick_x = adc_read();//Le o valor do eixo X

        adc_select_input(1);//Seleciona o eixo Y
        joystick_y = adc_read();//Le o valor do eixo Y
        //Determina a direcao do joystick com base nos valores lidos
        calcular_direcao_joystick(joystick_x, joystick_y, direcao_joystick);
        //Aguarda 100ms antes da próxima leitura para evitar consumo excessivo de CPU
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

//Funcao principal
int main() {
    //Inicializa a comunicacao serial para depuracao
    stdio_init_all();
    //Aguarda 5 segundos 
    sleep_ms(5000);
    //Inicia o modulo WIFI CYW43, retorna erro se falhar
    if (cyw43_arch_init()) return 1;
    //Configura o WIFI no modo (STA), permitindo conexao com uma rede
    cyw43_arch_enable_sta_mode();
    //Tenta conectar a rede WIFI com tempo de 10 segundos
    if (cyw43_arch_wifi_connect_timeout_ms(NOME_REDE, SENHA_REDE, CYW43_AUTH_WPA2_AES_PSK, 10000)) return 1;

    //Configuracao do LED Azul como saida
    gpio_init(PINO_LED_Azul);
    gpio_set_dir(PINO_LED_Azul, GPIO_OUT);
    //Inicia botoes fisicos como entrada com o pull-up ativado
    gpio_init(PINO_BOTAO_A);
    gpio_set_dir(PINO_BOTAO_A, GPIO_IN);
    gpio_pull_up(PINO_BOTAO_A);

    gpio_init(PINO_BOTAO_B);
    gpio_set_dir(PINO_BOTAO_B, GPIO_IN);
    gpio_pull_up(PINO_BOTAO_B);
    //Configuracao do ADC para leitura dos valores analogicos do joystick
    adc_init();
    adc_gpio_init(PINO_JOYSTICK_X);
    adc_gpio_init(PINO_JOYSTICK_Y);

    //Inicia o servidor HTTP  para comunicação
    iniciar_servidor_http();

    //Cria tarefas para manter a conectividade WIFI e monitorar botões e o joystick
    xTaskCreate(tarefa_wifi, "WiFi", 512, NULL, 1, NULL);
    xTaskCreate(tarefa_leitura, "Entradas", 512, NULL, 1, NULL);
    //Inicia o agendador do FreeRTOS para gerenciar as tarefas criadas
    vTaskStartScheduler();
    //Loop infinito para manter o sistema rodando
    while (1);
    return 0;
}
