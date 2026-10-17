#pragma once
#include <memory>
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
    void convert(float sharpen, float ignoreGrain,bool timing=false);
    struct DmaInput {
        uint32_t width{},height{},format{},planes{};
        uint64_t modifier{};
        std::array<int,4> fd{};
        std::array<uint32_t,4> pitch{},offset{};
        int acquireFd=-1;
    };



    void convertDirect(const DmaInput&,float sharpen,float ignoreGrain,bool timing=false);
    void releaseOutput();
    struct Timings {double prepare=0,engine=0,total=0,lastPrepare=0,lastEngine=0,lastTotal=0;uint64_t samples=0;};
    Timings timings() const;
};
