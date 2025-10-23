#pragma once

class LCDDev{
public:
    int init();
    int deinit();
public:
    static LCDDev* getInstance();

};

