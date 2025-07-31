#ifndef CONFIGURA_GERAL_H
#define CONFIGURA_GERAL_H

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/cyw43_arch.h"
#include "pico/time.h"
#include <stdint.h>
#include <stdbool.h>
#include "pico/mutex.h"

//LED RGB
#define LED_R 12
#define LED_G 11
#define LED_B 13
#define PWM_STEP 0xFFFF
//#define PWM_STEP (1 << 8)

//Pinos I2C
#define SDA_PIN 14
#define SCL_PIN 15

#define TEMPO_CONEXAO 2000
#define TEMPO_MENSAGEM 1000
#define TAM_FILA 16
#define INTERVALO_PING_MS 500

// Configurações de temporização WiFi
#define WIFI_INTERVALO_TESTE_MS 10000        // 10 segundos para teste
#define WIFI_INTERVALO_CONEXAO_MS 60000      // 60 segundos para produção
#define WIFI_TEMPO_CONEXAO_ATIVA_MS 30000    // 30 segundos ativo após conectar

#define WIFI_SSID "Internet" 
#define WIFI_PASS "12345678"
#define MQTT_BROKER_IP "192.168.202.58"
#define MQTT_BROKER_PORT 1883
#define TOPICO "teste"


// Buffers globais para OLED
extern uint8_t buffer_oled[];
extern struct render_area area;

void setup_init_oled(void);
void exibir_e_esperar(const char *mensagem, int linha_y);

#endif