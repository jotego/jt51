package main

import (
	"bytes"
	"encoding/binary"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func fixture() ([]byte, [32]byte) {
	b := make([]byte, 170)
	copy(b, "JT51FP01")
	u32 := func(n int, v uint32) { binary.LittleEndian.PutUint32(b[n:], v) }
	u64 := func(n int, v uint64) { binary.LittleEndian.PutUint64(b[n:], v) }
	for n, v := range map[int]uint32{8: 160, 12: 1, 16: 0x965101, 20: 100000000, 24: 1, 28: 0, 32: 2, 36: 2, 40: 3, 44: 0, 48: 4, 52: 0, 56: 2, 60: 2, 96: 1, 100: 47, 144: 1, 148: 5} {
		u32(n, v)
	}
	u64(64, 1000)
	u64(72, 1192)
	u64(80, 1194)
	var expected [32]byte
	for i := range expected {
		expected[i] = byte(i + 1)
	}
	copy(b[112:144], expected[:])
	copy(b[160:], []byte{255, 127, 0, 128, 128, 254, 255, 255, 255, 3})
	return b, expected
}
func TestSignedModesAndCadence(t *testing.T) {
	for mode := uint32(0); mode < 2; mode++ {
		data, hash := fixture()
		binary.LittleEndian.PutUint32(data[44:], mode)
		h, err := parse_result(data, hash)
		if err != nil {
			t.Fatal(err)
		}
		out := filepath.Join(t.TempDir(), "out.csv")
		if err = write_csv(out, data, h); err != nil {
			t.Fatal(err)
		}
		got, err := os.ReadFile(out)
		if err != nil {
			t.Fatal(err)
		}
		header := "sample,left,right,clock,status\n"
		if mode == 1 {
			header = "sample,xleft,xright,clock,status\n"
		}
		want := header + "0,-32768,32767,1000,128\n1,-1,-2,1192,3\n"
		if string(got) != want {
			t.Fatalf("got %s", got)
		}
		if err = write_csv(out, data, h); err == nil {
			t.Fatal("existing CSV overwritten")
		}
		after, _ := os.ReadFile(out)
		if !bytes.Equal(got, after) {
			t.Fatal("existing CSV changed")
		}
	}
}
func TestHeaderRejections(t *testing.T) {
	for _, tc := range []struct {
		name   string
		offset int
		value  uint32
	}{
		{"header", 8, 159}, {"version", 12, 2}, {"protocol", 16, 0}, {"clock", 20, 0}, {"zeroRun", 24, 0}, {"largeRun", 24, 0x1000000}, {"lane", 28, 4}, {"zeroCount", 32, 0}, {"largeCount", 32, 16385}, {"requested", 36, 1}, {"zeroStride", 40, 0}, {"largeStride", 40, 0x1000000}, {"mode", 44, 2}, {"notDone", 48, 0}, {"busy", 48, 6}, {"failed", 48, 12}, {"unknownStatus", 48, 20}, {"laneError", 48, 0x104}, {"error", 52, 1}, {"executed", 56, 1}, {"zeroTape", 60, 0}, {"largeTape", 60, 4097}, {"shift", 100, 64}, {"eventFirstZero", 144, 0}, {"eventFirstEnd", 144, 2}, {"sampleSize", 148, 4}, {"reserved", 152, 1},
	} {
		t.Run(tc.name, func(t *testing.T) {
			data, hash := fixture()
			binary.LittleEndian.PutUint32(data[tc.offset:], tc.value)
			if _, err := parse_result(data, hash); err == nil {
				t.Fatal("invalid header accepted")
			}
		})
	}
	for _, tc := range []struct {
		name   string
		offset int
		value  uint64
	}{
		{"cadence", 72, 1193}, {"firstOverflow", 64, max_clock + 1}, {"lastOverflow", 72, max_clock + 1}, {"logicalOverflow", 80, max_clock + 1}, {"logicalBeforeLast", 80, 1191}, {"failureClock", 88, 1}, {"idleOverflow", 104, max_clock}, {"beforeINIT", 104, 1000},
	} {
		t.Run(tc.name, func(t *testing.T) {
			data, hash := fixture()
			binary.LittleEndian.PutUint64(data[tc.offset:], tc.value)
			if _, err := parse_result(data, hash); err == nil {
				t.Fatal("invalid clock accepted")
			}
		})
	}
}
func TestSourceAndLength(t *testing.T) {
	data, hash := fixture()
	for _, n := range []int{0, 7, 159, 160, 169} {
		if _, err := parse_result(data[:n], hash); err == nil {
			t.Fatalf("length%d accepted", n)
		}
	}
	if _, err := parse_result(append(data, 0), hash); err == nil {
		t.Fatal("trailing byte accepted")
	}
	wrong := hash
	wrong[0] ^= 1
	if _, err := parse_result(data, wrong); err == nil {
		t.Fatal("wrong signature accepted")
	}
	if _, err := parse_result(data, [32]byte{}); err == nil {
		t.Fatal("zero expected signature accepted")
	}
	data[0] = 'X'
	if _, err := parse_result(data, hash); err == nil {
		t.Fatal("wrong magic accepted")
	}
}
func TestFailedCreatesNoCSV(t *testing.T) {
	data, hash := fixture()
	binary.LittleEndian.PutUint32(data[48:], 12)
	dir := t.TempDir()
	input, output := filepath.Join(dir, "failed.bin"), filepath.Join(dir, "new.csv")
	if err := os.WriteFile(input, data, 0600); err != nil {
		t.Fatal(err)
	}
	if _, err := decode(input, output, hash); err == nil || !strings.Contains(err.Error(), "failed") {
		t.Fatalf("wanted failed result rejection: %v", err)
	}
	if _, err := os.Stat(output); !os.IsNotExist(err) {
		t.Fatal("failed result produced CSV")
	}
}

func fixtureV2(stream bool) ([]byte, [32]byte) {
	old, hash := fixture()
	b := make([]byte, 266)
	copy(b, old[:160])
	copy(b[256:], old[160:])
	copy(b, "JT51FP02")
	u32 := func(n int, v uint32) { binary.LittleEndian.PutUint32(b[n:], v) }
	u32(8, 256)
	u32(20, 50000000)
	u32(12, 2)
	u32(16, 0x965102)
	for i := 160; i < 192; i++ {
		b[i] = byte(i)
	}
	u32(224, 1)
	u32(228, 2)
	if stream {
		u32(56, 10000)
		u32(60, 10000)
		u32(144, 9000)
		u32(224, 3)
		u32(228, 1810)
		u32(232, 1)
		for i := 192; i < 224; i++ {
			b[i] = byte(i)
		}
	}
	return b, hash
}
func TestV2ResidentAndStream(t *testing.T) {
	for _, stream := range []bool{false, true} {
		b, hash := fixtureV2(stream)
		h, e := parse_result(b, hash)
		if e != nil {
			t.Fatal(e)
		}
		if h.HeaderBytes != 256 || h.Version != 2 || h.Protocol != 0x965102 || h.RecordsSHA256 == "" || h.PageCount == 0 {
			t.Fatal("lost v2 identity")
		}
		out := filepath.Join(t.TempDir(), "out.csv")
		if e = write_csv(out, b, h); e != nil {
			t.Fatal(e)
		}
		got, e := os.ReadFile(out)
		if e != nil {
			t.Fatal(e)
		}
		if string(got) != "sample,left,right,clock,status\n0,-32768,32767,1000,128\n1,-1,-2,1192,3\n" {
			t.Fatalf("v2 payload offset: %s", got)
		}
	}
}
func TestV2Rejections(t *testing.T) {
	cases := []struct {
		name string
		off  int
		val  uint32
	}{
		{"clock", 20, 100000000}, {"zeroPages", 224, 0}, {"manyPages", 224, 4097}, {"zeroFinalExecuted", 228, 0}, {"largeFinalExecuted", 228, 4097}, {"unknownStream", 232, 2}, {"reserved", 236, 1}, {"hugeTotal", 60, 4000001}, {"version", 12, 1}, {"legacyProtocol", 16, 0x965101},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			b, h := fixtureV2(true)
			binary.LittleEndian.PutUint32(b[tc.off:], tc.val)
			if _, e := parse_result(b, h); e == nil {
				t.Fatal("invalid stream header accepted")
			}
		})
	}
	for _, region := range [][2]int{{160, 192}, {192, 224}} {
		b, h := fixtureV2(true)
		for i := region[0]; i < region[1]; i++ {
			b[i] = 0
		}
		if _, e := parse_result(b, h); e == nil {
			t.Fatal("zero chain digest accepted")
		}
	}
	for _, n := range []int{160, 200, 255, 256, 265} {
		b, h := fixtureV2(true)
		if _, e := parse_result(b[:n], h); e == nil {
			t.Fatalf("truncated v2 length%d accepted", n)
		}
	}
	b, h := fixtureV2(false)
	b[192] = 1
	if _, e := parse_result(b, h); e == nil {
		t.Fatal("resident accepted fabricated active page hash")
	}
}
