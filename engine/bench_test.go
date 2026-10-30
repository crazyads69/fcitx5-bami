package engine

import (
	"sort"
	"testing"
	"time"
)

const benchText = "nguwowif vieetj nam dduwowcj hoaf binhf tieengs vieetj quyeenf chuyeenr khoer " // one pass

func BenchmarkKey(b *testing.B) {
	e := New(Telex2, stdOpts)
	keys := []rune(benchText)
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		k := keys[i%len(keys)]
		if _, ok := e.Key(k); !ok {
			e.Commit()
		}
	}
}

// TestKeyLatencyP99 measures every key of a long text and checks the PRD target (p99 < 1 ms per key).
func TestKeyLatencyP99(t *testing.T) {
	if testing.Short() || raceOn {
		t.Skip("latency is only meaningful without -race / -short")
	}
	e := New(Telex2, stdOpts)
	var d []time.Duration
	for rep := 0; rep < 2000; rep++ {
		for _, k := range benchText {
			t0 := time.Now()
			if _, ok := e.Key(k); !ok {
				e.Commit()
			}
			d = append(d, time.Since(t0))
		}
	}
	sort.Slice(d, func(i, j int) bool { return d[i] < d[j] })
	p50, p99, max := d[len(d)/2], d[len(d)*99/100], d[len(d)-1]
	t.Logf("%d keys: p50=%v p99=%v max=%v", len(d), p50, p99, max)
	if p99 > time.Millisecond {
		t.Errorf("p99 %v exceeds the 1ms PRD target", p99)
	}
}
