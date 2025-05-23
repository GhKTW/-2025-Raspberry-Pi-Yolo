#include <Servo.h>

// === 핀 정의 ===
const int LineL =  2;
const int LineR =  3;
const int trigPin = 6;
const int echoPin = 7;

// === 서보 객체 ===
Servo servoL, servoR;

// --- 선 인식 상태 ---
bool lineL_state = false;
bool lineR_state = false;

// --- 선 추적 중 장애물 발견 시, 회피 금지 ---
bool trackingActive = false;

// --- 인터럽트 핸들러 ---
void handleLineL() {
  bool newState = digitalRead(LineL);
  if (newState != lineL_state) {
    lineL_state = newState;
    Serial.print("LineL changed: ");
    Serial.println(lineL_state ? "BLACK" : "WHITE");
  }
}

void handleLineR() {
  bool newState = digitalRead(LineR);
  if (newState != lineR_state) {
    lineR_state = newState;
    Serial.print("LineR changed: ");
    Serial.println(lineR_state ? "BLACK" : "WHITE");
  }
}

// === 상태 관리 변수 ===
enum Action {
  ACT_FORWARD = 0,
  ACT_LEFT    = 1,
  ACT_RIGHT   = 2,
  ACT_BACK    = 3,
  ACT_LINE    = 5,
  ACT_REST    = 6,
  ACT_NONE    = -1
};

int currentAction = ACT_NONE;
int lastAction = ACT_NONE;
bool inMotion = false;
unsigned long actionStartTime = 0;
unsigned long actionDuration  = 0;
const unsigned long lostTimeout = 500;
unsigned long lastLineDetected = 0;

// === 거리 측정 ===
int readDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000);
  if (duration == 0) {
    return 999;
  }
  return duration * 0.034 / 2;
}

int safeReadDistance() {
  long total = 0;
  const int samples = 5;
  for (int i = 0; i < samples; i++) {
    total += readDistance();
    delay(10);
  }
  return total / samples;
}

bool isObstacleAhead() {
  return safeReadDistance() <= 4;
}

// === 장애물 회피 ===
void avoid_obstacle() {
  Serial.println("장애물 감지! 회피 동작 시작");
  unsigned long t0 = millis();
  while (millis() - t0 < 1000) {
    go_back(); delay(20);
  }

  int dir = random(0, 2); // 0: 좌회전, 1: 우회전
  t0 = millis();
  while (millis() - t0 < 1500) {
    turn180(); delay(20);
  }

  stop_moving();
  Serial.println("장애물 회피 완료");
  inMotion = false;
  currentAction = ACT_NONE;
}

// === 모터(서보) 제어 함수 ===
void go_forward() {
  if (trackingActive == false && isObstacleAhead()) {
    avoid_obstacle();
    return;
  }
  servoL.write(45); servoR.write(135);
}

void go_back() {
  servoL.write(135); servoR.write(45);
}

void turn_left() {
  if (isObstacleAhead()) {
    avoid_obstacle();
    return;
  }
  servoL.write(90); servoR.write(135);
}

void turn_right() {
  if (isObstacleAhead()) {
    avoid_obstacle();
    return;
  }
  servoL.write(45); servoR.write(90);
}

void turn180() {
  if (isObstacleAhead()) {
    avoid_obstacle();
    return;
  }
  servoL.write(45); servoR.write(45);
}

void stop_moving() {
  servoL.write(90); servoR.write(90);
}

// === 선 추적 함수 ===
void track_line() {
  static unsigned long whiteStartTime = 0;
  static bool inBlack = false;

  bool leftWhite = lineL_state;
  bool rightWhite = lineR_state;
  
  trackingActive = true;  // 시작 시 활성화

  // 센서 오감지 방지 기능
  // 검정 감지 중이라면
  if (!leftWhite || !rightWhite) {
    if (!inBlack) {
      inBlack = true;
      whiteStartTime = 0;
    } else {
      whiteStartTime = 0;
    }
    lastLineDetected = millis();
  } else if (inBlack) {
    // 흰색 감지 시 시간 체크 시작
    if (whiteStartTime == 0) {
      whiteStartTime = millis();
    } else if (millis() - whiteStartTime >= 500) {
      // 500ms 이상 흰색이면 검정 상태 종료
      inBlack = false;
      trackingActive = false;  // 추적 종료
      return;  // ❗ 바로 함수 종료
    } else {
      // 아직 500ms가 안 됐다면 무시
      return;
    }
  }

  // 정상 선 추적 동작
  if (!leftWhite && !rightWhite) {
    go_forward();
  } else if (!leftWhite && rightWhite) {
    turn_right();
  } else if (leftWhite && !rightWhite) {
    turn_left();
  } else {
    go_forward();
  }
}

bool isEndOfLine() {
  if (lineL_state || lineR_state) {
    lastLineDetected = millis();
    return false;
  }
  return (millis() - lastLineDetected) >= lostTimeout;
}

// === 스파이럴 탐색 ===
int searchLineSpiral() {
  Serial.println("□ 사각 스파이럴 탐색 시작");

  int step = 2000;
  const int stepMin = 200;
  const int turnTime = 2000;
  const int shrink = 150;

  for (int i = 0; i < 20; i++) {
    if (lineL_state || lineR_state) {
      Serial.println("→ 선 발견! 탐색 종료");
      stop_moving();
      return 1;
    }

    Serial.print("→ 직진 "); Serial.println(step);
    unsigned long t0 = millis();
    while (millis() - t0 < step) {
      if (isObstacleAhead()) {
        Serial.println("→ 탐색 중 장애물 발견");
        avoid_obstacle();
        continue;
      }
      go_forward();

      if (lineL_state || lineR_state) {
        Serial.println("→ 선 발견 중지 (직진 중)");
        stop_moving();
        return 1;
      }
      delay(5);
    }

    stop_moving();
    delay(100);

    Serial.println("→ 오른쪽 회전");
    t0 = millis();
    while (millis() - t0 < turnTime) {
      turn_right();

      if (lineL_state || lineR_state) {
        Serial.println("→ 선 발견 중지 (회전 중)");
        stop_moving();
        return 1;
      }
      delay(5);
    }

    stop_moving();
    delay(100);

    if (step > stepMin) step -= shrink;
  }

  Serial.println("→ 선 탐색 실패: 사각 스파이럴 종료");
  stop_moving();
  return 0;
}

// === 설정 ===
void setup() { 
  Serial.begin(9600);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(LineL, INPUT);
  pinMode(LineR, INPUT);
  attachInterrupt(digitalPinToInterrupt(LineL), handleLineL, CHANGE);
  attachInterrupt(digitalPinToInterrupt(LineR), handleLineR, CHANGE);

  lineL_state = !digitalRead(LineL);
  lineR_state = !digitalRead(LineR);

  servoL.attach(9);
  servoR.attach(10);
  randomSeed(millis());
  lastLineDetected = millis();
}

// === 메인 루프 ===
void loop() {
  unsigned long now = millis();

  // 장애물 즉시 감지
  if (isObstacleAhead()) {
    avoid_obstacle();
    return;
  }

  if (currentAction == ACT_NONE) {
    float r = random(0, 1000) / 1000.0;
    Serial.print("랜덤값: "); Serial.println(r, 3);

    int selectedAction = ACT_NONE;

    if      (r < 0.4)   selectedAction = ACT_FORWARD;
    else if (r < 0.55)  selectedAction = ACT_LEFT;
    else if (r < 0.7)   selectedAction = ACT_RIGHT;
    else if (r < 0.8)   selectedAction = ACT_BACK;
    else if (r < 0.85)   selectedAction = ACT_LINE;
    else                selectedAction = ACT_REST;

    if (lastAction == ACT_LINE && selectedAction == ACT_LINE) {
      Serial.println("→ 연속 탐색 방지: 랜덤값 재추첨");
      return;
    }

    currentAction = selectedAction;
    lastAction = selectedAction;

    switch (currentAction) {
      case ACT_FORWARD: go_forward();  actionDuration = 4000; inMotion = true; break;
      case ACT_LEFT:    turn_left();   actionDuration = 3000; inMotion = true; break;
      case ACT_RIGHT:   turn_right();  actionDuration = 3000; inMotion = true; break;
      case ACT_BACK:    go_back();     actionDuration = 2000; inMotion = true; break;
      case ACT_LINE:    Serial.println("→ 선 탐색 모드 진입"); inMotion = false; break;
      case ACT_REST:    stop_moving(); actionDuration = 3000; inMotion = false; break;
    }

    actionStartTime = now;
    return;
  }

  switch (currentAction) {
    case ACT_FORWARD: case ACT_LEFT: case ACT_RIGHT: case ACT_BACK:
      if (inMotion && now - actionStartTime < actionDuration) {
        if (isObstacleAhead()) {
          avoid_obstacle();
        }
      } else {
        stop_moving(); inMotion = false; currentAction = ACT_NONE;
      }
      break;

    case ACT_LINE: {
      if (isObstacleAhead()) {
        avoid_obstacle();
        return;
      }
      int i = searchLineSpiral();
      if (i == 1) {
        long start_time = millis();
        while (!isEndOfLine() && millis() - 7000 < start_time) {
          track_line();
        }
        stop_moving();
        Serial.println("→ 선 추적 종료 (라인 종료 감지)");
        delay(5000);
        currentAction = ACT_NONE;
      } else {
        currentAction = ACT_NONE;
      }
      break;
    }

    case ACT_REST:
      if (isObstacleAhead()) {
        avoid_obstacle();
        return;
      }
      if (now - actionStartTime < actionDuration) {
        stop_moving();
      } else {
        currentAction = ACT_NONE;
      }
      break;

    default:
      break;
  }

  delay(10);
}
