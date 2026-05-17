/*
 * Dashboard PLC para TFT ST7789 (240x320)
 * Exemplo reutilizavel com API para dados simulados ou reais.
 */

#include "tft_display.h"
#include <SPI.h>
#include <math.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#define PIN_SCK   3
#define PIN_MOSI 45
#define PIN_CS   14
#define PIN_DC   47
#define PIN_RST  21

#define LARGURA  240
#define ALTURA   320

// Profundidade de historico por variavel para os graficos
#define TREND_SIZE 56

static Adafruit_ST7789* tft = nullptr;
static bool sim_enabled = true;

static float tempC = 32.0f;
static float humPct = 55.0f;
static float pressHpa = 1013.0f;
static float rpmMotor = 1450.0f;

static float tempTrend[TREND_SIZE];
static float humTrend[TREND_SIZE];
static float pressTrend[TREND_SIZE];
static float rpmTrend[TREND_SIZE];

// Estado de desenho incremental dos graficos (evita redraw completo)
static int trendX = 0;
static int g1PrevYA = -1;
static int g1PrevYB = -1;
static int g2PrevYA = -1;
static int g2PrevYB = -1;

// Cache de valores renderizados (dirty flags por conteudo)
static char prevTempText[16] = "";
static char prevHumText[16] = "";
static char prevPressText[16] = "";
static char prevRpmText[16] = "";
static bool prevWarn = false;
static bool prevWarnValid = false;

static unsigned long lastSampleMs = 0;
static unsigned long lastRenderMs = 0;
static unsigned long legacyDurationMs = 3000UL;

static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint16_t)(r & 0xF8) << 8)
       | ((uint16_t)(g & 0xFC) << 3)
       | (b >> 3);
}

// Paleta estilo HMI/PLC (contraste alto e leitura rapida)
static const uint16_t C_BG       = rgb(10, 20, 34);
static const uint16_t C_PANEL    = rgb(20, 33, 52);
static const uint16_t C_PANEL_2  = rgb(26, 44, 68);
static const uint16_t C_LINE     = rgb(56, 92, 130);
static const uint16_t C_TEXT     = rgb(224, 235, 246);
static const uint16_t C_MUTED    = rgb(145, 169, 196);
static const uint16_t C_ACCENT   = rgb(52, 173, 255);
static const uint16_t C_TEMP     = rgb(255, 116, 80);
static const uint16_t C_HUM      = rgb(56, 220, 186);
static const uint16_t C_PRESS    = rgb(250, 208, 90);
static const uint16_t C_RPM      = rgb(186, 140, 255);
static const uint16_t C_OK       = rgb(88, 213, 126);
static const uint16_t C_WARN     = rgb(255, 172, 63);

static void drawHeader();
static void drawPanels();
static bool drawMetrics(bool forceValues, bool forceTrends);
static void drawTrendShell();
static void plotTrendStep();
static bool pushTrendIfChanged();
static void seedTrend();
static void updateSimulatedValues();
static float clampf(float v, float vmin, float vmax);
static int scaleToY(float v, float vmin, float vmax, int top, int bottom);

void displaySetDuration(unsigned long ms) {
  // Mantido para compatibilidade; nao altera o comportamento do dashboard.
  legacyDurationMs = ms;
  (void)legacyDurationMs;
}

void displayUseSimulation(bool enabled) {
  sim_enabled = enabled;
}

void displaySetMeasurements(float temperatureC, float humidityPct, float pressureHpa, float motorRpm) {
  // Permite alimentar o dashboard com dados reais de sensores.
  tempC = temperatureC;
  humPct = humidityPct;
  pressHpa = pressureHpa;
  rpmMotor = motorRpm;
}

void displayInit() {
  pinMode(PIN_RST,  OUTPUT);
  pinMode(PIN_DC,   OUTPUT);
  pinMode(PIN_CS,   OUTPUT);
  digitalWrite(PIN_CS,  HIGH);
  digitalWrite(PIN_DC,  HIGH);
  digitalWrite(PIN_RST, HIGH);
  delay(20);

  digitalWrite(PIN_RST, LOW);  delay(100);
  digitalWrite(PIN_RST, HIGH); delay(200);

  tft = new Adafruit_ST7789(PIN_CS, PIN_DC, PIN_MOSI, PIN_SCK, PIN_RST);
  tft->init(LARGURA, ALTURA, SPI_MODE3);
  tft->setRotation(2);
  tft->sendCommand(0x36, (uint8_t[]){0x40}, 1);
  tft->invertDisplay(false);

  seedTrend();

  tft->fillScreen(C_BG);
  drawHeader();
  drawPanels();
  drawTrendShell();
  drawMetrics(true, true);

  lastSampleMs = millis();
  lastRenderMs = millis();
  Serial.println("CodeWave Dashboard pronto");
}

void displayTick() {
  if (!tft) return;

  if (sim_enabled) {
    updateSimulatedValues();
  }

  const unsigned long now = millis();
  bool trendDirty = false;

  // Amostragem desacoplada do frame para manter historico estavel.
  if (now - lastSampleMs >= 900) {
    lastSampleMs = now;
    trendDirty = pushTrendIfChanged();
  }

  // Atualiza em baixa taxa e somente o que mudou.
  if (now - lastRenderMs >= 220) {
    lastRenderMs = now;
    drawMetrics(false, trendDirty);
  }
}

static void drawHeader() {
  tft->fillRect(0, 0, LARGURA, 26, C_PANEL_2);
  tft->drawFastHLine(0, 25, LARGURA, C_LINE);
  tft->setTextWrap(false);
  tft->setTextSize(1);
  tft->setTextColor(C_ACCENT);
  tft->setCursor(8, 8);
  tft->print("CodeWave Dashboard");

  tft->setTextColor(C_MUTED);
  tft->setCursor(175, 8);
  tft->print("PLC/HMI");
}

static void drawCardShell(int x, int y, int w, int h) {
  tft->fillRect(x, y, w, h, C_PANEL);
  tft->drawRect(x, y, w, h, C_LINE);
}

static void drawPanels() {
  // Linha de KPIs
  drawCardShell(6, 32, 112, 48);
  drawCardShell(122, 32, 112, 48);
  drawCardShell(6, 84, 112, 48);
  drawCardShell(122, 84, 112, 48);

  // Barra de status de processo
  drawCardShell(6, 138, 228, 18);

  // Area de tendencia 1 e 2
  drawCardShell(6, 162, 228, 74);
  drawCardShell(6, 240, 228, 74);

  // Labels fixos (evita redesenho de texto estatico a cada frame)
  tft->setTextSize(1);
  tft->setTextColor(C_MUTED);
  tft->setCursor(11, 36);   tft->print("TEMPERATURA");
  tft->setCursor(127, 36);  tft->print("UMIDADE");
  tft->setCursor(11, 88);   tft->print("PRESSAO");
  tft->setCursor(127, 88);  tft->print("MOTOR");

  // Elementos fixos dos cards dinamicos (unidades e status)
  tft->setTextColor(C_MUTED);
  tft->setCursor(12, 143);  tft->print("STATUS:");
}

static bool drawMetricValueIfChanged(int x, int y, int w, int h,
                                     float value, const char* unit, uint16_t color,
                                     char* prevText, bool force) {
  char line[16];
  snprintf(line, sizeof(line), "%.1f", value);

  if (!force && strcmp(prevText, line) == 0) {
    return false;
  }

  strncpy(prevText, line, 15);
  prevText[15] = '\0';

  // Desenha texto com fundo para sobrescrever sem limpar card inteiro.
  tft->setTextColor(color, C_PANEL);
  tft->setTextSize(2); // mexi aqui
  tft->setCursor(x + 2, y + 4);
  tft->print(line);

  // Unidade alinhada no fim do card, com tamanho menor para caber em qualquer valor.
  tft->setTextColor(C_TEXT, C_PANEL);
  tft->setTextSize(1);
  int unitX = x + w - (int)strlen(unit) * 6 - 4;
  if (unitX < x + 34) unitX = x + 34;
  tft->setCursor(unitX, y + 4);
  tft->print(unit);
  return true;
}

static bool drawStatusBarIfChanged(bool force) {
  // Regra simples de exemplo para "status de planta"
  bool anyWarn = (tempC > 40.0f) || (humPct < 35.0f) || (rpmMotor > 1900.0f);

  if (!force && prevWarnValid && prevWarn == anyWarn) {
    return false;
  }

  prevWarn = anyWarn;
  prevWarnValid = true;

  uint16_t c = anyWarn ? C_WARN : C_OK;
  tft->setTextSize(1);
  tft->setTextColor(c, C_PANEL);
  tft->setCursor(60, 143);
  tft->print(anyWarn ? "ATENCAO         " : "OPERACAO NORMAL ");
  return true;
}

static bool drawMetrics(bool forceValues, bool forceTrends) {
  bool updated = false;

  updated |= drawMetricValueIfChanged(9, 50, 106, 26, tempC, "oC", C_TEMP, prevTempText, forceValues);
  updated |= drawMetricValueIfChanged(125, 50, 106, 26, humPct, "%", C_HUM, prevHumText, forceValues);
  updated |= drawMetricValueIfChanged(9, 102, 106, 26, pressHpa, "hPa", C_PRESS, prevPressText, forceValues);
  updated |= drawMetricValueIfChanged(125, 102, 106, 26, rpmMotor, "RPM", C_RPM, prevRpmText, forceValues);
  updated |= drawStatusBarIfChanged(forceValues);

  if (forceTrends) {
    plotTrendStep();
    updated = true;
  }

  return updated;
}

static void drawTrendShell() {
  // Quadro superior
  tft->fillRect(9, 163, 222, 68, C_PANEL);
  tft->setTextSize(1);
  tft->setTextColor(C_MUTED);
  tft->setCursor(12, 166); tft->print("Tendencia: Temp x Umidade");
  tft->setTextColor(C_TEMP);
  tft->setCursor(214, 166); tft->print("T");
  tft->setTextColor(C_HUM);
  tft->setCursor(222, 166); tft->print("U");

  // Quadro inferior
  tft->fillRect(9, 241, 222, 68, C_PANEL);
  tft->setTextColor(C_MUTED);
  tft->setCursor(12, 244); tft->print("Tendencia: Pressao x Rotacao");
  tft->setTextColor(C_PRESS);
  tft->setCursor(214, 244); tft->print("P");
  tft->setTextColor(C_RPM);
  tft->setCursor(222, 244); tft->print("R");

  // Grid base (uma vez)
  tft->drawFastHLine(10, 199, 220, rgb(44, 75, 110));
  tft->drawFastHLine(10, 277, 220, rgb(44, 75, 110));

  trendX = 0;
  g1PrevYA = g1PrevYB = g2PrevYA = g2PrevYB = -1;
}

static void plotTrendStep() {
  // Area util dos graficos
  const int g1Top = 178, g1Bottom = 227;
  const int g2Top = 256, g2Bottom = 305;
  const int left = 10, width = 220;

  int x = left + trendX;

  // Quando fecha uma volta, limpa apenas area de plot e reinicia continuidade.
  if (trendX == 0) {
    tft->fillRect(left, g1Top, width, g1Bottom - g1Top + 1, C_PANEL);
    tft->fillRect(left, g2Top, width, g2Bottom - g2Top + 1, C_PANEL);
    tft->drawFastHLine(left, (g1Top + g1Bottom) / 2, width, rgb(44, 75, 110));
    tft->drawFastHLine(left, (g2Top + g2Bottom) / 2, width, rgb(44, 75, 110));
    g1PrevYA = g1PrevYB = g2PrevYA = g2PrevYB = -1;
  }

  // Apaga apenas a coluna atual para desenhar novo ponto.
  tft->drawFastVLine(x, g1Top, g1Bottom - g1Top + 1, C_PANEL);
  tft->drawFastVLine(x, g2Top, g2Bottom - g2Top + 1, C_PANEL);

  int yTemp  = scaleToY(tempC, 20.0f, 55.0f, g1Top, g1Bottom);
  int yHum   = scaleToY(humPct, 20.0f, 90.0f, g1Top, g1Bottom);
  int yPress = scaleToY(pressHpa, 980.0f, 1040.0f, g2Top, g2Bottom);
  int yRpm   = scaleToY(rpmMotor, 1000.0f, 2200.0f, g2Top, g2Bottom);

  if (g1PrevYA >= 0) tft->drawLine(x - 1, g1PrevYA, x, yTemp, C_TEMP);
  else tft->drawPixel(x, yTemp, C_TEMP);
  if (g1PrevYB >= 0) tft->drawLine(x - 1, g1PrevYB, x, yHum, C_HUM);
  else tft->drawPixel(x, yHum, C_HUM);

  if (g2PrevYA >= 0) tft->drawLine(x - 1, g2PrevYA, x, yPress, C_PRESS);
  else tft->drawPixel(x, yPress, C_PRESS);
  if (g2PrevYB >= 0) tft->drawLine(x - 1, g2PrevYB, x, yRpm, C_RPM);
  else tft->drawPixel(x, yRpm, C_RPM);

  g1PrevYA = yTemp;
  g1PrevYB = yHum;
  g2PrevYA = yPress;
  g2PrevYB = yRpm;

  trendX++;
  if (trendX >= width) {
    trendX = 0;
  }
}

static void seedTrend() {
  for (int i = 0; i < TREND_SIZE; i++) {
    tempTrend[i] = tempC;
    humTrend[i] = humPct;
    pressTrend[i] = pressHpa;
    rpmTrend[i] = rpmMotor;
  }
}

static bool pushTrendIfChanged() {
  const bool changed = (fabsf(tempC - tempTrend[TREND_SIZE - 1]) > 0.04f)
                    || (fabsf(humPct - humTrend[TREND_SIZE - 1]) > 0.04f)
                    || (fabsf(pressHpa - pressTrend[TREND_SIZE - 1]) > 0.03f)
                    || (fabsf(rpmMotor - rpmTrend[TREND_SIZE - 1]) > 0.40f);

  if (!changed) {
    return false;
  }

  for (int i = 0; i < TREND_SIZE - 1; i++) {
    tempTrend[i] = tempTrend[i + 1];
    humTrend[i] = humTrend[i + 1];
    pressTrend[i] = pressTrend[i + 1];
    rpmTrend[i] = rpmTrend[i + 1];
  }
  tempTrend[TREND_SIZE - 1] = tempC;
  humTrend[TREND_SIZE - 1] = humPct;
  pressTrend[TREND_SIZE - 1] = pressHpa;
  rpmTrend[TREND_SIZE - 1] = rpmMotor;
  return true;
}

static void updateSimulatedValues() {
  // Simulacao periodica realista para demonstrar o dashboard sem sensores.
  const float t = millis() * 0.001f;
  tempC = 33.0f + 3.0f * sinf(t * 0.82f) + 0.7f * sinf(t * 2.10f);
  humPct = 57.0f + 9.0f * sinf(t * 0.33f + 0.9f);
  pressHpa = 1012.0f + 4.2f * sinf(t * 0.19f) + 1.2f * sinf(t * 0.77f + 0.4f);
  rpmMotor = 1480.0f + 210.0f * sinf(t * 1.18f) + 80.0f * sinf(t * 2.60f);
}

static float clampf(float v, float vmin, float vmax) {
  if (v < vmin) return vmin;
  if (v > vmax) return vmax;
  return v;
}

static int scaleToY(float v, float vmin, float vmax, int top, int bottom) {
  float n = (v - vmin) / (vmax - vmin);
  n = clampf(n, 0.0f, 1.0f);
  return bottom - (int)(n * (bottom - top));
}
