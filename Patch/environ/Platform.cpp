#include <cstdint>
#include <cstring>
#include <cstdlib>

void TVPAbortApplication(int code) { std::exit(code); }

#ifdef _WIN32
#define TVP_OS_WINDOWS
#endif

#ifdef __linux__
#define TVP_OS_LINUX
#endif

#ifdef __ANDROID__
#define TVP_OS_ANDROID
#endif

#ifdef __APPLE__
#define TVP_OS_MACOS
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
#undef TVP_OS_MACOS
#define TVP_OS_IOS
#endif
#endif

#ifdef __OHOS__
#define TVP_OS_OHOS
#endif

#ifdef __EMSCRIPTEN__
#define TVP_OS_WEB
#endif

#ifdef __FreeBSD__
#define TVP_OS_FREEBSD
#endif

enum TVPCPUFeatures {
    TVP_CPU_HAS_FPU   = 1 << 0,
    TVP_CPU_HAS_MMX   = 1 << 1,
    TVP_CPU_HAS_SSE   = 1 << 2,
    TVP_CPU_HAS_SSE2  = 1 << 3,
    TVP_CPU_HAS_SSE3  = 1 << 4,
    TVP_CPU_HAS_SSE41 = 1 << 5,
    TVP_CPU_HAS_SSE42 = 1 << 6,
    TVP_CPU_HAS_AVX   = 1 << 7,
    TVP_CPU_HAS_AVX2  = 1 << 8,
    TVP_CPU_HAS_NEON  = 1 << 9,
    TVP_CPU_HAS_CMOV  = 1 << 10,
    TVP_CPU_HAS_TSC   = 1 << 11,
};

uint32_t TVPGetCPUType();
uint32_t TVPGetCPUFeatures();
const char* TVPGetCPUVendor();
const char* TVPGetCPUName();
int TVPGetProcessorNum();

// ----------------------------------------------------------------------------
// cpu_info.cpp
// ----------------------------------------------------------------------------
#include <cstdio>
#include <cstdlib>

#ifdef TVP_OS_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#ifdef TVP_OS_LINUX
#include <unistd.h>
#include <sys/sysinfo.h>
#endif

#ifdef TVP_OS_ANDROID
#include <unistd.h>
#include <sys/sysinfo.h>
#endif

#ifdef TVP_OS_OHOS
#include <unistd.h>
#include <sys/sysinfo.h>
#endif

#ifdef TVP_OS_MACOS
#include <sys/sysctl.h>
#include <mach/mach.h>
#endif

#ifdef TVP_OS_FREEBSD
#include <sys/sysctl.h>
#endif

#ifdef __GNUC__
#include <cpuid.h>
#endif

#ifdef _MSC_VER
#include <intrin.h>
#endif

static uint32_t g_CPUType = 0;
static uint32_t g_CPUFeatures = 0;
static char g_CPUVendor[16] = {};
static char g_CPUName[64] = {};
static bool g_CPUChecked = false;

// ----------------------------------------------------------------------------
// x86 相关
// ----------------------------------------------------------------------------
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#define TVP_CPU_ARCH_X86
#endif

#ifdef TVP_CPU_ARCH_X86
static void TVPGetCPUId(int leaf, int subleaf, unsigned int regs[4])
{
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    __cpuidex((int*)regs, leaf, subleaf);
#elif defined(__GNUC__) || defined(__clang__)
    __cpuid_count(leaf, subleaf, regs[0], regs[1], regs[2], regs[3]);
#endif
}
#endif

// ----------------------------------------------------------------------------
static uint32_t TVPDetectX86Features()
{
    uint32_t features = 0;
#ifdef TVP_CPU_ARCH_X86
    unsigned int regs[4] = {};
    TVPGetCPUId(1, 0, regs);
    uint32_t ecx = regs[2];
    uint32_t edx = regs[3];

    if (edx & (1 << 0))  features |= TVP_CPU_HAS_FPU;
    if (edx & (1 << 15)) features |= TVP_CPU_HAS_CMOV;
    if (edx & (1 << 4))  features |= TVP_CPU_HAS_TSC;
    if (edx & (1 << 23)) features |= TVP_CPU_HAS_MMX;
    if (edx & (1 << 25)) features |= TVP_CPU_HAS_SSE;
    if (edx & (1 << 26)) features |= TVP_CPU_HAS_SSE2;

    if (ecx & (1 << 0))  features |= TVP_CPU_HAS_SSE3;
    if (ecx & (1 << 19)) features |= TVP_CPU_HAS_SSE41;
    if (ecx & (1 << 20)) features |= TVP_CPU_HAS_SSE42;
    if (ecx & (1 << 28)) features |= TVP_CPU_HAS_AVX;

    // AVX2: 用 leaf 7
    TVPGetCPUId(7, 0, regs);
    if (regs[1] & (1 << 5)) features |= TVP_CPU_HAS_AVX2;
#endif
    return features;
}

// ----------------------------------------------------------------------------
static void TVPDetectVendorAndBrand()
{
#ifdef TVP_CPU_ARCH_X86
    unsigned int regs[4] = {};

    TVPGetCPUId(0, 0, regs);
    memcpy(g_CPUVendor + 0, &regs[1], 4);
    memcpy(g_CPUVendor + 4, &regs[3], 4);
    memcpy(g_CPUVendor + 8, &regs[2], 4);
    g_CPUVendor[12] = '\0';

    for (int leaf = 0x80000002; leaf <= 0x80000004; leaf++) {
        TVPGetCPUId(leaf, 0, regs);
        memcpy(g_CPUName + (leaf - 0x80000002) * 16, regs, 16);
    }
    g_CPUName[63] = '\0';

    // trim trailing spaces
    for (int i = 63; i >= 0; i--) {
        if (g_CPUName[i] == ' ')
            g_CPUName[i] = '\0';
        else if (g_CPUName[i])
            break;
    }

    if (std::strcmp(g_CPUVendor, "GenuineIntel") == 0)
        g_CPUType |= 1; // Intel
    else if (std::strcmp(g_CPUVendor, "AuthenticAMD") == 0)
        g_CPUType |= 2; // AMD
    else
        g_CPUType |= 0; // Unknown

#else
    // ARM
#if defined(__aarch64__) || defined(_M_ARM64)
    std::strcpy(g_CPUVendor, "ARM64");
#else
    std::strcpy(g_CPUVendor, "ARM");
#endif
    std::strcpy(g_CPUName, g_CPUVendor);

#ifdef TVP_OS_WINDOWS
    // Windows on ARM: read from registry
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        WCHAR wbuf[64] = {};
        DWORD type = 0, size = sizeof(wbuf);
        if (RegQueryValueExW(hKey, L"ProcessorNameString", NULL, &type,
                             (LPBYTE)wbuf, &size) == ERROR_SUCCESS && type == REG_SZ) {
            int len = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, NULL, 0, NULL, NULL);
            if (len > 0 && len <= 64) {
                WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, g_CPUName, 64, NULL, NULL);
                g_CPUName[63] = '\0';
            }
        }
        RegCloseKey(hKey);
    }
#else
    // Linux / Android / OHOS: read from /proc/cpuinfo
    FILE* fp = fopen("/proc/cpuinfo", "r");
    if (fp) {
        char line[256];
        const char* keys[] = {"Hardware", "model name", "Processor"};
        while (fgets(line, sizeof(line), fp)) {
            for (int k = 0; k < 3; k++) {
                size_t klen = std::strlen(keys[k]);
                if (std::strncmp(line, keys[k], klen) == 0 && line[klen] == ':') {
                    const char* val = line + klen + 1;
                    while (*val == ' ' || *val == '\t') val++;
                    char* end = g_CPUName + std::strlen(g_CPUName);
                    if (end < g_CPUName + 60)
                        *end++ = ' ';
                    std::strncpy(end, val, g_CPUName + 63 - end);
                    g_CPUName[63] = '\0';
                    size_t nlen = std::strlen(g_CPUName);
                    while (nlen > 0 && (g_CPUName[nlen - 1] == '\n' || g_CPUName[nlen - 1] == '\r'))
                        g_CPUName[--nlen] = '\0';
                    break;
                }
            }
        }
        fclose(fp);
    }
#endif
#endif
}

// ----------------------------------------------------------------------------
static int TVPGetProcessorCount()
{
#ifdef TVP_OS_WINDOWS
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
#elif defined(TVP_OS_LINUX) || defined(TVP_OS_ANDROID) || defined(TVP_OS_OHOS)
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return (n > 0) ? (int)n : 1;
#elif defined(TVP_OS_MACOS) || defined(TVP_OS_IOS)
    int count = 0;
    size_t size = sizeof(count);
    sysctlbyname("hw.logicalcpu", &count, &size, NULL, 0);
    return count > 0 ? count : 1;
#elif defined(TVP_OS_FREEBSD)
    int count = 0;
    size_t size = sizeof(count);
    sysctlbyname("hw.ncpu", &count, &size, NULL, 0);
    return count > 0 ? count : 1;
#elif defined(TVP_OS_WEB)
    return 1; // WebAssembly 通常报告1核，实际由浏览器调度
#else
    return 1;
#endif
}

// ----------------------------------------------------------------------------
static uint64_t TVPGetSystemRAM()
{
#ifdef TVP_OS_WINDOWS
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem))
        return mem.ullTotalPhys / 1024 / 1024; // MB
#elif defined(TVP_OS_LINUX) || defined(TVP_OS_ANDROID) || defined(TVP_OS_OHOS)
    struct sysinfo info;
    if (sysinfo(&info) == 0)
        return (uint64_t)info.totalram * info.mem_unit / 1024 / 1024;
#elif defined(TVP_OS_MACOS) || defined(TVP_OS_IOS)
    int64_t mem = 0;
    size_t size = sizeof(mem);
    if (sysctlbyname("hw.memsize", &mem, &size, NULL, 0) == 0)
        return (uint64_t)mem / 1024 / 1024;
#endif
    return 0;
}

// ----------------------------------------------------------------------------
// 公开 API
// ----------------------------------------------------------------------------
void TVPDetectCPU()
{
    if (g_CPUChecked)
        return;
    g_CPUChecked = true;

    TVPDetectVendorAndBrand();

#ifdef TVP_CPU_ARCH_X86
    g_CPUFeatures = TVPDetectX86Features();
#endif

    // NEON 检测：ARMv7/AArch64 有 NEON 几乎默认支持
#if defined(__ARM_NEON) || defined(__aarch64__)
    g_CPUFeatures |= TVP_CPU_HAS_NEON;
#endif
#if defined(_M_ARM64) || defined(_M_ARM)
    g_CPUFeatures |= TVP_CPU_HAS_NEON;
#endif
}

uint32_t TVPGetCPUType()
{
    TVPDetectCPU();
    return g_CPUType;
}

uint32_t TVPGetCPUFeatures()
{
    TVPDetectCPU();
    return g_CPUFeatures;
}

const char* TVPGetCPUVendor()
{
    TVPDetectCPU();
    return g_CPUVendor;
}

const char* TVPGetCPUName()
{
    TVPDetectCPU();
    return g_CPUName;
}

int TVPGetProcessorNum()
{
    static int count = 0;
    if (!count) {
        count = TVPGetProcessorCount();
    }
    return count;
}