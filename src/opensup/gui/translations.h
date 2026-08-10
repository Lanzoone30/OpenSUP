// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder

#pragma once

#include <QString>
#include <QHash>
#include "opensup/version.h"

enum class Lang { EN, ES };

/// Return translated string by key for the given language.
/// Keys are defined in the static map inside.
inline QString tr_str(const QString& key, Lang lang)
{
    static const QHash<QString, QPair<QString, QString>> s = {
        // -- Header --
{"subtitle",    {"PGS Subtitle Encoder",
                          "PGS Subtitle Encoder"}},
        {"windowTitle", {"OpenSUP v" OPENSUP_VERSION_STRING,
                         "OpenSUP v" OPENSUP_VERSION_STRING}},

        // -- Project Scope --
        {"projectScope",{"Project Scope", "Proyecto"}},
        {"selectBdn",   {"\U0001F4C2 Select BDN XML",
                         "\U0001F4C2 Seleccionar BDN XML"}},
        {"noFile",      {"No file selected",
                         "Ning\u00fan archivo seleccionado"}},
        {"setOutput",   {"\U0001F4BE Set SUP Output",
                         "\U0001F4BE Establecer destino SUP"}},
        {"destNotSet",  {"Destination not set",
                         "Destino no establecido"}},

        // -- Parameters --
        {"parameters",  {"Parameters", "Par\u00e1metros"}},
        {"colorSpace",  {"Color Space", "Espacio de color"}},
        {"colorSpaceTip",{"Color matrix to use for YCbCr conversion.",
                          "Matriz de color para conversi\u00f3n YCbCr."}},
        {"quantizer",   {"Quantizer", "Cuantizador"}},
        {"quantizerTip",{"Image quantizer backend (Quality, Speed).",
                          "Motor de cuantizaci\u00f3n (Calidad, Velocidad)."}},

        // -- Engine Options --
        {"engineOpts",  {"Engine Options", "Opciones del Motor"}},
        {"allowNormal", {"Allow Normal Case", "Permitir Normal Case"}},
        {"preferNormal",{"Prefer Normal Case", "Preferir Normal Case"}},
        {"fullPalette", {"Write Full Palette", "Paleta Completa"}},
        {"bothFormatsTip",{"Generate both .sup and .pes/.mui output formats.",
                            "Generar formatos .sup y .pes/.mui."}},
        {"bothFormats", {"Both SUP + PES/MUI", "SUP + PES/MUI"}},
        {"overlapBuf",  {"Overlap Buffering",
                          "Buffer de Superposici\u00f3n"}},
        {"ignoreRes",   {"Ignore Resolution Validation",
                          "Ignorar Validaci\u00f3n de Resoluci\u00f3n"}},

        // -- Activity Log --
        {"activityLog", {"ACTIVITY LOG", "REGISTRO DE ACTIVIDAD"}},
        {"copy",        {"Copy", "Copiar"}},
        {"clear",       {"Clear", "Limpiar"}},
        {"copied",      {"Copied", "Copiado"}},
        {"copyLog",     {"Copy log", "Copiar registro"}},
        {"logLines",    {"[%1 lines]", "[%1 l\u00edneas]"}},

        // -- Progress --
        {"progress",    {"Progress", "Progreso"}},
        {"standingBy",  {"Standing by\u2026", "En espera\u2026"}},
        {"initEncode",  {"ENCODE",
                          "PROCESAR"}},
        {"abort",       {"ABORT", "ABORTAR"}},

        // -- Status messages --
        {"starting",    {"Encoding.", "Codificando\u2026"}},
        {"done",        {"Done", "Listo"}},
        {"failed",      {"Encoding FAILED \u2013 see log for details",
                          "CODIFICACI\u00d3N FALLIDA \u2013 ver el registro"}},
        {"abortedShort",{"Encoding Aborted", "Codificaci\u00f3n Abortada"}},

        // -- Theme --
        {"themeSystem", {"System", "Sistema"}},
        {"themeLight",  {"Light", "Claro"}},
        {"themeDark",   {"Dark", "Oscuro"}},

        // -- Checkbox tooltips --
        {"tipAllowNormal",
            {"Update only one composition out of the two, whenever updating both "
             "is not possible due to time constraints.\n"
             "This exploits the PG object buffer capabilities as intended by the "
             "format designers.\n"
             "Stream shall NOT be Built or Rebuilt at the authoring stage.",
             "Actualiza solo una composici\u00f3n de las dos, cuando no es posible "
             "actualizar ambas por restricciones de tiempo.\n"
             "Esto aprovecha las capacidades del buffer de objetos PG seg\u00fan lo "
             "dise\u00f1ado por el formato.\n"
             "El stream NO debe ser Construido ni Reconstruido en la etapa de "
             "authoring."}},
        {"tipPreferNormal",
            {"Update only one composition out of the two, even when decoding time "
             "is sufficient to refresh both (default).\n"
             "It can reduce the bitrate, but the palette is not shared across "
             "composition objects whenever it occurs.",
             "Actualiza solo una composici\u00f3n de las dos, incluso cuando hay "
             "tiempo suficiente para refrescar ambas (predeterminado).\n"
             "Puede reducir el bitrate, pero la paleta no se comparte entre objetos "
             "de composici\u00f3n cuando esto ocurre."}},
        {"tipFullPalette",
            {"Don't optimize palette reduction when there are too many colors.\n"
             "Instead, just use the full palette.\n"
             "May improve quality in some rare cases at the cost of bigger output size.",
             "No optimizar la reducci\u00f3n de paleta cuando hay demasiados colores.\n"
             "En su lugar, usar la paleta completa.\n"
             "Puede mejorar la calidad en algunos casos raros a costa de un "
             "tama\u00f1o de salida mayor."}},
        {"tipBothFormats",
            {"Export also a .pes/.mui file alongside the .sup file.",
             "Exportar tambi\u00e9n un archivo .pes/.mui junto con el archivo .sup."}},
        {"tipOverlapBuf",
            {"Allow this encoder to generate overlapping objects in the output "
             "stream.\n"
             "This method is more efficient but not well supported by some "
             "hardware decoders.",
             "Permitir que este codificador genere objetos superpuestos en el "
             "stream de salida.\n"
             "Este m\u00e9todo es m\u00e1s eficiente pero no es bien soportado por "
             "algunos decodificadores de hardware."}},
        {"tipIgnoreRes",
            {"Ignore the warning when the input video resolution does not match "
             "the expected BDN resolution.",
             "Ignorar la advertencia cuando la resoluci\u00f3n del video de entrada "
             "no coincide con la resoluci\u00f3n BDN esperada."}},
    };

    auto it = s.find(key);
    if (it == s.end())
        return {};
    return lang == Lang::ES ? it->second : it->first;
}