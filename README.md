# Cadiz Crypto Dashboard

Projeto de painel financeiro para ESP32-S3 com display ST7789 240x320, ticker horizontal, cards de preço, gráficos e status de conectividade.

## Autor

Eduardo Cadiz

## Foco

Exibir dados em tempo real do mercado cripto com interface enxuta, responsiva e organizada em tarefas FreeRTOS separadas.

## Data

2026-05-17

## Responsabilidades dos arquivos

- `tft_final.ino`: ponto de entrada do firmware, inicialização das tasks e do mercado.
- `tft_display.cpp` / `tft_display.h`: renderização do dashboard, cards, gráficos, status e integração com o ticker.
- `ticker_manager.cpp` / `ticker_manager.h`: construção e animação da faixa horizontal de tickers.
- `binance_market.cpp` / `binance_market.h`: conexão WiFi/WebSocket, captura de dados e cálculo dos valores derivados.
- `market_data.h`: estrutura do snapshot compartilhado entre as tasks.
- `display_utils.h`: utilitários de cor e blend para o display.

## Dependências

- Arduino ESP32 core
- Adafruit GFX Library
- Adafruit ST7735 and ST7789 Library
- ArduinoJson
- WebSockets (Markus Sattler)

## Como usar

1. Abra o projeto no Arduino IDE ou VS Code com suporte ao ESP32.
2. Ajuste `WIFI_SSID` e `WIFI_PASS` em `tft_final.ino`.
3. Compile e envie para a placa ESP32-S3.
4. Abra o Serial Monitor em `115200` para acompanhar WiFi e WebSocket.

## Observações

- O dashboard usa FreeRTOS para separar mercado, ticker e renderização principal.
- O ticker e o dashboard compartilham o SPI por mutex para evitar corrupção de desenho.
- O projeto foi organizado para facilitar publicação no GitHub e manutenção futura.
