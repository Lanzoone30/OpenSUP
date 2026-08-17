package engine

import (
	"bufio"
	"context"
	"embed"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"sync"
)

//go:embed bin/*
var embedded embed.FS

// Events are the names emitted to the Wails frontend via
// runtime.EventsEmit. Keep in sync with frontend listeners.
const (
	EventLog      = "engine:log"
	EventProgress = "engine:progress"
	EventDone     = "engine:done"
)

// Emitter is the minimal surface the runner needs from Wails.
// In production this is satisfied by *runtime.EventsEmit; tests
// can pass a fake to capture emissions without a GUI.
type Emitter interface {
	EventsEmit(ctx context.Context, eventName string, optionalData ...interface{})
}

// EncodeConfig mirrors the C++ CLI flags the runner passes through.
// Empty strings are omitted; booleans only emit when true (CLI11
// semantics for add_flag).
type EncodeConfig struct {
	InputPath       string
	OutputPath      string
	Quantizer       int
	BTMatrix        string
	Overwrite       bool
	IgnoreRes       bool
	BothFormats     bool
	FullPalette     bool
	AllowNormalCase bool
	PreferNormalCase bool
	Overlap         bool
	RedrawPeriod    float64
}

// Result is the terminal outcome of a Run call.
type Result struct {
	Success    bool
	Error      string
	Events     int
	Epochs     int
	Segments   int
	DurationMs int64
	Cancelled  bool
}

// Runner owns the lifecycle of a single subprocess encode.
type Runner struct {
	emitter Emitter
	ctx     context.Context

	mu      sync.Mutex
	cmd     *exec.Cmd
	cancel  context.CancelFunc
	running bool
}

// NewRunner constructs a runner bound to emitter. Pass nil emitter
// to discard events (useful in tests).
func NewRunner(emitter Emitter) *Runner {
	return &Runner{emitter: emitter}
}

// Run launches the engine with cfg and blocks until the subprocess
// exits or ctx is cancelled. The Result reflects the engine's
// reported outcome, or a synthetic error if the subprocess failed
// to start / was aborted / exited without a done event.
//
// A second concurrent Run on the same Runner returns ErrAlreadyRunning.
func (r *Runner) Run(ctx context.Context, cfg EncodeConfig) (Result, error) {
	r.mu.Lock()
	if r.running {
		r.mu.Unlock()
		return Result{}, ErrAlreadyRunning
	}
	runCtx, cancel := context.WithCancel(ctx)
	r.cmd = nil
	r.cancel = cancel
	r.running = true
	r.mu.Unlock()

	defer func() {
		r.mu.Lock()
		r.running = false
		r.cancel = nil
		r.cmd = nil
		r.mu.Unlock()
		cancel()
	}()

	binPath, cleanup, err := extractBinary()
	if err != nil {
		return Result{}, fmt.Errorf("extract engine: %w", err)
	}
	defer cleanup()

	args := buildArgs(cfg)
	cmd := exec.CommandContext(runCtx, binPath, args...)
	hideWindow(cmd)
	stdout, err := cmd.StdoutPipe()
	if err != nil {
		return Result{}, fmt.Errorf("stdout pipe: %w", err)
	}
	stderr, err := cmd.StderrPipe()
	if err != nil {
		return Result{}, fmt.Errorf("stderr pipe: %w", err)
	}

	if err := cmd.Start(); err != nil {
		return Result{}, fmt.Errorf("start engine: %w", err)
	}

	r.mu.Lock()
	r.cmd = cmd
	r.mu.Unlock()

	// Drain stderr in the background; the C++ side writes nothing on
	// it in --json mode, but capturing keeps the pipe from blocking
	// and surfaces any spurious writes for debugging.
	go drainStderr(stderr)

	reader := NewReader(stdout)
	var done *DoneEvent

	handle := func(ev Event) error {
		select {
		case <-runCtx.Done():
			return runCtx.Err()
		default:
		}
		switch {
		case ev.Log != nil:
			r.emit(EventLog, ev.Log)
		case ev.Progress != nil:
			r.emit(EventProgress, ev.Progress)
		case ev.Done != nil:
			done = ev.Done
			r.emit(EventDone, ev.Done)
		}
		return nil
	}

	streamErr := reader.Next(handle)
	waitErr := cmd.Wait()

	if errors.Is(runCtx.Err(), context.Canceled) {
		// Emit a synthetic done event so the frontend always receives a
		// terminal signal, even when the engine was killed before it could
		// emit its own. Guards against the UI staying stuck mid-encode.
		if done == nil {
			r.emit(EventDone, &DoneEvent{Cancelled: true})
		}
		return Result{Cancelled: true, Error: "cancelled by user"}, nil
	}

	if done != nil {
		return Result{
			Success:    done.Success,
			Error:      done.Error,
			Events:     done.Events,
			Epochs:     done.Epochs,
			Segments:   done.Segments,
			DurationMs: done.DurationMs,
		}, nil
	}

	if streamErr != nil {
		return Result{}, fmt.Errorf("stream: %w", streamErr)
	}
	if waitErr != nil {
		return Result{}, fmt.Errorf("engine exit: %w", waitErr)
	}
	return Result{Error: "engine exited without done event"}, nil
}

// Abort kills the subprocess if running. Safe to call from any
// goroutine; idempotent. Aborting leaves the partial output file
// behind; the caller is responsible for removing it.
func (r *Runner) Abort() {
	r.mu.Lock()
	cancel := r.cancel
	cmd := r.cmd
	r.mu.Unlock()
	if cancel != nil {
		cancel()
	}
	// Kill the process directly: exec.CommandContext only kills
	// asynchronously, so without this the engine keeps emitting
	// buffered events (and keeps running) for a while after abort.
	if cmd != nil && cmd.Process != nil {
		cmd.Process.Kill()
	}
}

func (r *Runner) emit(name string, payload interface{}) {
	if r.emitter == nil {
		return
	}
	r.emitter.EventsEmit(r.ctx, name, payload)
}

// ErrAlreadyRunning is returned by Run when another encode is in flight.
var ErrAlreadyRunning = errors.New("engine: already running")

// extractBinary materializes the engine binary from the embedded
// filesystem to a temp file, returning its path and a cleanup func.
// The extracted file is made executable on Unix platforms.
func extractBinary() (string, func(), error) {
	name, srcPath, err := pickBinary()
	if err != nil {
		return "", nil, err
	}
	data, err := embedded.ReadFile(srcPath)
	if err != nil {
		return "", nil, fmt.Errorf("read embedded %s: %w", srcPath, err)
	}

	tmp, err := os.CreateTemp("", "opensup_engine_*"+filepath.Ext(name))
	if err != nil {
		return "", nil, fmt.Errorf("tempfile: %w", err)
	}
	path := tmp.Name()
	cleanup := func() { _ = os.Remove(path) }

	if _, err := tmp.Write(data); err != nil {
		tmp.Close()
		cleanup()
		return "", nil, fmt.Errorf("write %s: %w", path, err)
	}
	if err := tmp.Close(); err != nil {
		cleanup()
		return "", nil, fmt.Errorf("close %s: %w", path, err)
	}
	if runtime.GOOS != "windows" {
		if err := os.Chmod(path, 0o755); err != nil {
			cleanup()
			return "", nil, fmt.Errorf("chmod %s: %w", path, err)
		}
	}
	return path, cleanup, nil
}

// pickBinary maps the current GOOS/GOARCH to the right file in
// bin/. Missing combinations return a descriptive error.
func pickBinary() (realName, embedPath string, err error) {
	ext := ""
	if runtime.GOOS == "windows" {
		ext = ".exe"
	}
	realName = "opensup_engine" + ext
	embedPath = filepath.ToSlash(filepath.Join("bin", runtime.GOOS+"_"+runtime.GOARCH, realName))

	if _, err := embedded.Open(embedPath); err == nil {
		return realName, embedPath, nil
	}
	// Fallback for layouts without per-OS subdirs.
	embedPath = filepath.ToSlash(filepath.Join("bin", realName))
	if _, err := embedded.Open(embedPath); err == nil {
		return realName, embedPath, nil
	}
	return "", "", fmt.Errorf("no embedded engine for %s/%s", runtime.GOOS, runtime.GOARCH)
}

// buildArgs assembles the CLI11 argument list from cfg. Always
// emits --json first so the engine never falls back to plain mode.
func buildArgs(cfg EncodeConfig) []string {
	args := []string{"--json"}
	if cfg.InputPath != "" {
		args = append(args, "-i", cfg.InputPath)
	}
	if cfg.OutputPath != "" {
		args = append(args, cfg.OutputPath)
	}
	if cfg.Quantizer != 0 {
		args = append(args, "-q", intToStr(cfg.Quantizer))
	}
	if cfg.BTMatrix != "" {
		args = append(args, "-b", cfg.BTMatrix)
	}
	if cfg.Overwrite {
		args = append(args, "-y")
	}
	if cfg.IgnoreRes {
		args = append(args, "--ignore-resolution")
	}
	if cfg.BothFormats {
		args = append(args, "-w")
	}
	if cfg.FullPalette {
		args = append(args, "-p")
	}
	if cfg.AllowNormalCase {
		args = append(args, "--allow-normal")
	}
	if cfg.PreferNormalCase {
		args = append(args, "--prefer-normal")
	}
	if cfg.Overlap {
		args = append(args, "--overlap")
	}
	if cfg.RedrawPeriod != 0 {
		args = append(args, "--redraw-period", floatToStr(cfg.RedrawPeriod))
	}
	return args
}

func drainStderr(r io.Reader) {
	scanner := bufio.NewScanner(r)
	scanner.Buffer(make([]byte, 0, 64*1024), 1024*1024)
	for scanner.Scan() {
		// Surface stderr on the host; the engine should be silent here
		// in --json mode. This is a safety net for misconfigurations.
		fmt.Fprintln(os.Stderr, "engine stderr:", scanner.Text())
	}
}

func floatToStr(f float64) string  { return strconvFloat(f) }
func intToStr(i int) string        { return strconvInt(i) }

// thin wrappers around strconv to keep the import block tight above.
func strconvFloat(f float64) string  { return fmt.Sprintf("%g", f) }
func strconvInt(i int) string        { return fmt.Sprintf("%d", i) }