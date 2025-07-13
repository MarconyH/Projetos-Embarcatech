#include <stdio.h>
//
#include "f_util.h"
#include "ff.h"
#include "pico/stdlib.h"
#include "rtc.h"
//
#include "hw_config.h"

int main() {
    // 1. Inicialização do sistema
    stdio_init_all();
    // Adiciona uma pausa importante para dar tempo ao monitor serial de se conectar
    sleep_ms(5000); 
    printf("\n--- INICIO DO PROGRAMA DE TESTE DO CARTAO SD ---\n");

    time_init();

    // 2. Obter configuração do cartão SD
    printf("[PASSO 1] Obtendo configuracao para o cartao SD '0:'...\n");
    sd_card_t *pSD = sd_get_by_num(0);
    if (!pSD) {
        printf("ERRO FATAL: Configuracao do SD '0:' nao encontrada em hw_config.c!\n");
        while(1); // Para a execução
    }
    printf(">> SUCESSO: Configuracao do SD carregada.\n\n");

    // 3. Montar o cartão SD
    printf("[PASSO 2] Tentando montar o cartao SD (f_mount)...\n");
    FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
    if (FR_OK != fr) {
        printf("ERRO FATAL: f_mount falhou! Codigo de erro: %s (%d)\n", FRESULT_str(fr), fr);
        // A função panic original pararia tudo, este while(1) faz o mesmo.
        while(1); 
    }
    printf(">> SUCESSO: Cartao SD montado!\n\n");

    // 4. Abrir o arquivo
    FIL fil;
    const char* const filename = "log_de_teste.txt";
    printf("[PASSO 3] Tentando abrir o arquivo '%s' em modo de adicao (append)...\n", filename);
    fr = f_open(&fil, filename, FA_OPEN_APPEND | FA_WRITE);
    if (FR_OK != fr) { // FR_EXIST não é um erro para este modo, então o removemos da checagem
        printf("ERRO FATAL: f_open(%s) falhou! Codigo de erro: %s (%d)\n", filename, FRESULT_str(fr), fr);
        while(1);
    }
    printf(">> SUCESSO: Arquivo '%s' aberto.\n\n", filename);

    // 5. Escrever no arquivo
    printf("[PASSO 4] Escrevendo no arquivo...\n");
    if (f_printf(&fil, "Hello, world! O cartao SD esta funcionando. Data: %s %s\n", __DATE__, __TIME__) < 0) {
        printf("ERRO: f_printf falhou!\n");
    } else {
        printf(">> SUCESSO: Escrita no arquivo finalizada.\n\n");
    }

    // 6. Fechar o arquivo para salvar os dados
    printf("[PASSO 5] Fechando o arquivo para salvar os dados (f_close)...\n");
    fr = f_close(&fil);
    if (FR_OK != fr) {
        printf("ERRO ao fechar o arquivo: %s (%d)\n", FRESULT_str(fr), fr);
    } else {
        printf(">> SUCESSO: Arquivo fechado. Dados estao seguros no cartao.\n\n");
    }

    // 7. Desmontar o drive
    printf("[PASSO 6] Desmontando o drive (f_unmount)...\n");
    f_unmount(pSD->pcName);
    printf(">> SUCESSO: Drive desmontado.\n\n");

    // 8. Finalização
    printf("--- FIM DO PROGRAMA ---\nO Pico esta em loop infinito para evitar re-execucao.\n");
    for (;;);
}