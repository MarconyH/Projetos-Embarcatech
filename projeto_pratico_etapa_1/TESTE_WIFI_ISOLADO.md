# Teste WiFi + MQTT Isolado - Ping Messages

## Objetivo

Testar WiFi isoladamente e implementar envio de mensagens MQTT de ping para o tópico configurado, validando conectividade completa.

## Modificações Realizadas

### ✅ Funcionalidade MQTT Adicionada

#### Core 1 - `conexao.c`
- ✅ **Inicialização MQTT**: Após conexão WiFi bem-sucedida
- ✅ **Envio de PING**: Mensagens periódicas a cada 10 segundos
- ✅ **Monitoramento**: Status da conexão e reconexão automática
- ✅ **Contador**: PING numerado para tracking

#### Configurações Utilizadas
```c
// De configura_geral.h:
#define WIFI_SSID "Starlink" 
#define WIFI_PASS "19999999"
#define MQTT_BROKER_IP "192.168.0.20"
#define MQTT_BROKER_PORT 1883
#define TOPICO "teste"
```

### ✅ Código Comentado (Desabilitado)

#### Core 0 - `projeto_pratico_etapa_1.c`
- ❌ **OLED/Display**: Todas as funções de display comentadas
- ❌ **RFID (MFRC522)**: Inicialização e operações comentadas  
- ❌ **SD Card**: Inicialização e acesso comentado
- ❌ **Botões GPIO**: Configuração e callbacks comentados
- ❌ **Estados da aplicação**: Lógica de menu comentada
- ❌ **SPI Manager**: Chamadas de ativação comentadas

#### Includes Desabilitados
```c
// COMENTADOS:
#include "ff.h" 
#include "f_util.h"
#include "oled_utils.h"
#include "ssd1306_i2c.h"
#include "setup_oled.h"
#include "display.h"
#include "inc/rfid/mfrc522.h"
#include "inc/rfid/tag_data_handler.h"
#include "inc/sd_card/sd_card_handler.h" 
#include "inc/spi_manager.h"
#include "hw_config.h"
```

### ✅ Código Ativo (Funcionando)

#### Core 0 - Funcionalidade Mínima
```c
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/sync.h"
#include "conexao.h"

// Apenas:
- stdio_init_all()
- mutex_init()
- inicia_core1()
- Loop simples com sleep
```

#### Core 1 - Teste WiFi + MQTT
```c
// Teste completo:
1. Aguarda 3 segundos
2. cyw43_arch_init() - Inicialização básica
3. cyw43_arch_enable_sta_mode() - Modo station
4. Tenta conectar WiFi (3 tentativas)
5. iniciar_mqtt_cliente() - Conecta ao broker
6. Loop MQTT com PING a cada 10 segundos
```

## Fluxo de Teste Esperado

```
[CORE 0] Iniciando - MODO TESTE WIFI APENAS...
[CORE 0] MODO TESTE: Apenas WiFi sera inicializado.
[CORE 0] Todos os outros perifericos estao comentados.
[CORE 0] Prestes a lancar o Nucleo 1...
[CORE 0] Nucleo 1 lancado com sucesso!
[CORE 0] Entrando em loop simples para teste WiFi...

[CORE 1] === TESTE WIFI ISOLADO ===
[CORE 1] Funcao WiFi iniciada no nucleo 1!
[CORE 1] Aguardando estabilizacao do sistema...
[CORE 1] === INICIANDO TESTE BASICO DO WiFi ===
[CORE 1] PASSO 1: Inicializando CYW43 basico...
[CORE 1] SUCESSO: CYW43 inicializado!
[CORE 1] PASSO 2: Ativando modo station...
[CORE 1] SUCESSO: Modo station ativado!
[CORE 1] PASSO 3: Testando conexao WiFi...
[CORE 1] Tentativa 1 de conexao...
[CORE 1] SUCESSO: WiFi conectado!
[CORE 1] IP obtido: 192.168.x.x
[CORE 1] Aguardando estabilizacao da rede...
[CORE 1] Inicializando cliente MQTT...
[CORE 1] Cliente MQTT inicializado!
[CORE 1] === TESTE WIFI E MQTT CONCLUIDO ===
[CORE 1] Entrando em loop de monitoramento com PING MQTT...
[CORE 1] Status WiFi: CONECTADO
[CORE 1] Enviando MQTT: PING #1 - Pico W funcionando!
[CORE 1] Status WiFi: CONECTADO
[CORE 1] Enviando MQTT: PING #2 - Pico W funcionando!
```

## Resultados Possíveis

### ✅ Se o WiFi + MQTT Funcionarem
- **Conclusão**: Sistema de comunicação WiFi/MQTT está 100% operacional
- **Resultado**: Mensagens de PING chegando no broker MQTT no tópico "teste"
- **Próximo passo**: Reativar periféricos gradualmente mantendo MQTT

### ❌ Se o MQTT Falhar (WiFi OK)
- **Conclusão**: Problema de conectividade com broker ou configuração MQTT
- **Verificar**: IP do broker (192.168.0.20), porta (1883), tópico ("teste")
- **Próximo passo**: Debug das configurações de rede

### 🔄 Se o WiFi Funcionar Mas MQTT Não Conectar
- **Conclusão**: Broker MQTT pode estar offline ou firewall bloqueando
- **Verificar**: Se o broker está rodando na rede e acessível
- **Próximo passo**: Testar conectividade manual ao broker

## Configurações de Teste

### WiFi Settings
```c
#define WIFI_SSID "Starlink" 
#define WIFI_PASS "19999999"
```

### MQTT Settings
```c
#define MQTT_BROKER_IP "192.168.0.20"
#define MQTT_BROKER_PORT 1883
#define TOPICO "teste"
```

### Mensagens MQTT
```c
// Formato das mensagens:
"PING #1 - Pico W funcionando!"
"PING #2 - Pico W funcionando!"
// ... incrementa a cada 10 segundos
```

### Timeout de Conexão
```c
cyw43_arch_wifi_connect_timeout_ms(
    WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000);
```

## Debug Information

### Códigos de Erro cyw43_arch_init()
- `0`: Sucesso
- `-1`: Erro genérico de inicialização
- `-2`: Erro de hardware
- `-3`: Erro de firmware

### Status de Link
- `CYW43_LINK_UP`: Conectado
- `CYW43_LINK_DOWN`: Desconectado

## Próximos Passos

1. **Teste Atual**: Executar código com WiFi + MQTT
2. **Verificar MQTT**: Monitorar tópico "teste" no broker 192.168.0.20
3. **Se Tudo Funcionar**: 
   - Implementar sistema de baixo consumo com MQTT
   - Reativar periféricos gradualmente:
     - Primeiro: Botões GPIO (sem conflito com WiFi)
     - Segundo: OLED (I2C separado)
     - Terceiro: RFID (com chaveamento SPI)
     - Quarto: SD Card (com chaveamento SPI)
4. **Se MQTT Falhar**: Debug configurações de rede

## Monitoramento MQTT

Para verificar se as mensagens estão chegando, use um cliente MQTT como:

### mosquitto_sub (Linux/Mac)
```bash
mosquitto_sub -h 192.168.0.20 -p 1883 -t teste
```

### MQTT Explorer (Windows/GUI)
- Host: 192.168.0.20
- Port: 1883
- Subscribe ao tópico: teste

### Mensagens Esperadas
```
PING #1 - Pico W funcionando!
PING #2 - Pico W funcionando!
PING #3 - Pico W funcionando!
...
```

A cada 10 segundos uma nova mensagem deve aparecer.

## Comandos para Voltar ao Estado Original

Quando terminar o teste, descomente:
- Includes no topo do arquivo
- Variáveis globais
- Funções de periféricos
- Loop principal da aplicação

---

**IMPORTANTE**: Este é um teste temporário. Após identificar o problema, todo o código deve ser descomentado para restaurar a funcionalidade completa.
