// Arquivo: inc/sd_card_handler.c

#include "sd_card_handler.h"
#include "hw_config.h"      // Para sd_get_by_num() e sd_init_driver()
#include "f_util.h"         // Para FRESULT_str()
#include "rtc.h"
#include "pico/stdlib.h"
#include <string.h>         // Para memcpy
#include <stdio.h>          // Para printf

// Instância global do sistema de arquivos para esta biblioteca
FATFS fs_global;

// Nome padrão para o arquivo de log
#define LOG_FILENAME "log_viagens.txt"


/**
 * @brief Inicializa o hardware SPI para o cartão SD e monta o sistema de arquivos.
 */
bool Sdh_Init(void) {
    // Esta função da biblioteca de exemplo lê a configuração em hw_config.c e prepara o SPI
    sd_init_driver(); 
    // time_init();

    printf("[INIT] Obtendo configuracao para o cartao SD '0:'...\n");
    sd_card_t *pSD = sd_get_by_num(0);
    if (!pSD) {
        printf("ERRO FATAL: Configuracao do SD '0:' nao encontrada em hw_config.c!\n");
        return false;
    }
    printf(">> SUCESSO: Configuracao do SD carregada.\n");

    printf("[INIT] Tentando montar o cartao SD (f_mount)...\n");
    // f_mount usa o nome do drive (ex: "0:") que está em pSD->pcName
    FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1); 
    if (fr != FR_OK) {
        printf("ERRO: Nao foi possivel montar o cartao SD! Codigo FatFs: %s (%d)\n", FRESULT_str(fr), fr);
        return false;
    }
    printf(">> SUCESSO: Cartao SD montado com sucesso.\n");
    
    // Copia a instância montada para a variável global para uso em outras funções
    memcpy(&fs_global, &pSD->fatfs, sizeof(FATFS));

    return true;
}


/**
 * @brief Grava um registro de embarque de aluno no arquivo de log no cartão SD.
 */
bool Sdh_LogBoarding(StudentDataBlock *data) {
    FIL fil;
    FRESULT fr;

    // Abre o arquivo de log no modo de "append" (adicionar ao final do arquivo)
    fr = f_open(&fil, LOG_FILENAME, FA_OPEN_APPEND | FA_WRITE);
    if (fr != FR_OK) {
        printf("SD_LOG: Falha ao abrir o arquivo %s. Codigo: %d\n", LOG_FILENAME, fr);
        return false;
    }
    
    // Monta a string que será gravada no log
    char log_buffer[128];
    // Usamos o tempo desde o boot como um timestamp temporário.
    // No futuro, aqui entraria a leitura de um módulo RTC.
    uint64_t timestamp = to_ms_since_boot(get_absolute_time()); 
    sprintf(log_buffer, "ID:%u,NOME:%s,VIAGEM:%u,TIMESTAMP_MS:%llu\n", 
            data->fields.student_id, 
            data->fields.student_name, 
            data->fields.trip_count,
            timestamp);

    // Escreve a linha de log no final do arquivo
    if (f_printf(&fil, log_buffer) < 0) {
        printf("SD_LOG: Falha ao escrever no arquivo de log.\n");
        f_close(&fil); // Tenta fechar mesmo em caso de erro
        return false;
    }
    
    // Fecha o arquivo - ESSENCIAL para garantir que os dados sejam salvos no cartão!
    fr = f_close(&fil);
    if (fr != FR_OK) {
        printf("SD_LOG: Falha ao fechar o arquivo. Codigo: %d\n", fr);
        return false;
    }

    printf("SD_LOG: Registro salvo com sucesso em %s\n", LOG_FILENAME);
    return true;
}


/**
 * @brief Lê o arquivo de log do cartão SD e imprime todo o seu conteúdo no monitor serial.
 */
bool Sdh_PrintLogsToSerial() {
    FIL fil;
    FRESULT fr;
    char line_buffer[256]; // Um buffer para armazenar cada linha lida

    printf("\n--- LENDO LOG DO CARTAO SD ---\n");

    // Abre o arquivo de log apenas para leitura
    fr = f_open(&fil, LOG_FILENAME, FA_READ);
    if (fr != FR_OK) {
        if (fr == FR_NO_FILE) {
            printf("Arquivo de log '%s' ainda nao existe. Nenhum registro para mostrar.\n", LOG_FILENAME);
            return true; // Não é um erro, apenas não há nada para ler.
        }
        printf("SD_READ: Falha ao abrir o arquivo %s. Codigo: %s (%d)\n", LOG_FILENAME, FRESULT_str(fr), fr);
        return false;
    }

    // Lê o arquivo linha por linha e imprime no serial
    printf("--- INICIO DO LOG ---\n");
    while (f_gets(line_buffer, sizeof(line_buffer), &fil)) {
        printf("%s", line_buffer);
    }
    printf("--- FIM DO LOG ---\n");

    // Fecha o arquivo
    fr = f_close(&fil);
    if (fr != FR_OK) {
        printf("SD_READ: Falha ao fechar o arquivo. Codigo: %s (%d)\n", fr);
        return false;
    }

    return true;
}


/**
 * @brief (Opcional) Função de teste que demonstra as capacidades da biblioteca.
 */
bool Sdh_RunTest(void) {
    FRESULT fr;
    
    // 1. Abrir o arquivo de teste
    FIL fil;
    const char* const filename = "log_de_teste.txt";
    printf("[TESTE] Tentando abrir o arquivo '%s'...\n", filename);
    fr = f_open(&fil, filename, FA_OPEN_APPEND | FA_WRITE);
    if (FR_OK != fr) {
        printf("ERRO DE TESTE: f_open(%s) falhou! Codigo: %s (%d)\n", filename, FRESULT_str(fr), fr);
        return false;
    }
    printf(">> SUCESSO DE TESTE: Arquivo '%s' aberto.\n", filename);

    // 2. Escrever no arquivo
    printf("[TESTE] Escrevendo no arquivo...\n");
    if (f_printf(&fil, "Teste da biblioteca OK! Compilado em: %s %s\n", __DATE__, __TIME__) < 0) {
        printf("ERRO DE TESTE: f_printf falhou!\n");
        f_close(&fil);
        return false;
    }
    printf(">> SUCESSO DE TESTE: Escrita no arquivo finalizada.\n");

    // 3. Fechar o arquivo
    printf("[TESTE] Fechando o arquivo...\n");
    fr = f_close(&fil);
    if (FR_OK != fr) {
        printf("ERRO DE TESTE ao fechar o arquivo: %s (%d)\n", FRESULT_str(fr), fr);
        return false;
    }
    printf(">> SUCESSO DE TESTE: Arquivo fechado.\n");

    printf("\n--- TESTE DO CARTAO SD CONCLUIDO COM SUCESSO ---\n");
    return true;
}