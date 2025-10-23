#include "CommonBase.h"

int WMBase::stop()
{
    m_stopRecordFlag = true;
    m_bIdleState = true;
    return 0;
}

