//go:build windows

package config

import (
	"runtime"
	"sync"
	"unsafe"

	"golang.org/x/sys/windows"
)

// relationProcessorCore asks for one entry per physical core; entry count is
// the physical core count (excludes SMT siblings).
const relationProcessorCore = 0

var (
	physicalOnce   sync.Once
	physicalCores  int
	kernel32       = windows.NewLazySystemDLL("kernel32.dll")
	getLogicalProc = kernel32.NewProc("GetLogicalProcessorInformationEx")
)

// PhysicalCores returns the number of physical CPU cores, falling back to
// the logical count if the OS query fails.
func PhysicalCores() int {
	physicalOnce.Do(func() {
		physicalCores = queryPhysicalCores()
	})
	return physicalCores
}

func queryPhysicalCores() int {
	var length uint32
	// First call is expected to return FALSE with ERROR_INSUFFICIENT_BUFFER while
	// filling in the required size. Only a zero length means we must fall back.
	_, _, _ = getLogicalProc.Call(
		uintptr(relationProcessorCore),
		0,
		uintptr(unsafe.Pointer(&length)),
	)
	if length == 0 {
		return runtime.NumCPU()
	}

	buffer := make([]byte, length)
	r, _, _ := getLogicalProc.Call(
		uintptr(relationProcessorCore),
		uintptr(unsafe.Pointer(&buffer[0])),
		uintptr(unsafe.Pointer(&length)),
	)
	if r == 0 {
		return runtime.NumCPU()
	}

	const headerSize = 8 // DWORD Relationship + DWORD Size
	cores := 0
	for offset := uint32(0); offset+headerSize <= length; {
		relationship := *(*uint32)(unsafe.Pointer(&buffer[offset]))
		size := *(*uint32)(unsafe.Pointer(&buffer[offset+4]))
		if relationship == relationProcessorCore {
			cores++
		}
		if size == 0 {
			break
		}
		offset += size
	}
	if cores == 0 {
		return runtime.NumCPU()
	}
	return cores
}
