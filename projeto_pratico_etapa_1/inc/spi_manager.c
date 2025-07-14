// Arquivo: spi_manager.c (VERSÃO MODIFICADA E MAIS ROBUSTA)

#include "spi_manager.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

// Incluímos os cabeçalhos para obter as definições dos pinos de ambos os módulos
#include "mfrc522.h"
#include "hw_config.h"

// Função auxiliar para desativar um pino, configurando-o como entrada.
// Isso garante que ele não irá interferir no barramento (alta impedância).
static void deactivate_pin(uint pin) {
    // Configura o pino como uma função de I/O padrão (não SPI)
    gpio_set_function(pin, GPIO_FUNC_SIO);
    // Configura como entrada para que não dirija o barramento
    gpio_set_dir(pin, GPIO_IN);
    // Desativa pull-up/pull-down para não influenciar o nível do pino
    gpio_disable_pulls(pin);
}

void spi_manager_activate_rfid() {
    // Desliga o periférico SPI0 completamente
    spi_deinit(spi0);

    // --- PASSO DE LIMPEZA ---
    // Pega a configuração do SD para saber quais pinos desativar.
    sd_card_t *pSD = sd_get_by_num(0);
    spi_t *pSPI = pSD->spi;
    // Desativa explicitamente todos os pinos usados pelo Cartão SD.
    deactivate_pin(pSPI->sck_gpio);
    deactivate_pin(pSPI->mosi_gpio);
    deactivate_pin(pSPI->miso_gpio);
    deactivate_pin(pSD->ss_gpio); // Desativa também o Chip Select do SD

    // --- REATIVAÇÃO PARA O RFID ---
    // Inicializa o SPI0 com a velocidade do RFID
    spi_init(spi0, 4000000); // 4 MHz

    // Mapeia as funções do SPI0 para os pinos do RFID
    gpio_set_function(sck_pin, GPIO_FUNC_SPI);
    gpio_set_function(mosi_pin, GPIO_FUNC_SPI);
    gpio_set_function(miso_pin, GPIO_FUNC_SPI);
    
    // O pino CS do RFID é um GPIO normal que a biblioteca MFRC522 controlará
    gpio_init(cs_pin);
    gpio_set_dir(cs_pin, GPIO_OUT);
    gpio_put(cs_pin, 1); // Garante que comece desativado (nível alto)
}

void spi_manager_activate_sd() {
    // Desliga o periférico SPI0 completamente
    spi_deinit(spi0);

    // --- PASSO DE LIMPEZA ---
    // Desativa explicitamente todos os pinos usados pelo RFID.
    deactivate_pin(sck_pin);
    deactivate_pin(mosi_pin);
    deactivate_pin(miso_pin);
    deactivate_pin(cs_pin); // Desativa também o Chip Select do RFID

    // --- REATIVAÇÃO PARA O CARTÃO SD ---
    sd_card_t *pSD = sd_get_by_num(0);
    spi_t *pSPI = pSD->spi;

    // Inicializa o SPI0 com a velocidade do Cartão SD
    spi_init(spi0, pSPI->baud_rate);

    // Mapeia as funções do SPI0 para os pinos do Cartão SD
    gpio_set_function(pSPI->sck_gpio, GPIO_FUNC_SPI);
    gpio_set_function(pSPI->mosi_gpio, GPIO_FUNC_SPI);
    gpio_set_function(pSPI->miso_gpio, GPIO_FUNC_SPI);
    
    // O pino CS do SD é um GPIO normal que a biblioteca FatFs/diskio controlará
    gpio_init(pSD->ss_gpio);
    gpio_set_dir(pSD->ss_gpio, GPIO_OUT);
    gpio_put(pSD->ss_gpio, 1); // Garante que comece desativado (nível alto)
}