// Package i18n provides bilingual string lookups (English / Spanish),
// mirrored by the frontend table in ui/frontend/src/shared/i18n.js.
package i18n

import "strings"

// Lang mirrors config.Language (0=EN, 1=ES).
type Lang int

const (
	EN Lang = iota
	ES
)

// Version is concatenated into the windowTitle and version keys.
const Version = "2.0.0"

// Table maps each key to a pair (EN, ES).
var Table = map[string][2]string{
	// -- Header --
	"subtitle":       {"PGS Subtitle Encoder", "PGS Subtitle Encoder"},
	"version":        {"v" + Version, "v" + Version},
	"windowTitle":    {"OpenSUP v" + Version, "OpenSUP v" + Version},

	// -- Project Scope --
	"projectScope":   {"Project Scope", "Proyecto"},
	"selectBdn":     {"Select BDN XML", "Seleccionar BDN XML"},
	"noFile":          {"No file selected", "Ningún archivo seleccionado"},
	"setOutput":      {"Set SUP Output", "Establecer destino SUP"},
	"openOutputFolder": {"Open output folder", "Abrir carpeta de salida"},
	"jumpToLive":      {"Jump to latest log entry", "Ir al final del registro"},
	"destNotSet":     {"Destination not set", "Destino no establecido"},

	// -- Parameters --
	"parameters":      {"Parameters", "Parámetros"},
	"advancedOptions": {"Advanced Options", "Opciones avanzadas"},
	"colorSpace":      {"Color Space", "Espacio de color"},
	"colorSpaceTip":  {"Color matrix for YCbCr conversion. Recommended: BT.709 (HD/SDR).", "Matriz de color para conversión YCbCr. Recomendado: BT.709 (HD/SDR)."},
	"quantizer":       {"Quantizer", "Quantizer"},
	"quantizerTip":   {"Image quantizer backend. Recommended: libimagequant for final output; HexTree for fast tests.", "Motor de cuantización. Recomendado: libimagequant para la salida final; HexTree para pruebas rápidas."},
	"redrawPeriod":   {"Anchor interval", "Intervalo de anclaje"},
	"redrawPeriodTip": {"Insert anchors so decoders can catch up on long events. 0: disabled (recommended). For events over ~5 s try 1 s.", "Inserta anclajes para que los decodificadores alcancen eventos largos. 0: desactivado (recomendado). Para eventos de más de ~5 s prueba 1 s."},
	"maxKbps":        {"Max bitrate", "Bitrate máximo"},
	"maxKbpsTip":     {"Validate output against a max bitrate (Kbps). 0: disabled (recommended). To cap it: 10000-30000.", "Valida la salida contra un bitrate máximo (Kbps). 0: desactivado (recomendado). Para limitarlo: 10000-30000."},
	"threads":        {"Threads", "Hilos"},
	"threadsTip":     {"Parallel epoch workers. Auto: recommended, uses all physical cores. 1: sequential.", "Hilos de épocas en paralelo. Auto: recomendado, usa todos los núcleos físicos. 1: secuencial."},
	"auto":           {"Auto", "Auto"},
	"compression":    {"Compression", "Compresión"},
	"compressionTip": {"Quality factor. 80 recommended (default). 0 = no compression (max quality, larger file); 100 = max compression.", "Factor de calidad. Recomendado 80 (por defecto). 0 = sin compresión (máx. calidad, archivo mayor); 100 = compresión máxima."},
	"acqrate":        {"Acq. Rate", "Tasa adq."},
	"acqrateTip":     {"Drought scaling (0-100). 100 recommended (default behavior).", "Escalado de sequía (0-100). Recomendado 100 (comportamiento por defecto)."},
	"ssimTol":        {"SSIM Tol.", "Tol. SSIM"},
	"ssimTolTip":     {"SSIM tolerance. 0 recommended. Negative = stricter, positive = more lenient.", "Tolerancia SSIM. Recomendado 0. Negativo = más estricto, positivo = más permisivo."},
	"extraAcq":       {"Extra Acq", "Adq. extra"},
	"extraAcqTip":    {"Force acquisition after N palette updates. 2 recommended (default). 0 = disabled.", "Forzar adquisición tras N actualizaciones de paleta. Recomendado 2 (por defecto). 0 = desactivado."},

	// -- Engine Options --
	"engineOpts":      {"Engine Options", "Opciones del Motor"},
	"allowNormal":     {"Allow normal case object redefinition.", "Permitir redefinición normal case"},
	"preferNormal":    {"Prefer normal case object redefinition.", "Preferir redefinición normal case"},
"fullPalette":     {"Write full palette.", "Paleta Completa"},
  "bothFormatsTip": {"Generate both .sup and .pes/.mui output formats.", "Generar formatos .sup y .pes/.mui."},
  "bothFormats":     {"Generate both SUP and PES+MUI files.", "SUP + PES/MUI"},
  "overlapBuf":      {"Allow palette update buffering.", "Permitir buffering de paleta"},
  "alternateOids":   {"Alternate per-window object ids (multi-window).", "Alternate per-window object ids (multi-window)."},
  "ignoreRes":       {"Ignore Resolution Validation (Experimental)", "Ignorar Validación de Resolución (Experimental)"},

	// -- Activity Log --
	"activityLog":     {"ACTIVITY LOG", "REGISTRO DE ACTIVIDAD"},
	"copy":            {"Copy", "Copiar"},
	"clear":           {"Clear", "Limpiar"},
	"copied":          {"Copied", "Copiado"},
	"copyLog":         {"Copy log", "Copiar registro"},
	"logLines":        {"[%1 lines]", "[%1 líneas]"},

	// -- Encode --
	"progress":        {"Progress", "Progreso"},
	"standingBy":      {"Standing by", "En espera"},
	"working":         {"Working", "Procesando"},
	"finished":        {"Finished", "Finalizado"},
	"initEncode":      {"ENCODE", "PROCESAR"},
	"abort":           {"ABORT", "ABORTAR"},
	"starting":        {"Encoding…", "Codificando…"},
	"done":            {"Done", "Listo"},
	"failed":          {"Encoding FAILED – see log for details", "CODIFICACIÓN FALLIDA – ver el registro"},
	"abortedShort":   {"Encoding Aborted", "Codificación Abortada"},
	"aborted":         {"Aborted", "Abortado"},
	"abortLogMsg":     {"Encoding aborted by user.", "Codificación abortada por el usuario."},

	// -- Theme --
	"themeSystem":    {"System", "Sistema"},
	"themeLight":     {"Light", "Claro"},
	"themeDark":      {"Dark", "Oscuro"},

	// -- Checkbox tooltips --
	"tipAllowNormal": {
		"Update only one composition out of the two, whenever updating both is not possible due to time constraints.\nThis exploits the PG object buffer capabilities as intended by the format designers.\nStream shall NOT be Built or Rebuilt at the authoring stage.",
		"Actualiza solo una composición de las dos, cuando no es posible actualizar ambas por restricciones de tiempo.\nEsto aprovecha las capacidades del buffer de objetos PG según lo diseñado por el formato.\nEl stream NO debe ser Construido ni Reconstruido en la etapa de authoring.",
	},
	"tipPreferNormal": {
		"Update only one composition out of the two, even when decoding time is sufficient to refresh both (default).\nIt can reduce the bitrate, but the palette is not shared across composition objects whenever it occurs.",
		"Actualiza solo una composición de las dos, incluso cuando hay tiempo suficiente para refrescar ambas (predeterminado).\nPuede reducir el bitrate, pero la paleta no se comparte entre objetos de composición cuando esto ocurre.",
	},
	"tipFullPalette": {
		"Don't optimize palette reduction when there are too many colors.\nInstead, just use the full palette.\nMay improve quality in some rare cases at the cost of bigger output size.",
		"No optimizar la reducción de paleta cuando hay demasiados colores.\nEn su lugar, usar la paleta completa.\nPuede mejorar la calidad en algunos casos raros a costa de un tamaño de salida mayor.",
	},
	"tipBothFormats": {
		"Export also a .pes/.mui file alongside the .sup file.",
		"Exportar también un archivo .pes/.mui junto con el archivo .sup.",
	},
	"tipOverlapBuf": {
		"Allow this encoder to generate overlapping objects in the output stream.\nThis method is more efficient but not well supported by some hardware decoders.",
		"Permitir que este codificador genere objetos superpuestos en el stream de salida.\nEste método es más eficiente pero no es bien soportado por algunos decodificadores de hardware.",
	},
	"tipAlternateOids": {
		"Alternate the object id per window on every acquisition (double buffering).\nAvoids tearing on hardware that reuses object buffers; identical visual output.",
		"Alternar el id de objeto por ventana en cada adquisición (doble buffer).\nEvita tearing en hardware que reutiliza buffers de objeto; salida visual idéntica.",
	},
	"tipIgnoreRes": {
		"Enable only if the BDN uses a non-standard resolution.\nUsing this option improperly may produce streams where some events do not display on some players.",
		"Actívalo solo si el BDN usa una resolución no estándar.\nUsar esta opción indebidamente puede producir streams donde algunos eventos no se muestren en algunos reproductores.",
	},
}

// Get returns the translated string for key in lang. If the key is
// unknown, the key itself is returned so missing translations are
// visible rather than blank.
func Get(lang Lang, key string) string {
	e, ok := Table[key]
	if !ok {
		return key
	}
	if lang == ES {
		return e[1]
	}
	return e[0]
}

// Format replaces positional tokens like "%1" with the supplied value
// (only %1..%9 is supported; the app rarely uses more than one).
func Format(s string, args ...string) string {
	for i, a := range args {
		s = strings.ReplaceAll(s, "%"+string(rune('0'+i+1)), a)
	}
	return s
}