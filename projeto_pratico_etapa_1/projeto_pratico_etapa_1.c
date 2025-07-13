#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/sync.h"
#include "pico/stdio_usb.h"
#include "ff.h" 
#include "rtc.h"
#include "f_util.h"

// Nossas bibliotecas customizadas e de suporte
#include "inc/mfrc522.h"
#include "inc/tag_data_handler.h"
#include "inc/sd_card_handler.h" // A biblioteca que vamos usar
#include "hw_config.h" 

// Mapeamento dos botões
#define BTN_A 5
#define BTN_B 6
#define JOY_SW 22

// Definição dos estados do nosso programa
typedef enum {
    STATE_SELECTION,
    STATE_WAIT_FOR_SECOND_KEY,
    STATE_READING,
    STATE_WRITING,
    STATE_SD_READ
} AppState;

// Variáveis globais para a lógica de interrupção
volatile AppState currentState = STATE_SELECTION;
volatile char first_key_pressed = 0; 
volatile uint32_t wait_start_time = 0;
volatile uint32_t last_press_time = 0;
#define WAIT_TIMEOUT_MS 4000 // 4 segundos de timeout

// =================================================================================
// FUNÇÃO DE CALLBACK PARA INTERRUPÇÃO DOS BOTÕES
// =================================================================================
void gpio_callback(uint gpio, uint32_t events) {
    // Debounce para evitar múltiplos cliques
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    if (current_time - last_press_time < 250) { 
        gpio_acknowledge_irq(gpio, events);
        return;
    }
    last_press_time = current_time;

    // Lógica da máquina de estados baseada no estado atual
    switch (currentState) {
        case STATE_SELECTION:
            if (gpio == BTN_A) {
                first_key_pressed = 'A';
                currentState = STATE_WAIT_FOR_SECOND_KEY;
            } else if (gpio == BTN_B) {
                first_key_pressed = 'B';
                currentState = STATE_WAIT_FOR_SECOND_KEY;
            }
            break;
        case STATE_WAIT_FOR_SECOND_KEY:
            if (first_key_pressed == 'A') {
                if (gpio == BTN_A) currentState = STATE_READING;
                else if (gpio == BTN_B) currentState = STATE_WRITING;
            } else if (first_key_pressed == 'B') {
                if (gpio == BTN_A) currentState = STATE_SD_READ;
                else if (gpio == BTN_B) printf("Sequencia B->B acionada (sem acao definida).\n");
            }
            if (gpio == JOY_SW) {
                currentState = STATE_SELECTION;
            }
            break;
        case STATE_READING:
        case STATE_WRITING:
        case STATE_SD_READ:
            if (gpio == JOY_SW) {
                currentState = STATE_SELECTION;
            }
            break;
    }
    
    gpio_acknowledge_irq(gpio, events);
}

// =================================================================================
// FUNÇÕES DE OPERAÇÃO
// =================================================================================

// Função para gravar novas tags
void provision_new_tag(MFRC522Ptr_t mfrc) {
    printf("\n--- MODO DE GRAVACAO DE NOVAS TAGS ---\n");
    printf("Aproxime uma tag para configurar um novo aluno.\n");
    printf("(Pressione o joystick para voltar ao menu)\n");

    while (!PICC_IsNewCardPresent(mfrc) || !PICC_ReadCardSerial(mfrc)) {
        if (currentState != STATE_WRITING) return;
        sleep_ms(100);
    }
    
    uint32_t student_id = 2025011;
    const char* student_name = "JOANA D.";
    printf("Gravando novo aluno: %s, ID: %u\n", student_name, student_id);

    StudentDataBlock new_data;
    Tdh_PrepareNewStudentTag(&new_data, student_id, student_name);
    
    uint8_t blockAddr = 4;
    MIFARE_Key key;
    for (uint8_t i = 0; i < MF_KEY_SIZE; i++) { key.keybyte[i] = 0xFF; }
    
    StatusCode status = Tdh_WriteStudentData(mfrc, &new_data, blockAddr, &key);
    if (status == STATUS_OK) printf(">>> SUCESSO! Tag configurada.\n");
    else printf(">>> FALHA ao configurar a tag.\n");

    PICC_HaltA(mfrc);
    sleep_ms(3000);
}

// Função para processar o embarque e chamar a biblioteca para gravar no SD
void process_student_boarding(MFRC522Ptr_t mfrc) {
    printf("\n--- OPERACAO DO ONIBUS ---\nAproxime a tag do aluno:\n");

    while (!PICC_IsNewCardPresent(mfrc) || !PICC_ReadCardSerial(mfrc)) {
        if (currentState != STATE_READING) return;
        sleep_ms(100);
    }
    
    StudentDataBlock student_data;
    MIFARE_Key key;
    for (uint8_t i = 0; i < MF_KEY_SIZE; i++) { key.keybyte[i] = 0xFF; }
    
    printf("Tag detectada. Processando embarque...\n");

    StatusCode status = Tdh_ProcessTrip(mfrc, 4, &key, &student_data);
    
    if (status == STATUS_OK) {
        printf("----------------------------------\n");
        printf(" Embarque Registrado via RFID!\n");
        printf(" Aluno: %s | Viagem No.: %u\n", student_data.fields.student_name, student_data.fields.trip_count);
        printf("----------------------------------\n");

        // --- CHAMA A BIBLIOTECA PARA SALVAR NO CARTÃO SD ---
        if (Sdh_LogBoarding(&student_data)) {
            printf("LOG: Registro salvo com sucesso no cartao SD.\n");
        } else {
            printf("!!! ALERTA: FALHA AO GRAVAR LOG NO CARTAO SD !!!\n");
        }
    } else {
        printf(">>> FALHA NO EMBARQUE! Status: %s <<<\n", GetStatusCodeName(status));
    }
    
    PICC_HaltA(mfrc);
    sleep_ms(2000);
}

// =================================================================================
// FUNÇÃO PRINCIPAL
// =================================================================================
void main() {
    stdio_init_all();
    
    // Espera pela conexão do monitor serial
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    
    printf("\nMonitor Serial Conectado! Iniciando sistema...\n");
    sleep_ms(1000);

    printf("Inicializando e montando cartao SD (SPI0)...\n");
    if (!Sdh_Init()) {
        printf("!!! ERRO CRITICO: CARTAO SD NAO FUNCIONAL. PARANDO. !!!\n");
        while(1);
    }

    // --- INICIALIZAÇÃO DOS MÓDULOS ---
    printf("Inicializando leitor RFID (SPI1)...\n");
    MFRC522Ptr_t mfrc = MFRC522_Init();
    PCD_Init(mfrc, spi1); 
    
    // --- INICIALIZAÇÃO DOS BOTÕES E INTERRUPÇÕES ---
    printf("Inicializando botoes...\n");
    gpio_init(BTN_A);
    gpio_set_dir(BTN_A, GPIO_IN);
    gpio_pull_up(BTN_A);
    gpio_init(BTN_B);
    gpio_set_dir(BTN_B, GPIO_IN);
    gpio_pull_up(BTN_B);
    gpio_init(JOY_SW);
    gpio_set_dir(JOY_SW, GPIO_IN);
    gpio_pull_up(JOY_SW);

    gpio_set_irq_enabled_with_callback(BTN_A, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(BTN_B, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(JOY_SW, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    AppState lastKnownState = STATE_SELECTION;

    // --- MENSAGEM DE BOAS-VINDAS ---
    printf("===========================================\n");
    printf("  Sistema de Gerenciamento de Tags RFID\n");
    printf("===========================================\n");
    printf("\n--- MENU PRINCIPAL ---\n");
    printf("Sequencia A->A: Modo Leitura (Onibus)\n");
    printf("Sequencia A->B: Modo Escrita (Novas Tags)\n");
    printf("Sequencia B->A: Ler Logs do SD\n");
    printf("Joystick: Cancelar/Voltar\n");
    printf("\nAguardando primeiro comando (A ou B)...\n");

    // --- LOOP PRINCIPAL DA APLICAÇÃO ---
    while (1) {
        // Bloco 1: Lida com as MUDANÇAS de estado
        if (currentState != lastKnownState) {
            switch (currentState) {
                case STATE_SELECTION:
                    printf("\n--- MENU PRINCIPAL ---\nAguardando primeiro comando (A ou B)...\n");
                    break;
                case STATE_WAIT_FOR_SECOND_KEY:
                    wait_start_time = to_ms_since_boot(get_absolute_time()); 
                    printf("Pressionado %c... Aguardando segundo comando... (Joystick para cancelar)\n", first_key_pressed);
                    break;
                case STATE_READING:
                    printf("\nMODO LEITURA (Onibus) ATIVADO [A->A].\n");
                    break;
                case STATE_WRITING:
                    printf("\nMODO ESCRITA (Novas Tags) ATIVADO [A->B].\n");
                    break;
                case STATE_SD_READ:
                     printf("\nMODO LEITURA DE SD ATIVADO [B->A].\n");
                    break;
            }
            lastKnownState = currentState;
        }

        // Bloco 2: Executa a AÇÃO contínua de cada estado
        switch (currentState) {
            case STATE_READING:
                process_student_boarding(mfrc);
                break;
            case STATE_WRITING:
                provision_new_tag(mfrc);
                break;
            case STATE_SD_READ:
                Sdh_PrintLogsToSerial();
                currentState = STATE_SELECTION; // Retorna ao menu automaticamente
                break;
            case STATE_WAIT_FOR_SECOND_KEY:
                if (to_ms_since_boot(get_absolute_time()) - wait_start_time > WAIT_TIMEOUT_MS) {
                    printf("\nTempo de espera esgotado! Voltando ao menu principal.\n");
                    currentState = STATE_SELECTION;
                }
                __wfi(); // Dorme enquanto espera
                break;
            case STATE_SELECTION:
                __wfi(); // Apenas dorme e espera
                break;
        }
    }
}