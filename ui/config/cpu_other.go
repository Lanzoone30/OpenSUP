//go:build !windows

package config

import "runtime"

// PhysicalCores falls back to the logical count on platforms without a
// physical-core query (development builds).
func PhysicalCores() int {
	return runtime.NumCPU()
}
