#pragma once

class MBaseUI
{
public:
    virtual int init()=0;
    virtual int deinit();
};

class MProphetUI : public MBaseUI
{
private:
    void *m_txtLabelHandle;
    void *m_buffHandle;
    int initUI();
    static void cbUpdateText(void *timer);
public:
    virtual int init();
    void appendText(const char *msg);

};



class MNetConnectUI : public MBaseUI
{
private:
    void *m_txtLabelHandle;

private:
    int initUI();

public:
    virtual int init();
    void setText(const char *txt);
};


class MSplashUI : public MBaseUI
{
private:
    void *m_txtLabelHandle;

private:
    int initUI();

public:
    virtual int init();
};

class MClockUI : public MBaseUI
{
private:
    void *m_timeLabelHandle;
    void *m_dateLabelHandle;
    void *m_wifiLabelHandle;
    void *m_batteryLabelHandle;

private:
    int initUI();
    void setDateTimeText(const char *dateTxt, const char *timeTxt);
    static void cbUpdateTime(void *timer);

public:
    virtual int init();
};


typedef enum {
    MUI_CLOCK,
    MUI_SPLASH,
    MUI_NETCONNECT,
    MUI_PROPHET,
}mui_type_t;

class MUIMgr
{
private:
    MBaseUI *m_curUI;
public:
    MBaseUI* setUI(mui_type_t mui);
    MBaseUI* getCurUIHandle(){return m_curUI;};
public:
    static MUIMgr *getInstance();
};
