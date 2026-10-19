package main

import (
	"crypto/sha256"
	"encoding/binary"
	"encoding/csv"
	"encoding/hex"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"os"
	"strconv"
	"strings"
)

const header_bytes = 160
const max_clock = uint64(1<<48) - 1

type result_header struct {
	HeaderBytes    uint32 `json:"header_bytes"`
	Version        uint32 `json:"version"`
	Protocol       uint32 `json:"protocol"`
	RecordsSHA256  string `json:"records_sha256,omitempty"`
	LastPageSHA256 string `json:"last_page_sha256,omitempty"`
	PageCount      uint32 `json:"page_count,omitempty"`
	Stream         uint32 `json:"stream"`
	TotalRecords   uint32 `json:"total_records"`
	EventFirst     uint32 `json:"event_first"`
	ClockHz        uint32 `json:"clock_Hz"`
	RunID          uint32 `json:"run_id"`
	Lane           uint32 `json:"lane"`
	Count          uint32 `json:"count"`
	Stride         uint32 `json:"stride"`
	Mode           uint32 `json:"mode"`
	First          uint64 `json:"first_clock"`
	Last           uint64 `json:"last_clock"`
	Logical        uint64 `json:"logical_clock"`
	Generation     uint32 `json:"generation"`
	EventShift     uint32 `json:"event_shift"`
	Idle           uint64 `json:"idle_clocks"`
}

func main() {
	expected_path := flag.String("expect-hash", "", "file containing exact64-hex source SHA256")
	flag.Parse()
	if flag.NArg() != 2 || *expected_path == "" {
		fmt.Fprintln(os.Stderr, "usage: decode --expect-hash source-hash.txt INPUT.bin NEW.csv")
		os.Exit(2)
	}
	text, err := os.ReadFile(*expected_path)
	var expected [32]byte
	if err == nil {
		var b []byte
		b, err = hex.DecodeString(strings.TrimSpace(string(text)))
		if err == nil && len(b) != 32 {
			err = errors.New("expected exactly32 hash bytes")
		}
		copy(expected[:], b)
	}
	var h result_header
	if err == nil {
		h, err = decode(flag.Arg(0), flag.Arg(1), expected)
	}
	if err != nil {
		fmt.Fprintln(os.Stderr, "decode:", err)
		os.Exit(1)
	}
	input, err := os.ReadFile(flag.Arg(0))
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	output, err := os.ReadFile(flag.Arg(1))
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	input_sha := sha256.Sum256(input)
	output_sha := sha256.Sum256(output)
	if err = json.NewEncoder(os.Stdout).Encode(struct {
		State  string        `json:"state"`
		Header result_header `json:"header"`
		Source string        `json:"source_sha256"`
		Input  string        `json:"input_sha256"`
		Output string        `json:"output_sha256"`
	}{"decoded_complete_result", h, hex.EncodeToString(expected[:]), hex.EncodeToString(input_sha[:]), hex.EncodeToString(output_sha[:])}); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func decode(input, output string, expected [32]byte) (result_header, error) {
	var h result_header
	info, err := os.Stat(input)
	if err != nil {
		return h, err
	}
	if info.Size() < 160 || info.Size() > 256+16384*5 {
		return h, errors.New("result file size out of bounds")
	}
	data, err := os.ReadFile(input)
	if err != nil {
		return h, err
	}
	h, err = parse_result(data, expected)
	if err != nil {
		return h, err
	}
	return h, write_csv(output, data, h)
}

func parse_result(data []byte, expected [32]byte) (result_header, error) {
	var h result_header
	bad := func(reason string) (result_header, error) { return h, errors.New(reason) }
	if len(data) < header_bytes {
		return bad("truncated result header")
	}
	u32 := func(n int) uint32 { return binary.LittleEndian.Uint32(data[n : n+4]) }
	u64 := func(n int) uint64 { return binary.LittleEndian.Uint64(data[n : n+8]) }
	v1 := string(data[:8]) == "JT51FP01" && u32(8) == 160 && u32(12) == 1 && u32(16) == 0x965101
	v2 := string(data[:8]) == "JT51FP02" && u32(8) == 256 && u32(12) == 2 && u32(16) == 0x965102
	if !v1 && !v2 {
		return bad("magic/header/version/protocol mismatch")
	}
	if len(data) < int(u32(8)) {
		return bad("truncated versioned header")
	}
	h = result_header{HeaderBytes: u32(8), Version: u32(12), Protocol: u32(16), TotalRecords: u32(60), EventFirst: u32(144), ClockHz: u32(20), RunID: u32(24), Lane: u32(28), Count: u32(32), Stride: u32(40), Mode: u32(44), First: u64(64), Last: u64(72), Logical: u64(80), Generation: u32(96), EventShift: u32(100), Idle: u64(104)}
	if h.ClockHz == 0 || h.RunID == 0 || h.RunID > 0xffffff || h.Lane >= 4 {
		return bad("invalid clock/run/lane")
	}
	if h.Count == 0 || h.Count > 16384 || h.Count != u32(36) || h.Stride == 0 || h.Stride > 0xffffff || h.Mode > 1 {
		return bad("invalid capture geometry/mode")
	}
	if u32(48)&4 == 0 || u32(48)&^uint32(5) != 0 || u32(52) != 0 {
		return bad("result is failed, busy, nonterminal or has an error")
	}
	if u32(60) == 0 || (v1 && u32(60) > 4096) || u32(56) != u32(60) || u32(144) == 0 || u32(144) >= u32(60) {
		return bad("incomplete or invalid tape execution")
	}
	if h.First > max_clock || h.Last > max_clock || h.Logical > max_clock || h.Logical < h.Last || u64(88) != 0 || h.Idle > max_clock-512 || h.EventShift > 63 {
		return bad("invalid logical/failure clock or lane config")
	}
	delta := uint64(h.Count-1) * 64 * uint64(h.Stride)
	if delta > max_clock-h.First || h.Last != h.First+delta {
		return bad("sample cadence/last_clock mismatch")
	}
	if h.First < 512+h.Idle {
		return bad("capture precedes INIT")
	}
	if u32(148) != 5 || u64(152) != 0 || len(data) != int(h.HeaderBytes)+int(h.Count)*5 {
		return bad("sample size/reserved/payload length mismatch")
	}
	if v2 {
		if h.ClockHz != 50000000 {
			return bad("unexpected v2 logical clock frequency")
		}
		h.RecordsSHA256 = hex.EncodeToString(data[160:192])
		h.LastPageSHA256 = hex.EncodeToString(data[192:224])
		h.PageCount = u32(224)
		h.Stream = u32(232)
		zeroHash := [32]byte{}
		if string(data[160:192]) == string(zeroHash[:]) || h.PageCount == 0 || h.PageCount > 4096 || h.Stream > 1 || u32(228) == 0 || u32(228) > 4096 || u32(228) > h.TotalRecords || h.TotalRecords > 4000000 {
			return bad("invalid stream digest/page geometry")
		}
		if h.Stream == 0 && (h.PageCount != 1 || h.TotalRecords > 4096 || u32(228) != h.TotalRecords || string(data[192:224]) != string(zeroHash[:])) {
			return bad("invalid resident v2 page identity")
		}
		if h.Stream == 1 && (h.TotalRecords < h.PageCount || string(data[192:224]) == string(zeroHash[:])) {
			return bad("invalid stream chain identity")
		}
		for _, b := range data[236:256] {
			if b != 0 {
				return bad("nonzero v2 reserved bytes")
			}
		}
	}
	var actual [32]byte
	copy(actual[:], data[112:144])
	if actual != expected || expected == [32]byte{} {
		return bad("source signature mismatch or zero expected signature")
	}
	return h, nil
}

func write_csv(output string, data []byte, h result_header) error {
	f, err := os.OpenFile(output, os.O_WRONLY|os.O_CREATE|os.O_EXCL, 0666)
	if err != nil {
		return err
	}
	w := csv.NewWriter(f)
	names := []string{"sample", "left", "right", "clock", "status"}
	if h.Mode == 1 {
		names = []string{"sample", "xleft", "xright", "clock", "status"}
	}
	if err = w.Write(names); err == nil {
		for i := uint32(0); i < h.Count; i++ {
			b := data[int(h.HeaderBytes)+int(i)*5:]
			right := int16(binary.LittleEndian.Uint16(b))
			left := int16(binary.LittleEndian.Uint16(b[2:]))
			clock := h.First + uint64(i)*64*uint64(h.Stride)
			if err = w.Write([]string{strconv.FormatUint(uint64(i), 10), strconv.FormatInt(int64(left), 10), strconv.FormatInt(int64(right), 10), strconv.FormatUint(clock, 10), strconv.Itoa(int(b[4]))}); err != nil {
				break
			}
		}
	}
	w.Flush()
	if err == nil {
		err = w.Error()
	}
	close_err := f.Close()
	if err != nil {
		return err
	}
	return close_err
}
