#ifndef SD_CARD_HANDLER_H
#define SD_CARD_HANDLER_H

#include <stdbool.h>
#include "../rfid/tag_data_handler.h"
#include "ff.h" 

// A estrutura global FATFS precisa ser acessível
extern FATFS fs_global;

bool Sdh_Init(void);
bool Sdh_LogBoarding(StudentDataBlock *data);
bool Sdh_PrintLogsToSerial(void);

bool Sdh_ReadLogFile(char *buffer, uint32_t buffer_size);

/**
 * @brief Apaga o arquivo de log do cartão SD.
 * * @return true se o arquivo foi apagado com sucesso ou se já não existia. Retorna false em caso de erro.
 */
bool Sdh_DeleteLogFile(void);

/**
 * @brief Executa uma rotina de teste completa no cartão SD:
 * monta, abre/cria um arquivo, escreve nele, fecha e desmonta.
 * @return true se todos os passos foram bem-sucedidos, false caso contrário.
 */
bool Sdh_RunTest(void);

#endif // SD_CARD_HANDLER_H