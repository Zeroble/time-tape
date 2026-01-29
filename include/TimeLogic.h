#ifndef TIME_LOGIC_H
#define TIME_LOGIC_H

#include <Arduino.h>
#include <time.h>

void setupTime();
bool getLocalTimeInfo(struct tm * info);
time_t parseDate(String dateStr);

// [수정] 통합 계산 함수들
float calculateProgress(int mode, struct tm * t, String sDate, String tDate);
int getDaysInMonth(int month, int year);
bool isLeap(int year);

// [신규] 분기 정보 반환 (총 일수, 지난 일수+시간)
void getQuarterInfo(struct tm * t, int &totalDays, float &passedDays);

#endif