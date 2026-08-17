// i18n.js — Bilingual string table (EN/ES), ported from translations.h.
// 49 keys matching the Go i18n.Table exactly. No emojis.

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
  destNotSet:     { en: "Destination not set",            es: "Destino no establecido" },

  // -- Parameters --
  parameters:     { en: "Parameters",                     es: "Parámetros" },
  colorSpace:     { en: "Color Space",                    es: "Espacio de color" },
  colorSpaceTip:  { en: "Color matrix to use for YCbCr conversion.", es: "Matriz de color para conversión YCbCr." },
  quantizer:      { en: "Quantizer",                      es: "Quantizer" },
  quantizerTip:   { en: "Image quantizer backend (Quality, Speed).", es: "Motor de cuantización (Calidad, Velocidad)." },
  redrawPeriod:   { en: "Anchor interval",                 es: "Intervalo de anclaje" },
  redrawPeriodTip:{ en: "Insert anchors at the specified interval to let decoders catch-up on long-lasting events. 0: disabled. Minimum: 1 second.", es: "Inserta anclajes en el intervalo indicado para que los decodificadores puedan alcanzar eventos de larga duración. 0: desactivado. Mínimo: 1 segundo." },

  // -- Engine Options --
  engineOpts:     { en: "Engine Options",                  es: "Opciones del Motor" },
  allowNormal:    { en: "Allow normal case object redefinition.", es: "Permitir redefinición normal case" },
  preferNormal:   { en: "Prefer normal case object redefinition.", es: "Preferir redefinición normal case" },
  fullPalette:    { en: "Write full palette.",            es: "Paleta Completa" },
  bothFormatsTip: { en: "Generate both .sup and .pes/.mui output formats.", es: "Generar formatos .sup y .pes/.mui." },
  bothFormats:    { en: "Generate both SUP and PES+MUI files.", es: "SUP + PES/MUI" },
  overlapBuf:     { en: "Allow palette update buffering.", es: "Permitir buffering de paleta" },
  ignoreRes:      { en: "Ignore Resolution Validation (Experimental)", es: "Ignorar Validación de Resolución (Experimental)" },

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
  tipIgnoreRes:    { en: "Ignore the warning when the input video resolution does not match the expected BDN resolution.", es: "Ignorar la advertencia cuando la resolución del video de entrada no coincide con la resolución BDN esperada." },
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