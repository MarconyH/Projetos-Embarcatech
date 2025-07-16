/**
 * @file display.c
 * @brief Implementação de função utilitária para exibição temporária de mensagens em display OLED.
 *
 * Este módulo provê uma função de apoio para exibir mensagens curtas no display OLED controlado via I²C,
 * utilizando o driver SSD1306. Ele é utilizado para mostrar instruções ou estados durante a execução de tarefas,
 * como conexão Wi-Fi, falhas ou confirmações.
 *
 * A função exibe o texto fornecido por um curto período e, em seguida, limpa a tela automaticamente.
 *
 * Dependências:
 * - `wifi_status.h`: fornece os buffers globais (`buffer_oled`, `area`) utilizados na renderização.
 * - Funções gráficas do driver SSD1306 para manipulação do display.
 */

#include "configura_geral.h"  // Acesso aos buffers OLED globais
#include "oled_utils.h"
#include "ssd1306_i2c.h"
#include "ssd1306.h"
#include "display.h"
#include "pico/time.h"

static repeating_timer_t active_timer;
/**
 * @brief Exibe uma mensagem na tela OLED por 2 segundos e em seguida limpa a tela.
 *
 * @param mensagem  Texto UTF-8 a ser exibido (pode conter múltiplas linhas).
 * @param linha_y   Posição vertical (em pixels) onde a mensagem começará a ser desenhada.
 *
 * Funcionalidade:
 * - Limpa o conteúdo atual do display.
 * - Desenha o texto informado na posição especificada.
 * - Aguarda por 2000 milissegundos.
 * - Limpa novamente o display.
 *
 * Utilizada para feedback visual durante eventos como: inicialização, conexão Wi-Fi, reconexão ou erros.
 */
void exibir_e_esperar(const char *mensagem, int linha_y) {
    // Limpa completamente o buffer gráfico do display
    oled_clear(buffer_oled, &area);

    // Escreve o texto em formato multilinha a partir da coordenada vertical fornecida
    ssd1306_draw_utf8_multiline(buffer_oled, 0, linha_y, mensagem);

    // Envia o buffer para ser renderizado fisicamente no display
    render_on_display(buffer_oled, &area);

    // Espera 2 segundos para permitir leitura da mensagem
    sleep_ms(TEMPO_MENSAGEM);

    // Limpa novamente o conteúdo
    oled_clear(buffer_oled, &area);
    render_on_display(buffer_oled, &area);
}

static bool clear_display_callback(repeating_timer_t *rt) {
    printf("[TIMER_CALLBACK] Limpando o display OLED.\n");

    // Limpa o conteúdo do buffer e envia para o display
    oled_clear(buffer_oled, &area);
    render_on_display(buffer_oled, &area);

    // Retornar 'false' efetivamente cancela o timer, fazendo-o atuar como "one-shot".
    return false;
}

void display_message_async(const char *message, int line_y) {
    // 1. Cancela qualquer timer de limpeza que já esteja agendado.
    // Isso evita que uma mensagem antiga apague uma nova.
    cancel_repeating_timer(&active_timer);

    // 2. Limpa, desenha e renderiza a nova mensagem imediatamente.
    oled_clear(buffer_oled, &area);
    ssd1306_draw_utf8_multiline(buffer_oled, 0, line_y, message);
    render_on_display(buffer_oled, &area);

    // 3. Agenda a função 'clear_display_callback' para ser chamada após TEMPO_MENSAGEM.
    // Usar um valor de delay negativo é a forma recomendada pela API do Pico SDK
    // para criar um timer "one-shot" (que dispara apenas uma vez).
    add_repeating_timer_ms(-TEMPO_MENSAGEM, clear_display_callback, NULL, &active_timer);
}
