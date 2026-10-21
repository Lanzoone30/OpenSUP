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
const Version = "2.1.0"

// Table maps each key to a pair (EN, ES).
var Table = map[string][2]string{
	// -- Header --
	"subtitle":    {"PGS Subtitle Encoder", "PGS Subtitle Encoder"},
	"version":     {"v" + Version, "v" + Version},
	"windowTitle": {"OpenSUP v" + Version, "OpenSUP v" + Version},

	// -- Project Scope --
	"projectScope":     {"Project Scope", "Proyecto"},
	"selectBdn":        {"Select bdn.xml file", "Seleccionar archivo bdn.xml"},
	"noFile":           {"No file selected", "Ningún archivo seleccionado"},
	"setOutput":        {"Set output", "Establecer salida"},
	"openOutputFolder": {"Open output folder", "Abrir carpeta de salida"},
	"jumpToLive":       {"Jump to latest log entry", "Ir al final del registro"},
	"destNotSet":       {"Destination not set", "Destino no establecido"},

	// -- Parameters --
	"parameters":      {"Parameters", "Parámetros"},
	"advancedOptions": {"Advanced Options", "Opciones avanzadas"},
	"colorSpace":      {"Color space", "Espacio de color"},
	"colorSpaceTip":   {"Color matrix for YCbCr conversion. Recommended: BT.709 (HD/SDR).", "Matriz de color para conversión YCbCr. Recomendado: BT.709 (HD/SDR)."},
	"quantizer":       {"Quantizer: ", "Quantizer: "},
	"quantHexTree":    {"HexTree (good, very fast)", "HexTree (bueno, muy rápido)"},
	"quantPiliq":      {"libimage (best, fast)", "libimage (el mejor, rápido)"},
	"quantizerTip":    {"Image quantizer backend. [0: QtzrUTC, 1: Pillow, 2: HexTree, 3: PNGQ/LIQ]. Available here: 2 and 3. Recommended: PNGQ/LIQ for final output; HexTree for fast tests.", "Motor de cuantización. [0: QtzrUTC, 1: Pillow, 2: HexTree, 3: PNGQ/LIQ]. Disponibles aquí: 2 y 3. Recomendado: PNGQ/LIQ para la salida final; HexTree para pruebas rápidas."},
	"redrawPeriod":    {"Redraw period", "Periodo de redibujado"},
	"redrawPeriodTip": {"Insert anchors so decoders can catch up on long events. Range: 0 s or more (0.1 s steps). 0: disabled (recommended). For events over ~5 s try 1 s.", "Inserta anclajes para que los decodificadores alcancen eventos largos. Rango: 0 s o más (pasos de 0,1 s). 0: desactivado (recomendado). Para eventos de más de ~5 s prueba 1 s."},
	"maxKbps":         {"Max bitrate", "Bitrate máximo"},
	"maxKbpsTip":      {"Validate output against a max bitrate (Kbps). Range: 0-48000 (step 100). 0: disabled (recommended). To cap it: 10000-30000.", "Valida la salida contra un bitrate máximo (Kbps). Rango: 0-48000 (paso 100). 0: desactivado (recomendado). Para limitarlo: 10000-30000."},
	"threads":         {"Threads", "Hilos"},
	"threadsTip":      {"Parallel epoch workers. Auto (0): recommended, uses all physical cores. 1: sequential.", "Hilos de épocas en paralelo. Auto (0): recomendado, usa todos los núcleos físicos. 1: secuencial."},
	"auto":            {"Auto", "Auto"},
	"compression":     {"Compression", "Compresión"},
	"compressionTip":  {"Quality factor (0-100). 80 recommended (default). 0 = no compression (max quality, larger file); 100 = max compression.", "Factor de calidad (0-100). Recomendado 80 (por defecto). 0 = sin compresión (máx. calidad, archivo mayor); 100 = compresión máxima."},
	"acqrate":         {"Acquisition rate", "Tasa de adquisición"},
	"acqrateTip":      {"Drought scaling (0-100). 100 recommended (default behavior).", "Escalado de sequía (0-100). Recomendado 100 (comportamiento por defecto)."},
	"ssimTol":         {"SSIM tolerance", "Tolerancia SSIM"},
	"ssimTolTip":      {"SSIM tolerance offset (-100 to 100). 0 recommended. Negative = stricter, positive = more lenient.", "Desviación de tolerancia SSIM (-100 a 100). Recomendado 0. Negativo = más estricto, positivo = más permisivo."},
	"extraAcq":        {"Extra acquisition", "Adquisición extra"},
	"extraAcqTip":     {"Force acquisition after N palette updates (0-100). 2 recommended (default). 0 = disabled.", "Forzar adquisición tras N actualizaciones de paleta (0-100). Recomendado 2 (por defecto). 0 = desactivado."},

	// -- Engine Options --
	"engineOpts":       {"Engine Options", "Opciones del Motor"},
	"allowNormal":      {"Allow normal case object redefinition.", "Permitir redefinición normal case"},
	"preferNormal":     {"Prefer normal case object redefinition.", "Preferir redefinición normal case"},
	"fullPalette":      {"Write the full palette.", "Escribir la paleta completa."},
	"bothFormatsTip":   {"Generate both .sup and .pes/.mui output formats.", "Generar formatos .sup y .pes/.mui."},
	"bothFormats":      {"Generate both SUP and PES+MUI files.", "SUP + PES/MUI"},
	"overlapBuf":       {"Allow palette update buffering.", "Permitir buffering de paleta"},
	"alternateOids":    {"Use global object IDs 0/1 (interoperability)", "Usar IDs de objeto globales 0/1 (interoperabilidad)"},
	"ignoreResolution": {"Ignore resolution validation.", "Ignorar validación de resolución."},
	"tightOds":         {"Fit ODS box to text", "Ajustar caja al texto"},

	// -- Activity Log --
	"activityLog": {"ACTIVITY LOG", "REGISTRO DE ACTIVIDAD"},
	"copy":        {"Copy", "Copiar"},
	"clear":       {"Clear", "Limpiar"},
	"copied":      {"Copied", "Copiado"},
	"copyLog":     {"Copy log", "Copiar registro"},
	"logLines":    {"[%1 lines]", "[%1 líneas]"},

	// -- Encode --
	"progress":     {"Progress", "Progreso"},
	"standingBy":   {"Standing by", "En espera"},
	"working":      {"Working", "Procesando"},
	"finished":     {"Finished", "Finalizado"},
	"initEncode":   {"ENCODE", "PROCESAR"},
	"abort":        {"ABORT", "ABORTAR"},
	"starting":     {"Encoding…", "Codificando…"},
	"done":         {"Done", "Listo"},
	"failed":       {"Encoding FAILED – see log for details", "CODIFICACIÓN FALLIDA – ver el registro"},
	"abortedShort": {"Encoding Aborted", "Codificación Abortada"},
	"aborted":      {"Aborted", "Abortado"},
	"abortLogMsg":  {"Encoding aborted by user.", "Codificación abortada por el usuario."},

	// -- Theme --
	"themeSystem": {"System", "Sistema"},
	"themeLight":  {"Light", "Claro"},
	"themeDark":   {"Dark", "Oscuro"},

	// -- Checkbox tooltips --
	"tipAllowNormal": {
		"Registering the same graphic multiple times produces re-timing of the palette and small color shifts (up to a 0.6% of the inner 8 palette rows);\nwhen disabled, palette updates take effect twice as fast with subtle visual changes.",
		"Registrar el mismo gráfico varias veces produce un reajuste de los tiempos de la paleta y pequeños cambios de color (hasta un 0.6% de las 8 filas internas de la paleta);\ncuando se desactiva, las actualizaciones de paleta surten efecto el doble de rápido con cambios visuales sutiles.",
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
		"Use global object IDs 0/1 instead of per-window ids.\nThe legacy mode can alter the picture when several windows share a display set; the per-window default avoids buffer clobbering.",
		"Usar IDs de objeto globales 0/1 en lugar de ids por ventana.\nEl modo legacy puede alterar la visualización cuando varias ventanas comparten display set; el default por ventana evita el pisado de buffers.",
	},
	"tipTightOds": {
		"Crops each event's ODS box to its exact content instead of reusing the union box shared across the event chain.\nImproves image fit of late events in authoring tools.",
		"Recorta la caja ODS de cada evento a su contenido exacto en lugar de reutilizar la caja unión compartida por la cadena de eventos.\nMejora el encaje de la imagen en herramientas de edición.",
	},
	"tipIgnoreResolution": {
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
