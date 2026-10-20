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
  colorSpace:     { en: "Color space",                    es: "Espacio de color" },
  colorSpaceTip:  { en: "Color matrix for YCbCr conversion. Recommended: BT.709 (HD/SDR).", es: "Matriz de color para conversión YCbCr. Recomendado: BT.709 (HD/SDR)." },
  quantizer:      { en: "Quantizer: ",                    es: "Quantizer: " },
  quantHexTree:   { en: "HexTree (good, very fast)",      es: "HexTree (bueno, muy rápido)" },
  quantPiliq:     { en: "libimage (best, fast)",           es: "libimage (el mejor, rápido)" },
  quantizerTip:   { en: "Image quantizer backend. [0: QtzrUTC, 1: Pillow, 2: HexTree, 3: PNGQ/LIQ]. Available here: 2 and 3. Recommended: PNGQ/LIQ for final output; HexTree for fast tests.", es: "Motor de cuantización. [0: QtzrUTC, 1: Pillow, 2: HexTree, 3: PNGQ/LIQ]. Disponibles aquí: 2 y 3. Recomendado: PNGQ/LIQ para la salida final; HexTree para pruebas rápidas." },
  redrawPeriod:   { en: "Redraw period",                   es: "Periodo de redibujado" },
  redrawPeriodTip:{ en: "Insert anchors so decoders can catch up on long events. Range: 0 s or more (0.1 s steps). 0: disabled (recommended). For events over ~5 s try 1 s.", es: "Inserta anclajes para que los decodificadores alcancen eventos largos. Rango: 0 s o más (pasos de 0,1 s). 0: desactivado (recomendado). Para eventos de más de ~5 s prueba 1 s." },
  maxKbps:        { en: "Max bitrate",                              es: "Bitrate máximo" },
  maxKbpsTip:     { en: "Validate output against a max bitrate (Kbps). Range: 0-48000 (step 100). 0: disabled (recommended). To cap it: 10000-30000.", es: "Valida la salida contra un bitrate máximo (Kbps). Rango: 0-48000 (paso 100). 0: desactivado (recomendado). Para limitarlo: 10000-30000." },
  threads:        { en: "Threads",                              es: "Hilos" },
  threadsTip:     { en: "Parallel epoch workers. Auto (0): recommended, uses all physical cores. 1: sequential.", es: "Hilos de épocas en paralelo. Auto (0): recomendado, usa todos los núcleos físicos. 1: secuencial." },
  auto:           { en: "Auto", es: "Auto" },
  compression:    { en: "Compression",                          es: "Compresión" },
  compressionTip: { en: "Quality factor (0-100). 80 recommended (default). 0 = no compression (max quality, larger file); 100 = max compression.", es: "Factor de calidad (0-100). Recomendado 80 (por defecto). 0 = sin compresión (máx. calidad, archivo mayor); 100 = compresión máxima." },
  acqrate:        { en: "Acquisition rate",                     es: "Tasa de adquisición" },
  acqrateTip:     { en: "Drought scaling (0-100). 100 recommended (default behavior).", es: "Escalado de sequía (0-100). Recomendado 100 (comportamiento por defecto)." },
  ssimTol:        { en: "SSIM tolerance",                       es: "Tolerancia SSIM" },
  ssimTolTip:     { en: "SSIM tolerance offset (-100 to 100). 0 recommended. Negative = stricter, positive = more lenient.", es: "Desviación de tolerancia SSIM (-100 a 100). Recomendado 0. Negativo = más estricto, positivo = más permisivo." },
  extraAcq:       { en: "Extra acquisition",                    es: "Adquisición extra" },
  extraAcqTip:    { en: "Force acquisition after N palette updates (0-100). 2 recommended (default). 0 = disabled.", es: "Forzar adquisición tras N actualizaciones de paleta (0-100). Recomendado 2 (por defecto). 0 = desactivado." },

  // -- Engine Options --
  engineOpts:     { en: "Engine Options",                  es: "Opciones del Motor" },
  allowNormal:    { en: "Allow normal case object redefinition.", es: "Permitir redefinición normal case" },
  preferNormal:   { en: "Prefer normal case object redefinition.", es: "Preferir redefinición normal case" },
  fullPalette:    { en: "Write the full palette.",         es: "Escribir la paleta completa." },
  bothFormatsTip: { en: "Generate both .sup and .pes/.mui output formats.", es: "Generar formatos .sup y .pes/.mui." },
  bothFormats:    { en: "Generate both SUP and PES+MUI files.", es: "SUP + PES/MUI" },
  overlapBuf:     { en: "Allow palette update buffering.",  es: "Permitir buffering de paleta" },
  alternateOids:  { en: "Use global object IDs 0/1 (interoperability)", es: "Usar IDs de objeto globales 0/1 (interoperabilidad)" },
  ignoreResolution: { en: "Ignore resolution validation.", es: "Ignorar validación de resolución." },
  tightOds:       { en: "Fit ODS box to text",               es: "Ajustar caja al texto" },

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
  tipAllowNormal:  { en: "Update only one composition out of the two, whenever updating both is not possible due to time constraints. This exploits the PG object buffer capabilities as intended by the format designers. Stream shall NOT be Built or Rebuilt at the authoring stage.", es: "Actualizar solo una de las dos composiciones, cuando no sea posible actualizar ambas debido a limitaciones de tiempo. Esto aprovecha las capacidades del búfer de objetos PG tal como lo concibieron los diseñadores del formato. El stream NO debe ser Built ni Rebuilt en la etapa de autoría." },
  tipPreferNormal: { en: "Update only one composition out of the two, even when decoding time is sufficient to refresh both. It can reduce the bitrate, but the palette is not shared across composition objects whenever it occurs.", es: "Actualizar solo una de las dos composiciones, incluso cuando el tiempo de decodificación es suficiente para refrescar ambas. Puede reducir el bitrate, pero la paleta no se comparte entre los objetos de composición cuando esto ocurre." },
  tipFullPalette:  { en: "Some authoring suites meddle with the imported files and may mess up the palette assignments. Writing the full palette everytime ensures palette data consistency throughout the stream.", es: "Algunas suites de autoría manipulan los archivos importados y pueden alterar las asignaciones de paleta. Escribir la paleta completa cada vez garantiza la consistencia de los datos de paleta en todo el stream." },
  tipBothFormats:  { en: "Export also a .pes/.mui file alongside the .sup file.", es: "Exportar también un archivo .pes/.mui junto con el archivo .sup." },
  tipOverlapBuf:   { en: "Buffer palette updates whenever possible to drop fewer events. Stream shall NOT be Built or Rebuilt at the authoring stage.", es: "Buffear las actualizaciones de paleta siempre que sea posible para descartar menos eventos. El stream NO debe ser Built ni Rebuilt en la etapa de autoría." },
  tipAlternateOids:{ en: "Use global object IDs 0/1 instead of per-window ids.\nThe legacy mode can alter the picture when several windows share a display set; the per-window default avoids buffer clobbering.", es: "Usar IDs de objeto globales 0/1 en lugar de ids por ventana.\nEl modo legacy puede alterar la visualización cuando varias ventanas comparten display set; el default por ventana evita el pisado de buffers." },
  tipTightOds:    { en: "Crops each event's ODS box to its exact content instead of reusing the union box shared across the event chain.\nImproves image fit of late events in authoring tools.", es: "Recorta la caja ODS de cada evento a su contenido exacto en lugar de reutilizar la caja unión compartida por la cadena de eventos.\nMejora el encaje de la imagen en herramientas de edición." },
  tipIgnoreResolution: { en: "Enable only if the BDN uses a non-standard resolution.\nUsing this option improperly may produce streams where some events do not display on some players.", es: "Actívalo solo si el BDN usa una resolución no estándar.\nUsar esta opción indebidamente puede producir streams donde algunos eventos no se muestren en algunos reproductores." },
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