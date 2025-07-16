/**
 * @file conexao.h
 * @brief Interface do módulo Wi-Fi no núcleo 1.
 */

#ifndef CONEXAO_H
#define CONEXAO_H

#include "../configura_geral.h"

void conectar_wifi(void);
void monitorar_conexao_e_reconectar(void);
bool wifi_esta_conectado(void);
void enviar_status_para_core0(uint16_t status, uint16_t tentativa);
void enviar_ip_para_core0(uint8_t *ip);
bool enviar_dados_rfid_mqtt(void);
bool carregar_dados_rfid_para_buffer(void);

// Novas funções para modo de baixo consumo
void wifi_entrar_modo_sleep(void);
void wifi_sair_modo_sleep(void);
void wifi_conectar_periodico(void);
bool wifi_deve_conectar_agora(void);

#endif
