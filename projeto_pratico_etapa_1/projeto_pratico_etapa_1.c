#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "inc/mfrc522.h"
#include "inc/tag_data_handler.h"
#include "pico/sync.h"

// Mapeamento dos botões
#define BTN_A 5
#define BTN_B 6
#define JOY_SW 22

// Definição dos estados do programa
typedef enum {
    STATE_SELECTION,
    STATE_READING,
    STATE_WRITING
} AppState;

// Variáveis globais para controlar o estado e o debounce dos botões
volatile AppState currentState = STATE_SELECTION;
volatile uint32_t last_press_time = 0;

// Callback: Esta função será chamada AUTOMATICAMENTE quando um botão for pressionado
void gpio_callback(uint gpio, uint32_t events) {
    // Lógica simples de debounce: ignora pressões muito rápidas
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    if (current_time - last_press_time < 250) { // 250ms de debounce
        return;
    }
    last_press_time = current_time;

    // Muda o estado do programa com base no botão pressionado
    switch (gpio) {
        case BTN_A:
            if (currentState == STATE_SELECTION) {
                currentState = STATE_READING;
            }
            break;
        case BTN_B:
            if (currentState == STATE_SELECTION) {
                currentState = STATE_WRITING;
            }
            break;
        case JOY_SW:
            // O joystick sempre retorna ao menu principal
            currentState = STATE_SELECTION;
            break;
    }
}

// As funções de operação (provision_new_tag e process_student_boarding) permanecem as mesmas
void provision_new_tag(MFRC522Ptr_t mfrc) {
    printf("\n--- MODO DE GRAVACAO DE NOVAS TAGS ---\n");
    printf("Aproxime uma tag para configurar um novo aluno.\n");
    printf("(Pressione o joystick para voltar ao menu)\n");

    while (!PICC_IsNewCardPresent(mfrc) || !PICC_ReadCardSerial(mfrc)) {
        if (currentState != STATE_WRITING) return; // Sai se o estado mudou
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

void process_student_boarding(MFRC522Ptr_t mfrc) {
    printf("\n--- OPERACAO DO ONIBUS ---\nAproxime a tag do aluno:\n");
    printf("(Pressione o joystick para voltar ao menu)\n");

    while (!PICC_IsNewCardPresent(mfrc) || !PICC_ReadCardSerial(mfrc)) {
        if (currentState != STATE_READING) return; // Sai se o estado mudou
        sleep_ms(100);
    }
    
    StudentDataBlock student_data;
    uint8_t blockAddr = 4;
    MIFARE_Key key;
    for (uint8_t i = 0; i < MF_KEY_SIZE; i++) { key.keybyte[i] = 0xFF; }
    
    printf("Tag detectada. Processando embarque...\n");

    StatusCode status = Tdh_ProcessTrip(mfrc, blockAddr, &key, &student_data);
    
    if (status == STATUS_OK) {
        printf("----------------------------------\n");
        printf(" Embarque Registrado!\n");
        printf(" Aluno: %s (ID: %u)\n", student_data.fields.student_name, student_data.fields.student_id);
        printf(" Viagem No.: %u\n", student_data.fields.trip_count);
        printf("----------------------------------\n");
    } else {
        printf(">>> FALHA NO EMBARQUE! Status: %s <<<\n", GetStatusCodeName(status));
    }
    
    PICC_HaltA(mfrc);
    sleep_ms(2000);
}


void main() {
    stdio_init_all();
    sleep_ms(4000);

    MFRC522Ptr_t mfrc = MFRC522_Init();
    // Usa spi0. A biblioteca mfrc522.c irá remapear os pinos internamente.
    PCD_Init(mfrc, spi0); 

    // Bloco de inicialização dos GPIOs dos botões
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

    // O laço 'for' problemático foi REMOVIDO daqui.

    // Configura as interrupções para os botões
    gpio_set_irq_enabled_with_callback(BTN_A, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(BTN_B, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(JOY_SW, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    AppState lastKnownState = STATE_SELECTION;

    printf("===========================================\n");
    printf("  Sistema de Gerenciamento de Tags RFID\n");
    printf("===========================================\n");
    printf("\n--- MENU PRINCIPAL ---\n");
    printf("Pressione Botao A para MODO LEITURA\n");
    printf("Pressione Botao B para MODO ESCRITA\n");

    while (1) {
        // A máquina de estados permanece a mesma
        switch (currentState) {
            case STATE_READING:
                if (lastKnownState != STATE_READING) {
                    printf("\nMODO LEITURA (Onibus) SELECIONADO.\n");
                    lastKnownState = STATE_READING;
                }
                process_student_boarding(mfrc);
                break;
                
            case STATE_WRITING:
                if (lastKnownState != STATE_WRITING) {
                    printf("\nMODO ESCRITA (Novas Tags) SELECIONADO.\n");
                    lastKnownState = STATE_WRITING;
                }
                provision_new_tag(mfrc);
                break;
                
            case STATE_SELECTION:
                if (lastKnownState != STATE_SELECTION) {
                    printf("\n--- MENU PRINCIPAL ---\n");
                    printf("Pressione Botao A para MODO LEITURA\n");
                    printf("Pressione Botao B para MODO ESCRITA\n");
                    lastKnownState = STATE_SELECTION;
                }
                __wfi(); // Wait For Interrupt
                break;
        }
    }
}