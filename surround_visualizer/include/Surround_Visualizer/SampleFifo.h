#pragma once

namespace surround_vis {

/** A single-producer, single-consumer FIFO queue to retrieve a single channel
 * of samples from the audio thread.
 * Unchanged from the original tremolo implementation — only the namespace
 * has been updated. */
template <typename SampleType>
class SampleFifo {
public:
  void prepare(double sampleRate) {
    const auto sampleCapacity = static_cast<int>(1.0 * sampleRate);
    buffer.setSize(1, sampleCapacity);
    buffer.clear();
    fifo.setTotalSize(sampleCapacity);
  }

  void push(SampleType sample) {
    const auto scope = fifo.write(1);
    if (scope.blockSize1 > 0)
      buffer.setSample(0, scope.startIndex1, sample);
    else if (scope.blockSize2 > 0)
      buffer.setSample(0, scope.startIndex2, sample);
  }

  void popAll(juce::AudioBuffer<SampleType>& bufferToFill) {
    const auto sampleCount = fifo.getNumReady();
    bufferToFill.setSize(1, sampleCount, false, false, true);
    const auto scope = fifo.read(sampleCount);
    const auto* src = buffer.getReadPointer(0);
    auto* dst = bufferToFill.getWritePointer(0);
    if (scope.blockSize1 > 0)
      std::copy_n(src + scope.startIndex1, scope.blockSize1, dst);
    if (scope.blockSize2 > 0)
      std::copy_n(src + scope.startIndex2, scope.blockSize2, dst + scope.blockSize1);
  }

  void reset() {
    fifo.reset();
    buffer.clear();
  }

private:
  static constexpr auto initialCapacity = 1024;
  juce::AbstractFifo fifo{initialCapacity};
  juce::AudioBuffer<SampleType> buffer{1, initialCapacity};
};

}  // namespace surround_vis
