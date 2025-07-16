// Arquivo: spi_manager.c (VERSÃO MODIFICADA E MAIS ROBUSTA)

#include "spi_manager.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/cyw43_arch.h"

// Incluímos os cabeçalhos para obter as definições dos pinos de ambos os módulos
#include "rfid/mfrc522.h"
#include "hw_config.h"

// Estado atual do sistema de periféricos
typedef enum {
    PERIPHERAL_NONE,
    PERIPHERAL_RFID,
    PERIPHERAL_SD,
    PERIPHERAL_WIFI
} peripheral_state_t;

static peripheral_state_t current_peripheral = PERIPHERAL_NONE;

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

// Função para desativar completamente todos os periféricos
void spi_manager_deactivate_all(void) {
    printf("[SPI_MANAGER] Desativando todos os perifericos...\n");
    
    // Desativa WiFi se estiver ativo
    if (current_peripheral == PERIPHERAL_WIFI) {
        wifi_deactivate();
    }
    
    // Desativa SPI0 se estiver sendo usado
    if (current_peripheral == PERIPHERAL_RFID || current_peripheral == PERIPHERAL_SD) {
        spi_deinit(spi0);
    }
    
    // Desativa todos os pinos RFID
    deactivate_pin(sck_pin);
    deactivate_pin(mosi_pin);
    deactivate_pin(miso_pin);
    deactivate_pin(cs_pin);
    
    // Desativa todos os pinos SD Card
    sd_card_t *pSD = sd_get_by_num(0);
    if (pSD && pSD->spi) {
        spi_t *pSPI = pSD->spi;
        deactivate_pin(pSPI->sck_gpio);
        deactivate_pin(pSPI->mosi_gpio);
        deactivate_pin(pSPI->miso_gpio);
        deactivate_pin(pSD->ss_gpio);
    }
    
    current_peripheral = PERIPHERAL_NONE;
    printf("[SPI_MANAGER] Todos os perifericos desativados.\n");
}

// Função para desativar o WiFi no Raspberry Pi Pico W
void wifi_deactivate() {
    if (current_peripheral == PERIPHERAL_WIFI) {
        printf("[SPI_MANAGER] Desativando WiFi...\n");
        cyw43_arch_disable_sta_mode();
        cyw43_arch_deinit();
        printf("[SPI_MANAGER] WiFi desativado.\n");
    }
}

// Função para ativar o WiFi no Raspberry Pi Pico W
void wifi_activate() {
    printf("[SPI_MANAGER] Inicializando subsistema CYW43...\n");
    
    // Inicializa o subsistema CYW43 (WiFi)
    if (cyw43_arch_init() != 0) {
        printf("[SPI_MANAGER] ERRO: Falha ao inicializar CYW43!\n");
        return;
    }
    
    printf("[SPI_MANAGER] CYW43 inicializado com sucesso.\n");
    
    // Ativa o modo station (WiFi) usando a SDK padrão
    cyw43_arch_enable_sta_mode();
    
    printf("[SPI_MANAGER] WiFi modo station ativado.\n");
}

void spi_manager_activate_rfid() {
    if (current_peripheral == PERIPHERAL_RFID) {
        printf("[SPI_MANAGER] RFID ja esta ativo.\n");
        return;
    }
    
    printf("[SPI_MANAGER] Ativando RFID...\n");
    
    // Desativa tudo primeiro
    spi_manager_deactivate_all();
    
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
    
    current_peripheral = PERIPHERAL_RFID;
    printf("[SPI_MANAGER] RFID ativado com sucesso.\n");
}

void spi_manager_activate_sd() {
    if (current_peripheral == PERIPHERAL_SD) {
        printf("[SPI_MANAGER] SD Card ja esta ativo.\n");
        return;
    }
    
    printf("[SPI_MANAGER] Ativando SD Card...\n");
    
    // Desativa tudo primeiro
    spi_manager_deactivate_all();
    
    // Pega a configuração do SD
    sd_card_t *pSD = sd_get_by_num(0);
    if (!pSD || !pSD->spi) {
        printf("[SPI_MANAGER] ERRO: Configuracao do SD Card nao encontrada!\n");
        return;
    }
    
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
    
    current_peripheral = PERIPHERAL_SD;
    printf("[SPI_MANAGER] SD Card ativado com sucesso.\n");
}

void spi_manager_activate_wifi() {
    if (current_peripheral == PERIPHERAL_WIFI) {
        printf("[SPI_MANAGER] WiFi ja esta ativo.\n");
        return;
    }
    
    printf("[SPI_MANAGER] Ativando WiFi...\n");
    
    // Apenas desativa SPI0 e pinos se necessário
    if (current_peripheral == PERIPHERAL_RFID || current_peripheral == PERIPHERAL_SD) {
        printf("[SPI_MANAGER] Desativando SPI0 para WiFi...\n");
        spi_deinit(spi0);
        
        // Desativa todos os pinos RFID
        deactivate_pin(sck_pin);
        deactivate_pin(mosi_pin);
        deactivate_pin(miso_pin);
        deactivate_pin(cs_pin);
        
        // Desativa todos os pinos SD Card
        sd_card_t *pSD = sd_get_by_num(0);
        if (pSD && pSD->spi) {
            spi_t *pSPI = pSD->spi;
            deactivate_pin(pSPI->sck_gpio);
            deactivate_pin(pSPI->mosi_gpio);
            deactivate_pin(pSPI->miso_gpio);
            deactivate_pin(pSD->ss_gpio);
        }
    }
    
    // Inicializa o WiFi apenas se ainda não foi inicializado
    static bool wifi_initialized = false;
    if (!wifi_initialized) {
        printf("[SPI_MANAGER] Primeira inicializacao do WiFi...\n");
        wifi_activate();
        wifi_initialized = true;
    } else {
        printf("[SPI_MANAGER] WiFi ja inicializado, apenas marcando como ativo...\n");
    }
    
    current_peripheral = PERIPHERAL_WIFI;
    printf("[SPI_MANAGER] WiFi ativado com sucesso.\n");
}

// Funções de desativação específicas
void spi_manager_deactivate_rfid(void) {
    if (current_peripheral == PERIPHERAL_RFID) {
        printf("[SPI_MANAGER] Desativando RFID...\n");
        spi_deinit(spi0);
        deactivate_pin(sck_pin);
        deactivate_pin(mosi_pin);
        deactivate_pin(miso_pin);
        deactivate_pin(cs_pin);
        current_peripheral = PERIPHERAL_NONE;
        printf("[SPI_MANAGER] RFID desativado.\n");
    }
}

void spi_manager_deactivate_sd(void) {
    if (current_peripheral == PERIPHERAL_SD) {
        printf("[SPI_MANAGER] Desativando SD Card...\n");
        spi_deinit(spi0);
        sd_card_t *pSD = sd_get_by_num(0);
        if (pSD && pSD->spi) {
            spi_t *pSPI = pSD->spi;
            deactivate_pin(pSPI->sck_gpio);
            deactivate_pin(pSPI->mosi_gpio);
            deactivate_pin(pSPI->miso_gpio);
            deactivate_pin(pSD->ss_gpio);
        }
        current_peripheral = PERIPHERAL_NONE;
        printf("[SPI_MANAGER] SD Card desativado.\n");
    }
}

void spi_manager_deactivate_wifi(void) {
    if (current_peripheral == PERIPHERAL_WIFI) {
        wifi_deactivate();
        current_peripheral = PERIPHERAL_NONE;
    }
}

// Nova função para desligar WiFi completamente (economia de energia máxima)
void spi_manager_shutdown_wifi_power_save(void) {
    printf("[SPI_MANAGER] Desligando WiFi para economia de energia maxima...\n");
    
    if (current_peripheral == PERIPHERAL_WIFI) {
        // Desconecta da rede WiFi se conectado
        if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP) {
            printf("[SPI_MANAGER] Desconectando da rede WiFi...\n");
            cyw43_arch_disable_sta_mode();
        }
        
        // Desliga o LED WiFi
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        
        // SOLUÇÃO MAIS AGRESSIVA: Desliga completamente o CYW43
        printf("[SPI_MANAGER] Desligando completamente o subsistema CYW43...\n");
        cyw43_arch_deinit();
        
        current_peripheral = PERIPHERAL_NONE;
        printf("[SPI_MANAGER] WiFi completamente desligado para economia maxima.\n");
    }
}

// Nova função para reativar WiFi do modo de economia de energia
void spi_manager_wakeup_wifi_power_save(void) {
    printf("[SPI_MANAGER] Reativando WiFi do modo de desligamento completo...\n");
    
    // Desativa outros periféricos primeiro
    if (current_peripheral == PERIPHERAL_RFID || current_peripheral == PERIPHERAL_SD) {
        spi_manager_deactivate_all();
    }
    
    // Re-inicializa completamente o CYW43
    printf("[SPI_MANAGER] Re-inicializando subsistema CYW43...\n");
    int result = cyw43_arch_init();
    if (result != 0) {
        printf("[SPI_MANAGER] ERRO: Falha ao re-inicializar CYW43! Codigo: %d\n", result);
        return;
    }
    
    printf("[SPI_MANAGER] CYW43 re-inicializado com sucesso.\n");
    cyw43_arch_enable_sta_mode();
    
    current_peripheral = PERIPHERAL_WIFI;
    printf("[SPI_MANAGER] WiFi completamente reativado.\n");
}

// Função especial para reativar o WiFi sem re-inicializar o CYW43
void spi_manager_reactivate_wifi_for_core1(void) {
    if (current_peripheral != PERIPHERAL_WIFI) {
        printf("[SPI_MANAGER] Reativando WiFi para Core 1...\n");
        
        // Desativa apenas SPI0 e pinos, mas não mexe no CYW43
        if (current_peripheral == PERIPHERAL_RFID || current_peripheral == PERIPHERAL_SD) {
            spi_deinit(spi0);
        }
        
        // Desativa todos os pinos SPI
        deactivate_pin(sck_pin);
        deactivate_pin(mosi_pin);
        deactivate_pin(miso_pin);
        deactivate_pin(cs_pin);
        
        sd_card_t *pSD = sd_get_by_num(0);
        if (pSD && pSD->spi) {
            spi_t *pSPI = pSD->spi;
            deactivate_pin(pSPI->sck_gpio);
            deactivate_pin(pSPI->mosi_gpio);
            deactivate_pin(pSPI->miso_gpio);
            deactivate_pin(pSD->ss_gpio);
        }
        
        current_peripheral = PERIPHERAL_WIFI;
        printf("[SPI_MANAGER] WiFi reativado para Core 1.\n");
    }
}