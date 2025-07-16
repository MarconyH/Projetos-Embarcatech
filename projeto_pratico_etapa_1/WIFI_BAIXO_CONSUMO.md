# Sistema de WiFi com Baixo Consumo de Energia

## Visão Geral

Este documento descreve o sistema implementado para economizar energia maximizando o tempo que o WiFi permanece desligado, conectando-se apenas periodicamente para enviar dados.

## Funcionamento

### Modo de Operação
- **WiFi Inicializado pelo Core 1**: Evita conflitos com GPIO no Core 0
- **WiFi em Sleep**: O chip CYW43 permanece completamente desligado a maior parte do tempo
- **Conexões Periódicas**: WiFi se conecta automaticamente em intervalos configuráveis
- **Envio de Dados**: Durante o período conectado, todos os dados acumulados são enviados
- **Retorno ao Sleep**: Após enviar os dados, desliga completamente o WiFi

### Configurações de Tempo

#### Modo de Teste (atual)
```c
#define WIFI_INTERVALO_TESTE_MS (30 * 1000)         // 30 segundos
#define WIFI_TEMPO_CONEXAO_ATIVA_MS (60 * 1000)     // 1 minuto conectado
```

#### Modo de Produção
```c
#define WIFI_INTERVALO_CONEXAO_MS (10 * 60 * 1000)  // 10 minutos
#define WIFI_TEMPO_CONEXAO_ATIVA_MS (60 * 1000)     // 1 minuto conectado
```

Para alterar para o modo de produção, modifique a linha 58 em `conexao.c`:
```c
// Altere de:
uint32_t intervalo = WIFI_INTERVALO_TESTE_MS;
// Para:
uint32_t intervalo = WIFI_INTERVALO_CONEXAO_MS;
```

## Arquitetura do Sistema

### Core 0 (Principal)
- Gerencia RFID, SD Card e interface do usuário
- Inicializa botões GPIO **ANTES** de qualquer operação WiFi
- **NÃO inicializa WiFi** - evita conflitos com interrupções GPIO
- Opera de forma totalmente independente do WiFi

### Core 1 (WiFi Manager)
- **Inicializa completamente o WiFi** após Core 0 estar estável
- Gerencia exclusivamente as operações de WiFi
- Implementa o ciclo de liga/desliga completo do WiFi
- Controla as conexões periódicas com proteção mutex
- Envia dados acumulados quando conectado

## Fluxo de Operação

### Inicialização
1. **Core 0**: Inicializa RFID, SD Card e botões GPIO
2. **Core 0**: **NÃO inicializa WiFi** - evita conflitos
3. **Core 0**: Lança Core 1 após sistema estar completamente estável
4. **Core 1**: Aguarda 5 segundos para garantir estabilidade
5. **Core 1**: Inicializa WiFi completamente
6. **Core 1**: Imediatamente desliga WiFi para economia máxima

### Ciclo de Conexão
1. **Verificação de Timer**: Core 1 verifica se é hora de conectar
2. **Re-inicialização**: Liga completamente o chip CYW43
3. **Conexão**: Tenta conectar à rede WiFi
4. **Envio de Dados**: Permanece conectado por 1 minuto para enviar dados
5. **Desligamento**: Desliga completamente o CYW43 (cyw43_arch_deinit)
6. **Repeat**: Reinicia o ciclo

## Economia de Energia

### Estimativas de Consumo
- **WiFi Ativo**: ~80-120mA
- **WiFi Completamente Desligado**: ~0-5mA
- **Economia**: 95-98% de redução no consumo do WiFi

### Configuração Otimizada
- **95% do tempo**: WiFi completamente desligado (economia máxima)
- **5% do tempo**: WiFi ativo para comunicação
- **Resultado**: Economia de energia de ~95% comparado ao WiFi sempre ativo

## Funções Principais

### Controle de Energia
```c
void wifi_entrar_modo_sleep(void);          // Coloca WiFi em sleep
void wifi_sair_modo_sleep(void);            // Acorda WiFi do sleep
void wifi_conectar_periodico(void);         // Ciclo completo de conexão
bool wifi_deve_conectar_agora(void);        // Verifica timer
```

### SPI Manager
```c
void spi_manager_shutdown_wifi_power_save(void);  // Desligamento completo
void spi_manager_wakeup_wifi_power_save(void);    // Re-inicialização completa
```

## Logs de Debug

O sistema produz logs detalhados para monitoramento:

```
[CORE 0] Botoes inicializados com sucesso!
[CORE 0] WiFi sera inicializado completamente pelo Core 1.
[CORE 1] Aguardando estabilizacao do sistema...
[CORE 1] Inicializando WiFi completamente no Core 1...
[CORE 1] WiFi inicializado com sucesso no Core 1.
[CORE 1] Colocando WiFi em modo de economia de energia...
[SPI_MANAGER] Desligando completamente o subsistema CYW43...
[CORE 1] WiFi em modo de baixo consumo.
[CORE 1] Conexoes automaticas a cada 30 segundos (modo teste).
```

## Integração com MQTT

Durante o período de conexão ativa (1 minuto), o sistema pode:
- Enviar dados de embarque acumulados
- Receber comandos remotos
- Sincronizar horário
- Atualizar configurações

## Benefícios

1. **Economia de Energia Máxima**: Redução de 95% no consumo do WiFi
2. **Vida Útil da Bateria**: Significativamente estendida 
3. **Operação Confiável**: Dados locais não dependem de conectividade
4. **Estabilidade**: Evita conflitos entre WiFi e GPIO no Core 0
5. **Flexibilidade**: Intervalos facilmente configuráveis
6. **Robustez**: Sistema continua funcionando mesmo com falhas de WiFi
7. **Isolamento de Cores**: Cada core tem responsabilidades bem definidas

## Manutenção

### Ajustar Intervalos
Modifique as constantes no início de `conexao.c` conforme necessário.

### Monitoramento
Use os logs do sistema para verificar o funcionamento correto e ajustar tempos se necessário.

### Troubleshooting
Se o WiFi não conectar, o sistema automaticamente volta ao sleep e tenta novamente no próximo ciclo.
