package main

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestDefaultWindowSize(t *testing.T) {
	tests := []struct {
		name             string
		screenW, screenH int
		wantW, wantH     int
	}{
		{"unknown screen", 0, 0, 1000, 825},
		{"1024x600", 1024, 600, 800, 600},
		{"1280x720", 1280, 720, 800, 624},
		{"1280x800", 1280, 800, 853, 704},
		{"1366x768", 1366, 768, 814, 672},
		{"1536x864", 1536, 864, 930, 768},
		{"1920x1080", 1920, 1080, 1000, 825},
		{"2560x1600", 2560, 1600, 1000, 825},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			w, h := defaultWindowSize(tt.screenW, tt.screenH)
			assert.Equal(t, tt.wantW, w)
			assert.Equal(t, tt.wantH, h)
		})
	}
}

func TestClampWindowSize(t *testing.T) {
	tests := []struct {
		name                   string
		w, h, screenW, screenH int
		wantW, wantH           int
	}{
		{"saved size fits", 1000, 825, 1920, 1080, 1000, 825},
		{"saved size exceeds screen", 1600, 1000, 1366, 768, 1302, 672},
		{"saved size below minimum", 700, 500, 1920, 1080, 800, 600},
		{"screen below minimum", 900, 700, 640, 480, 800, 600},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			w, h := clampWindowSize(tt.w, tt.h, tt.screenW, tt.screenH)
			assert.Equal(t, tt.wantW, w)
			assert.Equal(t, tt.wantH, h)
		})
	}
}

func TestFitPosition(t *testing.T) {
	single := screenInfo{width: 1920, height: 1080, count: 1}
	multi := screenInfo{width: 1920, height: 1080, count: 2}

	tests := []struct {
		name         string
		x, y, w, h   int
		info         screenInfo
		wantX, wantY int
	}{
		{"single screen clamps", -50, 2000, 1000, 825, single, 0, 255},
		{"single screen keeps", 400, 100, 1000, 825, single, 400, 100},
		{"single screen window too large", 400, 100, 2000, 1200, single, 0, 0},
		{"multi screen untouched", -1900, 100, 1000, 825, multi, -1900, 100},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			x, y := fitPosition(tt.x, tt.y, tt.w, tt.h, tt.info)
			assert.Equal(t, tt.wantX, x)
			assert.Equal(t, tt.wantY, y)
		})
	}
}

func TestOnScreen(t *testing.T) {
	multi := screenInfo{width: 1920, height: 1080, count: 2}

	assert.True(t, onScreen(-1900, 100, multi))
	assert.True(t, onScreen(-1920, -1080, multi))
	assert.True(t, onScreen(0, 0, multi))
	assert.False(t, onScreen(-32000, -32000, multi))
	assert.False(t, onScreen(4000, 100, multi))
}
