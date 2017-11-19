#include <TimerOne.h>
#include <DS1307.h>


#define TIME_BASE 1000000  // base de tempo do timer em segundos
#define NUM_VALVES 7
#define NUM_INTERVALS 2
#define VALVE_0 2
#define VALVE_1 3
#define VALVE_2 4
#define VALVE_3 5
#define VALVE_4 6
#define VALVE_5 7
#define VALVE_6 8
#define PUMP_ENB 9
 
bool timeout = false;
bool startedDailyIrrigation = false;

bool openedValves[NUM_VALVES];
bool isSoftwareTimerEnabled[NUM_VALVES];

unsigned int softwareTimers[NUM_VALVES];
unsigned int intervals[NUM_INTERVALS] = {1*60, 2*60}; // tamanho dos intervalos entre irrigacoes em segundos
unsigned int valveIterationsCounter[NUM_VALVES];

unsigned int irrigationTotalTime = 12*60; // tempo total de irrigacao em segundos

char irrigationStartTime[] = "09:00"; // horario em que a irrigacao comecara diariamente
char currentTime[6] = "00:00";

DS1307 rtc(A4, A5); // inicializa o RTC

void tCallback(void *tCall){
    timeout = true;
}
 
void usrInit(void) {
  Timer1.initialize(TIME_BASE); // configura o tempo base do timer
  Timer1.attachInterrupt(tCallback); // associa a rotina de interrupcao do timer
}

void updateSoftwareTimers() {
  for(int i = 0; i < NUM_VALVES; i++) {
    if(isSoftwareTimerEnabled[i])
      softwareTimers[i]++;
  }  
}

void checkSoftwareTimers() {
  
  if(isSoftwareTimerEnabled[NUM_VALVES-1] == true && softwareTimers[NUM_VALVES-1] == irrigationTotalTime) {
    softwareTimers[NUM_VALVES-1] = 0;
    isSoftwareTimerEnabled[NUM_VALVES-1] = false;
    openedValves[NUM_VALVES-1] = false;
  }

  for(int i = 0; i < (NUM_VALVES-1); i++) {
    if(isSoftwareTimerEnabled[i] == true) {
      if(openedValves[i] == true)
        if(softwareTimers[i] == (irrigationTotalTime/((i/NUM_INTERVALS)+4))) {
          softwareTimers[i] = 0;
          openedValves[i] = false;
        }
      else {
        if(softwareTimers[i] == intervals[(i%NUM_INTERVALS)]) {
          softwareTimers[i] = 0;
          openedValves[i] = true;
          valveIterationsCounter[i]++;
          if(valveIterationsCounter[i] == ((i/NUM_INTERVALS)+4)) {
            valveIterationsCounter[i] = 0;
            isSoftwareTimerEnabled[i] = false;
            openedValves[i] = false;
          }
        }
      }
    }
  }
}

void checkIrrigationEnd() {
  bool stillIrrigating = false;
  for(int i = 0; i < NUM_VALVES; i++) {
    stillIrrigating |= isSoftwareTimerEnabled[i];
  }
  startedDailyIrrigation = stillIrrigating;
}

bool isAnyValveEnabled() {
  bool returnValue = false;
  for(int i = 0; i < NUM_VALVES; i++) {
    returnValue |= openedValves[i];
  }
  return returnValue;
}

void updateOutputs() {
  bool portState = false;
  for(int i = 0; i < NUM_VALVES; i++) {
    portState = digitalRead(i+2);
    if(portState != openedValves[i]) {
      digitalWrite(i+2, !portState);
    }
  }
  if(isAnyValveEnabled()) {
    digitalWrite(PUMP_ENB, HIGH);
  }
  else {
    digitalWrite(PUMP_ENB, LOW);
  }
}

void startIrrigation() {
  for(int i = 0; i < NUM_VALVES; i++) {
    openedValves[i] = true;
    isSoftwareTimerEnabled[i] = true;
  }  
}

void checkDayTime() {
  strncpy(currentTime, rtc.getTimeStr(), 5);
  currentTime[6] = '\0';
  if(strcmp(currentTime, irrigationStartTime) == 0) {
    startIrrigation();
    usrInit();   // iniciar a interrupcao do Timer
    startedDailyIrrigation = true;
  }
}

void configRTC() {
  // aciona o relogio
  rtc.halt(false);
  // as linhas abaixo setam a data e hora do modulo e podem ser comentada apos a primeira utilizacao
  rtc.setDOW(SATURDAY);      // define o dia da semana
  rtc.setTime(00, 10, 0);  // define o horario
  rtc.setDate(18, 3, 2017); // define o dia, mes e ano
  // definicoes do pino SQW/Out
  rtc.setSQWRate(SQW_RATE_1);
  rtc.enableSQW(true);
}

void configPorts() {
  pinMode(VALVE_0, OUTPUT);
  pinMode(VALVE_1, OUTPUT);
  pinMode(VALVE_2, OUTPUT);
  pinMode(VALVE_3, OUTPUT);
  pinMode(VALVE_4, OUTPUT);
  pinMode(VALVE_5, OUTPUT);
  pinMode(VALVE_6, OUTPUT);
  pinMode(PUMP_ENB, OUTPUT);
}
 
void setup() {
  Serial.begin(115200);
  Serial.println();
  configPorts();
  configRTC(); // configurar o RTC
}
 
void loop() {
  if(startedDailyIrrigation == true) { // caso tenha chegado a hora de irrigar
    if(timeout) {
      updateSoftwareTimers();
      checkSoftwareTimers();
      checkIrrigationEnd();
      updateOutputs();
      Serial.println("cuco!");
      timeout = false;
    }
  }
  else {
    checkDayTime();
  }
  yield();
}
