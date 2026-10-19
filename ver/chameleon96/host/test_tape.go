// Compiler-only protocol tests; no DUT, physical fixture or external program required.
package main

import (
	"bytes"
	"encoding/binary"
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
)

type metadata struct {
	Length     uint32 `json:"length"`
	EventFirst uint32 `json:"event_first"`
	Key        uint64 `json:"first_event_KEY_offset"`
	Capture    uint64 `json:"capture_offset"`
	End        uint64 `json:"event_end_offset"`
	Carry      uint32 `json:"init_carry_out_thirds"`
}

var checks int

func check(v bool, why string) {
	checks++
	if !v {
		panic(why)
	}
}
func main() {
	if len(os.Args) != 2 {
		panic("usage: go run test_tape.go COMPILER")
	}
	compiler, err := filepath.Abs(os.Args[1])
	if err != nil {
		panic(err)
	}
	dir, err := os.MkdirTemp("", "jt51-tape-test-")
	if err != nil {
		panic(err)
	}
	defer os.RemoveAll(dir)
	serial := 0
	run := func(init, event []byte, options ...string) ([]byte, metadata, string, error) {
		serial++
		prefix := filepath.Join(dir, fmt.Sprint(serial))
		ip, ep, out := prefix+".init", prefix+".event", prefix+".tape"
		if err := os.WriteFile(ip, init, 0600); err != nil {
			panic(err)
		}
		if err := os.WriteFile(ep, event, 0600); err != nil {
			panic(err)
		}
		args := append(options, ip, ep, out)
		_, err := exec.Command(compiler, args...).CombinedOutput()
		data, _ := os.ReadFile(out)
		var m metadata
		if err == nil {
			j, e := os.ReadFile(out + ".json")
			if e != nil {
				panic(e)
			}
			if e = json.Unmarshal(j, &m); e != nil {
				panic(e)
			}
		}
		return data, m, out, err
	}
	data, m, out, err := run([]byte{0}, []byte{1, 8, 126, 3, 0})
	check(err == nil, "basic compile")
	check(len(data) == 64+9*8, "length")
	check(string(data[:8]) == "JT51TAP1", "magic")
	check(binary.LittleEndian.Uint32(data[8:]) == 1 && binary.LittleEndian.Uint32(data[12:]) == 64, "version/header")
	check(m.Length == 9 && m.EventFirst == 1 && m.Key == 256 && m.Capture == 49 && m.End == 393 && m.Carry == 1, "CPU rational timing")
	expected := [][4]uint64{{49, 3, 0, 0}, {120, 1, 0, 3}, {180, 0, 8, 0}, {182, 0, 8, 2}, {196, 1, 8, 3}, {256, 0, 126, 1}, {258, 0, 126, 3}, {333, 2, 0, 3}, {393, 4, 0, 0}}
	for i, e := range expected {
		w := binary.LittleEndian.Uint64(data[64+8*i:])
		check(w == e[0]<<16|e[1]<<12|e[2]<<4|e[3]<<1, "exact action")
	}
	before := append([]byte(nil), data...)
	_, err = exec.Command(compiler, out[:len(out)-5]+".init", out[:len(out)-5]+".event", out).CombinedOutput()
	check(err != nil, "overwrite rejected")
	after, _ := os.ReadFile(out)
	check(bytes.Equal(before, after), "existing output preserved")
	_, m, _, err = run([]byte{0}, []byte{0})
	check(err == nil && m.Key == ^uint64(0) && m.Capture == 49 && m.End == 98, "no active KEY sentinel and carry")
	_, m, _, err = run([]byte{0}, []byte{1, 8, 126, 0}, "--event-low", "253")
	check(err == nil && m.Key == 261, "pointer page carry")
	_, m, _, err = run([]byte{0}, []byte{2, 0, 32, 1, 8, 126, 0})
	check(err == nil && m.Key == 306430, "DELAY8192 exact CPU costs")
	for _, bad := range [][]byte{{}, {4, 0}, {1, 8, 0}, {0, 0}, {3}} {
		b, _, _, e := run([]byte{0}, bad)
		check(e != nil && len(b) == 0, "invalid program rejected before output")
	}
	b, _, _, err := run([]byte{0}, []byte{3, 0}, "--status-mask", "4")
	check(err != nil && len(b) == 0, "invalid status mask")
	many := []byte{}
	for i := 0; i < 682; i++ {
		many = append(many, 1, 8, 6)
	}
	many = append(many, 3, 3, 0)
	b, m, _, err = run([]byte{0}, many)
	check(err == nil && m.Length == 4096 && len(b) == 64+4096*8, "exact RAM limit")
	many = append(many[:len(many)-1], 3, 0)
	b, _, _, err = run([]byte{0}, many)
	check(err != nil && len(b) == 0, "RAM overflow rejected")
	fmt.Printf("PASS %d tape compiler checks; no DUT executed\n", checks)
}
