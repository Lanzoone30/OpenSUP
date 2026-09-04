package config

import (
	"os"
	"path/filepath"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestDefaults(t *testing.T) {
	d := defaults()
	assert.Equal(t, English, d.Language)
	assert.Equal(t, ThemeSystem, d.Theme)
	assert.Empty(t, d.LastBDNDir)
	assert.Empty(t, d.LastOutDir)
	// Collapsible-card flags are legacy: no longer written or defaulted.
	assert.False(t, d.ParamsOpen)
	assert.False(t, d.EngineOpen)
	assert.False(t, d.AdvancedOpen)
}

func TestRoundTrip(t *testing.T) {
	dir := t.TempDir()
	// override configDir by setting XDG_CONFIG_HOME (Linux only)
	t.Setenv("XDG_CONFIG_HOME", dir)

	s := Settings{
		Language:   Spanish,
		Theme:      ThemeDark,
		LastBDNDir: "/home/test/bdn",
		LastOutDir: "/home/test/out",
	}
	require.NoError(t, Save(s))

	// File should be at <dir>/opensup/settings.json
	path := filepath.Join(dir, "opensup", "settings.json")
	_, err := os.Stat(path)
	require.NoError(t, err)

	loaded, err := Load()
	require.NoError(t, err)
	assert.Equal(t, s, loaded)
}

func TestLoadDefaultsOnMissingFile(t *testing.T) {
	dir := t.TempDir()
	t.Setenv("XDG_CONFIG_HOME", dir)
	s, err := Load()
	require.NoError(t, err)
	assert.Equal(t, defaults(), s)
}

func TestLoadCorruptFile(t *testing.T) {
	dir := t.TempDir()
	t.Setenv("XDG_CONFIG_HOME", dir)
	path := filepath.Join(dir, "opensup")
	require.NoError(t, os.MkdirAll(path, 0o755))
	require.NoError(t, os.WriteFile(
		filepath.Join(path, "settings.json"),
		[]byte("{not json}"), 0o644))
	s, err := Load()
	require.NoError(t, err) // silently falls back
	assert.Equal(t, defaults(), s)
}