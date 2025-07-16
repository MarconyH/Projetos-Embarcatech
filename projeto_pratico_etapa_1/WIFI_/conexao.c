/**
 * @file conexao.c
 * @brief Núcleo 1 - Cliente Wi-Fi com reconexão automática e envio via FIFO.
 * Envia status da conexão (azul, verde, vermelho), número da tentativa e IP ao núcleo 0.
 */

#include "conexao.h"
#include "wifi_status.h"
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/sync.h"
#include "../inc/spi_manager.h"
#include "mqtt_lwip.h"  // Adiciona MQTT
#include <stdio.h>
#include <string.h>

// Includes para SD Card
#include "ff.h"
#include "f_util.h"

// Configurações de baixo consumo
#define WIFI_INTERVALO_CONEXAO_MS (10 * 60 * 1000)  // 10 minutos em modo normal
#define WIFI_INTERVALO_TESTE_MS (30 * 1000)         // 30 segundos para testes
#define WIFI_TEMPO_CONEXAO_ATIVA_MS (60 * 1000)     // 1 minuto conectado para enviar dados

/**
 * @brief Buffer para armazenar dados RFID na RAM
 */
#define MAX_RFID_ENTRIES 20
#define MAX_ENTRY_SIZE 256

typedef struct {
    char data[MAX_ENTRY_SIZE];
    bool valid;
} RfidEntry_t;

static RfidEntry_t rfid_buffer[MAX_RFID_ENTRIES];
static int rfid_buffer_count = 0;
static bool dados_carregados_do_sd = false;

// Variáveis de controle de baixo consumo
static volatile bool wifi_em_modo_sleep = false;
static volatile uint32_t ultimo_tempo_conexao = 0;
static volatile bool wifi_deve_desconectar = false;


uint8_t status_wifi_rgb = 0;

bool wifi_esta_conectado(void) {
    return cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP;
}

void enviar_status_para_core0(uint16_t status, uint16_t tentativa) {
    uint32_t pacote = ((tentativa & 0xFFFF) << 16) | (status & 0xFFFF);
    multicore_fifo_push_blocking(pacote);
}

void enviar_ip_para_core0(uint8_t *ip) {
    uint32_t ip_bin = (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | ip[3];
    // Usa tentativa = 0xFFFE para indicar pacote de IP
    uint32_t pacote = (0xFFFE << 16) | 0;
    multicore_fifo_push_blocking(pacote);
    multicore_fifo_push_blocking(ip_bin);
}

// Novas funções para controle de baixo consumo
void wifi_entrar_modo_sleep(void) {
    if (wifi_esta_conectado()) {
        printf("[CORE 1] Desconectando WiFi para modo sleep...\n");
        cyw43_arch_disable_sta_mode();
    }
    
    // Usa a nova função do spi_manager para economia máxima
    printf("[CORE 1] WiFi entrando em modo de baixo consumo maximo...\n");
    spi_manager_shutdown_wifi_power_save();
    
    wifi_em_modo_sleep = true;
    status_wifi_rgb = 0; // Status: desligado/sleep
    enviar_status_para_core0(status_wifi_rgb, 0);
}

void wifi_sair_modo_sleep(void) {
    if (wifi_em_modo_sleep) {
        printf("[CORE 1] WiFi saindo do modo de baixo consumo...\n");
        
        // Usa a nova função do spi_manager para reativar
        spi_manager_wakeup_wifi_power_save();
        
        wifi_em_modo_sleep = false;
        ultimo_tempo_conexao = to_ms_since_boot(get_absolute_time());
    }
}

bool wifi_deve_conectar_agora(void) {
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());
    uint32_t intervalo = WIFI_INTERVALO_TESTE_MS; // Use WIFI_INTERVALO_CONEXAO_MS para produção
    
    return (tempo_atual - ultimo_tempo_conexao) >= intervalo;
}

void wifi_conectar_periodico(void) {
    if (!wifi_deve_conectar_agora()) {
        return;
    }
    
    printf("[CORE 1] Hora de conectar WiFi (modo periodico)...\n");
    wifi_sair_modo_sleep();
    
    // Tenta conectar
    conectar_wifi();
    
    if (wifi_esta_conectado()) {
        printf("[CORE 1] WiFi conectado! Mantendo ativo por %d segundos...\n", 
               WIFI_TEMPO_CONEXAO_ATIVA_MS / 1000);
        
        // Aqui você pode enviar dados MQTT, etc.
        // TODO: Adicionar envio de dados acumulados
        
        // Mantém conectado por um tempo limitado
        sleep_ms(WIFI_TEMPO_CONEXAO_ATIVA_MS);
        
        printf("[CORE 1] Periodo de conexao terminado. Voltando ao modo sleep...\n");
    } else {
        printf("[CORE 1] Falha na conexao. Voltando ao modo sleep...\n");
    }
    
    // Volta ao modo sleep
    wifi_entrar_modo_sleep();
    ultimo_tempo_conexao = to_ms_since_boot(get_absolute_time());
}


void conectar_wifi(void) {
    status_wifi_rgb = 0;
    enviar_status_para_core0(status_wifi_rgb, 0); // inicializando
    
    printf("[CORE 1] Usando WiFi ja inicializado pelo Core 0...\n");
    
    // NÃO reativa o WiFi aqui - ele já está ativo desde o Core 0
    // Apenas garante que o Core 1 pode usar as funções de rede
    
    printf("[CORE 1] Prosseguindo com tentativas de conexao...\n");

    for (uint16_t tentativa = 1; tentativa <= 5; tentativa++) {
        int result = cyw43_arch_wifi_connect_timeout_ms(
            WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 3000);

        bool conectado = (result == 0) && wifi_esta_conectado();
        status_wifi_rgb = conectado ? 1 : 2;
        enviar_status_para_core0(status_wifi_rgb, tentativa);

        if (conectado) {
            uint8_t *ip = (uint8_t*)&cyw43_state.netif[0].ip_addr.addr;
            enviar_ip_para_core0(ip);
            return;
        }

        sleep_ms(TEMPO_CONEXAO);
    }

    status_wifi_rgb = 2;
    enviar_status_para_core0(status_wifi_rgb, 0);
}

void monitorar_conexao_e_reconectar(void) {
    while (true) {
        sleep_ms(TEMPO_CONEXAO);

        if (!wifi_esta_conectado()) {
            status_wifi_rgb = 2;
            enviar_status_para_core0(status_wifi_rgb, 0);

            cyw43_arch_enable_sta_mode();

            for (uint16_t tentativa = 1; tentativa <= 5; tentativa++) {
                int result = cyw43_arch_wifi_connect_timeout_ms(
                    WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, TEMPO_CONEXAO);

                bool reconectado = (result == 0) && wifi_esta_conectado();
                status_wifi_rgb = reconectado ? 1 : 2;
                enviar_status_para_core0(status_wifi_rgb, tentativa);

                if (reconectado) {
                    uint8_t *ip = (uint8_t*)&cyw43_state.netif[0].ip_addr.addr;
                    enviar_ip_para_core0(ip);

                    break;
                }

                sleep_ms(TEMPO_CONEXAO);
            }

            if (!wifi_esta_conectado()) {
                status_wifi_rgb = 2;
                enviar_status_para_core0(status_wifi_rgb, 0);
            }
        }
    }
}

// Função a ser chamada no núcleo 1 - VERSÃO PARA SISTEMA DE ESTADOS
void funcao_wifi_nucleo1(void) {
    printf("[CORE 1] === WIFI PARA SISTEMA DE ESTADOS ===\n");
    printf("[CORE 1] Funcao WiFi iniciada no nucleo 1!\n");
    fflush(stdout);
    
    // Aguarda para garantir que o Core 0 tenha terminado
    printf("[CORE 1] Aguardando estabilizacao do sistema...\n");
    sleep_ms(2000);
    
    // NÃO INICIALIZA WIFI - Já foi inicializado pelo Core 0
    printf("[CORE 1] === USANDO WIFI JA INICIALIZADO ===\n");
    
    // Verifica se WiFi está funcionando
    if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_DOWN) {
        printf("[CORE 1] WiFi não está conectado. Tentando conectar...\n");
        
        // Tenta conectar WiFi (sem reinicializar)
        for (int tentativa = 1; tentativa <= 3; tentativa++) {
            printf("[CORE 1] Tentativa %d de conexao...\n", tentativa);
            
            int connect_result = cyw43_arch_wifi_connect_timeout_ms(
                WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000);
            
            if (connect_result == 0) {
                printf("[CORE 1] SUCESSO: WiFi conectado!\n");
                
                // Mostra IP
                uint32_t ip = cyw43_state.netif[0].ip_addr.addr;
                printf("[CORE 1] IP obtido: %d.%d.%d.%d\n", 
                       ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);
                break;
            } else {
                printf("[CORE 1] Falha na tentativa %d (codigo: %d)\n", tentativa, connect_result);
                sleep_ms(2000);
            }
        }
    } else {
        printf("[CORE 1] WiFi já estava conectado!\n");
    }
    
    // Inicializa MQTT se WiFi conectou
    if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP) {
        printf("[CORE 1] === INICIANDO ENVIO MQTT DOS DADOS RFID ===\n");
        
        // Inicializa MQTT
        iniciar_mqtt_cliente();
        sleep_ms(3000); // Aguarda conexão MQTT
        
        bool dados_enviados = false;
        
        if (mqtt_esta_conectado()) {
            printf("[CORE 1] MQTT conectado. Enviando dados RFID do SD...\n");
            
            // Chama função para enviar dados do SD
            dados_enviados = enviar_dados_rfid_mqtt();
            
            if (dados_enviados) {
                printf("[CORE 1] Todos os dados RFID enviados com sucesso!\n");
                
                // Chama callback de conclusão para marcar sucesso
                extern void on_mqtt_send_complete(void);
                on_mqtt_send_complete();
                
                // Loop aguardando reset
                while (true) {
                    printf("[CORE 1] Aguardando reset do sistema...\n");
                    sleep_ms(5000);
                }
            } else {
                printf("[CORE 1] ERRO: Falha ao enviar dados RFID.\n");
                
                // Loop de aguardo para timeout do watchdog
                while (true) {
                    printf("[CORE 1] Erro no envio. Aguardando reset...\n");
                    sleep_ms(5000);
                }
            }
        } else {
            printf("[CORE 1] ERRO: MQTT nao conectou.\n");
            
            // Loop de aguardo para timeout do watchdog
            while (true) {
                printf("[CORE 1] MQTT falhou. Aguardando reset...\n");
                sleep_ms(5000);
            }
        }
    } else {
        printf("[CORE 1] WiFi nao conectou. Loop de aguardo...\n");
        while (true) {
            printf("[CORE 1] Aguardando... WiFi nao disponivel.\n");
            sleep_ms(10000);
        }
    }
}

/**
 * @brief Lê dados RFID do SD card e armazena no buffer RAM
 * @return true se dados foram carregados com sucesso, false caso contrário
 */
bool carregar_dados_rfid_para_buffer(void) {
    printf("[BUFFER_LOAD] Carregando dados RFID do SD para buffer RAM...\n");
    
    // Includes necessários para SD card
    extern void spi_manager_activate_sd(void);
    extern bool Sdh_Init(void);
    
    // Limpa buffer
    rfid_buffer_count = 0;
    memset(rfid_buffer, 0, sizeof(rfid_buffer));
    
    // Ativa SD Card
    spi_manager_activate_sd();
    
    if (!Sdh_Init()) {
        printf("[BUFFER_LOAD] ERRO: Falha ao inicializar SD Card para leitura\n");
        return false;
    }
    
    FIL fil;
    FRESULT fr;
    char buffer[512];
    
    // Abre arquivo de dados RFID
    fr = f_open(&fil, "rfid_queue.txt", FA_READ);
    if (fr != FR_OK) {
        printf("[BUFFER_LOAD] ERRO: Arquivo rfid_queue.txt não encontrado: %d\n", fr);
        return false;
    }
    
    printf("[BUFFER_LOAD] Arquivo de dados RFID encontrado. Carregando para RAM...\n");
    
    // Lê linha por linha e armazena no buffer
    while (f_gets(buffer, sizeof(buffer), &fil) && rfid_buffer_count < MAX_RFID_ENTRIES) {
        // Remove quebra de linha
        buffer[strcspn(buffer, "\n")] = 0;
        
        if (strlen(buffer) > 0) {
            strncpy(rfid_buffer[rfid_buffer_count].data, buffer, MAX_ENTRY_SIZE - 1);
            rfid_buffer[rfid_buffer_count].data[MAX_ENTRY_SIZE - 1] = '\0';
            rfid_buffer[rfid_buffer_count].valid = true;
            rfid_buffer_count++;
            
            printf("[BUFFER_LOAD] Carregado: %s\n", buffer);
        }
    }
    
    f_close(&fil);
    dados_carregados_do_sd = true;
    
    printf("[BUFFER_LOAD] SUCESSO: %d registros RFID carregados no buffer RAM!\n", rfid_buffer_count);
    return rfid_buffer_count > 0;
}

/**
 * @brief Envia dados RFID do buffer RAM via MQTT
 * @return true se todos os dados foram enviados com sucesso, false caso contrário
 */
bool enviar_dados_rfid_mqtt(void) {
    printf("[MQTT_SEND] Iniciando envio de dados RFID do buffer RAM...\n");
    
    if (!dados_carregados_do_sd || rfid_buffer_count == 0) {
        printf("[MQTT_SEND] ERRO: Nenhum dado RFID no buffer para envio\n");
        return false;
    }
    
    int dados_enviados = 0;
    
    // Envia cada entrada do buffer via MQTT
    for (int i = 0; i < rfid_buffer_count; i++) {
        if (rfid_buffer[i].valid) {
            printf("[MQTT_SEND] Enviando: %s\n", rfid_buffer[i].data);
            
            // Verifica se MQTT ainda está conectado
            if (!mqtt_esta_conectado()) {
                printf("[MQTT_SEND] ERRO: MQTT desconectado durante envio\n");
                return false;
            }
            
            // Envia via MQTT
            publicar_mensagem_mqtt(rfid_buffer[i].data);
            dados_enviados++;
            
            printf("[MQTT_SEND] Dado %d enviado com sucesso\n", dados_enviados);
            
            // Pequena pausa entre envios
            sleep_ms(2000);
        }
    }
    
    if (dados_enviados > 0) {
        printf("[MQTT_SEND] SUCESSO: %d registros RFID enviados via MQTT!\n", dados_enviados);
        return true;
    } else {
        printf("[MQTT_SEND] AVISO: Nenhum dado RFID enviado\n");
        return false;
    }
}
