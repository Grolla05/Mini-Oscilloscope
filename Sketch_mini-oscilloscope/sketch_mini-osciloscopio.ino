/*
 * Mini Osciloscópio — Arduino Uno + OLED SSD1306 128x64 (I2C)
 * Autor: Felipe Grolla
 *
 * Lê um sinal analógico em A0 com o ADC configurado para alta velocidade
 * (prescaler 16 -> clock de 1 MHz, ~13 µs por conversão, ~77 kS/s) e desenha
 * a forma de onda na OLED com trigger por borda, grade, frequência e Vpp.
 *
 *   A0  -> sinal condicionado (acoplamento AC + bias de 2,5 V, ver README)
 *   A1  -> potenciômetro 10 kΩ: escala de tempo (time/div)
 *   D2  -> botão HOLD/RUN (para -> GND, INPUT_PULLUP)
 *   D3  -> botão borda do trigger: subida / descida (para -> GND)
 *   D9  -> saída PWM do sinal de teste, com filtro RC (opcional, ver ENABLE_TESTE)
 *   A4/A5 -> SDA/SCL da OLED
 *
 * Bibliotecas: Adafruit GFX, Adafruit SSD1306 (e Adafruit BusIO).
 */

#include <Wire.h>
#include <avr/pgmspace.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------------------------------------------------------------- Configuração
#define OLED_LARGURA 128
#define OLED_ALTURA   64
#define OLED_ENDERECO 0x3C      // ajuste para 0x3D se o módulo for diferente

const uint8_t PIN_HOLD  = 2;
const uint8_t PIN_BORDA = 3;

// Área de plotagem. OLED bicolor: linhas 0-15 = faixa amarela (só texto do
// cabeçalho); linhas 16-63 = faixa azul (só grade e forma de onda).
const uint8_t N_AMOSTRAS  = 128;
const uint8_t PLOT_TOPO   = 16;
const uint8_t PLOT_BASE   = 63;
const uint8_t PLOT_CENTRO = 40;                      // linha central (nível médio do sinal)
const uint8_t PX_POR_DIV_V = 8;                      // 6 divisões verticais de 8 px (±3 div)

// Escala vertical AUTOMÁTICA: o sinal é centrado na tela e a escala (V/div) é
// escolhida para ocupar ~4-5 das 6 divisões, com picos e vales bem definidos.
const float VDIV[5] = {1.0f, 0.5f, 0.2f, 0.1f, 0.05f};
uint8_t idxV = 0;
float   meioV = 128.0f;                              // centro da tela, em passos do ADC
const uint8_t PX_POR_DIV  = 16;                      // 8 divisões horizontais

const uint8_t NIVEL_TRIGGER = 128;                   // ~2,5 V (meio da escala)
const uint16_t TENTATIVAS_TRIGGER = 400;             // depois disso: modo AUTO

// Intervalo entre amostras (µs) para cada posição do potenciômetro.
// 0 = o mais rápido possível (conversões consecutivas, ~14 µs).
const uint16_t INTERVALO_US[8] = {0, 20, 50, 100, 200, 500, 1000, 2000};

Adafruit_SSD1306 display(OLED_LARGURA, OLED_ALTURA, &Wire, -1);

// >>> TESTE_INICIO
// ---------------------------------------------------------------------------
// Gerador de sinal de teste por PWM (para quem não tem gerador de funções)
//   Timer1 -> PWM rápido de 5 bits em D9 (TOP = 31, portadora de 500 kHz)
//   Timer2 -> interrupção a 31250 Hz que atualiza o duty (síntese DDS)
//   Difusão de erro (1ª ordem): a resolução média continua de ~8 bits e o
//   ruído de quantização vai para frequências altas, onde o próprio R3 + Cf
//   da entrada (passa-baixa) já o atenua. NÃO precisa de filtro externo.
// LIGAÇÃO: D9 direto no SINAL+ (perna esquerda de C1) e GND no SINAL-.
// ---------------------------------------------------------------------------
#define FORMA_SENO        0
#define FORMA_TRIANGULAR  1
#define FORMA_QUADRADA    2
#define FORMA_DOIS_TONS   3     // seno de f + seno de 3f (dois picos no analisador)
#define FORMA_VARREDURA   4     // seno que varre 200 Hz -> 3 kHz a cada 4 s

#define ENABLE_TESTE      1     // 1 = gera o sinal de teste em D9
#define TESTE_FORMA       FORMA_SENO
#define TESTE_HZ          1000  // frequência (ignorada na varredura); use até ~3000 Hz
#define TESTE_AMPLITUDE   100   // 0..127 (127 = 5 Vpp no pino D9)

#if ENABLE_TESTE
const uint8_t  PIN_TESTE_PWM = 9;        // OC1A (Timer1)
const uint16_t TESTE_FS      = 31250;    // Hz: 16 MHz / 8 / 64
const uint8_t  PWM_TOP       = 31;       // 16 MHz / 32 = 500 kHz, 33 níveis (0..32)

// 127 * sin(2*pi*i/256)
const int8_t SENO_TAB[256] PROGMEM = {
     0,    3,    6,    9,   12,   16,   19,   22,   25,   28,   31,   34,   37,   40,   43,   46,
    49,   51,   54,   57,   60,   63,   65,   68,   71,   73,   76,   78,   81,   83,   85,   88,
    90,   92,   94,   96,   98,  100,  102,  104,  106,  107,  109,  111,  112,  113,  115,  116,
   117,  118,  120,  121,  122,  122,  123,  124,  125,  125,  126,  126,  126,  127,  127,  127,
   127,  127,  127,  127,  126,  126,  126,  125,  125,  124,  123,  122,  122,  121,  120,  118,
   117,  116,  115,  113,  112,  111,  109,  107,  106,  104,  102,  100,   98,   96,   94,   92,
    90,   88,   85,   83,   81,   78,   76,   73,   71,   68,   65,   63,   60,   57,   54,   51,
    49,   46,   43,   40,   37,   34,   31,   28,   25,   22,   19,   16,   12,    9,    6,    3,
     0,   -3,   -6,   -9,  -12,  -16,  -19,  -22,  -25,  -28,  -31,  -34,  -37,  -40,  -43,  -46,
   -49,  -51,  -54,  -57,  -60,  -63,  -65,  -68,  -71,  -73,  -76,  -78,  -81,  -83,  -85,  -88,
   -90,  -92,  -94,  -96,  -98, -100, -102, -104, -106, -107, -109, -111, -112, -113, -115, -116,
  -117, -118, -120, -121, -122, -122, -123, -124, -125, -125, -126, -126, -126, -127, -127, -127,
  -127, -127, -127, -127, -126, -126, -126, -125, -125, -124, -123, -122, -122, -121, -120, -118,
  -117, -116, -115, -113, -112, -111, -109, -107, -106, -104, -102, -100,  -98,  -96,  -94,  -92,
   -90,  -88,  -85,  -83,  -81,  -78,  -76,  -73,  -71,  -68,  -65,  -63,  -60,  -57,  -54,  -51,
   -49,  -46,  -43,  -40,  -37,  -34,  -31,  -28,  -25,  -22,  -19,  -16,  -12,   -9,   -6,   -3
};

volatile uint16_t testeFase  = 0;        // acumulador de fase (16 bits)
volatile uint16_t testePasso = 0;        // incremento por amostra = f * 65536 / TESTE_FS

static inline uint16_t passoParaHz(uint16_t hz) {
  return (uint16_t)(((uint32_t)hz * 65536UL) / TESTE_FS);
}

ISR(TIMER2_COMPA_vect) {
  static int8_t erro = 0;                 // resíduo da quantização (unidades de 1/8 de degrau)
  testeFase += testePasso;
  uint8_t i = testeFase >> 8;
  int16_t v;                              // -127..127
#if TESTE_FORMA == FORMA_TRIANGULAR
  uint8_t t = (i & 0x80) ? (uint8_t)(255 - i) : i;      // 0..127..0
  v = (int16_t)t * 2 - 127;
#elif TESTE_FORMA == FORMA_QUADRADA
  v = (i & 0x80) ? -127 : 127;
#elif TESTE_FORMA == FORMA_DOIS_TONS
  uint8_t j = (uint8_t)((uint16_t)(testeFase * 3U) >> 8);
  v = ((int8_t)pgm_read_byte(&SENO_TAB[i]) + (int8_t)pgm_read_byte(&SENO_TAB[j])) / 2;
#else
  v = (int8_t)pgm_read_byte(&SENO_TAB[i]);
#endif
  int16_t alvo = 128 + ((v * TESTE_AMPLITUDE) >> 7);    // 0..255
  int16_t soma = alvo + erro;                           // 8 unidades = 1 degrau do PWM
  int16_t duty = (soma + 4) >> 3;                       // arredonda para 0..32
  if (duty < 0) duty = 0;
  if (duty > PWM_TOP + 1) duty = PWM_TOP + 1;           // 32 = nível alto constante
  erro = (int8_t)(soma - (duty << 3));
  OCR1A = duty;
}

void iniciarTeste() {
  pinMode(PIN_TESTE_PWM, OUTPUT);
  testePasso = passoParaHz(TESTE_HZ);
  ICR1   = PWM_TOP;                      // Timer1: fast PWM, TOP = ICR1 (modo 14)
  OCR1A  = (PWM_TOP + 1) / 2;            // meio da escala (2,5 V médios)
  TCCR1A = _BV(COM1A1) | _BV(WGM11);     // D9 não invertido
  TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);   // sem prescaler -> 500 kHz
  TCCR2A = _BV(WGM21);                   // Timer2: modo CTC
  TCCR2B = _BV(CS21);                    // prescaler 8 -> 2 MHz
  OCR2A  = 63;                           // 2 MHz / 64 = 31250 Hz
  TIMSK2 = _BV(OCIE2A);                  // habilita a interrupção
}

void atualizarTeste() {
#if TESTE_FORMA == FORMA_VARREDURA
  static uint32_t ultimo = 0;
  uint32_t agora = millis();
  if (agora - ultimo >= 40) {
    ultimo = agora;
    uint16_t fase = (uint16_t)(agora % 4000UL);                          // 0..3999 ms
    uint16_t hz = 200 + (uint16_t)((uint32_t)fase * 2800UL / 4000UL);    // 200..3000 Hz
    uint16_t p = passoParaHz(hz);
    noInterrupts(); testePasso = p; interrupts();
  }
#endif
}
#else
void iniciarTeste() { }
void atualizarTeste() { }
#endif
// <<< TESTE_FIM

// ---------------------------------------------------------------------- Estado
uint8_t  amostras[N_AMOSTRAS];
uint8_t  idxTempo     = 4;
bool     bordaSubida  = true;
bool     congelado    = false;
bool     autoTrigger  = false;
float    dtUs         = 0;      // tempo real entre amostras (medido)
float    freqHz       = 0;
uint8_t  vMin = 0, vMax = 0;

uint32_t ultimoBotao  = 0;

// -------------------------------------------------------------------------- ADC
void configurarAdc() {
  ADMUX  = _BV(REFS0) | _BV(ADLAR);     // AVcc, resultado de 8 bits em ADCH, canal A0
  ADCSRA = _BV(ADEN)  | _BV(ADPS2);     // ADC ligado, prescaler 16 (1 MHz)
}

static inline uint8_t lerAdc() {
  ADCSRA |= _BV(ADSC);
  while (ADCSRA & _BV(ADSC)) { }
  return ADCH;
}

// Lê o potenciômetro em A1 (10 bits) e volta o ADC para A0 / 8 bits.
uint16_t lerPotenciometro() {
  ADMUX = _BV(REFS0) | 1;               // A1, justificado à direita
  ADCSRA |= _BV(ADSC); while (ADCSRA & _BV(ADSC)) { }   // descarta (troca de canal)
  ADCSRA |= _BV(ADSC); while (ADCSRA & _BV(ADSC)) { }
  uint16_t v = ADC;
  ADMUX = _BV(REFS0) | _BV(ADLAR);
  return v;
}

// --------------------------------------------------------------------- Captura
void aguardar(uint32_t &proximo, uint16_t intervalo) {
  if (!intervalo) return;
  proximo += intervalo;
  while ((int32_t)(micros() - proximo) < 0) { }
}

void capturar() {
  const uint16_t intervalo = INTERVALO_US[idxTempo];
  uint32_t proximo = micros();

  lerAdc();                             // descarta a 1ª leitura após trocar de canal
  uint8_t anterior = lerAdc();
  uint8_t atual    = anterior;
  autoTrigger = true;

  for (uint16_t t = 0; t < TENTATIVAS_TRIGGER; t++) {
    aguardar(proximo, intervalo);
    atual = lerAdc();
    bool cruzou = bordaSubida ? (anterior < NIVEL_TRIGGER && atual >= NIVEL_TRIGGER)
                              : (anterior > NIVEL_TRIGGER && atual <= NIVEL_TRIGGER);
    if (cruzou) { autoTrigger = false; break; }
    anterior = atual;
  }

  amostras[0] = atual;
  uint32_t t0 = micros();
  for (uint8_t i = 1; i < N_AMOSTRAS; i++) {
    aguardar(proximo, intervalo);
    amostras[i] = lerAdc();
  }
  dtUs = (float)(micros() - t0) / (N_AMOSTRAS - 1);
}

// ---------------------------------------------------------------------- Medidas
// >>> MEDIDAS_INICIO
// Valor da amostra "normalizado" para borda de subida (borda de descida inverte o sinal)
static inline uint8_t amostraNorm(uint8_t i) {
  return bordaSubida ? amostras[i] : (uint8_t)(255 - amostras[i]);
}

void medir() {
  vMin = 255; vMax = 0;
  for (uint8_t i = 0; i < N_AMOSTRAS; i++) {
    if (amostras[i] < vMin) vMin = amostras[i];
    if (amostras[i] > vMax) vMax = amostras[i];
  }
  freqHz = 0;
  uint8_t faixa = vMax - vMin;
  if (faixa < 8) return;                // sem sinal útil

  uint8_t meio = (uint8_t)(((uint16_t)vMin + vMax) >> 1);
  if (!bordaSubida) meio = 255 - meio;
  uint8_t histe = faixa >> 3;           // histerese de 1/8 da excursão
  uint8_t alto = meio + histe;
  uint8_t baixo = meio - histe;

  float primeiro = 0, ultimo = 0;
  uint8_t cruzamentos = 0;

  // A própria amostra 0 é um cruzamento (o trigger disparou nela): estima a posição
  // sub-amostra pela inclinação, o que permite medir com apenas ~1 período na tela.
  if (!autoTrigger) {
    float a0 = amostraNorm(0), a1 = amostraNorm(1);
    float pos = (a1 != a0) ? (meio - a0) / (a1 - a0) : 0;
    if (pos < -1) pos = -1;
    if (pos > 1) pos = 1;
    primeiro = ultimo = pos;
    cruzamentos = 1;
  }

  bool estadoAlto = amostraNorm(0) >= alto;
  for (uint8_t i = 1; i < N_AMOSTRAS; i++) {
    uint8_t v = amostraNorm(i);
    if (!estadoAlto && v >= alto) {
      estadoAlto = true;
      uint8_t j = i;                    // acha o primeiro ponto >= meio
      while (j > 0 && amostraNorm(j - 1) >= meio) j--;
      if (j == 0) continue;
      float frac = (float)(meio - amostraNorm(j - 1)) / (float)(amostraNorm(j) - amostraNorm(j - 1));
      float pos = (j - 1) + frac;
      if (cruzamentos == 0) primeiro = pos;
      ultimo = pos;
      cruzamentos++;
    } else if (estadoAlto && v <= baixo) {
      estadoAlto = false;
    }
  }
  if (cruzamentos >= 2) {
    float periodoAmostras = (ultimo - primeiro) / (cruzamentos - 1);
    if (periodoAmostras > 0) freqHz = 1000000.0f / (periodoAmostras * dtUs);
  }
}
// <<< MEDIDAS_FIM

// ------------------------------------------------------------------------- UI
void imprimirTdiv() {
  switch (idxTempo) {
    case 0: display.print(F("230us")); break;
    case 1: display.print(F("320us")); break;
    case 2: display.print(F("800us")); break;
    case 3: display.print(F("1.6ms")); break;
    case 4: display.print(F("3.2ms")); break;
    case 5: display.print(F("8ms"));   break;
    case 6: display.print(F("16ms"));  break;
    default: display.print(F("32ms")); break;
  }
}

void imprimirFreq() {
  if (freqHz <= 0) { display.print(F("---")); return; }
  if (freqHz < 1000.0f) {
    display.print((int)(freqHz + 0.5f));
    display.print(F("Hz"));
    return;
  }
  uint16_t centi = (uint16_t)(freqHz / 10.0f + 0.5f);     // centésimos de kHz
  if (centi >= 1000) {                                    // >= 10 kHz: 1 casa decimal
    uint16_t dec = (centi + 5) / 10;                      // décimos de kHz
    display.print(dec / 10); display.print('.'); display.print(dec % 10);
  } else {
    display.print(centi / 100); display.print('.');
    if ((centi % 100) < 10) display.print('0');
    display.print(centi % 100);
  }
  display.print(F("kHz"));
}

void imprimirVpp() {
  uint16_t cv = (uint32_t)(vMax - vMin) * 500UL / 255;    // centivolts
  if (cv < 100) {
    display.print(cv * 10); display.print(F("mV"));
  } else {
    uint16_t dec = (cv + 5) / 10;
    display.print(dec / 10); display.print('.'); display.print(dec % 10);
    display.print(F("Vpp"));
  }
}

void atualizarEscalaV() {
  float vpp = (float)(vMax - vMin) * 5.0f / 255.0f;
  while (idxV > 0 && vpp > 5.5f * VDIV[idxV]) idxV--;                  // sinal grande: escala maior
  while (idxV < 4 && vpp < 5.0f * VDIV[idxV + 1]) idxV++;              // sinal pequeno: amplia
  meioV = ((float)vMin + (float)vMax) * 0.5f;
}

void imprimirVdiv() {
  switch (idxV) {
    case 0:  display.print(F("1V/div"));    break;
    case 1:  display.print(F("0.5V/div"));  break;
    case 2:  display.print(F("0.2V/div"));  break;
    case 3:  display.print(F("0.1V/div"));  break;
    default: display.print(F("50mV/div"));  break;
  }
}

void imprimirCentro() {
  uint16_t c = (uint16_t)(meioV * 500.0f / 255.0f + 0.5f);             // centivolts
  display.print('@'); display.print(c / 100); display.print('.');
  if ((c % 100) < 10) display.print('0');
  display.print(c % 100); display.print('V');
}

uint8_t yDaAmostra(uint8_t s) {
  float cdiv = VDIV[idxV] * 255.0f / 5.0f;                             // passos do ADC por divisão
  float y = (float)PLOT_CENTRO - ((float)s - meioV) * PX_POR_DIV_V / cdiv;
  if (y < PLOT_TOPO) y = PLOT_TOPO;
  if (y > PLOT_BASE) y = PLOT_BASE;
  return (uint8_t)(y + 0.5f);
}

void desenharGrade() {
  // 8 divisões horizontais (time/div) x 6 verticais (V/div), pontilhadas
  for (uint8_t x = 0; x < OLED_LARGURA; x += PX_POR_DIV)
    for (uint8_t y = PLOT_TOPO; y <= PLOT_BASE; y += 3) display.drawPixel(x, y, SSD1306_WHITE);
  for (int8_t k = -3; k <= 2; k++) {
    uint8_t y = PLOT_CENTRO + k * PX_POR_DIV_V;
    for (uint8_t x = 0; x < OLED_LARGURA; x += 4) display.drawPixel(x, y, SSD1306_WHITE);
  }
}

void desenhar() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Cabeçalho (faixa amarela, linhas 0-15): duas linhas de texto
  display.setCursor(0, 0);   imprimirTdiv();
  display.setCursor(48, 0);  imprimirFreq();
  atualizarEscalaV();
  display.setCursor(0, 8);   imprimirVdiv();
  display.setCursor(54, 8);  imprimirVpp();
  display.setCursor(90, 8);  imprimirCentro();
  display.setCursor(122, 0);
  if (congelado)        display.print('H');
  else if (autoTrigger) display.print('A');
  else                  display.print(bordaSubida ? '/' : '\\');

  desenharGrade();

  uint8_t yAnt = yDaAmostra(amostras[0]);
  for (uint8_t x = 1; x < N_AMOSTRAS; x++) {
    uint8_t y = yDaAmostra(amostras[x]);
    display.drawLine(x - 1, yAnt, x, y, SSD1306_WHITE);
    yAnt = y;
  }
  display.display();
}

void tratarEntradas() {
  // Potenciômetro com histerese para o time/div não ficar oscilando
  uint16_t v = lerPotenciometro();
  uint16_t lo = (uint16_t)idxTempo * 128;
  uint16_t hi = lo + 127;
  if (v + 8 < lo || v > hi + 8) idxTempo = v >> 7;

  // Botões (com debounce por tempo)
  uint32_t agora = millis();
  if (agora - ultimoBotao > 200) {
    if (!digitalRead(PIN_HOLD))  { congelado = !congelado;     ultimoBotao = agora; }
    if (!digitalRead(PIN_BORDA)) { bordaSubida = !bordaSubida; ultimoBotao = agora; }
  }
}

// ------------------------------------------------------------------ Arduino API
void setup() {
  pinMode(PIN_HOLD,  INPUT_PULLUP);
  pinMode(PIN_BORDA, INPUT_PULLUP);

  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ENDERECO)) {
    for (;;) { }                        // OLED não encontrada: verifique ligação/endereço
  }
  Wire.setClock(400000);                // I2C rápido: ~25 ms por quadro completo

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 4);  display.print(F("Mini Osciloscopio"));
  display.setCursor(32, 36); display.print(F("Felipe Grolla"));
  display.display();
  delay(1200);

  configurarAdc();
  iniciarTeste();                       // sinal de teste por PWM em D9 (ver TESTE_INICIO)
}

void loop() {
  atualizarTeste();
  tratarEntradas();
  if (!congelado) {
    capturar();
    medir();
  }
  desenhar();
}
