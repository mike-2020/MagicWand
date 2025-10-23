#pragma once

class LCDDev
{
public:
    int init();
    int deinit();
    void *getDisplayHandle();
    bool lock(int timeout_ms);
    void unlock(void);
    static LCDDev* getInstance();
};
