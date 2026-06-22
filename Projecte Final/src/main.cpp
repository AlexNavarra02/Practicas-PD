#include <Arduino.h>
#include <driver/i2s.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SAMPLE_RATE 44100
#define I2S_BCLK 18
#define I2S_LRC  16
#define I2S_DOUT 17
#define NUM_VOICES 8
#define BUFFER_SIZE 256

// CONFIGURACIÓN PANTALLA OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define SINE_LUT_SIZE 2048
float sineTable[SINE_LUT_SIZE];
const float RAD_TO_LUT = SINE_LUT_SIZE / (2.0f * PI);

struct Instrument {
    const float *harm;
    uint8_t count;
    const char* name;
};

const float CONTRABAIX[] = {1.0,0.5,0.5,0.5,0.2};
const float CLARINET[]   = {1.0,0.8,0.1,0.3,0.3,0.1};
const float FLAUTA[]     = {1.0,0.25,0.1};
const float CLARINETE[]  = {0.5,0.45,0.5,0.6,1.0,1.0,0.4,0.3,0.1};
const float VIOLIN[]     = {0.5,0.25,1.0,0.6,0.4,0.2};
const float PIANO[]      = {1.0,0.25,0.1,0.0,0.1};

Instrument instrumentos[] = {
    {CONTRABAIX, sizeof(CONTRABAIX)/sizeof(float), "CONTRABAJO"},
    {CLARINET, sizeof(CLARINET)/sizeof(float), "CLARINETE 1"},
    {FLAUTA, sizeof(FLAUTA)/sizeof(float), "FLAUTA"},
    {CLARINETE, sizeof(CLARINETE)/sizeof(float), "CLARINETE 2"},
    {VIOLIN, sizeof(VIOLIN)/sizeof(float), "VIOLIN"},
    {PIANO, sizeof(PIANO)/sizeof(float), "PIANO"}
};

volatile uint8_t instrumentoActual = 0;

struct Voice {
    bool active;
    float freq;
    float phase;
    float env;
    float attack;
    float release;
    uint32_t age;
};

Voice voices[NUM_VOICES];

// Variables globales para la animación visual
volatile float freqGrafica = 0.0f; 
float faseVisual = 0.0f; 

float teclaToFreq(char tecla) {
    switch(tecla) {
        case 'z': return 261.63; case 's': return 277.18; case 'x': return 293.66;
        case 'd': return 311.13; case 'c': return 329.63; case 'v': return 349.23;
        case 'g': return 369.99; case 'b': return 392.00; case 'h': return 415.30;
        case 'n': return 440.00; case 'j': return 466.16; case 'm': return 493.88;
        case ',': return 523.25;
    }
    return 0;
}

void setupI2S() {
    i2s_config_t config = {};
    config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    config.sample_rate = SAMPLE_RATE;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT; 
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    
    // Corregido para evitar 'deprecated' warning en ESP32-S3
    config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S); 
    
    config.dma_buf_count = 8;
    config.dma_buf_len = 256;
    config.use_apll = false;

    i2s_pin_config_t pins = {};
    pins.bck_io_num = I2S_BCLK;
    pins.ws_io_num = I2S_LRC;
    pins.data_out_num = I2S_DOUT;
    pins.data_in_num = I2S_PIN_NO_CHANGE;

    i2s_driver_install(I2S_NUM_0, &config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pins);
}

void noteOn(float freq) {
    freqGrafica = freq; 
    for(int i=0; i<NUM_VOICES; i++) {
        if(!voices[i].active) {
            voices[i].active = true;
            voices[i].freq = freq;
            voices[i].phase = 0;
            voices[i].env = 0;
            voices[i].age = 0; 
            voices[i].attack = 0.002f;
            voices[i].release = 0.99992f;
            return;
        }
    }
}

void audioTask(void*) {
    int32_t buffer[BUFFER_SIZE*2];

    while(true) {
        int active_voices = 0;
        float ultimaFreqActiva = 0;
        
        for(int v=0; v<NUM_VOICES; v++) {
            if(voices[v].active) {
                active_voices++;
                ultimaFreqActiva = voices[v].freq;
            }
        }
        
        if(active_voices == 0) {
            freqGrafica = 0;
            active_voices = 1;
        } else {
            freqGrafica = ultimaFreqActiva;
        }

        float master_gain = 0.35f / sqrtf((float)active_voices);

        for(int i=0; i<BUFFER_SIZE; i++) {
            float mix = 0;
            Instrument &inst = instrumentos[instrumentoActual];

            for(int v=0; v<NUM_VOICES; v++) {
                if(!voices[v].active) continue;

                Voice &voice = voices[v];

                if (voice.age == 0) {
                    voice.env += voice.attack;
                    if (voice.env >= 1.0f) {
                        voice.env = 1.0f;
                        voice.age = 1; 
                    }
                } else {
                    voice.env *= voice.release; 
                    if (voice.env < 0.005f) {
                        voices[v].active = false; 
                        continue;
                    }
                }

                float sample = 0;
                for(int h=0; h<inst.count; h++) {
                    float angle = voice.phase * (h+1);
                    int index = (int)(angle * RAD_TO_LUT) % SINE_LUT_SIZE;
                    if (index < 0) index += SINE_LUT_SIZE;
                    
                    sample += inst.harm[h] * sineTable[index];
                }

                sample *= voice.env;
                mix += sample;

                voice.phase += 2.0f * PI * voice.freq / SAMPLE_RATE;
                if(voice.phase > 2*PI) voice.phase -= 2*PI;
            }

            mix *= master_gain;

            if (mix > 1.0f) mix = 1.0f;
            else if (mix < -1.0f) mix = -1.0f;
            else mix = mix * (1.5f - 0.5f * mix * mix);

            int32_t pcm = (int32_t)(mix * 2147483647.0f);
            
            buffer[i*2] = pcm;
            buffer[i*2+1] = pcm;
        }

        size_t written;
        i2s_write(I2S_NUM_0, buffer, sizeof(buffer), &written, portMAX_DELAY);
    }
}

void setup() {
    Serial.begin(115200);

    // Mapeo I2C exclusivo para tu ESP32-S3 en pines 35 y 36
    Wire.begin(35, 36); 

    // Inicializar Pantalla OLED
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
        Serial.println(F("OLED no encontrado"));
    }
    display.clearDisplay();
    display.display();

    for (int i = 0; i < SINE_LUT_SIZE; i++) {
        sineTable[i] = sinf((2.0f * PI * i) / SINE_LUT_SIZE);
    }

    setupI2S();

    xTaskCreatePinnedToCore(audioTask, "audio", 4096, NULL, 5, NULL, 1);

    Serial.println("\nSintetizador S3 Listo.");
}

void loop() {
    // 1. CAPTURAR SERIAL (Teclado)
    if(Serial.available()) {
        char c = Serial.read();
        if(c>='1' && c<='6') {
            instrumentoActual = c-'1';
            Serial.print("Instrumento "); Serial.println(c);
        } else {
            float freq = teclaToFreq(c);
            if(freq>0) {
                noteOn(freq);
                Serial.print("Nota "); Serial.println(freq);
            }
        }
    }

    // 2. CONTROL DE LA PANTALLA
    display.clearDisplay();
    
    // --- ONDA EN EL MEDIO (ZONA AZUL) ---
    int mitadY = 34;     // Centrado perfecto en la zona azul superior
    int alturaMax = 16;   // Amplitud óptima
    
    int ultimoX = 0;
    int ultimoY = mitadY;

    // Zoom ampliado (0.6f) para meter múltiples ciclos estéticos en pantalla
    float multiplicadorFrecuencia = (freqGrafica > 0) ? (freqGrafica / 100.0f) : 0.0f;

    for(int x = 0; x < SCREEN_WIDTH; x++) {
        int y = mitadY;
        
        if (freqGrafica > 0) {
            y = mitadY + (int)(sinf(x * 0.6f * multiplicadorFrecuencia - faseVisual) * alturaMax);
        }
        
        if(x > 0) {
            display.drawLine(ultimoX, ultimoY, x, y, SSD1306_WHITE);
        }
        
        ultimoX = x;
        ultimoY = y;
    }

    // --- TEXTOS ABAJO (ZONA AZUL INFERIOR) ---
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    display.setCursor(0, 5);
    display.print("INST: ");
    display.print(instrumentos[instrumentoActual].name);
    
    display.setCursor(0, 56);
    display.print("FREQ: ");
    if(freqGrafica > 0) {
        display.print(freqGrafica, 1);
        display.print(" Hz");
    } else {
        display.print("SILENCIO");
    }

    // Animación fluida de desplazamiento horizontal
    if(freqGrafica > 0) {
        faseVisual += 0.4f; 
    }

    display.display();
    delay(15); 
}
