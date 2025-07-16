#include "tag_data_handler.h"
#include <string.h>
#include <stdio.h>

// Função interna para calcular o checksum. Inalterada.
static uint8_t calculate_checksum(const uint8_t *data) {
    uint8_t checksum = 0;
    for (int i = 0; i < 15; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

// Função atualizada para preparar uma NOVA tag, zerando o contador.
void Tdh_PrepareNewStudentTag(StudentDataBlock *data_block, uint32_t id, const char* name) {
    memset(data_block->buffer, 0, sizeof(data_block->buffer));

    data_block->fields.student_id = id;
    strncpy(data_block->fields.student_name, name, sizeof(data_block->fields.student_name) - 1);
    data_block->fields.student_name[sizeof(data_block->fields.student_name) - 1] = '\0';
    data_block->fields.trip_count = 0; // O contador sempre começa em 0!
    data_block->fields.data_version = 3; // Nova versão da estrutura

    data_block->fields.checksum = calculate_checksum(data_block->buffer);
}

// Função de escrita. Essencialmente a mesma de antes.
StatusCode Tdh_WriteStudentData(MFRC522Ptr_t mfrc, StudentDataBlock *data_block, uint8_t blockAddr, MIFARE_Key *key) {
    StatusCode status = PCD_Authenticate(mfrc, PICC_CMD_MF_AUTH_KEY_A, blockAddr, key, &(mfrc->uid));
    if (status != STATUS_OK) {
        printf("Erro de autenticacao na escrita: %s\n", GetStatusCodeName(status));
        return status;
    }

    status = MIFARE_Write(mfrc, blockAddr, data_block->buffer, 16);
    if (status != STATUS_OK) {
        printf("Erro ao gravar na tag: %s\n", GetStatusCodeName(status));
    }
    
    PCD_StopCrypto1(mfrc);
    return status;
}

// Função de leitura. Essencialmente a mesma de antes.
StatusCode Tdh_ReadStudentData(MFRC522Ptr_t mfrc, StudentDataBlock *data_block, uint8_t blockAddr, MIFARE_Key *key) {
    uint8_t read_buffer[18];
    uint8_t read_size = sizeof(read_buffer);

    StatusCode status = PCD_Authenticate(mfrc, PICC_CMD_MF_AUTH_KEY_A, blockAddr, key, &(mfrc->uid));
    if (status != STATUS_OK) {
        // Não imprime erro para não poluir a tela em loops de leitura
        return status;
    }
    
    status = MIFARE_Read(mfrc, blockAddr, read_buffer, &read_size);
    if (status != STATUS_OK) {
        PCD_StopCrypto1(mfrc);
        return status;
    }
    
    memcpy(data_block->buffer, read_buffer, 16);
    uint8_t expected_checksum = data_block->fields.checksum;
    uint8_t actual_checksum = calculate_checksum(data_block->buffer);
    PCD_StopCrypto1(mfrc);

    if (expected_checksum != actual_checksum) {
        return STATUS_CRC_WRONG;
    }

    return STATUS_OK;
}

// Nova função que encapsula a lógica de negócio do embarque.
StatusCode Tdh_ProcessTrip(MFRC522Ptr_t mfrc, uint8_t blockAddr, MIFARE_Key *key, StudentDataBlock *data_read) {
    // Passo 1: Inicia uma ÚNICA sessão de autenticação
    StatusCode status = PCD_Authenticate(mfrc, PICC_CMD_MF_AUTH_KEY_A, blockAddr, key, &(mfrc->uid));
    if (status != STATUS_OK) {
        printf("Incremento falhou: Autenticacao inicial falhou.\n");
        return status;
    }

    // A sessão está ativa. Agora vamos ler, modificar e escrever.
    
    // Passo 2: Lê os dados da tag
    uint8_t read_buffer[18];
    uint8_t read_size = sizeof(read_buffer);
    status = MIFARE_Read(mfrc, blockAddr, read_buffer, &read_size);
    if (status != STATUS_OK) {
        printf("Incremento falhou: Nao foi possivel ler a tag apos autenticar.\n");
        PCD_StopCrypto1(mfrc); // Importante parar a criptografia em caso de falha
        return status;
    }
    
    // Copia os dados lidos para a estrutura do usuário e verifica o checksum
    memcpy(data_read->buffer, read_buffer, 16);
    if (data_read->fields.checksum != calculate_checksum(data_read->buffer)) {
         printf("Incremento falhou: Checksum invalido na leitura.\n");
         PCD_StopCrypto1(mfrc);
         return STATUS_CRC_WRONG;
    }

    // Passo 3: Incrementa o contador e recalcula o checksum
    data_read->fields.trip_count++;
    data_read->fields.checksum = calculate_checksum(data_read->buffer);
    
    // Passo 4: Escreve os dados atualizados de volta na tag (ainda na mesma sessão)
    status = MIFARE_Write(mfrc, blockAddr, data_read->buffer, 16);
    if (status != STATUS_OK) {
        printf("Incremento falhou: Erro ao reescrever na tag.\n");
    }

    // Passo 5: Finaliza a sessão criptografada
    PCD_StopCrypto1(mfrc);
    
    return status;
}