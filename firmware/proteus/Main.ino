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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

#define N_AUTOTEST 20
#define N_FILTRE 8
#define K_SIGMA 3.0f
#define ECART_MIN_CM 5.0f
#define N_REJET_MAX 3
#define N_PANNE 5
#define VITESSE_MAX_CM_S 300.0f
#define VITESSE_SUIVI_CM_S 50.0f
#define SIGMA_INSTABLE_CM 3.0f

#define T_DEFAUT 20.0f
#define T_MIN -10.0f
#define T_MAX 60.0f
#define N_MOY_T 16

#define PERIODE_NORMALE_MS 300UL
#define PERIODE_ECO_MS 2000UL
#define N_CYCLES_ECO 10
#define SEUIL_IMMOBILE_CM 2.0f
#define SERVO_MAINTIEN_MS 600UL

#define LM35_PIN PA3
#define SERVO_PIN PB8
#define UART Serial1

#define D_HORS 0x01
#define D_PANNE 0x02
#define D_BLOQUE 0x04
#define D_SAUT 0x08
#define D_INSTABLE 0x10

const int TRIG[NB_VOIES] = {PB0, PB1, PB10};
const int ECHO[NB_VOIES] = {PA0, PA1, PA2};
const int ANGLE[NB_VOIES] = {0, 120, 240};
const int ANGLE_SERVO[NB_VOIES] = {90, 150, 30};

enum Etat { OK, HORS_PORTEE, PANNE };
const char *NOM_ETAT[] = {"OK", "HORS", "PANNE"};
enum Mode { AUTO, NORMAL, ECO };

LiquidCrystal lcd(PA4, PA5, PA6, PA7, PB6, PB7);
Servo servo;

struct Voie {
  float tampon[N_FILTRE];
  int nb;
  int idx;
  float sigma;
  float ecartMax;
  float distance;
  float derniere;
  unsigned long tDerniere;
  int rejetsConsecutifs;
  int echecsConsecutifs;
  int bloquesConsecutifs;
  unsigned long rejetsTotal;
  Etat etat;
  uint8_t defauts;
  bool calibree;
  float kCal;
  float tOff;
  float tCal;
  float t1;
  float d1;
  bool cal1;
};

Voie voie[NB_VOIES];
float temperature = T_DEFAUT;
bool temperatureValide = false;
Mode mode = AUTO;
bool ecoActif = false;
int cyclesImmobiles = 0;
unsigned long periodeNormale = PERIODE_NORMALE_MS;
int dirServo = -1;
bool servoAttache = false;
unsigned long servoDepuis = 0;
char ligneRx[40];
int lgRx = 0;

void veille() {
#if !SIMULATION
  __asm__ volatile("wfi");
#endif
}

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

float vitesseSon(float t) {
  return 331.3f + 0.606f * t;
}

float tempsVersDistance(const Voie &v, float t) {
  if (v.calibree) {
    float k = v.kCal * vitesseSon(v.tCal) / vitesseSon(temperature);
    return (t - v.tOff) / k;
  }
  return t * vitesseSon(temperature) / 20000.0f;
}

long largeurEcho(int i) {
  if (analogRead(ECHO[i]) >= SEUIL_ADC) return -3;
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

float moyenne(const Voie &v) {
  float s = 0;
  for (int k = 0; k < v.nb; k++) s += v.tampon[k];
  return s / v.nb;
}

float bruit(const Voie &v) {
  if (v.nb < 3) return 0;
  float diff[N_FILTRE];
  int n = 0;
  for (int k = 1; k < v.nb; k++) {
    int a = (v.idx - v.nb + k - 1 + 2 * N_FILTRE) % N_FILTRE;
    int b = (v.idx - v.nb + k + 2 * N_FILTRE) % N_FILTRE;
    diff[n++] = v.tampon[b] - v.tampon[a];
  }
  float m = 0, s = 0;
  for (int k = 0; k < n; k++) m += diff[k];
  m /= n;
  for (int k = 0; k < n; k++) s += (diff[k] - m) * (diff[k] - m);
  return sqrtf(s / (n - 1) / 2.0f);
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

float moyenneTemps(int i, int n, int *valides) {
  float s = 0;
  *valides = 0;
  for (int k = 0; k < n; k++) {
    long t = largeurEcho(i);
    if (t > 0) {
      s += t;
      (*valides)++;
    }
    delay(PAUSE_ENTRE_VOIES_MS);
  }
  return *valides ? s / *valides : -1;
}

void autotest(int i) {
  Voie &v = voie[i];
  float d[N_AUTOTEST];
  int n = 0, montees = 0, bloques = 0;
  for (int k = 0; k < N_AUTOTEST; k++) {
    long t = largeurEcho(i);
    if (t == -3) bloques++;
    if (t != -1 && t != -3) montees++;
    if (t > 0) {
      float x = tempsVersDistance(v, t);
      if (x >= D_MIN_CM && x <= D_MAX_CM) d[n++] = x;
    }
    delay(PAUSE_ENTRE_VOIES_MS);
  }

  v.nb = 0;
  v.idx = 0;
  v.rejetsConsecutifs = 0;
  v.echecsConsecutifs = 0;
  v.bloquesConsecutifs = 0;
  v.rejetsTotal = 0;
  v.distance = -1;
  v.derniere = -1;
  v.sigma = 0;
  v.ecartMax = ECART_MIN_CM;
  v.defauts = 0;

  if (bloques == N_AUTOTEST) {
    v.etat = PANNE;
    v.defauts = D_BLOQUE;
    return;
  }
  if (montees == 0) {
    v.etat = PANNE;
    v.defauts = D_PANNE;
    return;
  }
  if (n < 2) {
    v.etat = HORS_PORTEE;
    v.defauts = D_HORS;
    return;
  }

  float m = 0;
  for (int k = 0; k < n; k++) m += d[k];
  m /= n;
  float var = 0;
  for (int k = 0; k < n; k++) var += (d[k] - m) * (d[k] - m);
  v.sigma = sqrtf(var / (n - 1));
  v.ecartMax = fmaxf(K_SIGMA * v.sigma, ECART_MIN_CM);
  v.etat = OK;
  reinitialiser(v, m);
  v.distance = m;
  v.derniere = m;
  v.tDerniere = millis();
}

void mettreAJour(int i) {
  Voie &v = voie[i];
  long t = largeurEcho(i);
  unsigned long maintenant = millis();
  v.defauts &= ~(D_SAUT | D_INSTABLE);

  if (t == -3) {
    v.bloquesConsecutifs++;
    if (v.bloquesConsecutifs >= N_PANNE) {
      v.etat = PANNE;
      v.defauts |= D_BLOQUE;
      v.distance = -1;
    }
    return;
  }
  v.bloquesConsecutifs = 0;
  v.defauts &= ~D_BLOQUE;

  if (t == -1) {
    v.echecsConsecutifs++;
    if (v.echecsConsecutifs >= N_PANNE) {
      v.etat = PANNE;
      v.defauts |= D_PANNE;
      v.distance = -1;
    }
    return;
  }
  v.echecsConsecutifs = 0;
  v.defauts &= ~D_PANNE;

  float d = (t > 0) ? tempsVersDistance(v, t) : -1;
  if (t == -2 || d < D_MIN_CM || d > D_MAX_CM) {
    v.etat = HORS_PORTEE;
    v.defauts |= D_HORS;
    v.distance = -1;
    v.nb = 0;
    v.idx = 0;
    v.derniere = -1;
    return;
  }
  v.defauts &= ~D_HORS;
  v.etat = OK;

  bool accepte = true, saut = false;
  if (v.derniere > 0) {
    float dt = (maintenant - v.tDerniere) / 1000.0f;
    float ecart = fabsf(d - v.derniere);
    if (dt > 0 && ecart / dt > VITESSE_MAX_CM_S) saut = true;
    if (saut || ecart > v.ecartMax + VITESSE_SUIVI_CM_S * dt) accepte = false;
  }

  if (accepte) {
    ajouter(v, d);
    v.rejetsConsecutifs = 0;
    v.derniere = d;
    v.tDerniere = maintenant;
  } else {
    v.rejetsTotal++;
    v.rejetsConsecutifs++;
    if (saut) v.defauts |= D_SAUT;
    if (v.rejetsConsecutifs >= N_REJET_MAX) {
      reinitialiser(v, d);
      v.rejetsConsecutifs = 0;
      v.derniere = d;
      v.tDerniere = maintenant;
    }
  }
  if (v.nb >= N_FILTRE && bruit(v) > SIGMA_INSTABLE_CM) v.defauts |= D_INSTABLE;
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

void piloterServo(int dir) {
  unsigned long maintenant = millis();
  if (dir >= 0 && dir != dirServo) {
    if (!servoAttache) {
      servo.attach(SERVO_PIN);
      servoAttache = true;
    }
    servo.write(ANGLE_SERVO[dir]);
    dirServo = dir;
    servoDepuis = maintenant;
  } else if (servoAttache && maintenant - servoDepuis > SERVO_MAINTIEN_MS) {
    servo.detach();
    servoAttache = false;
  }
}

void gererEnergie(const float *avant) {
  if (mode == NORMAL) {
    ecoActif = false;
    return;
  }
  if (mode == ECO) {
    ecoActif = true;
    return;
  }
  bool immobile = true;
  for (int i = 0; i < NB_VOIES; i++) {
    bool okAvant = avant[i] > 0, okApres = voie[i].distance > 0;
    if (okAvant != okApres) immobile = false;
    else if (okApres && fabsf(voie[i].distance - avant[i]) > SEUIL_IMMOBILE_CM) immobile = false;
  }
  if (immobile) {
    if (cyclesImmobiles < N_CYCLES_ECO) cyclesImmobiles++;
  } else {
    cyclesImmobiles = 0;
  }
  ecoActif = cyclesImmobiles >= N_CYCLES_ECO;
}

bool defautPresent() {
  if (!temperatureValide) return true;
  for (int i = 0; i < NB_VOIES; i++)
    if (voie[i].defauts & (D_PANNE | D_BLOQUE | D_SAUT | D_INSTABLE)) return true;
  return false;
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
  snprintf(ligne, sizeof(ligne), "T%c%d.%dC D:%s %c%c", signe, t10 / 10, t10 % 10, d,
           ecoActif ? 'E' : 'N', defautPresent() ? '!' : ' ');
  ecrireLigne(1, ligne);
}

void envoyerDefauts(uint8_t f) {
  if (f & D_HORS) UART.print("H");
  if (f & D_PANNE) UART.print("P");
  if (f & D_BLOQUE) UART.print("B");
  if (f & D_SAUT) UART.print("S");
  if (f & D_INSTABLE) UART.print("I");
}

void envoyerMesures(int dir) {
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
    if (voie[i].defauts & ~D_HORS) {
      UART.print("[");
      envoyerDefauts(voie[i].defauts & ~D_HORS);
      UART.print("]");
    }
  }
  UART.print(" | dir=");
  if (dir >= 0) UART.print(ANGLE[dir]);
  else UART.print("--");
  UART.println(ecoActif ? " | eco" : " | normal");
}

void envoyerStatut() {
  UART.print("Temperature ");
  UART.print(temperature, 1);
  UART.println(temperatureValide ? " C (valide)" : " C (hors plage, valeur precedente conservee)");
  UART.print("Mode ");
  UART.print(mode == AUTO ? "AUTO" : (mode == NORMAL ? "NORMAL" : "ECO"));
  UART.print(", periode ");
  UART.print(ecoActif ? PERIODE_ECO_MS : periodeNormale);
  UART.println(" ms");
  for (int i = 0; i < NB_VOIES; i++) {
    Voie &v = voie[i];
    UART.print("Voie ");
    UART.print(ANGLE[i]);
    UART.print(" : ");
    UART.print(NOM_ETAT[v.etat]);
    UART.print(", sigma=");
    UART.print(v.sigma, 2);
    UART.print(" cm, rejets=");
    UART.print(v.rejetsTotal);
    UART.print(", defauts=");
    if (v.defauts) envoyerDefauts(v.defauts);
    else UART.print("aucun");
    if (v.calibree) {
      UART.print(", calibree k=");
      UART.print(v.kCal, 3);
      UART.print(" us/cm a ");
      UART.print(v.tCal, 1);
      UART.print(" C, t0=");
      UART.print(v.tOff, 1);
      UART.println(" us");
    } else {
      UART.println(", non calibree (modele theorique)");
    }
  }
}

void envoyerAide() {
  UART.println("Commandes : STAT | CAL1 d [voie] | CAL2 d [voie] | CALRAZ | MODE AUTO|NORMAL|ECO | PER ms | AIDE");
  UART.println("d en cm, voie = 0, 120 ou 240 (toutes si absente)");
}

int indiceVoie(const char *s) {
  if (!s) return -1;
  int a = atoi(s);
  for (int i = 0; i < NB_VOIES; i++)
    if (ANGLE[i] == a) return i;
  return -2;
}

void calibrer(int etape, float dRef, int seule) {
  if (dRef < D_MIN_CM || dRef > D_MAX_CM) {
    UART.println("Distance de reference hors etendue");
    return;
  }
  temperature = lireTemperature();
  for (int i = 0; i < NB_VOIES; i++) {
    if (seule >= 0 && i != seule) continue;
    Voie &v = voie[i];
    int n;
    float t = moyenneTemps(i, N_AUTOTEST, &n);
    UART.print("Voie ");
    UART.print(ANGLE[i]);
    if (n < N_AUTOTEST / 2) {
      UART.println(" : pas assez d'echos valides, calibration refusee");
      continue;
    }
    if (etape == 1) {
      v.t1 = t;
      v.d1 = dRef;
      v.cal1 = true;
      UART.print(" : point 1 enregistre, t=");
      UART.print(t, 1);
      UART.println(" us");
    } else {
      if (!v.cal1 || fabsf(dRef - v.d1) < 10.0f) {
        UART.println(" : faire CAL1 avant, avec un ecart d'au moins 10 cm");
        continue;
      }
      v.kCal = (t - v.t1) / (dRef - v.d1);
      v.tOff = t - v.kCal * dRef;
      v.tCal = temperature;
      v.calibree = v.kCal > 0;
      UART.print(v.calibree ? " : calibree, k=" : " : pente negative, calibration refusee, k=");
      UART.print(v.kCal, 3);
      UART.print(" us/cm, t0=");
      UART.print(v.tOff, 1);
      UART.println(" us");
      v.nb = 0;
      v.idx = 0;
      v.derniere = -1;
    }
  }
}

void executer(char *cmd) {
  for (char *p = cmd; *p; p++) *p = toupper(*p);
  char *mot = strtok(cmd, " ");
  if (!mot) return;
  char *a1 = strtok(NULL, " ");
  char *a2 = strtok(NULL, " ");
  if (!strcmp(mot, "STAT")) envoyerStatut();
  else if (!strcmp(mot, "AIDE") || !strcmp(mot, "HELP")) envoyerAide();
  else if (!strcmp(mot, "CAL1") || !strcmp(mot, "CAL2")) {
    if (!a1) {
      envoyerAide();
      return;
    }
    int seule = indiceVoie(a2);
    if (seule == -2) {
      UART.println("Voie inconnue");
      return;
    }
    calibrer(mot[3] - '0', atof(a1), seule);
  } else if (!strcmp(mot, "CALRAZ")) {
    for (int i = 0; i < NB_VOIES; i++) {
      voie[i].calibree = false;
      voie[i].cal1 = false;
    }
    UART.println("Calibrations effacees, retour au modele theorique");
  } else if (!strcmp(mot, "MODE") && a1) {
    if (!strcmp(a1, "AUTO")) mode = AUTO;
    else if (!strcmp(a1, "NORMAL")) mode = NORMAL;
    else if (!strcmp(a1, "ECO")) mode = ECO;
    else {
      envoyerAide();
      return;
    }
    cyclesImmobiles = 0;
    ecoActif = (mode == ECO);
    UART.print("Mode ");
    UART.println(a1);
  } else if (!strcmp(mot, "PER") && a1) {
    long p = atol(a1);
    if (p < 200 || p > 10000) {
      UART.println("Periode entre 200 et 10000 ms");
      return;
    }
    periodeNormale = p;
    UART.print("Periode normale ");
    UART.print(p);
    UART.println(" ms");
  } else {
    envoyerAide();
  }
}

void lireCommandes() {
  while (UART.available()) {
    char c = UART.read();
    if (c == '\r' || c == '\n') {
      if (lgRx > 0) {
        ligneRx[lgRx] = 0;
        executer(ligneRx);
        lgRx = 0;
      }
    } else if (lgRx < (int)sizeof(ligneRx) - 1) {
      ligneRx[lgRx++] = c;
    }
  }
}

void attendreJusqua(unsigned long fin) {
  while ((long)(fin - millis()) > 0) {
    lireCommandes();
    veille();
  }
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
  for (int i = 0; i < NB_VOIES; i++) {
    pinMode(TRIG[i], OUTPUT);
    digitalWrite(TRIG[i], LOW);
    voie[i].calibree = false;
    voie[i].cal1 = false;
  }

  UART.println("CAPTEUR INTELLIGENT 3 VOIES");
  lcd.print("Autotest...");
  temperature = lireTemperature();

  for (int i = 0; i < NB_VOIES; i++) {
    autotest(i);
    UART.print("Autotest ");
    UART.print(ANGLE[i]);
    UART.print(" : ");
    UART.print(NOM_ETAT[voie[i].etat]);
    if (voie[i].defauts) {
      UART.print(" [");
      envoyerDefauts(voie[i].defauts);
      UART.print("]");
    }
    UART.print(" sigma=");
    UART.print(voie[i].sigma, 2);
    UART.print("cm ecartMax=");
    UART.print(voie[i].ecartMax, 2);
    UART.println("cm");
  }
  envoyerAide();
  lcd.clear();
}

void loop() {
#if SIMULATION
  peripheral_loop();
#endif
  unsigned long debut = millis();
  float avant[NB_VOIES];
  for (int i = 0; i < NB_VOIES; i++) avant[i] = voie[i].distance;

  temperature = lireTemperature();
  for (int i = 0; i < NB_VOIES; i++) {
    mettreAJour(i);
    lireCommandes();
    delay(PAUSE_ENTRE_VOIES_MS);
  }
  int dir = voieLaPlusProche();
  piloterServo(dir);
  gererEnergie(avant);
  afficherLCD(dir);
  envoyerMesures(dir);
  attendreJusqua(debut + (ecoActif ? PERIODE_ECO_MS : periodeNormale));
}

#pragma GCC pop_options
