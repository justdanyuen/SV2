#pragma once

#include <array>
#include <atomic>
#include <cstring>

namespace surround_vis {

inline constexpr int kFftBinCount  = 1024;
inline constexpr int kChannelCount = 6;   // L, R, C, LFE, Ls, Rs
inline constexpr int kGroupCount   = 6;   // Soprano, Mezzo, Alto, Tenor, Baritone, Bass

struct alignas(64) GroupSlot {
  std::atomic<uint32_t> seqLock{0};
  int32_t  colorGroupId{-1};
  int32_t  enabled{0};
  uint32_t lastWriteMs{0};  // juce::Time::getMillisecondCounter() at last write
  float    channelRms[kChannelCount]{};
  float    fftMagnitude[kFftBinCount]{};
};

struct SharedMemoryLayout {
  uint32_t magic{0x535653};
  uint32_t version{1};
  uint8_t  _pad[56]{};
  GroupSlot slots[kGroupCount];
};

class SharedMemoryBridge {
public:
  static juce::File getSharedFile() {
    // Use a fixed absolute path that works across macOS versions
    // including macOS 26 where sandbox temp paths may differ.
    const auto tmp = juce::File::getSpecialLocation(
        juce::File::SpecialLocationType::tempDirectory);
    jassert(tmp.exists());
    return tmp.getChildFile("sv2_shared.bin");
  }

  static constexpr juce::int64 kFileSize =
      static_cast<juce::int64>(sizeof(SharedMemoryLayout));

  bool open() {
    auto file = getSharedFile();
    if (!file.existsAsFile()) {
      juce::FileOutputStream out{file};
      if (out.failedToOpen()) return false;
      std::vector<uint8_t> zeros(static_cast<size_t>(kFileSize), 0);
      out.write(zeros.data(), zeros.size());
    } else {
      // Zero out stale slot timestamps so old data doesn't appear active
      // on first open before any satellites have written.
    }
    mappedFile = std::make_unique<juce::MemoryMappedFile>(
        file, juce::MemoryMappedFile::AccessMode::readWrite, false);
    if (!mappedFile->getData()) return false;
    layout = reinterpret_cast<SharedMemoryLayout*>(mappedFile->getData());
    if (layout->magic != 0x535653) {
      std::memset(layout, 0, sizeof(SharedMemoryLayout));
      layout->magic   = 0x535653;
      layout->version = 1;
    }
    return true;
  }

  void close() {
    mappedFile.reset();
    layout = nullptr;
  }

  bool isOpen() const noexcept { return layout != nullptr; }

  void writeSlot(int groupIndex,
                 int colorId,
                 bool isEnabled,
                 const float (&rms)[kChannelCount],
                 const float (&fft)[kFftBinCount]) {
    if (!layout) return;
    jassert(groupIndex >= 0 && groupIndex < kGroupCount);
    auto& slot = layout->slots[groupIndex];
    auto seq = slot.seqLock.load(std::memory_order_relaxed);
    slot.seqLock.store(seq + 1, std::memory_order_release);
    slot.colorGroupId = colorId;
    slot.enabled      = isEnabled ? 1 : 0;
    slot.lastWriteMs  = juce::Time::getMillisecondCounter();
    std::memcpy(slot.channelRms,   rms, sizeof(rms));
    std::memcpy(slot.fftMagnitude, fft, sizeof(fft));
    slot.seqLock.store(seq + 2, std::memory_order_release);
  }

  bool readSlot(int groupIndex,
                int&  outColorId,
                bool& outEnabled,
                float (&outRms)[kChannelCount],
                float (&outFft)[kFftBinCount]) const {
    if (!layout) return false;
    jassert(groupIndex >= 0 && groupIndex < kGroupCount);
    const auto& slot = layout->slots[groupIndex];
    for (int attempt = 0; attempt < 4; ++attempt) {
      const auto seq1 = slot.seqLock.load(std::memory_order_acquire);
      if (seq1 & 1u) continue;
      outColorId = slot.colorGroupId;
      outEnabled = slot.enabled != 0;
      const uint32_t writeMs = slot.lastWriteMs;
      std::memcpy(outRms, slot.channelRms,   sizeof(outRms));
      std::memcpy(outFft, slot.fftMagnitude, sizeof(outFft));
      const auto seq2 = slot.seqLock.load(std::memory_order_acquire);
      if (seq1 != seq2) continue;
      // Treat slot as stale if not written in the last 500ms
      const uint32_t now = juce::Time::getMillisecondCounter();
      const uint32_t age = now - writeMs;
      if (age > 500u) return false;
      return outEnabled;
    }
    return false;
  }

private:
  std::unique_ptr<juce::MemoryMappedFile> mappedFile;
  SharedMemoryLayout* layout{nullptr};
};

}  // namespace surround_vis
