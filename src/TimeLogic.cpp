#include "TimeLogic.h"
#include <NTPClient.h>
#include <WiFiUdp.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

void setupTime() {
    timeClient.begin();
    timeClient.setTimeOffset(3600 * 9); 
    configTime(3600 * 9, 0, "pool.ntp.org", "time.nist.gov");
}

bool getLocalTimeInfo(struct tm * info) {
    return getLocalTime(info);
}

bool isLeap(int year) { return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0); }

int getDaysInMonth(int month, int year) {
    int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (isLeap(year) && month == 1) return 29;
    return days[month];
}

time_t parseDate(String dateStr) {
    struct tm t = {0};
    int y, m, d;
    if(sscanf(dateStr.c_str(), "%d-%d-%d", &y, &m, &d) == 3) {
        t.tm_year = y - 1900;
        t.tm_mon = m - 1;
        t.tm_mday = d;
    }
    return mktime(&t);
}

// [신규] 분기 계산 핵심 로직 (정밀도 보장)
void getQuarterInfo(struct tm * t, int &totalDays, float &passedDays) {
    int q = t->tm_mon / 3;     // 0~3분기
    int startMonth = q * 3;    // 분기 시작 월 (0, 3, 6, 9)
    
    // 1. 이번 분기 시작일의 yday(연중 날짜) 구하기
    int startYday = 0;
    for(int i=0; i<startMonth; i++) {
        startYday += getDaysInMonth(i, t->tm_year + 1900);
    }

    // 2. 이번 분기 총 일수 구하기 (3개월치 합산)
    totalDays = 0;
    for(int i=0; i<3; i++) {
        totalDays += getDaysInMonth(startMonth + i, t->tm_year + 1900);
    }

    // 3. 지난 일수 계산 (현재 yday - 시작 yday + 시간 보정)
    // t->tm_yday는 0부터 시작하므로 그대로 사용 가능
    int daysPassedInt = t->tm_yday - startYday;
    
    // 시간 단위까지 정밀하게 (24.0으로 나누어 소수점 확보)
    passedDays = daysPassedInt + (t->tm_hour / 24.0) + (t->tm_min / 1440.0);
}

float calculateProgress(int mode, struct tm * t, String sDate, String tDate) {
    if (mode == 0) { // Year
        int total = isLeap(t->tm_year + 1900) ? 366 : 365;
        return (float)t->tm_yday / (float)total;
    } 
    else if (mode == 1) { // Month
        int total = getDaysInMonth(t->tm_mon, t->tm_year + 1900);
        return (float)(t->tm_mday - 1) / (float)total;
    }
    else if (mode == 2) { // Week (Mon=0)
        int wday = (t->tm_wday + 6) % 7; 
        float dayProg = (t->tm_hour * 3600 + t->tm_min * 60) / 86400.0;
        return (float)(wday + dayProg) / 7.0;
    }
    else if (mode == 3) { // Day
        int sec = t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec;
        return (float)sec / 86400.0;
    }
    else if (mode == 4) { // Custom D-Day
        time_t s = parseDate(sDate);
        time_t e = parseDate(tDate);
        time_t n = mktime(t);
        if (n <= s) return 0.0;
        if (n >= e) return 1.0;
        double total = difftime(e, s);
        double curr = difftime(n, s);
        if(total <= 0) return 1.0;
        return (float)(curr / total);
    }
    else if (mode == 5) { // Quarter (수정됨: 위 함수 활용)
        int totalDays;
        float passedDays;
        getQuarterInfo(t, totalDays, passedDays);
        if (totalDays == 0) return 0.0;
        return passedDays / (float)totalDays;
    }
    return 0.0;
}