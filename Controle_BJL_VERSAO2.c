#include "pico/cyw43_arch.h"//Modulo WIFI
#include "pico/stdlib.h"//Biblioteca padrao
#include "hardware/adc.h"//Leitura ADC
#include "hardware/gpio.h"//Controle de pinos
#include "pico/unique_id.h"
#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"
#include "lwip/altcp_tls.h"//Criptografia, segurança
#include "lwip/tcp.h"
#include "FreeRTOS.h"
#include "task.h"//Gerenciamento de tarefas
#include <stdio.h>//Entrada/Saida
#include <string.h>
#include <stdlib.h>

// ==== Pinos ====
#define PINO_BOTAO_A 5
#define PINO_BOTAO_B 6
#define PINO_JOYSTICK_X 27
#define PINO_JOYSTICK_Y 26

// ==== Wi-Fi ====
#define NOME_REDE WIFI_SSID
#define SENHA_REDE WIFI_PASSWORD

#ifndef MQTT_SERVER
#error "MQTT_SERVER não está definido"
#endif
#ifndef MQTT_USERNAME
#error "MQTT_USERNAME não está definido"
#endif
#ifndef MQTT_PASSWORD
#error "MQTT_PASSWORD não está definido"
#endif

#define MQTT_PORT 1883//Porta MQTT
#define MQTT_TOPIC "bitdoglab/status"//Topico de pubicacao

// ==== Variaveis Globais e Estado ====
char mensagem_botaoA[20] = "liberado";
char mensagem_botaoB[20] = "liberado";
int joystick_x = 0, joystick_y = 0;
char direcao_joystick[20] = "Centro";
float temperatura_c = 0.0;


// ==== MQTT ====
typedef struct {
    ip_addr_t remote_addr;
    mqtt_client_t* mqtt_client;
    bool connected;
} MQTT_CLIENT_STATE_T;

MQTT_CLIENT_STATE_T* estado_mqtt;

// ==== Funcao para calcular a direcao do joystick ====
void calcular_direcao_joystick(int x, int y, char *direcao) {
    const int zona_morta = 200;
    bool cima = y < (2048 - zona_morta);
    bool baixo = y > (2048 + zona_morta);
    bool esquerda = x < (2048 - zona_morta);
    bool direita = x > (2048 + zona_morta);

    if (!cima && !baixo && !esquerda && !direita) strcpy(direcao, "Centro");
    else if (cima && !esquerda && !direita) strcpy(direcao, "Norte");
    else if (baixo && !esquerda && !direita) strcpy(direcao, "Sul");
    else if (esquerda && !cima && !baixo) strcpy(direcao, "Oeste");
    else if (direita && !cima && !baixo) strcpy(direcao, "Leste");
    else if (cima && direita) strcpy(direcao, "Nordeste");
    else if (cima && esquerda) strcpy(direcao, "Noroeste");
    else if (baixo && direita) strcpy(direcao, "Sudeste");
    else if (baixo && esquerda) strcpy(direcao, "Sudoeste");
    else strcpy(direcao, "Centro");
}
// ==== Funcao para ler temperatura ====
float ler_temperatura() {
    adc_select_input(4);
    uint16_t leitura = adc_read();
    float tensao = leitura * 3.3f / (1 << 12);
    return 27.0f - (tensao - 0.706f) / 0.001721f;
}

// ==== MQTT ====
static void mqtt_conexao_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    MQTT_CLIENT_STATE_T* state = (MQTT_CLIENT_STATE_T*)arg;
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("MQTT conectado!\n");
        state->connected = true;
    } else {
        printf("Falha na conexão MQTT: %d\n", status);
        state->connected = false;
    }
}

static void mqtt_pub_cb(void *arg, err_t err) {
    if (err != ERR_OK) {
        printf("Erro ao publicar: %d\n", err);
    }
}

void iniciar_conexao_mqtt(MQTT_CLIENT_STATE_T* state) {
    struct mqtt_connect_client_info_t info = {0};
    char client_id[20];
    pico_get_unique_board_id_string(client_id, sizeof(client_id));
    info.client_id = client_id;
    info.keep_alive = 60;
    info.client_user =  MQTT_USERNAME;
    info.client_pass = MQTT_PASSWORD;

    state->mqtt_client = mqtt_client_new();
    if (!state->mqtt_client) {
        printf("Erro: mqtt_client_new\n");
        return;
    }

    err_t err = mqtt_client_connect(
        state->mqtt_client,
        &state->remote_addr,
        MQTT_PORT,
        mqtt_conexao_cb,
        state,
        &info
    );

    if (err != ERR_OK) {
        printf("Erro mqtt_client_connect: %d\n", err);
    }
}

void tarefa_mqtt(void *params) {
    MQTT_CLIENT_STATE_T* state = (MQTT_CLIENT_STATE_T*)params;
    while (true) {
        if (state->connected) {
            char payload[256];
            snprintf(payload, sizeof(payload),
                "{\"botaoA\":\"%s\",\"botaoB\":\"%s\",\"x\":%d,\"y\":%d,\"direcao\":\"%s\",\"temperatura\":%.2f}",
                mensagem_botaoA, mensagem_botaoB, joystick_x, joystick_y, direcao_joystick, temperatura_c);

            cyw43_arch_lwip_begin();
            mqtt_publish(state->mqtt_client, MQTT_TOPIC, payload, strlen(payload), 0, 0, mqtt_pub_cb, NULL);
            cyw43_arch_lwip_end();

            printf("Publicado: %s\n", payload);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
//----Tarefa WIFI, deixa a conexao ativa------------
void tarefa_wifi(void *params) {
    while (true) {
        cyw43_arch_poll();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
//----Tarefa leitura, ler continuamente os dados da placa-------
void tarefa_leitura(void *params) {
    while (true) {
        bool estadoA = !gpio_get(PINO_BOTAO_A);
        bool estadoB = !gpio_get(PINO_BOTAO_B);
        snprintf(mensagem_botaoA, sizeof(mensagem_botaoA), estadoA ? "pressionado" : "liberado");
        snprintf(mensagem_botaoB, sizeof(mensagem_botaoB), estadoB ? "pressionado" : "liberado");

        adc_select_input(0); joystick_x = adc_read();
        adc_select_input(1); joystick_y = adc_read();

        calcular_direcao_joystick(joystick_x, joystick_y, direcao_joystick);
        temperatura_c = ler_temperatura();

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
//-----Funcao Principal----------------
int main() {
    stdio_init_all();
    sleep_ms(2000);

    if (cyw43_arch_init()) return 1;
    cyw43_arch_enable_sta_mode();

    printf("Conectando ao Wi-Fi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(NOME_REDE, SENHA_REDE, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Erro Wi-Fi\n");
        return 1;
    }
    printf("Wi-Fi conectado! IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));

    // Inicializa pinos e ADC
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    gpio_init(PINO_BOTAO_A); gpio_set_dir(PINO_BOTAO_A, GPIO_IN); gpio_pull_up(PINO_BOTAO_A);
    gpio_init(PINO_BOTAO_B); gpio_set_dir(PINO_BOTAO_B, GPIO_IN); gpio_pull_up(PINO_BOTAO_B);
    adc_init();
    adc_gpio_init(PINO_JOYSTICK_X);
    adc_gpio_init(PINO_JOYSTICK_Y);
    adc_set_temp_sensor_enabled(true);

    estado_mqtt = calloc(1, sizeof(MQTT_CLIENT_STATE_T));
    if (!estado_mqtt) return 1;

    if (!ip4addr_aton(MQTT_SERVER, &estado_mqtt->remote_addr)) {
        printf("MQTT_SERVER inválido\n");
        return 1;
    }

    iniciar_conexao_mqtt(estado_mqtt);

    xTaskCreate(tarefa_wifi, "WiFi", 512, NULL, 1, NULL);
    xTaskCreate(tarefa_leitura, "Leitura", 512, NULL, 1, NULL);
    xTaskCreate(tarefa_mqtt, "MQTT", 1024, estado_mqtt, 1, NULL);

    vTaskStartScheduler();
    while (true);
    return 0;
}