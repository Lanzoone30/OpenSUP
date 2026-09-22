package engine

import (
	"errors"
	"strings"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestParse_LogEvent(t *testing.T) {
	line := `{"type":"log","level":"info","msg":"hello"}`
	ev, err := Parse(line)
	require.NoError(t, err)
	require.NotNil(t, ev.Log)
	assert.Equal(t, LogInfo, ev.Log.Level)
	assert.Equal(t, "hello", ev.Log.Msg)
	assert.Nil(t, ev.Progress)
	assert.Nil(t, ev.Done)
}

func TestParse_ProgressEvent(t *testing.T) {
	line := `{"type":"progress","percent":42,"epoch":3,"total":12}`
	ev, err := Parse(line)
	require.NoError(t, err)
	require.NotNil(t, ev.Progress)
	assert.Equal(t, 42, ev.Progress.Percent)
	assert.Equal(t, 3, ev.Progress.Epoch)
	assert.Equal(t, 12, ev.Progress.Total)
}

func TestParse_DoneSuccess(t *testing.T) {
	line := `{"type":"done","success":true,"events":336,"epochs":12,"segments":336,"duration_ms":1234}`
	ev, err := Parse(line)
	require.NoError(t, err)
	require.NotNil(t, ev.Done)
	assert.True(t, ev.Done.Success)
	assert.Equal(t, 336, ev.Done.Events)
	assert.Equal(t, 12, ev.Done.Epochs)
	assert.Equal(t, 336, ev.Done.Segments)
	assert.Equal(t, int64(1234), ev.Done.DurationMs)
}

func TestParse_DoneFailure(t *testing.T) {
	line := `{"type":"done","success":false,"error":"output exists"}`
	ev, err := Parse(line)
	require.NoError(t, err)
	require.NotNil(t, ev.Done)
	assert.False(t, ev.Done.Success)
	assert.Equal(t, "output exists", ev.Done.Error)
}

func TestParse_BlankAndWhitespace(t *testing.T) {
	cases := []string{"", "   ", "\t\n", "\n"}
	for _, c := range cases {
		_, err := Parse(c)
		assert.ErrorIs(t, err, ErrBlankLine, "input: %q", c)
	}
}

func TestParse_UnknownType(t *testing.T) {
	line := `{"type":"weird","foo":1}`
	_, err := Parse(line)
	require.Error(t, err)
	assert.ErrorIs(t, err, ErrUnknownType)
}

func TestParse_MalformedJSON(t *testing.T) {
	line := `{"type":"log","level":`
	_, err := Parse(line)
	require.Error(t, err)
	assert.False(t, errors.Is(err, ErrBlankLine))
	assert.False(t, errors.Is(err, ErrUnknownType))
}

func TestParse_LogLevelNames(t *testing.T) {
	cases := []LogLevel{
		LogLDebug, LogHDebug, LogInfo, LogIInfo, LogEInfo,
		LogWarn, LogPass, LogError, LogFail, LogFatal,
	}
	for _, lvl := range cases {
		line := `{"type":"log","level":"` + string(lvl) + `","msg":"x"}`
		ev, err := Parse(line)
		require.NoError(t, err, "level %q", lvl)
		require.NotNil(t, ev.Log)
		assert.Equal(t, lvl, ev.Log.Level)
	}
}

func TestReader_StreamsAllEvents(t *testing.T) {
	input := strings.Join([]string{
		`{"type":"log","level":"info","msg":"start"}`,
		``,
		`{"type":"progress","percent":50,"epoch":1,"total":2}`,
		`{"type":"log","level":"warn","msg":"halfway"}`,
		`{"type":"progress","percent":100,"epoch":2,"total":2}`,
		`{"type":"done","success":true,"events":2,"segments":2}`,
	}, "\n")

	r := NewReader(strings.NewReader(input))
	var got []Event
	err := r.Next(func(e Event) error {
		got = append(got, e)
		return nil
	})
	require.NoError(t, err)
	require.Len(t, got, 5)

	assert.NotNil(t, got[0].Log)
	assert.Equal(t, "start", got[0].Log.Msg)

	assert.NotNil(t, got[1].Progress)
	assert.Equal(t, 50, got[1].Progress.Percent)

	assert.NotNil(t, got[2].Log)
	assert.Equal(t, LogWarn, got[2].Log.Level)

	assert.NotNil(t, got[3].Progress)
	assert.Equal(t, 100, got[3].Progress.Percent)

	require.NotNil(t, got[4].Done)
	assert.True(t, got[4].Done.Success)
	assert.Equal(t, 2, got[4].Done.Events)
}

func TestReader_StopsOnHandlerError(t *testing.T) {
	input := strings.Join([]string{
		`{"type":"log","level":"info","msg":"ok"}`,
		`{"type":"log","level":"info","msg":"stop here"}`,
		`{"type":"done","success":true}`,
	}, "\n")

	stop := errors.New("stop")
	r := NewReader(strings.NewReader(input))
	var seen int
	err := r.Next(func(Event) error {
		seen++
		if seen == 2 {
			return stop
		}
		return nil
	})
	require.ErrorIs(t, err, stop)
	assert.Equal(t, 2, seen)
}