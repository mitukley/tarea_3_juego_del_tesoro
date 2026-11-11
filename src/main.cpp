#include <Arduino.h>
#include <SoftwareSerial.h>

SoftwareSerial serialMaestro(10, 11); // RX, TX

const int PIN_X = A0;
const int PIN_Y = A1;
const int PIN_BOTON = 2;
const int PIN_LED_X = 3;
const int PIN_LED_Y = 5;

unsigned long tEnvio = 0;
unsigned long tDebounce = 0;
unsigned long tUltimoByte = 0;
bool estadoAntBoton = HIGH;

void setup() {
  pinMode(PIN_BOTON, INPUT_PULLUP);
  pinMode(PIN_LED_X, OUTPUT);
  pinMode(PIN_LED_Y, OUTPUT);
  serialMaestro.begin(9600);
  tUltimoByte = millis();
}

void loop() {
  unsigned long ahora = millis();

  // 1. Enviar coordenadas cada 50ms
  if (ahora - tEnvio >= 50) {
    tEnvio = ahora;
    
    // ADC uniforme y estabilización
    analogRead(PIN_X);
    uint8_t x = (analogRead(PIN_X) * 100L) / 1024;
    analogRead(PIN_Y);
    uint8_t y = (analogRead(PIN_Y) * 100L) / 1024;

    serialMaestro.write('X'); 
    serialMaestro.write(x);
    serialMaestro.write('Y'); 
    serialMaestro.write(y);
  }

  // 2. Leer botón de inicio con antirrebote
  bool lecturaBoton = digitalRead(PIN_BOTON);
  if (lecturaBoton == LOW && estadoAntBoton == HIGH && (ahora - tDebounce > 50)) {
    serialMaestro.write(250); // Solicitud de inicio
    tDebounce = ahora;
  }
  estadoAntBoton = lecturaBoton;

  // 3. Recibir datos del Maestro
  while (serialMaestro.available()) {
    tUltimoByte = ahora;
    uint8_t cmd = serialMaestro.read();
    
    if (cmd == 'L' && serialMaestro.available() >= 2) {
      uint8_t nx = serialMaestro.read();
      uint8_t ny = serialMaestro.read();
      
      // Aplicar PWM según nivel (0=0, 1=20, 2=80, 3=255)
      analogWrite(PIN_LED_X, nx == 3 ? 255 : (nx == 2 ? 80 : (nx == 1 ? 20 : 0)));
      analogWrite(PIN_LED_Y, ny == 3 ? 255 : (ny == 2 ? 80 : (ny == 1 ? 20 : 0)));
    } 
    else if (cmd == 252) { // VICTORIA
      for(int i = 0; i < 10; i++) {
        analogWrite(PIN_LED_X, 255); analogWrite(PIN_LED_Y, 255); delay(250);
        analogWrite(PIN_LED_X, 0);   analogWrite(PIN_LED_Y, 0);   delay(250);
      }
    } 
    else if (cmd == 253 || cmd == 251) { // DERROTA o INACTIVO
      analogWrite(PIN_LED_X, 0);
      analogWrite(PIN_LED_Y, 0);
    }
  }

  // 4. Timeout de enlace (2000 ms) -> Parpadeo alternado
  if (ahora - tUltimoByte > 2000) {
    bool parpadeo = (ahora / 500) % 2 == 0;
    analogWrite(PIN_LED_X, parpadeo ? 255 : 0);
    analogWrite(PIN_LED_Y, parpadeo ? 0 : 255);
  }
}