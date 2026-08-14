//go:build windows

package engine

import (
	"os/exec"
	"syscall"
)

// hideWindow prevents a console window from flashing on Windows when the
// engine subprocess is spawned. The engine is a console exe; without this,
// a cmd window appears and closes during every encode.
func hideWindow(cmd *exec.Cmd) {
	cmd.SysProcAttr = &syscall.SysProcAttr{
		HideWindow:    true,
		CreationFlags: 0x08000000, // CREATE_NO_WINDOW
	}
}
