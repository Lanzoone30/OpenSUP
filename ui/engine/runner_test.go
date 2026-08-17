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
	}
	args := buildArgs(cfg)
	want := map[string]bool{
		"--json": true, "-i": true, "in.xml": true, "out.sup": true,
		"-q": true, "1": true, "-b": true, "bt2020": true,
		"-y": true,
		"--ignore-resolution": true, "-w": true, "-p": true,
		"--allow-normal": true, "--prefer-normal": true, "--overlap": true,
		"--redraw-period": true, "-m": true, "48000": true,
		"-j": true, "4": true,
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
	r := NewRunner(&fakeEmitter{})
	// We can't easily run a real encode without an embedded binary,
	// so verify the concurrency guard by reaching into internals via
	// a second Run attempt that should fail fast.
	r.running = true // simulate in-flight encode
	defer func() { r.running = false }()

	_, err := r.Run(context.Background(), EncodeConfig{InputPath: "x", OutputPath: "y"})
	require.ErrorIs(t, err, ErrAlreadyRunning)
}

func TestRunner_AbortIsIdempotent(t *testing.T) {
	r := NewRunner(&fakeEmitter{})
	// Should not panic even when nothing is running.
	r.Abort()
	r.Abort()
}

func TestEmitter_FakeCapturesEvents(t *testing.T) {
	f := &fakeEmitter{}
	ctx, cancel := context.WithTimeout(context.Background(), time.Second)
	defer cancel()

	f.EventsEmit(ctx, EventLog, &LogEvent{Level: LogInfo, Msg: "hi"})
	f.EventsEmit(ctx, EventProgress, &ProgressEvent{Percent: 50})
	f.EventsEmit(ctx, EventDone, &DoneEvent{Success: true})

	got := f.snapshot()
	require.Len(t, got, 3)
	assert.Equal(t, EventLog, got[0].name)
	assert.Equal(t, EventProgress, got[1].name)
	assert.Equal(t, EventDone, got[2].name)
}

func TestParse_RegressionEscapedMsg(t *testing.T) {
	// Ensure the parser correctly decodes JSON-escaped messages
	// (newlines, quotes, backslashes) that the C++ emitter produces.
	line := `{"type":"log","level":"warn","msg":"line1\nline2 \"q\" \\b"}`
	ev, err := Parse(line)
	require.NoError(t, err)
	require.NotNil(t, ev.Log)
	assert.Equal(t, "line1\nline2 \"q\" \\b", ev.Log.Msg)
}

// Silence the unused-import linter for context.Canceled-style errors
// we keep referenced for future abort-path tests.
var _ = errors.New