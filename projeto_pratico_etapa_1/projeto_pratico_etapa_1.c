/**
 * @file projeto_pratico_etapa_1.c
 * @brief Núcleo 0: Gerencia a UI (OLED, Botões), periféricos SPI (RFID/SD)
 * e salva os dados de embarque localmente no cartão SD.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "pico/sync.h"

// INCLUDES PARA NOVA ABORDAGEM WIFI + SD + RFID COM WATCHDOG
#include "ff.h" 
#include "f_util.h"
#include "hardware/watchdog.h"
#include "hardware/regs/rosc.h"
#include "hardware/regs/addressmap.h"

// Bibliotecas do SD Card
#include "inc/sd_card/sd_card_handler.h" 
#include "inc/spi_manager.h"
#include "hw_config.h" 

// Bibliotecas do RFID
#include "inc/rfid/mfrc522.h"
#include "inc/rfid/tag_data_handler.h"

// WiFi e MQTT necessários
#include "conexao.h"
#include "WIFI_/mqtt_lwip.h"

// --- Novas Inclusões para o Display OLED ---
#include "oled_utils.h"
#include "ssd1306_i2c.h"
#include "setup_oled.h"
#include "display.h"

// Declaração da função que rodará no Núcleo 1
extern void inicia_core1();

// =================================================================================
// SISTEMA DE ESTADOS PERSISTENTES COM WATCHDOG
// =================================================================================

// Estados do sistema que sobrevivem ao reset
typedef enum {
    SYSTEM_MODE_RFID_SD = 0,         // Modo principal: Ler RFID e escrever no SD Card
    SYSTEM_MODE_SD_READ_SEND = 1,    // Ler SD e enviar via WiFi
    SYSTEM_MODE_SD_CLEANUP = 2,      // Limpar dados enviados do SD
    SYSTEM_MODE_NORMAL_WIFI = 3,     // Modo WiFi temporário para envio
    SYSTEM_MODE_POST_SEND = 4        // Após envio, voltar ao RFID
} system_mode_t;

// Endereço na RAM que sobrevive ao reset (região não inicializada)
#define PERSISTENT_STATE_ADDR 0x20040000  // Final da RAM, área não usada pelo programa
volatile system_mode_t *persistent_mode = (volatile system_mode_t *)PERSISTENT_STATE_ADDR;
volatile uint32_t *persistent_counter = (volatile uint32_t *)(PERSISTENT_STATE_ADDR + 4);
volatile uint32_t *persistent_magic = (volatile uint32_t *)(PERSISTENT_STATE_ADDR + 8);
volatile uint32_t *wifi_retry_count = (volatile uint32_t *)(PERSISTENT_STATE_ADDR + 12);
#define MAGIC_VALUE 0xDEADBEEF  // Valor mágico para verificar se os dados são válidos

// Tempos de operação
#define WATCHDOG_TIMEOUT_MS 8000   // 8 segundos para reset automático
#define SD_OPERATION_TIME_MS 30000 // 30 segundos para operações SD
#define WIFI_OPERATION_TIME_MS 180000 // 3 minutos para operações WiFi (mais tempo)
#define RFID_SD_OPERATION_TIME_MS 45000 // 45 segundos para operações RFID+SD
#define MAX_WIFI_RETRY_CYCLES 3    // Máximo de ciclos de retry antes de voltar ao RFID

// Declarações das funções
void init_persistent_state(void);
system_mode_t get_current_mode(void);
void set_next_mode(system_mode_t mode);
void trigger_watchdog_reset(void);
bool execute_sd_read_send_mode(void);
bool execute_sd_cleanup_mode(void);
bool save_rfid_data_to_sd(const char* rfid_data);
bool read_and_send_sd_data(void);
void execute_rfid_sd_mode_new(void);
void on_mqtt_send_complete(void);
bool mark_send_success_in_sd(void);
bool check_pending_data_in_sd(void);
bool increment_wifi_retry(void);

// --- Funções de Sinalização ---
void setup_leds_and_oled(void);
void display_message_with_led(const char* line1, const char* line2, int led_pin, bool led_state, int delay_ms);
void set_system_status_leds(int mode);

// --- Mutex para proteger o acesso ao barramento SPI ---
// Essencial para multicore, mesmo no teste
mutex_t spi_mutex;

// --- LEDs de Sinalização ---
#define LED_RFID 11    // LED Verde - RFID ativo
#define LED_WIFI 12    // LED Azul - WiFi ativo  
#define LED_ERROR 13   // LED Vermelho - Erro/Status

// --- Variáveis Globais para OLED ---
uint8_t buffer_oled[ssd1306_buffer_length];
struct render_area area;

// TODAS AS FUNÇÕES COMENTADAS PARA TESTE WIFI ISOLADO
/*
// --- Função Auxiliar para o Display OLED ---
void display_message(const char* line1, const char* line2, int delay_ms) {
    oled_clear(buffer_oled, &area);
    if (line1) {
        ssd1306_draw_utf8_multiline(buffer_oled, 0, 0, line1);
    }
    if (line2) {
        ssd1306_draw_utf8_multiline(buffer_oled, 0, 16, line2); // Ajuste a posição Y para a segunda linha
    }
    render_on_display(buffer_oled, &area);
    if (delay_ms > 0) {
        sleep_ms(delay_ms);
    }
}

// Callback dos botões (sem alterações na lógica)
void gpio_callback(uint gpio, uint32_t events) {
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    if (current_time - last_press_time < 250) { 
        gpio_acknowledge_irq(gpio, events);
        return;
    }
    last_press_time = current_time;

    switch (currentState) {
        case STATE_SELECTION:
            if (gpio == BTN_A) { first_key_pressed = 'A'; currentState = STATE_WAIT_FOR_SECOND_KEY; } 
            else if (gpio == BTN_B) { first_key_pressed = 'B'; currentState = STATE_WAIT_FOR_SECOND_KEY; }
            break;
        case STATE_WAIT_FOR_SECOND_KEY:
            if (first_key_pressed == 'A') {
                if (gpio == BTN_A) currentState = STATE_READING;
                else if (gpio == BTN_B) currentState = STATE_WRITING;
            } else if (first_key_pressed == 'B') {
                if (gpio == BTN_A) currentState = STATE_SD_READ;
            }
            if (gpio == JOY_SW) { currentState = STATE_SELECTION; }
            break;
        case STATE_READING:
        case STATE_WRITING:
        case STATE_SD_READ:
            if (gpio == JOY_SW) { currentState = STATE_SELECTION; }
            break;
    }
    gpio_acknowledge_irq(gpio, events);
}

// --- FUNÇÕES DE OPERAÇÃO (Adaptadas com OLED e Mutex) ---
void provision_new_tag(MFRC522Ptr_t mfrc) {
    // ... função completa comentada ...
}

void process_student_boarding(MFRC522Ptr_t mfrc) {
    // ... função completa comentada ...
}
*/

// =================================================================================
// FUNÇÕES DE SINALIZAÇÃO E INTERFACE
// =================================================================================

/**
 * @brief Configura LEDs e OLED para sinalização
 */
void setup_leds_and_oled(void) {
    printf("[UI] Configurando LEDs e OLED...\n");
    
    // Configuração dos LEDs
    gpio_init(LED_RFID);
    gpio_set_dir(LED_RFID, GPIO_OUT);
    gpio_put(LED_RFID, 0);  // Inicia desligado
    
    gpio_init(LED_WIFI);
    gpio_set_dir(LED_WIFI, GPIO_OUT);
    gpio_put(LED_WIFI, 0);  // Inicia desligado
    
    gpio_init(LED_ERROR);
    gpio_set_dir(LED_ERROR, GPIO_OUT);
    gpio_put(LED_ERROR, 0);  // Inicia desligado
    
    // Configuração do OLED usando a biblioteca dedicada
    printf("[UI] Inicializando OLED usando bibliotecas...\n");
    setup_init_oled();  // Usa a função da biblioteca OLED existente
    
    // Teste inicial dos LEDs
    printf("[UI] Testando LEDs...\n");
    gpio_put(LED_RFID, 1);
    sleep_ms(200);
    gpio_put(LED_RFID, 0);
    
    gpio_put(LED_WIFI, 1);
    sleep_ms(200);
    gpio_put(LED_WIFI, 0);
    
    gpio_put(LED_ERROR, 1);
    sleep_ms(200);
    gpio_put(LED_ERROR, 0);
    
    // Mensagem inicial usando a função da biblioteca
    exibir_e_esperar("Sistema RFID\nInicializando...", 0);
    
    printf("[UI] LEDs e OLED configurados!\n");
}

/**
 * @brief Mostra mensagem no OLED com controle de LED
 */
void display_message_with_led(const char* line1, const char* line2, int led_pin, bool led_state, int delay_ms) {
    // Controla LED
    if (led_pin >= 0) {
        gpio_put(led_pin, led_state);
    }
    
    // Prepara mensagem combinada para o OLED
    char combined_message[128];
    if (line1 && line2) {
        snprintf(combined_message, sizeof(combined_message), "%s\n%s", line1, line2);
    } else if (line1) {
        snprintf(combined_message, sizeof(combined_message), "%s", line1);
    } else if (line2) {
        snprintf(combined_message, sizeof(combined_message), "%s", line2);
    } else {
        strcpy(combined_message, "");
    }
    
    // Usa a função da biblioteca para exibir mensagem
    if (delay_ms > 0) {
        // Exibe mensagem e limpa automaticamente após o delay
        oled_clear(buffer_oled, &area);
        ssd1306_draw_utf8_multiline(buffer_oled, 0, 0, combined_message);
        render_on_display(buffer_oled, &area);
        sleep_ms(delay_ms);
        oled_clear(buffer_oled, &area);
        render_on_display(buffer_oled, &area);
    } else {
        // Exibe mensagem sem delay (permanente até próxima atualização)
        oled_clear(buffer_oled, &area);
        ssd1306_draw_utf8_multiline(buffer_oled, 0, 0, combined_message);
        render_on_display(buffer_oled, &area);
    }
}

/**
 * @brief Define padrão de LEDs baseado no modo do sistema
 */
void set_system_status_leds(int mode) {
    // Desliga todos os LEDs primeiro
    gpio_put(LED_RFID, 0);
    gpio_put(LED_WIFI, 0);
    gpio_put(LED_ERROR, 0);
    
    switch (mode) {
        case SYSTEM_MODE_RFID_SD:
            // LED Verde piscando - modo RFID ativo
            gpio_put(LED_RFID, 1);
            break;
            
        case SYSTEM_MODE_SD_READ_SEND:
        case SYSTEM_MODE_NORMAL_WIFI:
            // LED Azul aceso - WiFi ativo
            gpio_put(LED_WIFI, 1);
            break;
            
        case SYSTEM_MODE_SD_CLEANUP:
            // LED Verde e Azul - limpeza
            gpio_put(LED_RFID, 1);
            gpio_put(LED_WIFI, 1);
            break;
            
        default:
            // LED Vermelho - erro ou estado desconhecido
            gpio_put(LED_ERROR, 1);
            break;
    }
}

// =================================================================================
// FUNÇÕES DO SISTEMA DE ESTADOS PERSISTENTES
// =================================================================================

/**
 * @brief Inicializa o sistema de estados persistentes
 */
void init_persistent_state(void) {
    // Verifica se é a primeira inicialização ou se os dados são válidos
    if (*persistent_magic != MAGIC_VALUE) {
        printf("[SYSTEM] Primeira inicializacao - configurando estado persistente\n");
        *persistent_mode = SYSTEM_MODE_RFID_SD;  // Modo principal: RFID
        *persistent_counter = 0;
        *wifi_retry_count = 0;
        *persistent_magic = MAGIC_VALUE;
    } else {
        printf("[SYSTEM] Estado persistente encontrado: modo=%d, contador=%lu, wifi_retries=%lu\n", 
               *persistent_mode, *persistent_counter, *wifi_retry_count);
        
        // CORREÇÃO: Se estiver em loop de limpeza, força reset para RFID
        if (*persistent_mode == SYSTEM_MODE_SD_CLEANUP && *persistent_counter > 5) {
            printf("[SYSTEM] DETECTADO LOOP DE LIMPEZA! Forcando reset para modo RFID...\n");
            *persistent_mode = SYSTEM_MODE_RFID_SD;
            *persistent_counter = 0;
            *wifi_retry_count = 0;
        }
    }
}

/**
 * @brief Obtém o modo atual do sistema
 */
system_mode_t get_current_mode(void) {
    return *persistent_mode;
}

/**
 * @brief Define o próximo modo e incrementa o contador
 */
void set_next_mode(system_mode_t mode) {
    *persistent_mode = mode;
    (*persistent_counter)++;
    printf("[SYSTEM] Modo alterado para %d, contador=%lu\n", mode, *persistent_counter);
}

/**
 * @brief Triggera um reset via watchdog
 */
void trigger_watchdog_reset(void) {
    printf("[SYSTEM] Iniciando reset via watchdog em 1 segundo...\n");
    watchdog_enable(1000, 1); // 1 segundo, sem pause_on_debug
    // Loop infinito para garantir que o watchdog dispare
    while(1) {
        tight_loop_contents();
    }
}

/**
 * @brief Salva dados RFID no cartão SD
 */
bool save_rfid_data_to_sd(const char* rfid_data) {
    printf("[SD_WRITE] Salvando dados RFID no cartão SD...\n");
    
    // Primeiro, inicializa o SD Card propriamente
    if (!Sdh_Init()) {
        printf("[SD_WRITE] ERRO: Falha ao inicializar/montar SD Card\n");
        return false;
    }
    
    FIL fil;
    FRESULT fr;
    
    // Abre/cria arquivo de dados RFID
    fr = f_open(&fil, "rfid_queue.txt", FA_OPEN_APPEND | FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) {
        printf("[SD_WRITE] ERRO: Falha ao abrir arquivo rfid_queue.txt: %d\n", fr);
        return false;
    }
    
    // Cria entrada de dados RFID com informações expandidas
    char data_entry[512];
    uint64_t timestamp = to_ms_since_boot(get_absolute_time());
    
    // Calcula horário atual (simples baseado no timestamp)
    uint32_t hours = (timestamp / (1000 * 60 * 60)) % 24;
    uint32_t minutes = (timestamp / (1000 * 60)) % 60;
    uint32_t seconds = (timestamp / 1000) % 60;
    
    snprintf(data_entry, sizeof(data_entry), 
             "TIMESTAMP:%llu,CYCLE:%lu,RFID_DATA:%s,STATUS:PENDING,ROUTE:1,TIME:%02lu:%02lu:%02lu\n",
             timestamp, *persistent_counter, rfid_data, hours, minutes, seconds);
    
    // Escreve dados
    UINT bytes_written = 0;
    fr = f_write(&fil, data_entry, strlen(data_entry), &bytes_written);
    if (fr != FR_OK) {
        printf("[SD_WRITE] ERRO: Falha ao escrever dados RFID: %d\n", fr);
        f_close(&fil);
        return false;
    }
    
    // Sincroniza e fecha
    f_sync(&fil);
    f_close(&fil);
    
    printf("[SD_WRITE] Dados RFID salvos com sucesso! (%u bytes)\n", bytes_written);
    return true;
}

/**
 * @brief Lê dados do SD e prepara para envio
 */
bool read_and_send_sd_data(void) {
    printf("[MODE] Lendo dados RFID do cartão SD...\n");
    
    FIL fil;
    FRESULT fr;
    char buffer[512];
    
    // Abre arquivo de dados RFID
    fr = f_open(&fil, "rfid_queue.txt", FA_READ);
    if (fr != FR_OK) {
        printf("[MODE] AVISO: Arquivo rfid_queue.txt não encontrado: %d\n", fr);
        return false;
    }
    
    printf("[MODE] Dados RFID encontrados para envio:\n");
    
    // Lê e exibe dados (para demonstração)
    while (f_gets(buffer, sizeof(buffer), &fil)) {
        // Remove quebra de linha
        buffer[strcspn(buffer, "\n")] = 0;
        printf("[MODE] -> %s\n", buffer);
    }
    
    f_close(&fil);
    printf("[MODE] Dados RFID lidos com sucesso!\n");
    return true;
}

/**
 * @brief Executa o modo SD_READ_SEND - lê dados do SD e inicia WiFi para envio
 */
bool execute_sd_read_send_mode(void) {
    printf("[MODE] === EXECUTANDO MODO SD_READ_SEND ===\n");
    
    // Verifica se ainda há tentativas disponíveis
    if (!increment_wifi_retry()) {
        printf("[MODE] Limite de tentativas WiFi atingido. Voltando ao RFID...\n");
        set_next_mode(SYSTEM_MODE_RFID_SD);
        watchdog_disable();
        trigger_watchdog_reset();
        return false;
    }
    
    // Configura watchdog para segurança (tempo maior)
    watchdog_enable(WIFI_OPERATION_TIME_MS, 1);
    
    // NOVA ABORDAGEM: Carrega dados do SD ANTES de ativar WiFi
    printf("[MODE] Carregando dados RFID do SD para buffer RAM...\n");
    spi_manager_activate_sd();
    
    // Declara função externa para carregar buffer
    extern bool carregar_dados_rfid_para_buffer(void);
    
    if (!carregar_dados_rfid_para_buffer()) {
        printf("[MODE] Nenhum dado RFID para enviar. Voltando ao RFID...\n");
        *wifi_retry_count = 0;  // Reset contador
        set_next_mode(SYSTEM_MODE_RFID_SD);  // Volta para modo principal
        watchdog_disable();
        trigger_watchdog_reset();
        return false;
    }
    
    // Desativa SD ANTES de ativar WiFi
    spi_manager_deactivate_sd();
    printf("[MODE] Dados carregados no buffer. SD desativado. Iniciando WiFi (tentativa %lu)...\n", *wifi_retry_count);
    
    // Desativa watchdog antes de iniciar WiFi
    watchdog_disable();
    
    // Agora ativa WiFi com dados já em RAM
    spi_manager_activate_wifi();
    
    printf("[MODE] WiFi ativado com dados em buffer RAM. Iniciando Core 1 para envio...\n");
    
    // Define próximo modo - se falhar, tenta novamente
    set_next_mode(SYSTEM_MODE_SD_READ_SEND);
    
    // Inicia Core 1 que fará o envio MQTT
    return true;
}

/**
 * @brief Executa limpeza dos dados enviados do SD Card
 */
bool execute_sd_cleanup_mode(void) {
    printf("[MODE] === EXECUTANDO MODO SD_CLEANUP (SUCESSO ASSUMIDO) ===\n");
    
    // Define LEDs para modo de limpeza
    set_system_status_leds(SYSTEM_MODE_SD_CLEANUP);
    display_message_with_led("Limpeza SD", "Iniciando...", -1, false, 1000);
    
    // Configura watchdog para segurança
    watchdog_enable(SD_OPERATION_TIME_MS, 1);
    
    // Ativa SD Card para limpeza
    spi_manager_activate_sd();
    
    if (!Sdh_Init()) {
        printf("[MODE] ERRO: Falha ao inicializar SD Card para limpeza\n");
        display_message_with_led("ERRO SD!", "Falha init", LED_ERROR, true, 3000);
        set_next_mode(SYSTEM_MODE_RFID_SD);  // Volta para modo principal
        watchdog_disable();
        trigger_watchdog_reset();
        return false;
    }
    
    printf("[MODE] SD inicializado. Procedendo com limpeza (sucesso assumido do MQTT)...\n");
    display_message_with_led("SD OK", "Removendo dados", -1, false, 1000);
    
    // Remove arquivo de dados enviados DIRETAMENTE (sem verificar confirmação)
    FRESULT fr = f_unlink("rfid_queue.txt");
    if (fr == FR_OK) {
        printf("[MODE] ✅ Arquivo rfid_queue.txt removido com sucesso!\n");
        display_message_with_led("Sucesso!", "Dados removidos", LED_RFID, true, 1000);
    } else {
        printf("[MODE] ⚠️  Arquivo rfid_queue.txt não existia ou falha ao remover: %d\n", fr);
        display_message_with_led("Aviso", "Arquivo inexistente", LED_ERROR, true, 1000);
    }
    
    // Remove qualquer arquivo de confirmação anterior (se existir)
    fr = f_unlink("send_success.txt");
    if (fr == FR_OK) {
        printf("[MODE] Arquivo de confirmação anterior removido.\n");
    }
    
    // Cria arquivo de log de limpeza
    display_message_with_led("Criando log", "de limpeza...", -1, false, 1000);
    FIL fil;
    fr = f_open(&fil, "cleanup_log.txt", FA_OPEN_APPEND | FA_WRITE);
    if (fr == FR_OK) {
        char log_entry[128];
        uint64_t timestamp = to_ms_since_boot(get_absolute_time());
        snprintf(log_entry, sizeof(log_entry), 
                 "CLEANUP:CYCLE_%lu,TIMESTAMP:%llu,SUCCESS:ASSUMED_FROM_MQTT\n", 
                 *persistent_counter, timestamp);
        
        f_write(&fil, log_entry, strlen(log_entry), NULL);
        f_sync(&fil);
        f_close(&fil);
        printf("[MODE] Log de limpeza salvo (sucesso assumido do MQTT).\n");
        display_message_with_led("Log salvo", "Cleanup OK", LED_RFID, true, 1000);
    }
    
    // Reset contador de retry WiFi após limpeza bem-sucedida
    *wifi_retry_count = 0;
    
    printf("[MODE] ✅ Limpeza concluída! Dados RFID removidos (MQTT enviou com sucesso).\n");
    printf("[MODE] Voltando ao modo RFID para novas leituras...\n");
    display_message_with_led("Limpeza OK!", "Voltando RFID", LED_RFID, true, 2000);
    
    // CORREÇÃO: Desativa watchdog ANTES de alterar estado
    watchdog_disable();
    
    // Define próximo modo e força conclusão
    set_next_mode(SYSTEM_MODE_RFID_SD);  // Volta para modo principal
    
    printf("[MODE] Estado alterado para RFID. Aguardando 3 segundos antes do reset...\n");
    
    // Desliga LEDs antes do reset
    gpio_put(LED_RFID, 0);
    gpio_put(LED_WIFI, 0);
    gpio_put(LED_ERROR, 0);
    
    // Aguarda um pouco para garantir que as operações finalizem
    sleep_ms(3000);
    
    // Reset manual direto (sem watchdog)
    printf("[MODE] Executando reset manual...\n");
    watchdog_enable(500, 1);  // Reset em 500ms
    while(1) tight_loop_contents();
    
    return true;
}

/**
 * @brief Função chamada quando envio MQTT for concluído (será chamada pelo Core 1)
 */
void on_mqtt_send_complete(void) {
    printf("[MODE] Envio MQTT concluído! Assumindo sucesso sem confirmação SD...\n");
    
    // NOVA ESTRATÉGIA SIMPLIFICADA: Se MQTT retornou sucesso, consideramos enviado
    // Não tentamos acessar SD enquanto WiFi está ativo - conflito SPI impossível de resolver
    
    printf("[MODE] ✅ MQTT confirmou envio - considerando SUCESSO automaticamente!\n");
    printf("[MODE] Pulando confirmação SD (conflito SPI) - indo direto para limpeza\n");
    
    // Reset o contador de retry WiFi após sucesso
    *wifi_retry_count = 0;
    
    // Define próximo modo para limpeza (sem criar arquivo de confirmação)
    set_next_mode(SYSTEM_MODE_SD_CLEANUP);
    
    // Reset em 2 segundos para limpeza
    watchdog_enable(2000, 1);
    printf("[MODE] Sistema resetará em 2s para limpeza do SD (sucesso assumido)\n");
    
    // Loop até watchdog resetar
    printf("[MODE] Aguardando reset para limpeza...\n");
    uint32_t countdown = 0;
    while(1) {
        sleep_ms(500);
        countdown++;
        if (countdown % 4 == 0) { // A cada 2 segundos
            printf("[MODE] Reset em %lu segundos...\n", 2 - (countdown/2));
        }
        // Aguarda o watchdog resetar
    }
}

/**
 * @brief Marca no SD card que o envio foi bem-sucedido (Core 0 tem controle total)
 */
bool mark_send_success_in_sd(void) {
    printf("[SD_SUCCESS] Core 0 marcando sucesso no SD (acesso direto, sem spi_manager)...\n");
    
    // Core 0 NÃO usa spi_manager para evitar conflitos com WiFi do Core 1
    printf("[SD_SUCCESS] Core 0 tentando acesso direto ao SD (ignorando spi_manager)...\n");
    
    // Não chama spi_manager_activate_sd() - acesso direto
    // Aguarda mais tempo para Core 1 se estabilizar
    sleep_ms(500);
    
    printf("[SD_SUCCESS] Tentando inicializar SD Card diretamente pelo Core 0...\n");
    if (!Sdh_Init()) {
        printf("[SD_SUCCESS] Primeira tentativa falhou. Core 0 tentando novamente...\n");
        
        // Aguarda mais tempo e tenta novamente
        sleep_ms(1000);
        
        if (!Sdh_Init()) {
            printf("[SD_SUCCESS] ❌ ERRO: Core 0 não conseguiu inicializar SD diretamente\n");
            return false;
        }
    }
    
    printf("[SD_SUCCESS] ✅ SD inicializado com sucesso pelo Core 0!\n");
    
    FIL fil;
    FRESULT fr;
    
    // Cria arquivo de marcação de sucesso
    printf("[SD_SUCCESS] Criando arquivo de confirmação...\n");
    fr = f_open(&fil, "send_success.txt", FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) {
        printf("[SD_SUCCESS] ❌ ERRO: Falha ao criar arquivo: %d\n", fr);
        return false;
    }
    
    // Dados de sucesso mais detalhados
    char success_data[200];
    uint64_t timestamp = to_ms_since_boot(get_absolute_time());
    
    snprintf(success_data, sizeof(success_data), 
             "SUCCESS_TIMESTAMP:%llu,CYCLE:%lu,STATUS:SENT_OK,CONTROLLER:CORE0,WIFI_RETRIES:%lu\n",
             timestamp, *persistent_counter, *wifi_retry_count);
    
    UINT bytes_written = 0;
    fr = f_write(&fil, success_data, strlen(success_data), &bytes_written);
    if (fr != FR_OK) {
        printf("[SD_SUCCESS] ❌ ERRO: Falha ao escrever: %d\n", fr);
        f_close(&fil);
        return false;
    }
    
    // Força sincronização múltipla para garantir
    fr = f_sync(&fil);
    if (fr != FR_OK) {
        printf("[SD_SUCCESS] ⚠️  AVISO: Primeira sincronização falhou: %d\n", fr);
        // Tenta novamente
        f_sync(&fil);
    }
    
    f_close(&fil);
    
    printf("[SD_SUCCESS] ✅ SUCESSO COMPLETO: Arquivo criado pelo Core 0! (%u bytes)\n", bytes_written);
    printf("[SD_SUCCESS] 📄 Conteúdo: %s", success_data);
    printf("[SD_SUCCESS] 🎯 Core 0 completou marcação de sucesso independentemente do Core 1!\n");
    
    return true;
}

/**
 * @brief Verifica se há dados pendentes para envio no SD
 */
bool check_pending_data_in_sd(void) {
    printf("[SD_CHECK] Verificando dados pendentes no SD...\n");
    
    // Ativa SD Card
    spi_manager_activate_sd();
    
    if (!Sdh_Init()) {
        printf("[SD_CHECK] ERRO: Falha ao inicializar SD Card\n");
        return false;
    }
    
    FIL fil;
    FRESULT fr;
    
    // Verifica se existe arquivo de dados RFID
    fr = f_open(&fil, "rfid_queue.txt", FA_READ);
    if (fr != FR_OK) {
        printf("[SD_CHECK] Nenhum dado RFID pendente encontrado\n");
        return false;
    }
    
    f_close(&fil);
    printf("[SD_CHECK] Dados RFID pendentes encontrados\n");
    return true;
}

/**
 * @brief Incrementa contador de retry WiFi e verifica limite
 */
bool increment_wifi_retry(void) {
    (*wifi_retry_count)++;
    printf("[WIFI_RETRY] Tentativa WiFi %lu de %d\n", *wifi_retry_count, MAX_WIFI_RETRY_CYCLES);
    
    if (*wifi_retry_count >= MAX_WIFI_RETRY_CYCLES) {
        printf("[WIFI_RETRY] Limite de tentativas atingido. Voltando ao RFID...\n");
        *wifi_retry_count = 0;  // Reset contador
        return false;  // Não deve tentar novamente
    }
    
    return true;  // Pode tentar novamente
}

// =================================================================================
// FUNÇÃO MAIN DO NÚCLEO 0 - SISTEMA DE ESTADOS COM WATCHDOG
// =================================================================================
int main() {
    stdio_init_all();
    while (!stdio_usb_connected()) { sleep_ms(100); }
    printf("\n[CORE 0] === INICIANDO SISTEMA COM ESTADOS PERSISTENTES E SINALIZACAO ===\n");
    
    // --- INICIALIZAÇÃO DO MUTEX ---
    mutex_init(&spi_mutex);
    
    // --- CONFIGURAÇÃO DE LEDs E OLED ---
    setup_leds_and_oled();
    
    // --- INICIALIZA SISTEMA DE ESTADOS PERSISTENTES ---
    init_persistent_state();
    system_mode_t current_mode = get_current_mode();
    
    printf("[MAIN] Modo atual: %d, Contador: %lu\n", current_mode, *persistent_counter);
    
    // Define LEDs para o modo atual
    set_system_status_leds(current_mode);
    
    // --- EXECUTA BASEADO NO MODO ATUAL ---
    switch (current_mode) {
        case SYSTEM_MODE_RFID_SD:
        default:
            printf("[MAIN] === MODO PRINCIPAL: LEITURA RFID + SD CARD ===\n");
            display_message_with_led("Modo RFID", "Aguardando tags...", LED_RFID, true, 2000);
            execute_rfid_sd_mode_new();
            // Função não retorna - reset via watchdog
            break;
            
        case SYSTEM_MODE_SD_READ_SEND:
            printf("[MAIN] === MODO: LER SD E ENVIAR VIA WIFI ===\n");
            display_message_with_led("Carregando dados", "do SD Card...", LED_WIFI, true, 1000);
            
            if (execute_sd_read_send_mode()) {
                // Inicia Core 1 para envio MQTT
                printf("[MAIN] Iniciando Core 1 para envio MQTT (tentativa %lu)...\n", *wifi_retry_count);
                display_message_with_led("WiFi Ativo", "Enviando MQTT...", LED_WIFI, true, 1000);
                inicia_core1();
                
                // Loop aguardando conclusão do envio com watchdog mais longo
                printf("[MAIN] Aguardando envio MQTT (tempo expandido: 3 minutos)...\n");
                
                // Configura watchdog para envio (3 minutos)
                watchdog_enable(WIFI_OPERATION_TIME_MS, 1);
                
                uint32_t start_time = to_ms_since_boot(get_absolute_time());
                uint32_t last_update = start_time;
                uint32_t last_display_update = start_time;
                bool led_blink_state = true;
                
                while (1) {
                    sleep_ms(1000);
                    
                    uint32_t current_time = to_ms_since_boot(get_absolute_time());
                    
                    // Atualiza watchdog a cada 5 segundos
                    if (current_time - last_update >= 5000) {
                        watchdog_update();
                        last_update = current_time;
                        
                        uint32_t elapsed = (current_time - start_time) / 1000;
                        printf("[MAIN] WiFi ativo há %lu segundos (tentativa %lu/%d)...\n", 
                               elapsed, *wifi_retry_count, MAX_WIFI_RETRY_CYCLES);
                    }
                    
                    // Atualiza display e LED a cada 2 segundos
                    if (current_time - last_display_update >= 2000) {
                        last_display_update = current_time;
                        led_blink_state = !led_blink_state;
                        
                        uint32_t elapsed = (current_time - start_time) / 1000;
                        char msg[32];
                        snprintf(msg, sizeof(msg), "Enviando: %lus", elapsed);
                        display_message_with_led("WiFi MQTT", msg, LED_WIFI, led_blink_state, 0);
                    }
                    
                    // Core 1 chamará on_mqtt_send_complete() quando terminar com sucesso
                    // Se falhar, o watchdog resetará e tentará novamente
                }
            }
            break;
            
        case SYSTEM_MODE_SD_CLEANUP:
            printf("[MAIN] === MODO: LIMPEZA DO SD CARD ===\n");
            display_message_with_led("Limpando SD", "Removendo dados", LED_RFID, true, 1000);
            execute_sd_cleanup_mode();
            // Função não retorna - reset via watchdog
            break;
            
        case SYSTEM_MODE_NORMAL_WIFI:
            printf("[MAIN] === MODO: WIFI TEMPORÁRIO PARA ENVIO ===\n");
            display_message_with_led("Modo WiFi", "Temporario", LED_WIFI, true, 1000);
            
            // Verifica se ainda pode tentar
            if (!increment_wifi_retry()) {
                printf("[MAIN] Limite de tentativas WiFi atingido. Voltando ao RFID...\n");
                display_message_with_led("Limite WiFi", "Voltando RFID", LED_ERROR, true, 2000);
                set_next_mode(SYSTEM_MODE_RFID_SD);
                trigger_watchdog_reset();
                break;
            }
            
            // Ativa WiFi e inicia Core 1
            spi_manager_activate_wifi();
            printf("[MAIN] WiFi ativado (tentativa %lu). Iniciando Core 1...\n", *wifi_retry_count);
            display_message_with_led("Ativando WiFi", "Conectando...", LED_WIFI, true, 1000);
            
            inicia_core1();
            printf("[MAIN] Core 1 iniciado. Aguardando envio MQTT...\n");
            
            // Configura watchdog para envio (tempo estendido)
            watchdog_enable(WIFI_OPERATION_TIME_MS, 1);
            
            uint32_t start_time = to_ms_since_boot(get_absolute_time());
            uint32_t last_update = start_time;
            uint32_t last_display_update = start_time;
            bool led_blink_state = true;
            
            // Loop aguardando envio
            while (1) {
                sleep_ms(1000);
                
                uint32_t current_time = to_ms_since_boot(get_absolute_time());
                
                // Atualiza watchdog a cada 5 segundos
                if (current_time - last_update >= 5000) {
                    watchdog_update();
                    last_update = current_time;
                    
                    uint32_t elapsed = (current_time - start_time) / 1000;
                    printf("[MAIN] Aguardando MQTT há %lu segundos (tentativa %lu/%d)...\n", 
                           elapsed, *wifi_retry_count, MAX_WIFI_RETRY_CYCLES);
                }
                
                // Atualiza display a cada 3 segundos
                if (current_time - last_display_update >= 3000) {
                    last_display_update = current_time;
                    led_blink_state = !led_blink_state;
                    
                    char msg[32];
                    snprintf(msg, sizeof(msg), "Tent: %lu/%d", *wifi_retry_count, MAX_WIFI_RETRY_CYCLES);
                    display_message_with_led("Aguard. MQTT", msg, LED_WIFI, led_blink_state, 0);
                }
            }
            break;
            
        case SYSTEM_MODE_POST_SEND:
            printf("[MAIN] === MODO: PÓS-ENVIO - VOLTANDO AO RFID ===\n");
            display_message_with_led("Pos-envio", "Voltando RFID", LED_RFID, true, 2000);
            set_next_mode(SYSTEM_MODE_RFID_SD);
            trigger_watchdog_reset();
            break;
    }
    
    return 0;
}

/**
 * @brief Executa modo RFID + SD - nova implementação simplificada
 */
void execute_rfid_sd_mode_new(void) {
    printf("[RFID] === EXECUTANDO MODO RFID + SD (MODO PRINCIPAL) ===\n");
    
    // Define LEDs para modo RFID
    set_system_status_leds(SYSTEM_MODE_RFID_SD);
    
    // Primeiro verifica se há dados pendentes para envio
    if (check_pending_data_in_sd()) {
        printf("[RFID] Dados pendentes encontrados! Iniciando envio WiFi...\n");
        display_message_with_led("Dados pendentes", "Iniciando WiFi...", LED_WIFI, true, 2000);
        set_next_mode(SYSTEM_MODE_SD_READ_SEND);
        trigger_watchdog_reset();
        return;
    }
    
    // Configura watchdog para operações RFID/SD
    watchdog_enable(RFID_SD_OPERATION_TIME_MS, 1);
    
    // Ativa RFID
    printf("[RFID] Ativando leitor RFID...\n");
    display_message_with_led("Sistema RFID", "Ativando leitor...", LED_RFID, true, 1000);
    spi_manager_activate_rfid();
    
    // Usar as funções RFID já existentes
    MFRC522Ptr_t mfrc = MFRC522_Init();
    if (!mfrc) {
        printf("[RFID] Erro: Falha ao inicializar leitor RFID\n");
        display_message_with_led("ERRO RFID!", "Falha ao iniciar", LED_ERROR, true, 3000);
        printf("[RFID] Tentando novamente em 5 segundos...\n");
        set_next_mode(SYSTEM_MODE_RFID_SD);  // Tenta novamente
        watchdog_enable(5000, 1);
        while(1) tight_loop_contents(); // Aguarda reset
    }
    
    PCD_Init(mfrc, spi0);
    printf("[RFID] Leitor RFID inicializado. Aguardando tags...\n");
    display_message_with_led("RFID Pronto", "Aproxime cartao...", LED_RFID, true, 0);
    
    // Loop de leitura RFID por tempo limitado
    uint32_t start_time = to_ms_since_boot(get_absolute_time());
    uint32_t tags_lidas = 0;
    bool tag_processada = false;
    uint32_t last_blink = start_time;
    bool led_state = true;
    
    printf("[RFID] Aproxime um cartão RFID para leitura...\n");
    
    while (to_ms_since_boot(get_absolute_time()) - start_time < 30000) { // 30 segundos
        // Atualiza watchdog
        watchdog_update();
        
        uint32_t current_time = to_ms_since_boot(get_absolute_time());
        
        // LED piscando para indicar que está aguardando
        if (current_time - last_blink >= 1000) {  // Pisca a cada 1 segundo
            last_blink = current_time;
            led_state = !led_state;
            gpio_put(LED_RFID, led_state);
        }
        
        // Verifica se há cartão presente
        if (PICC_IsNewCardPresent(mfrc) && PICC_ReadCardSerial(mfrc)) {
            printf("[RFID] Cartão RFID detectado! UID: ");
            for (int i = 0; i < mfrc->uid.size; i++) {
                printf("%02X", mfrc->uid.uidByte[i]);
            }
            printf("\n");
            
            // LED fixo quando detecta cartão
            display_message_with_led("Cartao detectado!", "Lendo dados...", LED_RFID, true, 0);
            
            // Estrutura para dados de estudante
            StudentDataBlock student_data;
            bool dados_validos = false;
            
            // Tenta ler dados estruturados
            if (Tdh_ReadStudentData(mfrc, &student_data, 4, NULL) == STATUS_OK) {
                printf("[RFID] Dados do estudante lidos:\n");
                printf("  ID: %u\n", student_data.fields.student_id);
                printf("  Nome: %s\n", student_data.fields.student_name);
                printf("  Viagens: %u\n", student_data.fields.trip_count);
                
                display_message_with_led("Estudante:", student_data.fields.student_name, LED_RFID, true, 2000);
                
                // Prepara dados para salvar com informações expandidas
                char rfid_info[256];
                snprintf(rfid_info, sizeof(rfid_info), "NAME:%s,ID:%u,TRIPS:%u,ROUTE:1", 
                         student_data.fields.student_name, 
                         student_data.fields.student_id,
                         student_data.fields.trip_count);
                
                dados_validos = true;
                
                // Ativa SD e salva dados
                display_message_with_led("Salvando no", "SD Card...", LED_RFID, true, 0);
                spi_manager_activate_sd();
                if (!Sdh_Init()) {
                    printf("[RFID] ERRO: Falha ao inicializar SD Card\n");
                    display_message_with_led("ERRO SD!", "Falha ao salvar", LED_ERROR, true, 2000);
                } else if (save_rfid_data_to_sd(rfid_info)) {
                    printf("[RFID] Dados salvos com sucesso no SD\n");
                    display_message_with_led("Sucesso!", "Dados salvos", LED_RFID, true, 2000);
                    tags_lidas++;
                    tag_processada = true;
                } else {
                    printf("[RFID] Erro ao salvar dados no SD\n");
                    display_message_with_led("ERRO!", "Falha ao salvar", LED_ERROR, true, 2000);
                }
                
            } else {
                printf("[RFID] Cartão sem dados estruturados. Salvando UID...\n");
                display_message_with_led("Cartao vazio", "Salvando UID...", LED_RFID, true, 1000);
                
                // Salva apenas UID
                char uid_info[128];
                snprintf(uid_info, sizeof(uid_info), "UID:");
                for (int i = 0; i < mfrc->uid.size; i++) {
                    char hex[4];
                    snprintf(hex, sizeof(hex), "%02X", mfrc->uid.uidByte[i]);
                    strcat(uid_info, hex);
                }
                strcat(uid_info, ",TYPE:UNKNOWN");
                
                // Ativa SD e salva dados
                spi_manager_activate_sd();
                if (!Sdh_Init()) {
                    printf("[RFID] ERRO: Falha ao inicializar SD Card para UID\n");
                    display_message_with_led("ERRO SD!", "Falha UID", LED_ERROR, true, 2000);
                } else if (save_rfid_data_to_sd(uid_info)) {
                    printf("[RFID] UID salvo com sucesso no SD\n");
                    display_message_with_led("UID salvo!", "Sucesso", LED_RFID, true, 2000);
                    tags_lidas++;
                    tag_processada = true;
                } else {
                    printf("[RFID] Erro ao salvar UID no SD\n");
                    display_message_with_led("ERRO!", "Falha UID", LED_ERROR, true, 2000);
                }
            }
            
            // Se processou uma tag com sucesso, sai do loop para enviar
            if (tag_processada) {
                printf("[RFID] Tag processada! Preparando para envio...\n");
                display_message_with_led("Tag processada!", "Preparando envio", LED_WIFI, true, 2000);
                break;
            }
            
            // Aguarda tag ser removida
            printf("[RFID] Aguarde remover o cartão...\n");
            display_message_with_led("Remova o cartao", "Aguardando...", LED_RFID, true, 0);
            while (PICC_IsNewCardPresent(mfrc)) {
                sleep_ms(100);
                watchdog_update();
            }
            
            printf("[RFID] Cartão removido. Aguardando próximo...\n");
            display_message_with_led("RFID Pronto", "Aproxime cartao...", LED_RFID, true, 0);
        }
        
        sleep_ms(500);
    }
    
    printf("[RFID] Ciclo de leitura finalizado. Tags lidas: %lu\n", tags_lidas);
    
    // Se processou alguma tag, vai para envio WiFi
    if (tag_processada) {
        printf("[RFID] Dados coletados! Iniciando envio WiFi...\n");
        display_message_with_led("Iniciando", "envio WiFi...", LED_WIFI, true, 2000);
        *wifi_retry_count = 0;  // Reset contador para novos dados
        set_next_mode(SYSTEM_MODE_SD_READ_SEND);
    } else {
        printf("[RFID] Nenhuma tag processada. Reiniciando ciclo RFID...\n");
        display_message_with_led("Timeout RFID", "Reiniciando...", LED_ERROR, true, 2000);
        set_next_mode(SYSTEM_MODE_RFID_SD);  // Continua no modo principal
    }
    
    // Desliga LEDs antes do reset
    gpio_put(LED_RFID, 0);
    gpio_put(LED_WIFI, 0);
    gpio_put(LED_ERROR, 0);
    
    trigger_watchdog_reset();
}