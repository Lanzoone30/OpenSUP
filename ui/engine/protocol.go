// Package engine spawns the C++ core as a subprocess and consumes
// newline-delimited JSON events emitted on stdout.
package engine

import (
	"bufio"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"strings"
)

// LogLevel mirrors opensup::common::log_level_e from the C++ side.
// The string values must match exactly; the parser maps them back
// to these constants for typed comparisons.
type LogLevel string

const (
	LogLDebug LogLevel = "ldebug"
	LogHDebug LogLevel = "hdebug"
	LogInfo   LogLevel = "info"
	LogIInfo  LogLevel = "iinfo"
	LogEInfo  LogLevel = "einfo"
	LogWarn   LogLevel = "warn"
	LogPass   LogLevel = "pass"
	LogError  LogLevel = "error"
	LogFail   LogLevel = "fail"
	LogFatal  LogLevel = "fatal"
)

// Event is the union of every JSON line the engine can emit.
// Exactly one of Log / Progress / Done is populated per line.
type Event struct {
	// Log event: type=="log".
	Log *LogEvent
	// Progress event: type=="progress".
	Progress *ProgressEvent
	// Done event: type=="done" (always the last line emitted).
	Done *DoneEvent
}

// LogEvent is a single log line from the C++ logger.
type LogEvent struct {
	Level LogLevel `json:"level"`
	Msg   string   `json:"msg"`
}

// ProgressEvent reports encoder progress per epoch.
type ProgressEvent struct {
	Percent int `json:"percent"`
	Epoch   int `json:"epoch"`
	Total   int `json:"total"`
}

// DoneEvent is the terminal event; one is always emitted before exit.
type DoneEvent struct {
	Success    bool   `json:"success"`
	Error      string `json:"error,omitempty"`
	Events     int    `json:"events,omitempty"`
	Epochs     int    `json:"epochs,omitempty"`
	Segments   int    `json:"segments,omitempty"`
	DurationMs int64  `json:"duration_ms,omitempty"`
	// Cancelled is set only by the runner (synthetic done events);
	// the engine itself never emits it.
	Cancelled bool `json:"cancelled,omitempty"`
}

// rawEnvelope is the minimal shape we need to dispatch a line to
// the right concrete event type.
type rawEnvelope struct {
	Type string `json:"type"`
}

// Parse reads one NDJSON line and returns the typed event. Lines that
// are blank or contain only whitespace are skipped; malformed JSON or
// an unknown type is returned as an error so the caller can surface it.
func Parse(line string) (Event, error) {
	trimmed := strings.TrimSpace(line)
	if trimmed == "" {
		return Event{}, ErrBlankLine
	}

	var env rawEnvelope
	if err := json.Unmarshal([]byte(trimmed), &env); err != nil {
		return Event{}, fmt.Errorf("parse envelope: %w", err)
	}

	switch env.Type {
	case "log":
		var l LogEvent
		if err := json.Unmarshal([]byte(trimmed), &l); err != nil {
			return Event{}, fmt.Errorf("parse log: %w", err)
		}
		return Event{Log: &l}, nil
	case "progress":
		var p ProgressEvent
		if err := json.Unmarshal([]byte(trimmed), &p); err != nil {
			return Event{}, fmt.Errorf("parse progress: %w", err)
		}
		return Event{Progress: &p}, nil
	case "done":
		var d DoneEvent
		if err := json.Unmarshal([]byte(trimmed), &d); err != nil {
			return Event{}, fmt.Errorf("parse done: %w", err)
		}
		return Event{Done: &d}, nil
	default:
		return Event{}, fmt.Errorf("%w: %q", ErrUnknownType, env.Type)
	}
}

// ErrBlankLine is returned by Parse for empty input lines.
var ErrBlankLine = errors.New("blank line")

// ErrUnknownType is wrapped around unknown event type values.
var ErrUnknownType = errors.New("unknown event type")

// Reader streams NDJSON from r and calls handle for each parsed event.
// A nil handle is treated as a no-op. The caller owns r and is
// responsible for closing it; Reader stops on EOF or on the first
// non-recoverable error and returns it.
type Reader struct {
	scanner *bufio.Scanner
}

// NewReader wraps r with a scanner configured for the typical NDJSON
// line length emitted by the engine (well under the 64KiB default,
// but we raise the limit for safety on verbose log messages).
func NewReader(r io.Reader) *Reader {
	s := bufio.NewScanner(r)
	s.Buffer(make([]byte, 0, 64*1024), 1024*1024)
	return &Reader{scanner: s}
}

// Handle is invoked once per parsed event. Return a non-nil error to
// stop iteration early; that error is returned by Next.
type Handle func(Event) error

// Next reads the next event and passes it to handle. It returns
// io.EOF when the underlying reader is exhausted.
func (r *Reader) Next(handle Handle) error {
	for r.scanner.Scan() {
		ev, err := Parse(r.scanner.Text())
		if errors.Is(err, ErrBlankLine) {
			continue
		}
		if err != nil {
			return err
		}
		if handle != nil {
			if hErr := handle(ev); hErr != nil {
				return hErr
			}
		}
	}
	return r.scanner.Err()
}