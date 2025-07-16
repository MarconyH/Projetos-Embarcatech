#ifndef SPI_MANAGER_H
#define SPI_MANAGER_H

// Ativa e configura o barramento SPI0 para o Leitor RFID (pinos 0, 1, 2, 3)
void spi_manager_activate_rfid(void);

// Ativa e configura o barramento SPI0 para o Cartão SD (pinos 16, 17, 18, 19)
void spi_manager_activate_sd(void);

// Desativa o SPI0 e ativa o WiFi (CYW43439 interno)
void spi_manager_activate_wifi(void);

// Funções de desativação explícita
void spi_manager_deactivate_rfid(void);
void spi_manager_deactivate_sd(void);
void spi_manager_deactivate_wifi(void);

// Função para desativar tudo
void spi_manager_deactivate_all(void);

// Função especial para reativar WiFi sem re-inicializar CYW43
void spi_manager_reactivate_wifi_for_core1(void);

// Novas funções para economia de energia máxima do WiFi
void spi_manager_shutdown_wifi_power_save(void);
void spi_manager_wakeup_wifi_power_save(void);

// Funções auxiliares de WiFi
void wifi_activate(void);
void wifi_deactivate(void);

#endif