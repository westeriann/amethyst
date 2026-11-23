/* Converts MilkDrop .milk text into Butterchurn presets off the main thread,
   so importing or switching to a new preset never stalls the visuals. */
importScripts('vendor/milkdrop-preset-converter.min.js', 'shader-fix.js');

async function convert(text) {
  const conv = self.milkdropPresetConverter;
  try {
    // Parenthesise shader expressions first (works around a converter bug, see shader-fix.js).
    return await conv.convertPreset(self.AmethystShaderFix.fixMilk(text));
  } catch (_) {
    return conv.convertPreset(text);
  }
}

self.onmessage = async (e) => {
  const { id, text } = e.data;
  try {
    const preset = await convert(text);
    if (!preset || typeof preset.baseVals !== 'object') throw new Error('Converter returned no preset');
    self.postMessage({ id, preset });
  } catch (err) {
    self.postMessage({ id, error: String((err && err.message) || err || 'Unknown conversion error') });
  }
};
