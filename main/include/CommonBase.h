#pragma once

class WMBase{
protected:
    bool m_stopRecordFlag;
    bool m_bIdleState;

public:
    virtual int start()=0;
    virtual int stop();
};


