#ifndef TAG_DATA_HANDLER_H
#define TAG_DATA_HANDLER_H

#include "mfrc522.h"

// Estrutura de dados simplificada focada no contador de viagens
typedef union {
    struct {
        uint32_t student_id;
        char     student_name[9];
        uint8_t  trip_count;    // <-- ÚNICO CAMPO DE DADO VARIÁVEL
        uint8_t  data_version;
        uint8_t  checksum;
        uint8_t  padding[2]; // Bytes para completar 16. Não utilizados.
    } fields;
    uint8_t buffer[16];
} StudentDataBlock;

/**
 * @brief Prepara os dados de um novo aluno, INICIANDO o contador de viagens em 0.
 * @param data_block Ponteiro para o bloco de dados a ser preenchido.
 * @param id O ID do aluno.
 * @param name O nome curto do aluno.
 */
void Tdh_PrepareNewStudentTag(StudentDataBlock *data_block, uint32_t id, const char* name);

/**
 * @brief Escreve um bloco de dados na tag. Função genérica de escrita.
 */
StatusCode Tdh_WriteStudentData(MFRC522Ptr_t mfrc, StudentDataBlock *data_block, uint8_t blockAddr, MIFARE_Key *key);

/**
 * @brief Lê os dados de um aluno da tag e verifica a integridade.
 */
StatusCode Tdh_ReadStudentData(MFRC522Ptr_t mfrc, StudentDataBlock *data_block, uint8_t blockAddr, MIFARE_Key *key);

/**
 * @brief Função principal da operação: lê a tag, incrementa o contador e reescreve.
 * @return StatusCode indicando o resultado da operação completa.
 */
StatusCode Tdh_ProcessTrip(MFRC522Ptr_t mfrc, uint8_t blockAddr, MIFARE_Key *key, StudentDataBlock *data_read);

#endif // TAG_DATA_HANDLER_H