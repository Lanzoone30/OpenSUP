package engine

import (
	"context"
	"errors"
	"sync"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// fakeEmitter records calls so tests can assert what the runner emits.
type fakeEmitter struct {
	mu     sync.Mutex
	events []emittedEvent
}

type emittedEvent struct {
	name    string
	payload interface{}
}

func (f *fakeEmitter) EventsEmit(_ context.Context, name string, data ...interface{}) {
	f.mu.Lock()
	defer f.mu.Unlock()
	var payload interface{}
	if len(data) > 0 {
		payload = data[0]
	}
	f.events = append(f.events, emittedEvent{name: name, payload: payload})
}

func (f *fakeEmitter) snapshot() []emittedEvent {
	f.mu.Lock()
	defer f.mu.Unlock()
	out := make([]emittedEvent, len(f.events))
	copy(out, f.events)
	return out
}

func TestBuildArgs_DefaultsAlwaysJSON(t *testing.T) {
	args := buildArgs(EncodeConfig{InputPath: "in.xml", OutputPath: "out.sup"})
	require.NotEmpty(t, args)
	assert.Equal(t, "--json", args[0])
	assert.Contains(t, args, "-i")
	assert.Contains(t, args, "in.xml")
	assert.Contains(t, args, "out.sup")
	// Zero-value flags must be omitted so we don't fight CLI11 defaults.
	assert.NotContains(t, args, "-y")
	assert.NotContains(t, args, "-q")
	assert.NotContains(t, args, "-b")
}

func TestBuildArgs_AllBooleansAndOverrides(t *testing.T) {
	cfg := EncodeConfig{
		InputPath:       "in.xml",
		OutputPath:      "out.sup",
		Quantizer:       1,
		BTMatrix:        "bt2020",
		Overwrite:       true,
		IgnoreRes:       true,
		BothFormats:     true,
		FullPalette:     true,
		AllowNormalCase: true,
		PreferNormalCase: true,
		Overlap:         true,
		RedrawPeriod:    0.5,
		MaxKbps:         48000,
		Threads:         4,
		Compression:     75,
		Acqrate:         90,
		SsimTol:         10,
		ExtraAcq:        3,
	}
	args := buildArgs(cfg)
	want := map[string]bool{
		"--json": true, "-i": true, "in.xml": true, "out.sup": true,
		"-q": true, "1": true, "-b": true, "bt2020": true,
		"-y": true,
		"--ignore-resolution": true, "-w": true, "-p": true,
		"--allow-normal": true, "--prefer-normal": true,
		"--overlap": true,
		"--redraw-period": true, "0.5": true,
		"-m": true, "48000": true,
		"-j": true, "4": true,
		"-c": true, "75": true,
		"-a": true, "90": true,
		"-t": true, "10": true,
		"-e": true, "3": true,
	}
	for _, a := range args {
		delete(want, a)
	}
	assert.Empty(t, want, "missing args: %v", want)
}

func TestPickBinary_FindsEmbedded(t *testing.T) {
	// Now that we ship a real engine binary in bin/, pickBinary should
	// find it for the current platform and return a non-empty path.
	name, _, err := pickBinary()
	require.NoError(t, err)
	assert.NotEmpty(t, name)
}

func TestRunner_AlreadyRunning(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	r := NewRunner(&fakeEmitter{})
	// first run
	runErr := make(chan error, 1)
	go func() {
		_, err := r.Run(ctx, EncodeConfig{InputPath: "x.xml", OutputPath: "y.sup"})
		runErr <- err
	}()
	time.Sleep(10 * time.Millisecond)
	// second run immediately should fail
	_, err := r.Run(ctx, EncodeConfig{InputPath: "a.xml", OutputPath: "b.sup"})
	assert.ErrorIs(t, err, ErrAlreadyRunning)
	cancel()
	_ = <-runErr
}

func TestRunner_CancelStopsEncode(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	em := &fakeEmitter{}
	r := NewRunner(em)
	cancel()
	_, err := r.Run(ctx, EncodeConfig{InputPath: "x.xml", OutputPath: "y.sup"})
	assert.ErrorIs(t, err, context.Canceled)
}

func TestRunner_EncodeError(t *testing.T) {
	ctx := context.Background()
	em := &fakeEmitter{}
	r := NewRunner(em)
	_, err := r.Run(ctx, EncodeConfig{InputPath: "nonexistent.xml", OutputPath: "y.sup"})
	assert.Error(t, err)
}

func TestRunner_Timeout(t *testing.T) {
	ctx, cancel := context.WithTimeout(context.Background(), time.Millisecond)
	defer cancel()
	em := &fakeEmitter{}
	r := NewRunner(em)
	// This should timeout or cancel
	_, err := r.Run(ctx, EncodeConfig{InputPath: "x.xml", OutputPath: "y.sup"})
	assert.True(t, errors.Is(err, context.DeadlineExceeded) || errors.Is(err, context.Canceled))
}

func TestRunner_ContextCancelledBeforeStart(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	em := &fakeEmitter{}
	r := NewRunner(em)
	_, err := r.Run(ctx, EncodeConfig{InputPath: "x.xml", OutputPath: "y.sup"})
	assert.ErrorIs(t, err, context.Canceled)
}