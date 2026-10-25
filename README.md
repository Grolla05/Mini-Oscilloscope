<div align="center">

# 📟 Mini Osciloscópio

**Osciloscópio de bolso com Arduino Uno: ADC rápido, trigger por borda e forma de onda em tempo real na OLED**

[![Arduino](https://img.shields.io/badge/Arduino-Uno-00979D?style=for-the-badge&logo=Arduino&logoColor=white)](https://www.arduino.cc/)
[![C++](https://img.shields.io/badge/C%2B%2B-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![MIT License](https://img.shields.io/badge/License-MIT-green.svg?style=for-the-badge)](https://choosealicense.com/licenses/mit/)

</div>

---

## 📋 Descrição

O **Mini Osciloscópio** lê um sinal analógico no pino A0 com o ADC do ATmega328P reconfigurado para alta velocidade (prescaler 16, clock de 1 MHz, ~14 µs por conversão, cerca de 70 mil amostras por segundo) e desenha a forma de onda numa tela OLED SSD1306 de 128×64 pixels, com grade, trigger por borda, frequência e tensão pico a pico.

Um potenciômetro ajusta a escala de tempo (time/div) em 8 posições, de 230 µs/div a 32 ms/div. Um circuito de entrada com capacitores e resistores faz o acoplamento AC, centra o sinal em 2,5 V e filtra ruído de alta frequência, para que sinais com excursão positiva e negativa possam ser lidos por um ADC que só enxerga 0–5 V.

O projeto demonstra configuração direta de registradores do ADC (`ADMUX`, `ADCSRA`), amostragem com temporização por `micros()`, trigger por software, medição de frequência com interpolação sub-amostra e renderização gráfica em um microcontrolador de 2 KB de RAM.

---

## ⚙️ Componentes Utilizados

| Quantidade | Componente | Especificação |
|:---:|---|---|
| 1x | Arduino Uno (ou compatível) | Microcontrolador ATmega328P |
| 1x | Display OLED SSD1306 | 128×64, I2C (A4 = SDA, A5 = SCL), endereço `0x3C` |
| 1x | Potenciômetro linear | 10 kΩ (escala de tempo) |
| 2x | Push buttons | HOLD/RUN (D2) e borda do trigger (D3) |
| 1x | Capacitor C1 | 1 µF, cerâmico ou filme (acoplamento AC) |
| 1x | Capacitor Cf | 4,7 nF (filtro passa-baixa, fc ≈ 34 kHz) |
| 2x | Resistores R1, R2 | 10 kΩ (divisor de bias de 2,5 V) |
| 1x | Resistor R3 | 1 kΩ (série com A0) |
| 1x | Resistor de teste | 10 kΩ (atenua a saída de teste D8) |
| 2x | Diodos 1N4148 *(opcional)* | Grampo de proteção em A0 (0 V / 5 V) |
| — | Jumper wires, protoboard | — |

> **Sobre o transistor do roadmap:** a ideia original previa um transistor na entrada. Esta versão protege o ADC com o divisor de bias + resistor série de 1 kΩ + diodos grampeadores opcionais, que são mais simples e não deslocam o nível DC do sinal. Um buffer com transistor (seguidor de emissor) pode ser acrescentado depois se for preciso aumentar a impedância de entrada.

---

## 🔌 Pinagem

```
Arduino Uno
├── A0  ← sinal condicionado (C1 → bias 2,5 V → R3 → A0)
├── A1  ← cursor do potenciômetro 10 kΩ (extremos em 5V e GND)
├── D2  ← botão HOLD/RUN        [INPUT_PULLUP, outro terminal em GND]
├── D3  ← botão borda do trigger [INPUT_PULLUP, outro terminal em GND]
├── D8  → saída de teste 1 kHz (onda quadrada, opcional)
├── A4  ↔ SDA da OLED
├── A5  → SCL da OLED
├── 5V  → VCC da OLED (confira se o seu módulo aceita 5 V) e extremo do potenciômetro
└── GND → GND comum (OLED, potenciômetro, botões, referência do sinal)
```

---

## 🧰 Circuito de Entrada

```
                          +5V
                           │
                          [R1 10k]
                           │
 SINAL ──[C1 1µF]──────────┼────[R3 1k]────┬────────── A0
                           │                │
                          [R2 10k]       [Cf 4,7nF]   (opcional: 1N4148 para
                           │                │          +5V e para GND em A0)
                          GND              GND
```

* **C1** bloqueia a componente DC do sinal medido; **R1/R2** polarizam o nó em 2,5 V para que a metade negativa da onda não seja cortada.
* **R3 + Cf** formam um filtro passa-baixa (fc ≈ 34 kHz, próximo da metade da taxa de amostragem) e limitam a corrente de entrada.
* Para **acoplamento DC** (medir tensão contínua de 0 a 5 V), faça uma ponte sobre C1 e retire R1/R2.
* Para sinais maiores que 5 Vpp, use antes um divisor resistivo (por exemplo 10:1).

---

## 🖼️ Esquemático

O arquivo `Esquemático/mini-osciloscopio.asc` abre no **LTspice** e traz o **esquema completo de montagem**: circuito de entrada, Arduino Uno, OLED I2C, potenciômetro de escala de tempo (A1), botões HOLD (D2) e borda do trigger (D3), saída de autoteste (D8) e alimentação, com uma legenda "COMO MONTAR" na própria folha. O Arduino, a OLED, o potenciômetro e o botão são símbolos próprios (`.asy`) e a simulação usa modelos simplificados (`modelos_simulacao.lib`); **mantenha todos esses arquivos na mesma pasta** `Esquemático/`.

![Simulação](circuit_images/image_simulador.png)
![Circuito Real](circuit_images/Circuito_real.jpg)

---

## 🖥️ Tela

```
230us 1.00kHz 4.9Vpp /      ← escala · frequência · Vpp · status
┌┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┐
│         ╱╲        ╱╲     │   grade: 8 divisões × 4 divisões
│        ╱  ╲      ╱  ╲    │   (1 divisão horizontal = 16 amostras)
│   ╲   ╱    ╲    ╱    ╲   │
│    ╲_╱      ╲__╱      ╲_ │
└┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┘
```

| Status (último caractere) | Significado |
|:---:|---|
| `/` | Trigger em borda de subida |
| `\` | Trigger em borda de descida |
| `A` | Modo AUTO: nenhum cruzamento encontrado, exibe a captura livre |
| `H` | Tela congelada (HOLD) |

---

## 📊 Especificações

| Parâmetro | Valor |
|---|---|
| Resolução vertical | 8 bits (5 V / 256 ≈ 19,5 mV por passo) |
| Faixa de entrada | 0–5 V (±2,5 V em acoplamento AC) |
| Taxa máxima de amostragem | ≈ 70 kS/s (conversões consecutivas) |
| Amostras por tela | 128 (16 por divisão) |
| Trigger | Borda de subida/descida no nível de 2,5 V, com timeout e modo AUTO |
| Medidas | Frequência (interpolação sub-amostra) e tensão pico a pico |

| Posição do pot. | Intervalo entre amostras | Escala | Janela total | Frequência mínima medida |
|:---:|:---:|:---:|:---:|:---:|
| 0 | ≈ 14 µs | 230 µs/div | 1,9 ms | ≈ 540 Hz |
| 1 | 20 µs | 320 µs/div | 2,6 ms | ≈ 390 Hz |
| 2 | 50 µs | 800 µs/div | 6,4 ms | ≈ 160 Hz |
| 3 | 100 µs | 1,6 ms/div | 12,8 ms | ≈ 80 Hz |
| 4 | 200 µs | 3,2 ms/div | 25,6 ms | ≈ 40 Hz |
| 5 | 500 µs | 8 ms/div | 64 ms | ≈ 16 Hz |
| 6 | 1 ms | 16 ms/div | 128 ms | ≈ 8 Hz |
| 7 | 2 ms | 32 ms/div | 256 ms | ≈ 4 Hz |

> A frequência só é calculada quando a janela contém pelo menos um período completo além do ponto de trigger. Na escala mais rápida o intervalo real entre amostras é medido a cada captura, então as medidas não dependem do valor nominal da tabela.

---

## 💻 Como Funciona

### ADC em alta velocidade

Por padrão a biblioteca Arduino usa prescaler 128 (≈ 9,6 kS/s). O sketch reconfigura o ADC para o prescaler 16 e lê só os 8 bits mais significativos (`ADLAR = 1`, resultado em `ADCH`):

```cpp
ADMUX  = _BV(REFS0) | _BV(ADLAR);   // AVcc, 8 bits em ADCH, canal A0
ADCSRA = _BV(ADEN)  | _BV(ADPS2);   // ADC ligado, prescaler 16 (1 MHz)

ADCSRA |= _BV(ADSC);                // inicia a conversão
while (ADCSRA & _BV(ADSC)) { }      // espera (~13 ciclos de ADC)
uint8_t v = ADCH;
```

Com o clock do ADC acima de 200 kHz a precisão efetiva cai um pouco, mas 8 bits continuam úteis para exibir uma forma de onda.

### Base de tempo

Para as escalas mais lentas, o sketch agenda cada amostra por tempo absoluto (`proximo += intervalo`), o que evita acumular erro de fase. Na escala mais rápida não há espera: as conversões são encadeadas.

### Trigger por software

Antes de gravar a janela, o código lê amostras até encontrar um cruzamento do nível de 128 (≈ 2,5 V) na borda escolhida. Se nenhum cruzamento aparecer em 400 tentativas, o osciloscópio entra em modo AUTO e mostra a captura livre, como um osciloscópio real. A amostra do trigger vira a posição 0 da tela, o que mantém ondas periódicas estáveis.

### Medição de frequência

Os cruzamentos pelo ponto médio (com histerese de 1/8 da excursão) são localizados por interpolação linear entre duas amostras. O período é a média entre o primeiro e o último cruzamento, e a frequência sai do período multiplicado pelo intervalo real medido entre amostras.

### Orçamento de memória

| Recurso | Uso aproximado |
|---|---|
| Buffer da OLED (Adafruit SSD1306) | 1024 bytes de RAM |
| Buffer de amostras | 128 bytes |
| Demais variáveis e tabelas | menos de 100 bytes |

Textos fixos usam `F()` e ficam na Flash. Confira o relatório da Arduino IDE após compilar.

---

## 🗂️ Estrutura dos Arquivos

```
mini-osciloscopio/
├── README.md                              # Esta documentação
├── post_linkedin.txt                      # Rascunho de post
├── Esquemático/
│   ├── mini-osciloscopio.asc              # Esquema completo (LTspice)
│   ├── UNO_OSC.asy, OLED_I2C.asy          # Símbolos do Arduino e da OLED
│   ├── POT10K.asy, BTN.asy                # Símbolos do potenciômetro e do botão
│   └── modelos_simulacao.lib              # Modelos simplificados p/ simulação
├── sketch_mini-osciloscopio/
│   └── sketch_mini-osciloscopio.ino       # Firmware completo
└── circuit_images/
    ├── image_simulador.png                # Captura da simulação
    └── Circuito_real.jpg                  # Foto da montagem
```

---

## 🚀 Como Usar

1. **Instale as bibliotecas** pelo Gerenciador de Bibliotecas da Arduino IDE: `Adafruit GFX Library`, `Adafruit SSD1306` e `Adafruit BusIO`.
2. **Monte o circuito** conforme a pinagem e o circuito de entrada.
3. **Abra o sketch:** `sketch_mini-osciloscopio/sketch_mini-osciloscopio.ino`.
4. **Selecione a placa:** `Tools → Board → Arduino Uno` e a porta COM correspondente.
5. **Faça upload** (`Ctrl+U`).
6. **Autoteste:** ligue o pino **D8 ao nó de bias (entre C1 e R1/R2) passando por um resistor de 10 kΩ**, como está no esquemático. O display deve mostrar uma onda quadrada de ≈ 1 kHz. Para desligar o gerador de teste, coloque `#define ENABLE_TESTE 0`.
7. **Meça seus sinais:** ligue a ponta de prova (e a referência no GND comum), ajuste a escala no potenciômetro, use **D2** para congelar a tela e **D3** para trocar a borda do trigger.

---

## ⚠️ Segurança e Limitações

* Nunca aplique em A0 mais de 5 V nem tensão negativa sem o circuito de entrada; **não meça a rede elétrica** (110/220 V).
* O GND do sinal medido precisa ser o mesmo GND do Arduino.
* É um instrumento didático: 8 bits, banda útil de poucos kHz e sem proteção de grau industrial.
* Nas escalas lentas o ADC amostra bem abaixo da taxa de Nyquist do filtro de entrada, portanto componentes de alta frequência podem aparecer distorcidas (aliasing), como em qualquer osciloscópio digital sem filtro adaptativo.

---

## 🔧 Personalização

* **Nível de trigger:** altere `NIVEL_TRIGGER` (0–255).
* **Escalas de tempo:** edite o vetor `INTERVALO_US[]` e os textos em `imprimirTdiv()`.
* **Endereço da OLED:** `OLED_ENDERECO` (`0x3C` ou `0x3D`).
* **Mais velocidade:** com prescaler 8 o ADC chega a ≈ 150 kS/s, com perda adicional de precisão.

*Desenvolvido por Felipe Grolla*

