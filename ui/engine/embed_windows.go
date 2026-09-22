//go:build windows

package engine

import "embed"

// Only the Windows engine is embedded in a Windows build; the Linux
// binary would be dead weight (~8 MB) in the shipped .exe.
//
//go:embed bin/windows_amd64/opensup_engine.exe
var embedded embed.FS
