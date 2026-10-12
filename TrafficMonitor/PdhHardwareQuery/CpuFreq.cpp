#include "stdafx.h"
#include "CpuFreq.h"
#include <powerbase.h>
#include <sysinfoapi.h>


///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////

typedef struct _PROCESSOR_POWER_INFORMATION {
    ULONG Number;
    ULONG MaxMhz;
    ULONG CurrentMhz;
    ULONG MhzLimit;
    ULONG MaxIdleState;
    ULONG CurrentIdleState;
} PROCESSOR_POWER_INFORMATION, * PPROCESSOR_POWER_INFORMATION;

// 定制：CPU频率改为“前8个物理核心（CCD0）的平均频率”。
// 9950X3D 等双 CCD 处理器上，前 16 个逻辑处理器（LP0~LP15）对应第一个 CCD 的 8 个物理核心。
#define CUSTOM_CCD0_LP_COUNT 16

CPdhCpuFreq::CPdhCpuFreq()
    : CPdhQuery(_T("\\Processor Information(*)\\% Processor Performance"))   // 定制：查询全部逻辑处理器（GetCpuFreq 中只取前16个求平均）
{
    //获取max_cpu_freq
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    auto ppInfo = std::vector<PROCESSOR_POWER_INFORMATION>(si.dwNumberOfProcessors);
    auto status = CallNtPowerInformation(POWER_INFORMATION_LEVEL::ProcessorInformation,
        NULL, 0, &ppInfo[0], sizeof(PROCESSOR_POWER_INFORMATION) * ppInfo.size());
    for (size_t i = 0; i < ppInfo.size(); i++)
    {
        max_cpu_freq = max(max_cpu_freq, ppInfo[i].MaxMhz / 1000.f);
    }
}

bool CPdhCpuFreq::GetCpuFreq(float& freq)
{
    // 定制：取所有逻辑处理器的 "% Processor Performance"（实例名形如 "0,0"~"0,31"），
    // 只统计第一个处理器组中编号 0~15 的实例（= 前 8 个物理核心 = CCD0），求平均值。
    std::vector<CPdhQuery::CounterValueItem> values;
    if (!QueryValues(values))
        return false;

    double sum{};
    int count{};
    for (const auto& item : values)
    {
        int group = -1;
        int number = -1;
        if (swscanf_s(item.name.c_str(), L"%d,%d", &group, &number) == 2)
        {
            if (group == 0 && number >= 0 && number < CUSTOM_CCD0_LP_COUNT)
            {
                sum += item.value;
                count++;
            }
        }
    }
    if (count == 0)
        return false;

    freq = static_cast<float>(sum / count / 100.0 * max_cpu_freq);
    return true;
}
