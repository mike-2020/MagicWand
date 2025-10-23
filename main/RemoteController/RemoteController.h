#pragma once
#include "CommonBase.h"

class RemoteController : public WMBase{
private:
    bool m_bReportData;
private:
    int mouse();
    static void proc(void *ctx);
public:
    virtual int start();

    void enableDataReport() { m_bReportData = true; };
    void disableDataReport() { m_bReportData = false; };
    void onHostConnected();
    void onHostDisconnected();
    static RemoteController* getInstance();
};
