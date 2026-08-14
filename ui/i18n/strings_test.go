package i18n

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestTableComplete(t *testing.T) {
	// Cross-check the key count matches translations.h (43 keys).
	assert.Len(t, Table, 43)
}

func TestGetUnknownKeyReturnsItself(t *testing.T) {
	assert.Equal(t, "missing", Get(EN, "missing"))
	assert.Equal(t, "missing", Get(ES, "missing"))
}

func TestGetBilingual(t *testing.T) {
	assert.Equal(t, "Parameters", Get(EN, "parameters"))
	assert.Equal(t, "Parámetros", Get(ES, "parameters"))
	assert.Equal(t, "ABORT", Get(EN, "abort"))
	assert.Equal(t, "ABORTAR", Get(ES, "abort"))
}

func TestWindowTitleHasVersion(t *testing.T) {
	enTitle := Get(EN, "windowTitle")
	esTitle := Get(ES, "windowTitle")
	assert.Contains(t, enTitle, Version)
	assert.Contains(t, esTitle, Version)
	assert.Equal(t, enTitle, esTitle) // the title is identical in both
}

func TestNoEmojisInStrings(t *testing.T) {
	// No emoji characters should appear in any translated string.
	for key, pair := range Table {
		for _, s := range pair {
			for _, r := range s {
				if r > 0x2FFF {
					t.Errorf("key %q contains non-text char U+%04X in %q", key, r, s)
					break
				}
			}
		}
	}
}

func TestFormat(t *testing.T) {
	got := Format(Get(EN, "logLines"), "42")
	assert.Equal(t, "[42 lines]", got)

	gotES := Format(Get(ES, "logLines"), "42")
	assert.Equal(t, "[42 líneas]", gotES)
}

func TestMultilineTooltipsLiterals(t *testing.T) {
	en := Get(EN, "tipAllowNormal")
	require.NotEmpty(t, en)
	assert.Contains(t, en, "shall NOT be Built")

	es := Get(ES, "tipOverlapBuf")
	require.NotEmpty(t, es)
	assert.Contains(t, es, "decodificadores de hardware")
}