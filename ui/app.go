package main

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	gruntime "runtime"
	"sync"

	"github.com/Lanzoone30/OpenSUP-go/ui/config"
	"github.com/Lanzoone30/OpenSUP-go/ui/engine"
	"github.com/Lanzoone30/OpenSUP-go/ui/i18n"
	"github.com/wailsapp/wails/v2/pkg/runtime"
)

// App is the struct exposed to the Wails frontend. Every exported
// method becomes callable from JavaScript via window.go.main.App.*.
type App struct {
	ctx     context.Context
	runner  *engine.Runner
	settings config.Settings

	mu      sync.Mutex
	lastBDNDir string
	lastOutDir string
}

func NewApp() *App {
	return &App{}
}

func (a *App) startup(ctx context.Context) {
	a.ctx = ctx

	// Load persisted settings before the frontend renders.
	s, err := config.Load()
	if err != nil {
		// Non-fatal: defaults are returned even on error.
		s = config.Settings{}
	}
	a.settings = s
	a.lastBDNDir = s.LastBDNDir
	a.lastOutDir = s.LastOutDir

	// Create the engine runner with a Wails-backed emitter that
	// forwards events to the frontend.
	a.runner = engine.NewRunner(&wailsEmitter{ctx: ctx})
}

// SelectBDN opens a file dialog and returns the chosen path.
// Returns "" if the user cancels.
func (a *App) SelectBDN() (string, error) {
	path, err := runtime.OpenFileDialog(a.ctx, runtime.OpenDialogOptions{
		Title:            "Select BDN XML",
		Filters: []runtime.FileFilter{
			{DisplayName: "BDN XML (*.xml)", Pattern: "*.xml"},
			{DisplayName: "All files (*.*)", Pattern: "*.*"},
		},
		DefaultDirectory: a.lastBDNDir,
	})
	if err != nil {
		return "", fmt.Errorf("open file dialog: %w", err)
	}
	if path == "" {
		return "", nil
	}
	a.mu.Lock()
	a.lastBDNDir = filepath.Dir(path)
	a.mu.Unlock()
	return path, nil
}

// SetOutput opens a save dialog and returns the chosen path.
// Returns "" if the user cancels.
func (a *App) SetOutput() (string, error) {
	path, err := runtime.SaveFileDialog(a.ctx, runtime.SaveDialogOptions{
		Title:            "Set SUP Output",
		DefaultDirectory: a.lastOutDir,
		Filters: []runtime.FileFilter{
			{DisplayName: "PGS Subtitle (*.sup)", Pattern: "*.sup"},
			{DisplayName: "All files (*.*)", Pattern: "*.*"},
		},
	})
	if err != nil {
		return "", fmt.Errorf("save file dialog: %w", err)
	}
	if path == "" {
		return "", nil
	}
	a.mu.Lock()
	a.lastOutDir = filepath.Dir(path)
	a.mu.Unlock()
	return path, nil
}

// RevealOutput opens the OS file manager (Explorer on Windows) showing the
// folder of the given output path. No-op when the path is empty.
func (a *App) RevealOutput(path string) error {
	if path == "" {
		return nil
	}
	abs, err := filepath.Abs(path)
	if err != nil {
		return fmt.Errorf("reveal output: resolve path: %w", err)
	}
	dir := filepath.Dir(abs)
	// If the arg already points to an existing folder, reveal that folder.
	if info, err := os.Stat(abs); err == nil && info.IsDir() {
		dir = abs
	}
	fmt.Fprintf(os.Stderr, "[RevealOutput] path=%q dir=%q\n", path, dir)

	var name string
	var args []string
	switch gruntime.GOOS {
	case "windows":
		name = "explorer"
		args = []string{dir}
	case "darwin":
		name = "open"
		args = []string{dir}
	default:
		name = "xdg-open"
		args = []string{dir}
	}
	// Detach: the opener outlives the app; ignore wait status.
	return exec.Command(name, args...).Start()
}

// StartEncode launches the engine subprocess with the given config.
// The frontend listens for engine:log, engine:progress, engine:done
// events emitted by the runner.
func (a *App) StartEncode(cfg engine.EncodeConfig) (bool, error) {
	result, err := a.runner.Run(a.ctx, cfg)
	if err != nil {
		return false, fmt.Errorf("start encode: %w", err)
	}
	if result.Cancelled {
		return false, nil
	}
	return result.Success, nil
}

// AbortEncode kills the in-flight engine subprocess.
func (a *App) AbortEncode() {
	a.runner.Abort()
}

// LoadSettings returns the persisted settings as a JSON-serializable
// struct for the frontend.
func (a *App) LoadSettings() (config.Settings, error) {
	return a.settings, nil
}

// SaveSettings persists the given settings to disk.
func (a *App) SaveSettings(s config.Settings) (bool, error) {
	a.mu.Lock()
	a.lastBDNDir = s.LastBDNDir
	a.lastOutDir = s.LastOutDir
	a.mu.Unlock()
	if err := config.Save(s); err != nil {
		return false, fmt.Errorf("save settings: %w", err)
	}
	a.settings = s
	return true, nil
}

// GetTranslations returns the full i18n table keyed by language code
// ("en" or "es") so the frontend can lazy-load strings.
func (a *App) GetTranslations(lang int) (map[string]string, error) {
	l := i18n.EN
	if lang == 1 {
		l = i18n.ES
	}
	out := make(map[string]string, len(i18n.Table))
	for k, v := range i18n.Table {
		if l == i18n.ES {
			out[k] = v[1]
		} else {
			out[k] = v[0]
		}
	}
	return out, nil
}

// Version returns the current UI version string.
func (a *App) Version() string {
	return i18n.Version
}

// wailsEmitter satisfies engine.Emitter by forwarding events to the
// Wails runtime, which the frontend receives via EventsOn.
type wailsEmitter struct {
	ctx context.Context
}

func (w *wailsEmitter) EventsEmit(ctx context.Context, name string, data ...interface{}) {
	// Use the Wails runtime to emit; ctx from the struct is more
	// reliable than the one passed by the engine (which may be a
	// subprocess context).
	runtime.EventsEmit(w.ctx, name, data...)
}

// Suppress unused-import linter when json is not needed yet.
var _ = json.Marshal