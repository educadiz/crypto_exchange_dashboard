/*
 * Implementação do módulo de display TFT.
 * Contém a inicialização do Adafruit_ST7789 e todas as cenas.
 */

#include "tft_display.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// PINAGEM (mesma do sketch original)
#define PIN_SCK   3
#define PIN_MOSI 45
#define PIN_CS   14
#define PIN_DC   47
#define PIN_RST  21

#define LARGURA  240
#define ALTURA   320

// Ponteiro para objeto Adafruit (interna ao módulo)
static Adafruit_ST7789* tft = nullptr;

// Estado do slideshow
static int cena_atual = 0;
static const int TOTAL_CENAS = 6;
static unsigned long ultimo_tempo = 0;
static unsigned long DURACAO = 3000UL;

// Helper para converter RGB -> RGB565
static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint16_t)(r & 0xF8) << 8)
       | ((uint16_t)(g & 0xFC) << 3)
       | (b >> 3);
}

// Prototipos das cenas (internos)
static void tela_boas_vindas();
static void cena_retangulos();
static void cena_circulos();
static void cena_arco_iris();
static void cena_texto_colorido();
static void cena_triangulos();
static void cena_gradiente();

// Public API ---------------------------------------------------------------
void displaySetDuration(unsigned long ms) {
  DURACAO = ms;
}

void displayInit() {
  // Configura pinos em estado seguro
  pinMode(PIN_RST,  OUTPUT);
  pinMode(PIN_DC,   OUTPUT);
  pinMode(PIN_CS,   OUTPUT);
  digitalWrite(PIN_CS,  HIGH);
  digitalWrite(PIN_DC,  HIGH);
  digitalWrite(PIN_RST, HIGH);
  delay(20);

  // Reset manual
  digitalWrite(PIN_RST, LOW);  delay(100);
  digitalWrite(PIN_RST, HIGH); delay(200);

  // Cria objeto (alocação dinâmica para compatibilidade com ESP32)
  tft = new Adafruit_ST7789(PIN_CS, PIN_DC, PIN_MOSI, PIN_SCK, PIN_RST);

  // Inicializa controlador com resolução correta
  tft->init(LARGURA, ALTURA, SPI_MODE3);

  // Define rotação e corrige MADCTL para evitar espelhamento
  tft->setRotation(2);
  // MADCTL: 0x40 é valor comum para corrigir espelhamento horizontal
  tft->sendCommand(0x36, (uint8_t[]){0x40}, 1);

  tft->invertDisplay(false);
  tft->fillScreen(ST77XX_BLACK);

  Serial.println("TFT init OK (módulo)");

  // Exibe tela inicial e inicializa temporizador interno
  tela_boas_vindas();
  delay(3000);
  ultimo_tempo = millis();
}

void displayTick() {
  if (!tft) return;
  if (millis() - ultimo_tempo >= DURACAO) {
    ultimo_tempo = millis();
    cena_atual = (cena_atual + 1) % TOTAL_CENAS;
    switch (cena_atual) {
      case 0: cena_retangulos();     break;
      case 1: cena_circulos();       break;
      case 2: cena_arco_iris();      break;
      case 3: cena_texto_colorido(); break;
      case 4: cena_triangulos();     break;
      case 5: cena_gradiente();      break;
    }
  }
}

// Implementação das cenas (copiado do sketch original, com comentários)
static void tela_boas_vindas() {
  tft->fillScreen(rgb(0, 0, 30));
  tft->drawRect(2, 2, LARGURA-4, ALTURA-4, ST77XX_CYAN);
  tft->drawRect(5, 5, LARGURA-10, ALTURA-10, ST77XX_BLUE);

  tft->setTextWrap(false);
  tft->setTextColor(ST77XX_YELLOW);
  tft->setTextSize(2);
  tft->setCursor(14, 65);
  tft->print("ESP32-S3 + TFT");

  tft->setTextColor(ST77XX_WHITE);
  tft->setTextSize(1);
  tft->setCursor(28, 98);
  tft->print("Display 2.8\"  240x320");

  tft->drawFastHLine(20, 118, LARGURA-40, ST77XX_CYAN);

  tft->setTextColor(ST77XX_GREEN);
  tft->setTextSize(2);
  tft->setCursor(22, 138);
  tft->print("TESTE GRAFICO");

  tft->setTextColor(rgb(150,150,150));
  tft->setTextSize(1);
  tft->setCursor(30, 175);
  tft->print("Iniciando em 3s...");

  // Barra de progresso
  tft->drawRect(20, 200, 200, 16, rgb(80,80,80));
  for (int i = 0; i <= 196; i += 4) {
    tft->fillRect(22, 202, i, 12, rgb(i, 100, 255-i));
    delay(12);
  }

  tft->setTextColor(ST77XX_MAGENTA);
  tft->setTextSize(1);
  tft->setCursor(12, 248); tft->print("Lib: Adafruit ST7789");
  tft->setCursor(12, 264); tft->print("MOSI=45 SCK=3 CS=14");
  tft->setCursor(12, 280); tft->print("DC=47  RST=21");

  Serial.println("Boas-vindas OK (módulo)");
}

static void cena_retangulos() {
  tft->fillScreen(ST77XX_BLACK);
  uint16_t cores[] = {
    ST77XX_RED,   rgb(255,165,0), ST77XX_YELLOW,
    ST77XX_GREEN, ST77XX_CYAN,   ST77XX_BLUE,
    rgb(128,0,128), ST77XX_MAGENTA, rgb(255,105,180), ST77XX_WHITE
  };
  for (int i = 0; i < 10; i++) {
    int m = i * 11;
    tft->drawRect(m, m, LARGURA-(m*2), ALTURA-(m*2), cores[i]);
  }
  tft->setTextColor(ST77XX_WHITE);
  tft->setTextSize(1);
  tft->setCursor(85, 156);
  tft->print("RETANGULOS");
  Serial.println("Cena retangulos OK (módulo)");
}

static void cena_circulos() {
  tft->fillScreen(ST77XX_BLACK);
  tft->fillCircle( 60,  80, 55, ST77XX_RED);
  tft->fillCircle(180,  80, 55, ST77XX_BLUE);
  tft->fillCircle( 60, 240, 55, ST77XX_GREEN);
  tft->fillCircle(180, 240, 55, ST77XX_YELLOW);
  tft->fillCircle(120, 160, 50, ST77XX_MAGENTA);
  tft->drawCircle( 60,  80, 55, ST77XX_WHITE);
  tft->drawCircle(180,  80, 55, ST77XX_WHITE);
  tft->drawCircle( 60, 240, 55, ST77XX_WHITE);
  tft->drawCircle(180, 240, 55, ST77XX_WHITE);
  tft->drawCircle(120, 160, 50, ST77XX_WHITE);
  tft->fillCircle(120, 160,  8, ST77XX_WHITE);
  Serial.println("Cena circulos OK (módulo)");
}

static void cena_arco_iris() {
  int alt = ALTURA / 7;
  uint16_t cores[] = {
    ST77XX_RED, rgb(255,165,0), ST77XX_YELLOW, ST77XX_GREEN,
    rgb(0,100,255), ST77XX_BLUE, rgb(128,0,128)
  };
  for (int i = 0; i < 7; i++)
    tft->fillRect(0, i*alt, LARGURA, alt, cores[i]);
  tft->setTextColor(ST77XX_BLACK);
  tft->setTextSize(2);
  tft->setCursor(41, 157); tft->print("ARC-IRIS!");
  tft->setTextColor(ST77XX_WHITE);
  tft->setCursor(40, 156); tft->print("ARC-IRIS!");
  Serial.println("Cena arcoiris OK (módulo)");
}

static void cena_texto_colorido() {
  uint16_t bg = rgb(15,15,25);
  tft->fillScreen(bg);

  tft->setTextColor(ST77XX_YELLOW); tft->setTextSize(3);
  tft->setCursor(10, 10);  tft->print("ESP32-S3");

  tft->setTextColor(ST77XX_CYAN);   tft->setTextSize(2);
  tft->setCursor(10, 55);  tft->print("Display TFT");
  tft->setTextColor(ST77XX_GREEN);
  tft->setCursor(10, 82);  tft->print("240 x 320 px");

  tft->drawFastHLine(0, 110, LARGURA, rgb(60,60,60));

  uint16_t c[] = { ST77XX_RED, rgb(255,165,0), ST77XX_YELLOW,
                   ST77XX_GREEN, ST77XX_CYAN, ST77XX_BLUE, ST77XX_MAGENTA };
  const char l[] = "ABCDEFG";
  for (int i = 0; i < 7; i++) {
    tft->setTextColor(c[i]);
    tft->setTextSize(i%2==0 ? 2 : 3);
    tft->setCursor(10+i*32, 125+(i%3)*30);
    tft->print(l[i]);
  }

  tft->drawFastHLine(0, 235, LARGURA, rgb(60,60,60));
  tft->setTextColor(rgb(255,130,180)); tft->setTextSize(1);
  tft->setCursor(10, 248); tft->print("SCK:3  MOSI:45");
  tft->setCursor(10, 262); tft->print("CS:14  DC:47  RST:21");
  tft->setCursor(10, 276); tft->print("Adafruit ST7789");
  Serial.println("Cena texto OK (módulo)");
}

static void cena_triangulos() {
  tft->fillScreen(ST77XX_BLACK);
  tft->fillTriangle(120, 10,  10,150, 230,150, ST77XX_RED);
  tft->fillTriangle(120,310,  10,170, 230,170, ST77XX_BLUE);
  tft->fillTriangle( 10, 10,  10,310, 115,160, ST77XX_GREEN);
  tft->fillTriangle(230, 10, 230,310, 125,160, ST77XX_YELLOW);
  tft->drawTriangle(120, 10,  10,150, 230,150, ST77XX_WHITE);
  tft->drawTriangle(120,310,  10,170, 230,170, ST77XX_WHITE);
  tft->fillCircle(120,160,12, ST77XX_MAGENTA);
  tft->drawCircle(120,160,12, ST77XX_WHITE);
  Serial.println("Cena triangulos OK (módulo)");
}

static void cena_gradiente() {
  for (int y = 0; y < ALTURA; y++) {
    uint8_t r = map(y, 0, ALTURA, 255, 0);
    uint8_t g = (y < ALTURA/2)
                  ? map(y, 0, ALTURA/2, 0, 255)
                  : map(y, ALTURA/2, ALTURA, 255, 0);
    uint8_t b = map(y, 0, ALTURA, 0, 255);
    tft->drawFastHLine(0, y, LARGURA, rgb(r,g,b));
  }
  tft->setTextColor(ST77XX_BLACK); tft->setTextSize(2);
  tft->setCursor(21,157); tft->print("GRADIENTE RGB");
  tft->setTextColor(ST77XX_WHITE);
  tft->setCursor(20,156); tft->print("GRADIENTE RGB");
  tft->setTextSize(1);
  tft->setCursor(28,182); tft->print("ESP32-S3 | Adafruit");
  Serial.println("Cena gradiente OK (módulo)");
}
