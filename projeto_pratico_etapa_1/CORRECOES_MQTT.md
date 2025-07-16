# Correções MQTT - Resolução dos Problemas de Pool de Memória

## Problema Identificado

O erro `sys_timeout: timeout != NULL, pool MEMP_SYS_TIMEOUT is empty` indica que o sistema esgotou o pool de memória para timeouts, geralmente causado por:

1. **Tentativas prematuras de MQTT**: Envio de mensagens antes da conexão estar estabelecida
2. **Falta de sincronização**: Não aguardar callbacks de conexão
3. **Pool de memória insuficiente**: lwIP com poucos recursos disponíveis

## Correções Implementadas

### 🔧 **1. Verificação de Estado MQTT**

#### Adicionada variável global de estado:
```c
static volatile bool mqtt_conectado = false;
```

#### Nova função de verificação:
```c
bool mqtt_esta_conectado(void) {
    return mqtt_conectado && client && mqtt_client_is_connected(client);
}
```

### 🔧 **2. Aguardo de Conexão MQTT**

#### Timeout estruturado:
```c
// Aguarda até 20 segundos pela conexão MQTT
for (int mqtt_wait = 0; mqtt_wait < 20; mqtt_wait++) {
    sleep_ms(1000);
    if (mqtt_esta_conectado()) {
        printf("[CORE 1] MQTT conectado com sucesso!\n");
        break;
    }
    printf("[CORE 1] Aguardando MQTT... (%d/20)\n", mqtt_wait + 1);
}
```

### 🔧 **3. Callback Melhorado**

#### Callback com melhor feedback:
```c
void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("[MQTT] CONECTADO AO BROKER!\n");
        mqtt_conectado = true;
        publicar_mensagem_mqtt("Pico W online - Conexao estabelecida");
    } else {
        printf("[MQTT] FALHA NA CONEXAO - Status: %d\n", status);
        mqtt_conectado = false;
    }
}
```

### 🔧 **4. Loop de PING Robusto**

#### Verificações duplas antes do envio:
```c
// Verifica WiFi
bool conectado = (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP);

// Verifica MQTT
bool mqtt_ok = mqtt_esta_conectado();

// Só envia se ambos estiverem OK
if (conectado && mqtt_ok) {
    // Envia PING
}
```

### 🔧 **5. Recuperação de Erros**

#### Reconexão automática de MQTT:
```c
if (!mqtt_ok) {
    contador_erros_mqtt++;
    
    if (contador_erros_mqtt >= 3) {
        printf("[CORE 1] Muitos erros MQTT - reiniciando cliente...\n");
        contador_erros_mqtt = 0;
        sleep_ms(2000);
        iniciar_mqtt_cliente();
        sleep_ms(3000);
    }
}
```

### 🔧 **6. Intervalos Aumentados**

- **Ping MQTT**: 15 segundos (era 10s)
- **Aguardo de conexão**: 20 segundos máximo
- **Delays de estabilização**: 2-3 segundos entre operações

## Fluxo Corrigido

```
WiFi Connect → Stabilize (2s) → MQTT Init → Wait Connection (20s) → Verify Both → Send PING
     ↑                                                                           ↓
     ←------- Auto Reconnect ←------- Error Recovery ←------- Monitor Status ←---
```

## Log Esperado Após Correções

```
[CORE 1] SUCESSO: WiFi conectado!
[CORE 1] IP obtido: 192.168.0.21
[CORE 1] Aguardando estabilizacao da rede...
[CORE 1] Inicializando cliente MQTT...
[CORE 1] Aguardando conexao MQTT...
[CORE 1] Aguardando MQTT... (1/20)
[CORE 1] Aguardando MQTT... (2/20)
[MQTT] CONECTADO AO BROKER!
[CORE 1] MQTT conectado com sucesso!
[CORE 1] Cliente MQTT pronto para uso!
[CORE 1] === TESTE WIFI E MQTT CONCLUIDO ===
[CORE 1] Entrando em loop de monitoramento com PING MQTT...
[CORE 1] Status WiFi: CONECTADO
[CORE 1] Status MQTT: CONECTADO
[CORE 1] Preparando mensagem MQTT...
[CORE 1] Tentando enviar MQTT: PING #1 - Pico W funcionando!
[CORE 1] Aguardando 15 segundos ate proximo PING...
```

## Monitoramento MQTT

As mensagens devem aparecer no tópico "teste":

```bash
# Terminal com mosquitto_sub:
mosquitto_sub -h 192.168.0.20 -p 1883 -t teste

# Output esperado:
Pico W online - Conexao estabelecida
PING #1 - Pico W funcionando!
PING #2 - Pico W funcionando!
PING #3 - Pico W funcionando!
...
```

## Benefícios das Correções

1. **✅ Estabilidade**: Elimina erro de pool de memória
2. **✅ Robustez**: Verifica estados antes de enviar
3. **✅ Recuperação**: Reconecta automaticamente em falhas
4. **✅ Debug**: Logs detalhados para troubleshooting
5. **✅ Performance**: Intervalos otimizados para evitar sobrecarga

## Próximos Passos

1. **Teste o código corrigido** - deve funcionar sem "Hard assert"
2. **Verifique mensagens MQTT** - monitore o tópico "teste"
3. **Se funcionar perfeitamente**: Implementar baixo consumo com MQTT
4. **Reativar periféricos**: Gradualmente adicionar RFID, SD, etc.

O sistema agora deve enviar PINGs MQTT estáveis sem travamentos! 🚀
