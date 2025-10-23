#pragma once

class NetworkMgr
{
public:
    int init(bool bShowUI);
    int deinit();
    int getWIFIRSSI();
    static NetworkMgr* getInstance();
};

