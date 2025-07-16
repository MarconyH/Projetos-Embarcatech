/* hw_config.c
Copyright 2021 Carl John Kugler III
... (licença)
*/

#include <string.h>
#include "my_debug.h"
#include "hw_config.h"
#include "ff.h" 
#include "diskio.h" 

// Configuração dos barramentos SPI
static spi_t spis[] = {  // Um para cada SPI.
    {
        .hw_inst = spi0,      // <--- MUDANÇA: Usando o periférico spi0
        .miso_gpio = 16,      // <--- MUDANÇA: Pino MISO padrão do spi0
        .mosi_gpio = 19,      // <--- MUDANÇA: Pino MOSI padrão do spi0
        .sck_gpio = 18,       // <--- MUDANÇA: Pino SCK padrão do spi0
        .baud_rate = 12500 * 1000
    }
};

// Configuração dos Cartões SD
static sd_card_t sd_cards[] = {  // Um para cada cartão SD
    {
        .pcName = "0:",           // Nome para montar o drive
        .spi = &spis[0],          // Ponteiro para a configuração SPI acima
        .ss_gpio = 17,            // <--- MUDANÇA: Pino CS padrão do spi0
        // O pino de detecção de cartão pode ser qualquer pino livre.
        // Se não estiver usando, pode ser desativado.
        .use_card_detect = false, // <--- MUDANÇA: Desativado para simplificar
        .card_detect_gpio = 22,   // Exemplo, pode ser qualquer pino livre se ativado
        .card_detected_true = 1   
    }
};

/* ********************************************************************** */
// Funções de acesso (sem alterações)
size_t sd_get_num() { return count_of(sd_cards); }
sd_card_t *sd_get_by_num(size_t num) {
    if (num < sd_get_num()) {
        return &sd_cards[num];
    } else {
        return NULL;
    }
}
size_t spi_get_num() { return count_of(spis); }
spi_t *spi_get_by_num(size_t num) {
    if (num < spi_get_num()) {
        return &spis[num];
    } else {
        return NULL;
    }
}
/* [] END OF FILE */