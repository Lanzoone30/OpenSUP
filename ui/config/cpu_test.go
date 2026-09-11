package config

import "testing"

func TestPhysicalCoresIsSane(t *testing.T) {
	cores := PhysicalCores()
	if cores < 1 {
		t.Fatalf("PhysicalCores() = %d, want >= 1", cores)
	}
}
