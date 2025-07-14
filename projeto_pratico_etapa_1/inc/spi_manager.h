#ifndef SPI_MANAGER_H
#define SPI_MANAGER_H

// Ativa e configura o barramento SPI0 para o Leitor RFID (pinos 0, 1, 2, 3)
void spi_manager_activate_rfid(void);

// Ativa e configura o barramento SPI0 para o Cartão SD (pinos 16, 17, 18, 19)
void spi_manager_activate_sd(void);

#endif