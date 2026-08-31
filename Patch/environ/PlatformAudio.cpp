// miniaudio-based non-SDL audio backend for the Patch EGL/GLES platform.
// Pattern follows OHOS/SDL3: the engine pushes via AppendBuffer into a
// fixed ring, the device callback pulls. The callback uses a non-blocking
// trylock so it NEVER blocks on the engine thread; on lock failure or
// underrun it emits silence. Volume is applied via the device master
// volume. miniaudio loads the system audio backend (ALSA/PulseAudio) at
// runtime, so no extra link-time dependency is required.
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#include "miniaudio.h"

#include "tjsCommHead.h"
#include "Platform.h"
#include "PlatformAudio.h"
#include <atomic>
#include <cstring>
#include <mutex>
#include <vector>

// Fixed-capacity byte ring buffer: O(1) read/write, no element moves.
class ByteRing {
    std::vector<unsigned char> buf_;
    size_t rpos_ = 0;
    size_t wpos_ = 0;
public:
    explicit ByteRing(size_t capacity) : buf_(capacity > 0 ? capacity : 1) {}
    size_t capacity() const { return buf_.size(); }
    size_t queued() const
    {
        return (wpos_ >= rpos_) ? (wpos_ - rpos_) : (buf_.size() - rpos_ + wpos_);
    }
    bool empty() const { return wpos_ == rpos_; }
    void clear() { rpos_ = 0; wpos_ = 0; }
    // Non-blocking read (callback side). Returns bytes actually read.
    size_t readNB(void* dst, size_t want)
    {
        size_t avail = queued();
        size_t n = want < avail ? want : avail;
        if (n == 0) return 0;
        auto* out = (unsigned char*)dst;
        size_t tail = buf_.size() - rpos_;
        if (n <= tail) {
            std::memcpy(out, &buf_[rpos_], n);
        } else {
            std::memcpy(out, &buf_[rpos_], tail);
            std::memcpy(out + tail, &buf_[0], n - tail);
        }
        rpos_ = (rpos_ + n) % buf_.size();
        return n;
    }
    // Write len bytes; drops the oldest bytes first if it does not fit.
    void write(const void* data, size_t len)
    {
        const auto* src = (const unsigned char*)data;
        if (len >= buf_.size())
        {
            // larger than the ring: keep only the tail that fits
            src += len - buf_.size();
            len = buf_.size();
            clear();
        }
        size_t avail = queued();
        if (avail + len > buf_.size())
        {
            size_t drop = avail + len - buf_.size();
            rpos_ = (rpos_ + drop) % buf_.size();
        }
        size_t tail = buf_.size() - wpos_;
        if (len <= tail) {
            std::memcpy(&buf_[wpos_], src, len);
        } else {
            std::memcpy(&buf_[wpos_], src, tail);
            std::memcpy(&buf_[0], src + tail, len - tail);
        }
        wpos_ = (wpos_ + len) % buf_.size();
    }
};

class tTVPMiniAudioSoundBuffer final : public iTVPSoundBuffer {
    tTVPWaveFormat format_;
    unsigned int limit_;
    size_t frameBytes_ = 0;
    size_t bufSize_ = 0; // ~50ms per "buffer" for limit accounting
    ma_device device_;
    bool deviceOk_ = false;
    bool playing_ = false;
    float volume_ = 1.0f;
    float pan_ = 0.0f;

    std::mutex mutex_;
    ByteRing ring_;

    static void DataCallback(ma_device* pDevice, void* pOutput, const void*, ma_uint32 frameCount)
    {
        auto* self = static_cast<tTVPMiniAudioSoundBuffer*>(pDevice->pUserData);
        self->Drain(pOutput, frameCount);
    }

    void Drain(void* pOutput, ma_uint32 frameCount)
    {
        size_t need = (size_t)frameCount * frameBytes_;
        // Non-blocking: never stall the audio thread on the engine's lock.
        if (!mutex_.try_lock())
        {
            std::memset(pOutput, 0, need);
            return;
        }
        size_t got = ring_.readNB(pOutput, need);
        mutex_.unlock();
        if (got < need)
            std::memset((char*)pOutput + got, 0, need - got);
    }

public:
    tTVPMiniAudioSoundBuffer(const tTVPWaveFormat& fmt, int count)
      : format_(fmt), limit_(count > 0 ? (unsigned)count : 1), ring_(0)
    {
        frameBytes_ = (size_t)fmt.Channels * (fmt.BitsPerSample / 8);
        // 2-second ring: WMV video frames can be large (thousands of samples).
        size_t rate = fmt.SamplesPerSec > 0 ? fmt.SamplesPerSec : 48000;
        ring_ = ByteRing(rate * frameBytes_ * 2);
        bufSize_ = frameBytes_ * (rate / 20); // ~50ms per buffer
        if (bufSize_ == 0) bufSize_ = 1;
    }

    bool Init() override
    {
        ma_format fmt = ma_format_unknown;
        switch (format_.BitsPerSample)
        {
            case 8: fmt = ma_format_u8; break;
            case 16: fmt = ma_format_s16; break;
            case 32: fmt = ma_format_s32; break;
            default: return false;
        }
        ma_device_config config = ma_device_config_init(ma_device_type_playback);
        config.playback.format = fmt;
        config.playback.channels = format_.Channels > 0 ? format_.Channels : 2;
        config.sampleRate = format_.SamplesPerSec > 0 ? format_.SamplesPerSec : 48000;
        config.dataCallback = DataCallback;
        config.pUserData = this;
        config.periodSizeInFrames = 512;
        config.periods = 2;
        ma_result res = ma_device_init(NULL, &config, &device_);
        if (res != MA_SUCCESS)
        {
            TVPConsoleLog("miniaudio device init failed (%d) for %dHz/%dch/%dbits",
                          (int)res, (int)config.sampleRate,
                          (int)config.playback.channels, (int)format_.BitsPerSample);
            return false;
        }
        deviceOk_ = true;
        TVPConsoleLog("miniaudio playback device ready: %s (%dHz %dch)",
                      device_.playback.name,
                      (int)config.sampleRate, (int)config.playback.channels);
        if (volume_ != 1.0f)
            ma_device_set_master_volume(&device_, volume_);
        return true;
    }

    ~tTVPMiniAudioSoundBuffer() override
    {
        if (deviceOk_)
        {
            ma_device_stop(&device_);
            ma_device_uninit(&device_);
        }
    }

    void Release() override { delete this; }
    void Play() override
    {
        if (playing_ || !deviceOk_) return;
        if (ma_device_start(&device_) == MA_SUCCESS)
            playing_ = true;
    }
    void Pause() override
    {
        if (!playing_ || !deviceOk_) return;
        ma_device_stop(&device_);
        playing_ = false;
    }
    void Stop() override
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ring_.clear();
        }
        if (deviceOk_) ma_device_stop(&device_);
        playing_ = false;
    }
    void Reset() override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ring_.clear();
    }
    bool IsPlaying() override { return playing_; }
    void SetVolume(float v) override
    {
        volume_ = v;
        if (deviceOk_) ma_device_set_master_volume(&device_, v);
    }
    float GetVolume() override { return volume_; }
    void SetPan(float v) override { pan_ = v; }
    float GetPan() override { return pan_; }
    void AppendBuffer(const void* buf, unsigned int len) override
    {
        if (!len) return;
        std::lock_guard<std::mutex> lock(mutex_);
        ring_.write(buf, len);
    }
    bool IsBufferValid() override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return (ring_.queued() / bufSize_) < limit_;
    }
    bool IsValidFormat(tTVPWaveFormat& fmt) override
    {
        return format_.SamplesPerSec == fmt.SamplesPerSec &&
               format_.BitsPerSample == fmt.BitsPerSample &&
               format_.Channels == fmt.Channels;
    }
    tjs_uint GetCurrentPlaySamples() override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return frameBytes_ ? (tjs_uint)(ring_.queued() / frameBytes_) : 0;
    }
    tjs_uint GetLatencySamples() override { return 0; }
    float GetLatencySeconds() override { return 0; }
    int GetRemainBuffers() override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return (int)(ring_.queued() / bufSize_);
    }
    void SetPosition(float, float, float) override {}
};

void TVPInitDirectSound(int)
{
    // miniaudio initializes devices lazily per buffer; nothing to do here.
}

void TVPUninitDirectSound()
{
}

iTVPSoundBuffer* TVPCreateSoundBuffer(tTVPWaveFormat& fmt, int count)
{
    auto* buffer = new tTVPMiniAudioSoundBuffer(fmt, count);
    if (buffer->Init())
        return buffer;
    delete buffer;
    return nullptr;
}
