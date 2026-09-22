//go:build !windows

package engine

import "embed"

// Only the Linux engine is embedded in a non-Windows build.
//
//go:embed bin/linux_amd64/opensup_engine
var embedded embed.FS
