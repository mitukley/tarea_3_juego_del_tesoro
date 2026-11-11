#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Configuración de Comunicación y Pantalla
SoftwareSerial serialEsclavo(10, 11); // Pines para hablar con el Esclavo
LiquidCrystal_I2C lcd(0x27, 16, 2);   // Pantalla LCD I2C

// Los 4 estados posibles del juego
enum Estado { INACTIVO, JUGANDO, VICTORIA, DERROTA };
Estado estadoActual = INACTIVO;

// Pines y Variables del Juego
const int PIN_LED_VERDE = 6;          // LED que se apaga durante los 60 segundos
int tesoroX = 0, tesoroY = 0;         // Dónde está escondido el tesoro
int jugadorX = 0, jugadorY = 0;       // Dónde está el jugador (según el joystick)
unsigned long tiempoInicio = 0;       // Cuándo empezó la partida
unsigned long tiempoEnCelda = 0;      // Cuenta los 2 segundos necesarios para ganar
unsigned long ultimoMensaje = 0;      // Control para detectar si se desconectó el esclavo

// Función para saber qué tan cerca estás (0=Frío, 1=Tibio, 2=Caliente, 3=Aquí)
uint8_t calcularNivelProximidad(int distancia) {
  if (distancia > 10) return 0; // Frío
  if (distancia >= 5) return 1; // Tibio
  if (distancia >= 1) return 2; // Caliente
  return 3;                     // ¡Aquí! (distancia 0)
}

void setup() {
  pinMode(PIN_LED_VERDE, OUTPUT);
  serialEsclavo.begin(9600);
  randomSeed(analogRead(A3)); // Genera posiciones aleatorias distintas cada vez
  lcd.init(); 
  lcd.backlight();
}

void loop() {
  unsigned long ahora = millis();

  // 1. LEER LO QUE MANDA EL ESCLAVO (Joystick y Botón de Inicio)
  while (serialEsclavo.available()) {
    ultimoMensaje = ahora;
    uint8_t dato = serialEsclavo.read();
    
    if (dato == 'X' && serialEsclavo.available()) jugadorX = serialEsclavo.read();
    if (dato == 'Y' && serialEsclavo.available()) jugadorY = serialEsclavo.read();
    
    // Si recibe el comando 250 (presionaron el botón) y estamos inactivos -> ¡Inicia el juego!
    if (dato == 250 && estadoActual == INACTIVO) {
      tesoroX = random(12, 101); // Esconde el tesoro al azar
      tesoroY = random(12, 101);
      tiempoInicio = ahora;
      tiempoEnCelda = 0;
      estadoActual = JUGANDO;
    }
  }

  // Si pasan más de 2 segundos sin recibir datos, se corta la comunicación
  if (ahora - ultimoMensaje > 2000 && estadoActual == JUGANDO) {
    estadoActual = INACTIVO;
  }

  // 2. CONTROL DEL JUEGO SEGÚN EL ESTADO ACTUAL
  switch (estadoActual) {

    case INACTIVO:
      analogWrite(PIN_LED_VERDE, 0);
      serialEsclavo.write(251); // Avisa al esclavo que estamos esperando
      lcd.setCursor(0, 0); lcd.print(" BUSQUEDA TESORO");
      lcd.setCursor(0, 1); lcd.print("Presione Inicio ");
      break;

    case JUGANDO: {
      unsigned long tiempoTranscurrido = ahora - tiempoInicio;
      
      // Si pasan 60 segundos (60000 ms) -> Perdiste
      if (tiempoTranscurrido >= 60000) {
        estadoActual = DERROTA;
        break;
      }

      // El LED verde se va apagando poco a poco (de 255 a 0)
      int brilloLED = map(60000 - tiempoTranscurrido, 0, 60000, 0, 255);
      analogWrite(PIN_LED_VERDE, brilloLED);

      // Calcular distancia al tesoro
      int distanciaX = abs(jugadorX - tesoroX);
      int distanciaY = abs(jugadorY - tesoroY);

      // Enviar las pistas de proximidad al esclavo para que encienda sus LEDs
      serialEsclavo.write('L');
      serialEsclavo.write(calcularNivelProximidad(distanciaX));
      serialEsclavo.write(calcularNivelProximidad(distanciaY));

      // CONDICIÓN DE VICTORIA: Estar en la zona del tesoro por 2 segundos seguidos
      if ((jugadorX == tesoroX || jugadorX == tesoroX - 1) && 
          (jugadorY == tesoroY || jugadorY == tesoroY - 1)) {
        if (tiempoEnCelda == 0) tiempoEnCelda = ahora;
        if (ahora - tiempoEnCelda >= 2000) estadoActual = VICTORIA;
      } else {
        tiempoEnCelda = 0; // Si te sales, se reinicia el contador
      }

      // Mostrar en la pantalla LCD las coordenadas y los segundos que quedan
      int segundosRestantes = (60000 - tiempoTranscurrido) / 1000;
      lcd.setCursor(0, 0); lcd.print("X:" + String(jugadorX) + " Y:" + String(jugadorY) + "     ");
      lcd.setCursor(0, 1); lcd.print("T:" + String(segundosRestantes) + "s JUGANDO   ");
      break;
    }

    case VICTORIA:
      analogWrite(PIN_LED_VERDE, 255);
      serialEsclavo.write(252); // Avisa victoria al esclavo
      lcd.setCursor(0, 0); lcd.print("  ¡GANASTE! :)  ");
      lcd.setCursor(0, 1); lcd.print("                ");
      delay(2500);
      estadoActual = INACTIVO;
      break;

    case DERROTA:
      analogWrite(PIN_LED_VERDE, 0);
      serialEsclavo.write(253); // Avisa derrota al esclavo
      lcd.setCursor(0, 0); lcd.print("  TIEMPO AGOTADO");
      lcd.setCursor(0, 1); lcd.print("    DERROTA :(  ");
      delay(2500);
      estadoActual = INACTIVO;
      break;
  }
}