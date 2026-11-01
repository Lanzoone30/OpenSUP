package main

import (
	"os"
	"path/filepath"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestRemovePartialOutput(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "out.sup")

	require.NoError(t, os.WriteFile(path, []byte("old"), 0o644))
	before, err := os.Stat(path)
	require.NoError(t, err)

	removePartialOutput(path, before)
	assert.FileExists(t, path, "untouched pre-existing output must be kept")

	require.NoError(t, os.WriteFile(path, []byte("partial-bytes"), 0o644))
	removePartialOutput(path, before)
	assert.NoFileExists(t, path, "rewritten output must be removed")

	removePartialOutput(path, nil)
	removePartialOutput("", nil)
}
