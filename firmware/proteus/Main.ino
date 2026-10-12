//---CONFIG_BEGIN---
#pragma GCC push_options
#pragma GCC optimize ("Os")

#include <Core.h>
#include <Cpu.h>

#pragma GCC pop_options

CPU &CPU = Cpu;

void peripheral_setup () {
}

void peripheral_loop() {
}
//---CONFIG_END---

#pragma GCC push_options
#pragma GCC optimize ("O1")

#include <LiquidCrystal.h>
#include <Servo.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define SIMULATION 1

#if SIMULATION
#define VREF_MV 5000.0f
#define V_ECHO_MV 3333.0f
#else
#define VREF_MV 3300.0f
#define V_ECHO_MV 3000.0f
#endif

#define ADC_MAX 4095.0f
#define SEUIL_ADC ((int)(V_ECHO_MV * 0.5f * ADC_MAX / VREF_MV))

#define NB_VOIES 3
#define D_MIN_CM 2.0f
#define D_MAX_CM 400.0f
#define TIMEOUT_MONTEE_US 5000UL
#define TIMEOUT_ECHO_US 25000UL
#define PAUSE_ENTRE_VOIES_MS 60

#define REF_CM 0.0f
#define N_CAL 20
#define N_FILTRE 8
#define K_SIGMA 3.0f
#define ECART_MIN_CM 5.0f
#define N_REJET_MAX 3
#define N_PANNE 5

#define T_DEFAUT 20.0f
#define T_MIN -10.0f
#define T_MAX 60.0f
#define N_MOY_T 16

#define LM35_PIN PA3
#define SERVO_PIN PB8
#define UART Serial1

const int TRIG[NB_VOIES] = {PB0, PB1, PB10};
const int ECHO[NB_VOIES] = {PA0, PA1, PA2};
const int ANGLE[NB_VOIES] = {0, 120, 240};
const int ANGLE_SERVO[NB_VOIES] = {90, 150, 30};

enum Etat { OK, HORS_PORTEE, PANNE };
const char *NOM_ETAT[] = {"OK", "HORS", "PANNE"};

LiquidCrystal lcd(PA4, PA5, PA6, PA7, PB6, PB7);
Servo servo;

struct Voie {
  float offset;
  float ecartMax;
  float tampon[N_FILTRE];
  int nb;
  int idx;
  int rejetsConsecutifs;
  int echecsConsecutifs;
  unsigned long rejetsTotal;
  Etat etat;
  float distance;
};

// Prototypes explicites : le générateur de prototypes de Proteus les place
// avant la définition de Etat et Voie, ce qui fait échouer la compilation.
float mesureBrute(int i, Etat *etat);
float moyenne(const Voie &v);
void ajouter(Voie &v, float d);
void reinitialiser(Voie &v, float d);

Voie voie[NB_VOIES];
float temperature = T_DEFAUT;
bool temperatureValide = false;

float lireTemperature() {
  long somme = 0;
  for (int k = 0; k < N_MOY_T; k++) somme += analogRead(LM35_PIN);
  float t = (somme / (float)N_MOY_T) * (VREF_MV / ADC_MAX) / 10.0f;
  if (t < T_MIN || t > T_MAX) {
    temperatureValide = false;
    return temperature;
  }
  temperatureValide = true;
  return t;
}

float vitesseSon() {
  return 331.3f + 0.606f * temperature;
}

long largeurEcho(int i) {
  digitalWrite(TRIG[i], LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG[i], HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG[i], LOW);

  unsigned long t0 = micros();
  while (analogRead(ECHO[i]) < SEUIL_ADC) {
    if (micros() - t0 > TIMEOUT_MONTEE_US) return -1;
  }
  unsigned long debut = micros();
  while (analogRead(ECHO[i]) >= SEUIL_ADC) {
    if (micros() - debut > TIMEOUT_ECHO_US) return -2;
  }
  return (long)(micros() - debut);
}

float mesureBrute(int i, Etat *etat) {
  long t = largeurEcho(i);
  if (t == -1) {
    *etat = PANNE;
    return -1;
  }
  if (t == -2) {
    *etat = HORS_PORTEE;
    return -1;
  }
  float d = t * vitesseSon() / 20000.0f;
  if (d < D_MIN_CM || d > D_MAX_CM) {
    *etat = HORS_PORTEE;
    return -1;
  }
  *etat = OK;
  return d;
}

float moyenne(const Voie &v) {
  float s = 0;
  for (int k = 0; k < v.nb; k++) s += v.tampon[k];
  return s / v.nb;
}

void ajouter(Voie &v, float d) {
  v.tampon[v.idx] = d;
  v.idx = (v.idx + 1) % N_FILTRE;
  if (v.nb < N_FILTRE) v.nb++;
}

void reinitialiser(Voie &v, float d) {
  v.nb = 0;
  v.idx = 0;
  ajouter(v, d);
}

void calibrer(int i) {
  Voie &v = voie[i];
  float valeurs[N_CAL];
  int n = 0;
  int montees = 0;
  for (int k = 0; k < N_CAL; k++) {
    Etat e;
    float d = mesureBrute(i, &e);
    if (e != PANNE) montees++;
    if (e == OK) valeurs[n++] = d;
    delay(PAUSE_ENTRE_VOIES_MS);
  }

  v.offset = 0;
  v.ecartMax = ECART_MIN_CM;
  v.nb = 0;
  v.idx = 0;
  v.rejetsConsecutifs = 0;
  v.echecsConsecutifs = 0;
  v.rejetsTotal = 0;
  v.distance = -1;

  if (montees == 0) {
    v.etat = PANNE;
    return;
  }
  if (n < 2) {
    v.etat = HORS_PORTEE;
    return;
  }

  float m = 0;
  for (int k = 0; k < n; k++) m += valeurs[k];
  m /= n;
  float var = 0;
  for (int k = 0; k < n; k++) var += (valeurs[k] - m) * (valeurs[k] - m);
  float sigma = sqrtf(var / (n - 1));

  if (REF_CM > 0) v.offset = m - REF_CM;
  v.ecartMax = fmaxf(K_SIGMA * sigma, ECART_MIN_CM);
  v.etat = OK;
  reinitialiser(v, m - v.offset);
  v.distance = moyenne(v);
}

void mettreAJour(int i) {
  Voie &v = voie[i];
  Etat e;
  float d = mesureBrute(i, &e);

  if (e == PANNE) {
    v.echecsConsecutifs++;
    if (v.echecsConsecutifs >= N_PANNE) {
      v.etat = PANNE;
      v.distance = -1;
    }
    return;
  }
  v.echecsConsecutifs = 0;

  if (e == HORS_PORTEE) {
    v.etat = HORS_PORTEE;
    v.distance = -1;
    v.nb = 0;
    v.idx = 0;
    return;
  }

  d -= v.offset;
  v.etat = OK;

  if (v.nb < N_FILTRE) {
    ajouter(v, d);
  } else if (fabsf(d - moyenne(v)) <= v.ecartMax) {
    ajouter(v, d);
    v.rejetsConsecutifs = 0;
  } else {
    v.rejetsTotal++;
    v.rejetsConsecutifs++;
    if (v.rejetsConsecutifs >= N_REJET_MAX) {
      reinitialiser(v, d);
      v.rejetsConsecutifs = 0;
    }
  }
  v.distance = moyenne(v);
}

int voieLaPlusProche() {
  int dir = -1;
  float dmin = 1e9;
  for (int i = 0; i < NB_VOIES; i++) {
    if (voie[i].etat == OK && voie[i].distance > 0 && voie[i].distance < dmin) {
      dmin = voie[i].distance;
      dir = i;
    }
  }
  return dir;
}

void texteVoie(int i, char *s) {
  if (voie[i].etat == PANNE) strcpy(s, " HS");
  else if (voie[i].etat == HORS_PORTEE) strcpy(s, " --");
  else snprintf(s, 4, "%3d", (int)lroundf(voie[i].distance));
}

void ecrireLigne(int rang, const char *texte) {
  lcd.setCursor(0, rang);
  int n = 0;
  while (texte[n] && n < 16) lcd.print(texte[n++]);
  while (n++ < 16) lcd.print(' ');
}

void afficherLCD(int dir) {
  char a[4], b[4], c[4], d[4], ligne[40];
  texteVoie(0, a);
  texteVoie(1, b);
  texteVoie(2, c);
  snprintf(ligne, sizeof(ligne), "%s %s %s cm", a, b, c);
  ecrireLigne(0, ligne);

  int t10 = (int)lroundf(temperature * 10);
  char signe = t10 < 0 ? '-' : ' ';
  t10 = abs(t10);
  if (dir >= 0) snprintf(d, sizeof(d), "%d", ANGLE[dir]);
  else strcpy(d, "--");
  snprintf(ligne, sizeof(ligne), "T%c%d.%dC%s D:%s", signe, t10 / 10, t10 % 10, temperatureValide ? "" : "?", d);
  ecrireLigne(1, ligne);
}

void envoyerUART(int dir) {
  UART.print("T=");
  UART.print(temperature, 1);
  UART.print(temperatureValide ? "C" : "C(defaut)");
  for (int i = 0; i < NB_VOIES; i++) {
    UART.print(" | ");
    UART.print(ANGLE[i]);
    UART.print(":");
    if (voie[i].etat == OK) {
      UART.print(voie[i].distance, 1);
      UART.print("cm");
    } else {
      UART.print(NOM_ETAT[voie[i].etat]);
    }
    UART.print(" rej=");
    UART.print(voie[i].rejetsTotal);
  }
  UART.print(" | dir=");
  if (dir >= 0) UART.println(ANGLE[dir]);
  else UART.println("--");
}

void setup() {
#if SIMULATION
  peripheral_setup();
#endif
#if defined(ARDUINO_ARCH_STM32)
  analogReadResolution(12);
#endif
  UART.begin(9600);
  lcd.begin(16, 2);
  servo.attach(SERVO_PIN);
  servo.write(90);
  for (int i = 0; i < NB_VOIES; i++) {
    pinMode(TRIG[i], OUTPUT);
    digitalWrite(TRIG[i], LOW);
  }

  UART.println("CAPTEUR INTELLIGENT 3 VOIES");
  lcd.print("Calibration...");
  temperature = lireTemperature();

  for (int i = 0; i < NB_VOIES; i++) {
    calibrer(i);
    UART.print("Calibration ");
    UART.print(ANGLE[i]);
    UART.print(" : ");
    UART.print(NOM_ETAT[voie[i].etat]);
    UART.print(" offset=");
    UART.print(voie[i].offset, 2);
    UART.print("cm ecartMax=");
    UART.print(voie[i].ecartMax, 2);
    UART.println("cm");
  }
  lcd.clear();
}

void loop() {
#if SIMULATION
  peripheral_loop();
#endif
  temperature = lireTemperature();
  for (int i = 0; i < NB_VOIES; i++) {
    mettreAJour(i);
    delay(PAUSE_ENTRE_VOIES_MS);
  }
  int dir = voieLaPlusProche();
  if (dir >= 0) servo.write(ANGLE_SERVO[dir]);
  afficherLCD(dir);
  envoyerUART(dir);
}

#pragma GCC pop_options
