package main

import (
	"context"

	"github.com/Lanzoone30/OpenSUP-go/ui/config"
	"github.com/wailsapp/wails/v2/pkg/runtime"
)

const (
	designWidth     = 1000
	designHeight    = 825
	minWindowWidth  = 800
	minWindowHeight = 600
	// Wails reports the full screen, not the work area.
	windowMarginX = 64
	windowMarginY = 96
)

type screenInfo struct {
	width, height int
	count         int
}

func (a *App) applyWindowSize(ctx context.Context) {
	info := primaryScreen(ctx)
	w, h := a.settings.WindowWidth, a.settings.WindowHeight
	if w <= 0 || h <= 0 {
		w, h = defaultWindowSize(info.width, info.height)
	}
	w, h = clampWindowSize(w, h, info.width, info.height)
	runtime.WindowSetSize(ctx, w, h)

	if a.settings.WindowMaximised {
		runtime.WindowCenter(ctx)
		runtime.WindowMaximise(ctx)
		return
	}

	if !a.settings.WindowPositioned {
		runtime.WindowCenter(ctx)
		return
	}
	x, y := fitPosition(a.settings.WindowX, a.settings.WindowY, w, h, info)
	if info.count > 1 && !onScreen(x, y, info) {
		runtime.WindowCenter(ctx)
		return
	}
	runtime.WindowSetPosition(ctx, x, y)
}

func (a *App) beforeClose(ctx context.Context) bool {
	maximised := runtime.WindowIsMaximised(ctx)
	w, h := runtime.WindowGetSize(ctx)
	if w < minWindowWidth || h < minWindowHeight {
		return false
	}
	x, y := runtime.WindowGetPosition(ctx)

	a.mu.Lock()
	a.settings.WindowWidth, a.settings.WindowHeight = w, h
	a.settings.WindowMaximised = maximised
	if !maximised {
		a.settings.WindowX, a.settings.WindowY = x, y
		a.settings.WindowPositioned = true
	}
	s := a.settings
	a.mu.Unlock()

	_ = config.Save(s)
	return false
}

func primaryScreen(ctx context.Context) screenInfo {
	screens, err := runtime.ScreenGetAll(ctx)
	if err != nil || len(screens) == 0 {
		return screenInfo{}
	}
	for _, s := range screens {
		if s.IsPrimary {
			return screenInfo{width: s.Size.Width, height: s.Size.Height, count: len(screens)}
		}
	}
	return screenInfo{width: screens[0].Size.Width, height: screens[0].Size.Height, count: len(screens)}
}

// Keep the original 1000x825 design at 1080p and scale it down below that.
func defaultWindowSize(screenW, screenH int) (int, int) {
	if screenW <= 0 || screenH <= 0 {
		return designWidth, designHeight
	}
	h := min(designHeight, screenH-windowMarginY)
	w := h * designWidth / designHeight
	w = min(w, screenW-windowMarginX)
	return max(w, minWindowWidth), max(h, minWindowHeight)
}

func clampWindowSize(w, h, screenW, screenH int) (int, int) {
	w = max(w, minWindowWidth)
	h = max(h, minWindowHeight)
	if screenW > 0 {
		w = min(w, screenW-windowMarginX)
	}
	if screenH > 0 {
		h = min(h, screenH-windowMarginY)
	}
	return max(w, minWindowWidth), max(h, minWindowHeight)
}

// Multi-monitor coordinates can be negative or past the primary screen.
func fitPosition(x, y, w, h int, info screenInfo) (int, int) {
	if info.count != 1 || info.width <= 0 || info.height <= 0 {
		return x, y
	}
	x = min(max(x, 0), max(0, info.width-w))
	y = min(max(y, 0), max(0, info.height-h))
	return x, y
}

// Reject the off-screen rect Windows reports for minimized windows.
func onScreen(x, y int, info screenInfo) bool {
	return x >= -info.width && y >= -info.height &&
		x < 2*info.width && y < 2*info.height
}
