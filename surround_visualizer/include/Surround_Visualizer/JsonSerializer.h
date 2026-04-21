#pragma once

namespace surround_vis {

// Serializes and deserializes plugin parameters to/from a simple JSON format.
// Mirrors the pattern from the original tremolo JsonSerializer but handles
// the new surround_vis Parameters struct.
struct JsonSerializer {
  static void serialize(const Parameters& params,
                        juce::OutputStream& stream) {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("voiceGroup",   params.voiceGroup.getIndex());
    obj->setProperty("enabled",      params.enabled.get());
    obj->setProperty("role",         params.role.getIndex());
    obj->setProperty("analysisTrim", params.analysisTrim.get());

    juce::var root{obj.get()};
    stream.writeString(juce::JSON::toString(root));
  }

  static juce::Result deserialize(juce::InputStream& stream,
                                  Parameters& params) {
    const auto jsonText = stream.readEntireStreamAsString();
    juce::var root;
    const auto parseResult = juce::JSON::parse(jsonText, root);
    if (parseResult.failed())
      return parseResult;

    auto* obj = root.getDynamicObject();
    if (!obj)
      return juce::Result::fail("JSON root is not an object");

    if (obj->hasProperty("voiceGroup"))
      params.voiceGroup.setValueNotifyingHost(
          params.voiceGroup.convertTo0to1(
              static_cast<int>(obj->getProperty("voiceGroup"))));

    if (obj->hasProperty("enabled"))
      params.enabled.setValueNotifyingHost(
          static_cast<bool>(obj->getProperty("enabled")) ? 1.f : 0.f);

    if (obj->hasProperty("role"))
      params.role.setValueNotifyingHost(
          params.role.convertTo0to1(
              static_cast<int>(obj->getProperty("role"))));

    if (obj->hasProperty("analysisTrim"))
      params.analysisTrim.setValueNotifyingHost(
          params.analysisTrim.convertTo0to1(
              static_cast<float>(obj->getProperty("analysisTrim"))));

    return juce::Result::ok();
  }
};

}  // namespace surround_vis
