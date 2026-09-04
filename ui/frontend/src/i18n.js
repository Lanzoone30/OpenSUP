// i18n.js — Bilingual string table (EN/ES), ported from translations.h.
// 63 keys (UI-side subset; the Go table also holds engine-side keys). No emojis.

const Table = {
  // -- Header --
  subtitle:       { en: "PGS Subtitle Encoder",           es: "PGS Subtitle Encoder" },
  windowTitle:    { en: "OpenSUP v" + "1.1.0",            es: "OpenSUP v" + "1.1.0" },

  // -- Project Scope --
  projectScope:   { en: "Project Scope",                  es: "Proyecto" },
  selectBdn:      { en: "Select BDN XML",                 es: "Seleccionar BDN XML" },
  noFile:         { en: "No file selected",               es: "Ningún archivo seleccionado" },
  setOutput:      { en: "Set SUP Output",                 es: "Establecer destino SUP" },
  openOutputFolder:{ en: "Open output folder",             es: "Abrir carpeta de salida" },
  jumpToLive:     { en: "Jump to latest log entry",        es: "Ir al final del registro" },
  destNotSet:     { en: "Destination not set",            es: "Destino no establecido" },

  // -- Parameters --
  parameters:     { en: "Parameters",                     es: "Parámetros" },
  advancedOptions:{ en: "Advanced Options",              es: "Opciones avanzadas" },
  colorSpace:     { en: "Color Space",                    es: "Espacio de color" },
  colorSpaceTip:  { en: "Color matrix for YCbCr conversion. Recommended: BT.709 (HD/SDR).", es: "Matriz de color para conversión YCbCr. Recomendado: BT.709 (HD/SDR)." },
  quantizer:      { en: "Quantizer",                      es: "Quantizer" },
  quantizerTip:   { en: "Image quantizer backend. Recommended: libimagequant for final output; HexTree for fast tests.", es: "Motor de cuantización. Recomendado: libimagequant para la salida final; HexTree para pruebas rápidas." },
  redrawPeriod:   { en: "Anchor interval",                 es: "Intervalo de anclaje" },
  redrawPeriodTip:{ en: "Insert anchors so decoders can catch up on long events. 0: disabled (recommended). For events over ~5 s try 1 s.", es: "Inserta anclajes para que los decodificadores alcancen eventos largos. 0: desactivado (recomendado). Para eventos de más de ~5 s prueba 1 s." },
  maxKbps:        { en: "Max bitrate",                              es: "Bitrate máximo" },
  maxKbpsTip:     { en: "Validate output against a max bitrate (Kbps). 0: disabled (recommended). To cap it: 10000-30000.", es: "Valida la salida contra un bitrate máximo (Kbps). 0: desactivado (recomendado). Para limitarlo: 10000-30000." },
  threads:        { en: "Threads",                              es: "Hilos" },
  threadsTip:     { en: "Parallel epoch workers. 0: Auto (recommended, uses all cores). 1: sequential.", es: "Hilos de épocas en paralelo. 0: Automático (recomendado, usa todos los núcleos). 1: secuencial." },
  compression:    { en: "Compression",                        es: "Compresión" },
  compressionTip: { en: "Quality factor. 80 recommended (default). 0 = no compression (max quality, larger file); 100 = max compression.", es: "Factor de calidad. Recomendado 80 (por defecto). 0 = sin compresión (máx. calidad, archivo mayor); 100 = compresión máxima." },
  acqrate:        { en: "Acq. Rate",                          es: "Tasa adq." },
  acqrateTip:     { en: "Drought scaling (0-100). 100 recommended (default behavior).", es: "Escalado de sequía (0-100). Recomendado 100 (comportamiento por defecto)." },
  ssimTol:        { en: "SSIM Tol.",                          es: "Tol. SSIM" },
  ssimTolTip:     { en: "SSIM tolerance. 0 recommended. Negative = stricter, positive = more lenient.", es: "Tolerancia SSIM. Recomendado 0. Negativo = más estricto, positivo = más permisivo." },
  extraAcq:       { en: "Extra Acq",                          es: "Adq. extra" },
  extraAcqTip:    { en: "Force acquisition after N palette updates. 2 recommended (default). 0 = disabled.", es: "Forzar adquisición tras N actualizaciones de paleta. Recomendado 2 (por defecto). 0 = desactivado." },

  // -- Engine Options --
  engineOpts:     { en: "Engine Options",                  es: "Opciones del Motor" },
  allowNormal:    { en: "Allow normal case object redefinition.", es: "Allow normal case object redefinition." },
  preferNormal:   { en: "Prefer normal case object redefinition.", es: "Prefer normal case object redefinition." },
  fullPalette:    { en: "Write full palette.",            es: "Write full palette." },
  bothFormatsTip: { en: "Generate both .sup and .pes/.mui output formats.", es: "Generar formatos .sup y .pes/.mui." },
  bothFormats:    { en: "Generate both SUP and PES+MUI files.", es: "Generar archivos SUP y PES+MUI." },
  overlapBuf:     { en: "Allow palette update buffering.", es: "Allow palette update buffering." },
  ignoreRes:      { en: "Ignore Resolution Validation", es: "Ignorar Validación de Resolución" },

  // -- Activity Log --
  activityLog:    { en: "ACTIVITY LOG",                   es: "REGISTRO DE ACTIVIDAD" },
  copy:           { en: "Copy",                           es: "Copiar" },
  clear:          { en: "Clear",                          es: "Limpiar" },
  copied:         { en: "Copied",                          es: "Copiado" },
  copyLog:        { en: "Copy log",                       es: "Copiar registro" },
  logLines:       { en: "[%1 lines]",                     es: "[%1 líneas]" },

  // -- Encode --
  progress:       { en: "Progress",                      es: "Progreso" },
  standingBy:     { en: "Standing by",                    es: "En espera" },
  working:        { en: "Working",                        es: "Procesando" },
  finished:       { en: "Finished",                       es: "Finalizado" },
  initEncode:     { en: "ENCODE",                          es: "PROCESAR" },
  abort:          { en: "ABORT",                           es: "ABORTAR" },
  starting:       { en: "Encoding",                       es: "Codificando" },
  done:           { en: "Done",                            es: "Listo" },
  failed:         { en: "Encoding FAILED — see log for details", es: "Codificación FALLIDA — ver el registro" },
  abortedShort:   { en: "Encoding Aborted",               es: "Codificación Abortada" },
  aborted:        { en: "Aborted",                        es: "Abortado" },
  abortLogMsg:    { en: "Encoding aborted by user.",      es: "Codificación abortada por el usuario." },

  // -- Theme --
  themeSystem:    { en: "System",                         es: "Sistema" },
  themeLight:     { en: "Light",                          es: "Claro" },
  themeDark:      { en: "Dark",                           es: "Oscuro" },

  // -- Checkbox tooltips --
  tipAllowNormal:  { en: "Update only one composition out of the two, whenever updating both is not possible due to time constraints.\nThis exploits the PG object buffer capabilities as intended by the format designers.\nStream shall NOT be Built or Rebuilt at the authoring stage.", es: "Actualiza solo una composición de las dos, cuando no es posible actualizar ambas por restricciones de tiempo.\nEsto aprovecha las capacidades del buffer de objetos PG según lo diseñado por el formato.\nEl stream NO debe ser Construido ni Reconstruido en la etapa de authoring." },
  tipPreferNormal: { en: "Update only one composition out of the two, even when decoding time is sufficient to refresh both (default).\nIt can reduce the bitrate, but the palette is not shared across composition objects whenever it occurs.", es: "Actualiza solo una composición de las dos, incluso cuando hay tiempo suficiente para refrescar ambas (predeterminado).\nPuede reducir el bitrate, pero la paleta no se comparte entre objetos de composición cuando esto ocurre." },
  tipFullPalette:  { en: "Don't optimize palette reduction when there are too many colors.\nInstead, just use the full palette.\nMay improve quality in some rare cases at the cost of bigger output size.", es: "No optimizar la reducción de paleta cuando hay demasiados colores.\nEn su lugar, usar la paleta completa.\nPuede mejorar la calidad en algunos casos raros a costa de un tamaño de salida mayor." },
  tipBothFormats:  { en: "Export also a .pes/.mui file alongside the .sup file.", es: "Exportar también un archivo .pes/.mui junto con el archivo .sup." },
  tipOverlapBuf:   { en: "Allow this encoder to generate overlapping objects in the output stream.\nThis method is more efficient but not well supported by some hardware decoders.", es: "Permitir que este codificador genere objetos superpuestos en el stream de salida.\nEste método es más eficiente pero no es bien soportado por algunos decodificadores de hardware." },
  alternateOids:   { en: "Alternate per-window object ids (multi-window).", es: "Alternate per-window object ids (multi-window)." },
  tipAlternateOids:{ en: "Alternate the object id per window on every acquisition (double buffering).\nAvoids tearing on hardware that reuses object buffers; identical visual output.", es: "Alternar el id de objeto por ventana en cada adquisición (doble buffer).\nEvita tearing en hardware que reutiliza buffers de objeto; salida visual idéntica." },
  tipIgnoreRes:    { en: "Ignore the warning when the input video resolution does not match the expected BDN resolution.\nUsing this option improperly may produce streams that fail on some players.", es: "Ignorar la advertencia cuando la resolución del video de entrada no coincide con la resolución BDN esperada.\nUsar esta opción indebidamente puede producir streams que fallen en algunos reproductores." },
};

// Current language: 0 = EN, 1 = ES
let currentLang = 0;

/** Translate a key in the current language. Unknown keys return themselves. */
export function t(key) {
  const e = Table[key];
  if (!e) return key;
  return currentLang === 1 ? e.es : e.en;
}

/** Format a string replacing %1..%9 with args. */
export function fmt(str, ...args) {
  return args.reduce((s, a, i) => s.replace("%" + (i + 1), a), str);
}

/** Set the active language (0=EN, 1=ES). */
export function setLang(lang) { currentLang = lang; }

/** Get the active language. */
export function getLang() { return currentLang; }

/** Return the full table. */
export { Table };