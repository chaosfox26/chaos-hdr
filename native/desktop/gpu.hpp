#pragma once
#include <memory>
#include "../picture_settings.hpp"
#include <cstdint>
#include <array>



class DesktopGPU {
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    DesktopGPU(uint32_t width, uint32_t height,bool direct=false);
    ~DesktopGPU();
    unsigned inputTexture() const;
    unsigned outputTexture() const;
    bool ready()const;
    bool retirementReady();
    static bool recoveryRequired();
    static void quarantine();
#ifdef TRUEHDR_DEVELOPMENT
    void testCompletion(int mode);
#endif
    void setSettings(const PictureSettings& settings);
    void convert(float sharpen, float ignoreGrain,bool timing=false,bool hdr=true);
    struct DmaInput {
        uint32_t width{},height{},format{},planes{};
        uint64_t modifier{};
        std::array<int,4> fd{};
        std::array<uint32_t,4> pitch{},offset{};
        int acquireFd=-1;
    };



    static bool supportsFormat(uint32_t format);
    static bool producerReady(const DmaInput& input);
    void convertDirect(const DmaInput&,float sharpen,float ignoreGrain,bool timing=false,bool hdr=true);
    void releaseOutput();
    struct Timings {double prepare=0,engine=0,total=0,lastPrepare=0,lastEngine=0,lastTotal=0;uint64_t samples=0;};
    Timings timings() const;
};
