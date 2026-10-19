// Bilingual string table, mirrored by the Go table in ui/i18n/strings.go.

const VERSION = "2.0.0";

const Table = {
  // -- Header --
  subtitle:       { en: "PGS Subtitle Encoder",           es: "PGS Subtitle Encoder" },
  version:        { en: "v" + VERSION,                    es: "v" + VERSION },
  windowTitle:    { en: "OpenSUP v" + VERSION,            es: "OpenSUP v" + VERSION },

  // -- Project Scope --
  projectScope:   { en: "Project Scope",                  es: "Proyecto" },
  selectBdn:      { en: "Select bdn.xml file",            es: "Seleccionar archivo bdn.xml" },
  noFile:         { en: "No file selected",               es: "Ningún archivo seleccionado" },
  setOutput:      { en: "Set output",                     es: "Establecer salida" },
  openOutputFolder:{ en: "Open output folder",             es: "Abrir carpeta de salida" },
  jumpToLive:     { en: "Jump to latest log entry",        es: "Ir al final del registro" },
  destNotSet:     { en: "Destination not set",            es: "Destino no establecido" },

  // -- Parameters --
  parameters:     { en: "Parameters",                     es: "Parámetros" },
  advancedOptions:{ en: "Advanced Options",              es: "Opciones avanzadas" },
  colorSpace:     { en: "Color space: ",                  es: "Espacio de color: " },
  colorSpaceTip:  { en: "Color matrix for YCbCr conversion. Recommended: BT.709 (HD/SDR).", es: "Matriz de color para conversión YCbCr. Recomendado: BT.709 (HD/SDR)." },
  quantizer:      { en: "Quantizer: ",                    es: "Quantizer: " },
  quantHexTree:   { en: "HexTree (good, very fast)",      es: "HexTree (bueno, muy rápido)" },
  quantPiliq:     { en: "libimage (best, fast)",           es: "libimage (el mejor, rápido)" },
  quantizerTip:   { en: "Image quantizer backend. [0: QtzrUTC, 1: Pillow, 2: HexTree, 3: PNGQ/LIQ, 4: pngquant]. Available here: 2 and 3. Recommended: PNGQ/LIQ for final output; HexTree for fast tests.", es: "Motor de cuantización. [0: QtzrUTC, 1: Pillow, 2: HexTree, 3: PNGQ/LIQ, 4: pngquant]. Disponibles aquí: 2 y 3. Recomendado: PNGQ/LIQ para la salida final; HexTree para pruebas rápidas." },
  redrawPeriod:   { en: "Anchor interval",                 es: "Intervalo de anclaje" },
  redrawPeriodTip:{ en: "Insert anchors so decoders can catch up on long events. 0: disabled (recommended). For events over ~5 s try 1 s.", es: "Inserta anclajes para que los decodificadores alcancen eventos largos. 0: desactivado (recomendado). Para eventos de más de ~5 s prueba 1 s." },
  maxKbps:        { en: "Max bitrate",                              es: "Bitrate máximo" },
  maxKbpsTip:     { en: "Validate output against a max bitrate (Kbps). 0: disabled (recommended). To cap it: 10000-30000.", es: "Valida la salida contra un bitrate máximo (Kbps). 0: desactivado (recomendado). Para limitarlo: 10000-30000." },
  threads:        { en: "Threads: ",                            es: "Hilos: " },
  threadsTip:     { en: "Parallel epoch workers. Auto: recommended, uses all physical cores. 1: sequential.", es: "Hilos de épocas en paralelo. Auto: recomendado, usa todos los núcleos físicos. 1: secuencial." },
  auto:           { en: "Auto", es: "Auto" },
  compression:    { en: "Compression [int]%: ",                 es: "Compresión [int]%: " },
  compressionTip: { en: "Quality factor. 80 recommended (default). 0 = no compression (max quality, larger file); 100 = max compression.", es: "Factor de calidad. Recomendado 80 (por defecto). 0 = sin compresión (máx. calidad, archivo mayor); 100 = compresión máxima." },
  acqrate:        { en: "Acquisition rate [int]%: ",            es: "Tasa de adquisición [int]%: " },
  acqrateTip:     { en: "Drought scaling (0-100). 100 recommended (default behavior).", es: "Escalado de sequía (0-100). Recomendado 100 (comportamiento por defecto)." },
  ssimTol:        { en: "SSIM tolerance offset [-100;100]: ",   es: "Desviación de tolerancia SSIM [-100;100]: " },
  ssimTolTip:     { en: "SSIM tolerance. 0 recommended. Negative = stricter, positive = more lenient.", es: "Tolerancia SSIM. Recomendado 0. Negativo = más estricto, positivo = más permisivo." },
  extraAcq:       { en: "Insert acquisition after N palette updates [0-100]: ", es: "Insertar adquisición tras N actualizaciones de paleta [0-100]: " },
  extraAcqTip:    { en: "Force acquisition after N palette updates. 2 recommended (default). 0 = disabled.", es: "Forzar adquisición tras N actualizaciones de paleta. Recomendado 2 (por defecto). 0 = desactivado." },

  // -- Engine Options --
  engineOpts:     { en: "Engine Options",                  es: "Opciones del Motor" },
  allowNormal:    { en: "Allow normal case object redefinition.", es: "Permitir redefinición normal case" },
  preferNormal:   { en: "Prefer normal case object redefinition.", es: "Preferir redefinición normal case" },
  fullPalette:    { en: "Write the full palette.",         es: "Escribir la paleta completa." },
  bothFormatsTip: { en: "Generate both .sup and .pes/.mui output formats.", es: "Generar formatos .sup y .pes/.mui." },
  bothFormats:    { en: "Generate both SUP and PES+MUI files.", es: "SUP + PES/MUI" },
  overlapBuf:     { en: "Allow palette update buffering.",  es: "Permitir buffering de paleta" },
  alternateOids:  { en: "Force legacy global object ids (0/1).", es: "Forzar ids de objeto globales legacy (0/1)." },

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
  tipAllowNormal:  { en: "Registering the same graphic multiple times produces re-timing of the palette and small color shifts (up to a 0.6% of the inner 8 palette rows);\nwhen disabled, palette updates take effect twice as fast with subtle visual changes.", es: "Registrar el mismo gráfico varias veces produce un reajuste de los tiempos de la paleta y pequeños cambios de color (hasta un 0.6% de las 8 filas internas de la paleta);\ncuando se desactiva, las actualizaciones de paleta surten efecto el doble de rápido con cambios visuales sutiles." },
  tipFullPalette:  { en: "Don't optimize palette reduction when there are too many colors.\nInstead, just use the full palette.\nMay improve quality in some rare cases at the cost of bigger output size.", es: "No optimizar la reducción de paleta cuando hay demasiados colores.\nEn su lugar, usar la paleta completa.\nPuede mejorar la calidad en algunos casos raros a costa de un tamaño de salida mayor." },
  tipBothFormats:  { en: "Export also a .pes/.mui file alongside the .sup file.", es: "Exportar también un archivo .pes/.mui junto con el archivo .sup." },
  tipOverlapBuf:   { en: "Allow this encoder to generate overlapping objects in the output stream.\nThis method is more efficient but not well supported by some hardware decoders.", es: "Permitir que este codificador genere objetos superpuestos en el stream de salida.\nEste método es más eficiente pero no es bien soportado por algunos decodificadores de hardware." },
  tipAlternateOids:{ en: "Force legacy global object ids (0/1) instead of per-window ids.\nMay help older players; identical visual output.", es: "Forzar ids de objeto globales legacy (0/1) en lugar de ids por ventana.\nPuede ayudar a reproductores antiguos; salida visual idéntica." },
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