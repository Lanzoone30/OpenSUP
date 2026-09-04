// Package config persists user settings (last-used directories,
// language, theme) as a JSON file in the user's config directory.
//
// On Windows the file lives in %AppData%/OpenSUP/settings.json.
// On Linux / macOS it follows the XDG Base Directory specification.
package config

import (
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
)

// Language mirrors the Qt cmb_language combo: 0 = EN, 1 = ES.
type Language int

const (
	English Language = iota
	Spanish
)

// Theme mirrors the Qt cmb_theme combo: 0 = System, 1 = Light, 2 = Dark.
type Theme int

const (
	ThemeSystem Theme = iota
	ThemeLight
	ThemeDark
)

// Settings holds every persistent key. The JSON keys match the
// original QSettings names so a future migration is lossless.
type Settings struct {
	Language    Language `json:"language"`
	Theme       Theme    `json:"theme"`
	LastBDNDir  string   `json:"last_bdn_dir"`
	LastOutDir  string   `json:"last_output_dir"`
	// Legacy collapsible-card state. No longer written by the UI (cards
	// always start at their markup default: Parameters/Engine expanded,
	// Advanced collapsed). Kept only so older settings.json files decode.
	ParamsOpen   bool `json:"params_open"`
	EngineOpen   bool `json:"engine_open"`
	AdvancedOpen bool `json:"advanced_open"`
}

func defaults() Settings {
	return Settings{
		Language:   English,
		Theme:      ThemeSystem,
		LastBDNDir: "",
		LastOutDir: "",
	}
}

// configDir returns the directory where settings.json should live.
func configDir() (string, error) {
	switch runtime.GOOS {
	case "windows":
		appdata := os.Getenv("APPDATA")
		if appdata == "" {
			return "", errors.New("APPDATA not set")
		}
		return filepath.Join(appdata, "OpenSUP"), nil
	case "darwin":
		home, err := os.UserHomeDir()
		if err != nil {
			return "", err
		}
		return filepath.Join(home, "Library", "Application Support", "OpenSUP"), nil
	default: // linux, *bsd, etc.
		// XDG_CONFIG_HOME, fallback to ~/.config
		xdg := os.Getenv("XDG_CONFIG_HOME")
		if xdg != "" {
			return filepath.Join(xdg, "opensup"), nil
		}
		home, err := os.UserHomeDir()
		if err != nil {
			return "", err
		}
		return filepath.Join(home, ".config", "opensup"), nil
	}
}

// path returns the full path to settings.json.
func path() (string, error) {
	dir, err := configDir()
	if err != nil {
		return "", fmt.Errorf("config dir: %w", err)
	}
	return filepath.Join(dir, "settings.json"), nil
}

// Load reads settings.json, returning defaults on first run.
func Load() (Settings, error) {
	p, err := path()
	if err != nil {
		return defaults(), err
	}
	data, err := os.ReadFile(p)
	if err != nil {
		if errors.Is(err, os.ErrNotExist) {
			return defaults(), nil
		}
		return defaults(), fmt.Errorf("read settings: %w", err)
	}
	s := defaults()
	if err := json.Unmarshal(data, &s); err != nil {
		// Corrupt file: fall back to defaults silently, like QSettings.
		// A subsequent Save overwrites the bad file with valid JSON.
		return defaults(), nil
	}
	return s, nil
}

// Save writes settings.json, creating the config directory first.
func Save(s Settings) error {
	p, err := path()
	if err != nil {
		return err
	}
	dir := filepath.Dir(p)
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return fmt.Errorf("create config dir: %w", err)
	}
	data, err := json.MarshalIndent(s, "", "  ")
	if err != nil {
		return fmt.Errorf("marshal settings: %w", err)
	}
	if err := os.WriteFile(p, data, 0o644); err != nil {
		return fmt.Errorf("write settings: %w", err)
	}
	return nil
}