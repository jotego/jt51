package main

import (
	"bytes"
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
)

func nativeTool(t *testing.T) string {
	t.Helper()
	dst := filepath.Join(t.TempDir(), "c96-jt51")
	c := exec.Command("gcc", "-std=c99", "-O2", "-Wall", "-Wextra", "-Werror", "-o", dst, "c96-jt51.c")
	if out, e := c.CombinedOutput(); e != nil {
		t.Fatalf("native build: %s: %v", out, e)
	}
	return dst
}
func TestNativeSHA256(t *testing.T) {
	tool := nativeTool(t)
	for _, n := range []int{0, 1, 3, 55, 56, 57, 63, 64, 65, 1031, 4096, 1000000} {
		data := bytes.Repeat([]byte{'a'}, n)
		path := filepath.Join(t.TempDir(), "in")
		if e := os.WriteFile(path, data, 0600); e != nil {
			t.Fatal(e)
		}
		got, e := exec.Command(tool, "hash", path).CombinedOutput()
		want := sha256.Sum256(data)
		if e != nil || strings.TrimSpace(string(got)) != hex.EncodeToString(want[:]) {
			t.Fatalf("SHA256 len%d: %s (%v)", n, got, e)
		}
	}
}
func tapePage(words []uint64, event uint32) []byte {
	b := make([]byte, 64+8*len(words))
	copy(b, "JT51TAP1")
	for o, v := range map[int]uint32{8: 1, 12: 64, 16: uint32(len(words)), 20: event} {
		binary.LittleEndian.PutUint32(b[o:], v)
	}
	binary.LittleEndian.PutUint64(b[40:], words[len(words)-1]>>16)
	for i, w := range words {
		binary.LittleEndian.PutUint64(b[64+8*i:], w)
	}
	return b
}
func TestNativeStreamInputs(t *testing.T) {
	tool := nativeTool(t)
	type change func([][]byte, []string) ([][]byte, []string)
	cases := []struct {
		name   string
		valid  bool
		mutate change
	}{
		{"valid", true, nil},
		{"index", false, func(p [][]byte, m []string) ([][]byte, []string) {
			m[1] = strings.Replace(m[1], "0\t", "1\t", 1)
			return p, m
		}},
		{"path", false, func(p [][]byte, m []string) ([][]byte, []string) {
			m[1] = strings.Replace(m[1], "p0.tape", "../p0.tape", 1)
			return p, m
		}},
		{"digest", false, func(p [][]byte, m []string) ([][]byte, []string) {
			m[1] = strings.Replace(m[1], "p0.tape\t", "p0.tape\t0", 1)
			return p, m
		}},
		{"wrongdigest", false, func(p [][]byte, m []string) ([][]byte, []string) {
			f := strings.Split(m[1], "\t")
			f[2] = strings.Repeat("0", 64)
			m[1] = strings.Join(f, "\t")
			return p, m
		}},
		{"globalzero", false, func(p [][]byte, m []string) ([][]byte, []string) {
			f := strings.Split(m[0], "\t")
			f[2] = "0"
			m[0] = strings.Join(f, "\t")
			return p, m
		}},
		{"globalend", false, func(p [][]byte, m []string) ([][]byte, []string) {
			f := strings.Split(m[0], "\t")
			f[2] = "5"
			m[0] = strings.Join(f, "\t")
			return p, m
		}},
		{"recorddigest", false, func(p [][]byte, m []string) ([][]byte, []string) {
			f := strings.Split(m[0], "\t")
			f[4] = strings.Repeat("0", 64) + "\n"
			m[0] = strings.Join(f, "\t")
			return p, m
		}},
		{"localboundary", false, func(p [][]byte, m []string) ([][]byte, []string) {
			binary.LittleEndian.PutUint32(p[1][20:], 0)
			return p, m
		}},
		{"missingPAGE", false, func(p [][]byte, m []string) ([][]byte, []string) {
			binary.LittleEndian.PutUint64(p[0][80:], 20<<16|4<<12)
			return p, m
		}},
		{"missingEND", false, func(p [][]byte, m []string) ([][]byte, []string) {
			binary.LittleEndian.PutUint64(p[1][72:], 24<<16|5<<12)
			return p, m
		}},
		{"noCAPTURE", false, func(p [][]byte, m []string) ([][]byte, []string) {
			binary.LittleEndian.PutUint64(p[0][64:], 2<<1)
			return p, m
		}},
		{"activeWR", false, func(p [][]byte, m []string) ([][]byte, []string) {
			binary.LittleEndian.PutUint64(p[0][72:], 2<<16)
			return p, m
		}},
		{"reserved", false, func(p [][]byte, m []string) ([][]byte, []string) { p[0][80] |= 1; return p, m }},
		{"refillmargin", false, func(p [][]byte, m []string) ([][]byte, []string) {
			binary.LittleEndian.PutUint64(p[1][64:], 21<<16|2<<12|3<<1)
			return p, m
		}},
		{"trailing", false, func(p [][]byte, m []string) ([][]byte, []string) { m = append(m, "extra\n"); return p, m }},
		{"truncatedpage", false, func(p [][]byte, m []string) ([][]byte, []string) { p[0] = p[0][:len(p[0])-1]; return p, m }},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			dir := t.TempDir()
			p := [][]byte{tapePage([]uint64{3 << 12, 2<<16 | 3<<1, 20<<16 | 5<<12}, 3), tapePage([]uint64{22<<16 | 2<<12 | 3<<1, 24<<16 | 4<<12}, 1)}
			recordHash := sha256.New()
			for _, b := range p {
				recordHash.Write(b[64:])
			}
			m := []string{fmt.Sprintf("JT51STREAM2\t5\t4\t2\t%x\n", recordHash.Sum(nil))}
			for i, b := range p {
				h := sha256.Sum256(b)
				m = append(m, fmt.Sprintf("%d\tp%d.tape\t%x\t%d\t%d\n", i, i, h, len(b[64:])/8, binary.LittleEndian.Uint32(b[20:])))
			}
			if tc.mutate != nil {
				p, m = tc.mutate(p, m)
			}
			// Repin modified files so chronology/opcode failures test semantic checks,
			// while explicit digest cases intentionally retain their bad declarations.
			for i, b := range p {
				if tc.name != "digest" && tc.name != "wrongdigest" {
					f := strings.Split(m[i+1], "\t")
					h := sha256.Sum256(b)
					f[2] = fmt.Sprintf("%x", h)
					m[i+1] = strings.Join(f, "\t")
				}
				if e := os.WriteFile(filepath.Join(dir, fmt.Sprintf("p%d.tape", i)), b, 0600); e != nil {
					t.Fatal(e)
				}
			}
			if e := os.WriteFile(filepath.Join(dir, "stream.tsv"), []byte(strings.Join(m, "")), 0600); e != nil {
				t.Fatal(e)
			}
			jobs := "name\trun_id\tidle_clocks\tevent_shift\tframes\tstride\tmode\na\t1\t0\t0\t2\t1\t0\n"
			if e := os.WriteFile(filepath.Join(dir, "jobs.tsv"), []byte(jobs), 0600); e != nil {
				t.Fatal(e)
			}
			out, e := exec.Command(tool, "validate-stream", filepath.Join(dir, "stream.tsv"), filepath.Join(dir, "jobs.tsv")).CombinedOutput()
			if (e == nil) != tc.valid {
				t.Fatalf("valid%v: %s (%v)", tc.valid, out, e)
			}
		})
	}
}
