//go:build !windows

package engine

import "os/exec"

// hideWindow is a no-op on non-Windows platforms where the engine is a
// normal binary and never spawns a console window.
func hideWindow(*exec.Cmd) {}
