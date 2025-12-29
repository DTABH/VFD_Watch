#ifndef MANUAL_TIME_SET
#define MANUAL_TIME_SET

#include "OneButton.h"
#include <Vfd_Display.h>
#include <sys/time.h>

OneButton button_left(BTN0, true,true);
OneButton button_middle(BTN1, true,true);
OneButton button_right(BTN2, true,true);

uint manual_hours = 12;
uint manual_minutes = 30;
uint manual_day = 1;
uint manual_month = 1;
uint manual_year = 2025;

bool middlePressed = false;


static void hoursUp(){
    manual_hours = (manual_hours +1) % 24;
}

static void hoursDown(){
    manual_hours = (manual_hours + (24-1)) % 24;
}

static void minutesUp(){
    manual_minutes = (manual_minutes + 1) % 60;
}

static void minutesDown(){
    manual_minutes = (manual_minutes + (60-1)) % 60;
}

static void dayUp(){
    if(manual_day < 31) { manual_day = manual_day + 1; } else { manual_day = 1;}
}

static void dayDown(){
    if(manual_day > 1) { manual_day = manual_day -1;  } else { manual_day = 31;}
}

static void monthUp(){
    if(manual_month < 12) { manual_month = manual_month + 1; } else { manual_month = 1;}
}

static void monthDown(){
   if(manual_month > 1) { manual_month = manual_month -1;  } else { manual_month = 12;}
}

static void yearUp(){
    if(manual_year < 2100) { manual_year = manual_year + 1; } else { manual_year = 2025;}
}

static void yearDown(){
    if(manual_year > 2025) { manual_year = manual_year -1;  } else { manual_year = 2025;}
}

static void middleButtonWasPressed(){
    middlePressed = true;
}



unsigned long int startManualTimeSet(vfdDisplay &vfd, int ShowDate)
{
    button_left.attachClick(hoursDown);
    button_right.attachClick(hoursUp);
    button_middle.attachClick(middleButtonWasPressed);
    timeval tv;
    unsigned long int unixtime = 1767135600;

    while(!middlePressed)
    {
        vfd.setDP(1,1);
        button_left.tick();
        button_middle.tick();
        button_right.tick();
        vfd.setHours(manual_hours);
    }

    middlePressed = false;

    button_left.attachClick(minutesDown);
    button_right.attachClick(minutesUp);

    while(!middlePressed)
    {   vfd.setDP(1,1);
        vfd.setHours(manual_hours);
        button_left.tick();
        button_middle.tick();
        button_right.tick();
        vfd.setMinutes(manual_minutes);
    }

    if (ShowDate)
    {
        middlePressed = false;

        button_left.attachClick(dayDown);
        button_right.attachClick(dayUp);

        while(!middlePressed)
        {   vfd.setDP(1,0);
            button_left.tick();
            button_middle.tick();
            button_right.tick();
            vfd.setHours(manual_day);
        }
        middlePressed = false;

        button_left.attachClick(monthDown);
        button_right.attachClick(monthUp);

        while(!middlePressed)
        {   vfd.setDP(1,0);
            vfd.setHours(manual_day);
            button_left.tick();
            button_middle.tick();
            button_right.tick();
            vfd.setMinutes(manual_month);
        }
        
        middlePressed = false;
        button_left.attachClick(yearDown);
        button_right.attachClick(yearUp);

        while(!middlePressed)
        {   vfd.setDP(0,0);
            button_left.tick();
            button_middle.tick();
            button_right.tick();
            vfd.setHours(manual_year/100);
            vfd.setMinutes(manual_year%100);
        }
    }

    // 1767135600 31.12.2025 hour 0 so that unixtime is > 124 (2024)
    unixtime = unixtime + (manual_hours)*3600 + manual_minutes*60;    
    tv.tv_sec = time_t(unixtime);
    settimeofday(&tv, NULL);

    middlePressed = false;    
 
    return unixtime;
}

#endif