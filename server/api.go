package main

import (
	"bufio"
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"
	"strconv"
	"strings"
	"sync"
)

type Item struct {
	ID       int     `json:"id"`
	A        float64 `json:"a"`
	B        float64 `json:"b"`
	Left     float64 `json:"left"`
	Right    float64 `json:"right"`
	Selected bool    `json:"selected"`
	Cluster  int     `json:"cluster"`
}

type Snapshot struct {
	Alpha      float64       `json:"alpha"`
	Items      []Item        `json:"items"`
	Criterion  string        `json:"criterion"`
	Selected   int           `json:"selected"`
	Method     string        `json:"method"`
	Iterations int           `json:"iterations"`
	Clusters   []ClusterInfo `json:"clusters"`
}

type Server struct {
	mu        sync.Mutex
	logPath   string
	items     []Uniform
	alpha     float64
	left      []float64
	right     []float64
	selected  []bool
	criterion string
	labels    []int
	method    string
	iter      int
	clusters  []ClusterInfo
}

func NewServer(logPath string) *Server {
	return &Server{logPath: logPath, alpha: 0.7, criterion: "none"}
}

func (s *Server) Routes(mux *http.ServeMux) {
	mux.HandleFunc("/api/state", s.handle(s.state))
	mux.HandleFunc("/api/generate", s.handle(s.generate))
	mux.HandleFunc("/api/add", s.handle(s.add))
	mux.HandleFunc("/api/clear", s.handle(s.clear))
	mux.HandleFunc("/api/alpha", s.handle(s.setAlpha))
	mux.HandleFunc("/api/select", s.handle(s.selectItems))
	mux.HandleFunc("/api/cluster", s.handle(s.cluster))
	mux.HandleFunc("/api/import", s.handle(s.importCSV))
	mux.HandleFunc("/api/op", s.operate)
	mux.HandleFunc("/api/export", s.exportCSV)
	mux.HandleFunc("/api/log", s.journal)
}

func writeJSON(w http.ResponseWriter, code int, v any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(code)
	json.NewEncoder(w).Encode(v)
}

func writeErr(w http.ResponseWriter, err error) {
	writeJSON(w, http.StatusBadRequest, map[string]string{"error": err.Error()})
}

func (s *Server) handle(f func(*http.Request) error) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		s.mu.Lock()
		defer s.mu.Unlock()
		if err := f(r); err != nil {
			writeErr(w, err)
			return
		}
		writeJSON(w, http.StatusOK, s.snapshot())
	}
}

func decode(r *http.Request, v any) error {
	if r.Method != http.MethodPost {
		return errors.New("POST required")
	}
	return json.NewDecoder(r.Body).Decode(v)
}

func (s *Server) snapshot() Snapshot {
	items := make([]Item, len(s.items))
	cnt := 0
	for i, u := range s.items {
		items[i] = Item{ID: i, A: u.A, B: u.B, Left: s.left[i], Right: s.right[i], Selected: s.selected[i], Cluster: -1}
		if s.labels != nil {
			items[i].Cluster = s.labels[i]
		}
		if s.selected[i] {
			cnt++
		}
	}
	clusters := s.clusters
	if clusters == nil {
		clusters = []ClusterInfo{}
	}
	return Snapshot{s.alpha, items, s.criterion, cnt, s.method, s.iter, clusters}
}

func (s *Server) resetClusters() {
	s.labels = nil
	s.clusters = nil
	s.method = ""
	s.iter = 0
}

func (s *Server) recompute() error {
	l, r, err := Intervals(s.items, s.alpha)
	if err != nil {
		return err
	}
	s.left, s.right = l, r
	s.selected = make([]bool, len(s.items))
	for i := range s.selected {
		s.selected[i] = true
	}
	s.criterion = "none"
	s.resetClusters()
	return nil
}

func (s *Server) state(r *http.Request) error {
	if s.left == nil {
		return s.recompute()
	}
	return nil
}

func (s *Server) generate(r *http.Request) error {
	var req struct {
		N     int     `json:"n"`
		Seed  uint64  `json:"seed"`
		Lo    float64 `json:"lo"`
		Hi    float64 `json:"hi"`
		WMin  float64 `json:"wmin"`
		WMax  float64 `json:"wmax"`
		Alpha float64 `json:"alpha"`
	}
	if err := decode(r, &req); err != nil {
		return err
	}
	if req.Alpha < 0 || req.Alpha > 1 {
		return errors.New("alpha must be in [0, 1]")
	}
	items, err := Generate(req.Seed, req.N, req.Lo, req.Hi, req.WMin, req.WMax)
	if err != nil {
		return err
	}
	s.items = items
	s.alpha = req.Alpha
	return s.recompute()
}

func (s *Server) add(r *http.Request) error {
	var u Uniform
	if err := decode(r, &u); err != nil {
		return err
	}
	if !(u.A < u.B) {
		return errors.New("need a < b")
	}
	s.items = append(s.items, u)
	LogMessage(fmt.Sprintf("assign X%d = U(%.4f, %.4f)", len(s.items)-1, u.A, u.B))
	return s.recompute()
}

func (s *Server) clear(r *http.Request) error {
	s.items = nil
	LogMessage("set cleared")
	return s.recompute()
}

func (s *Server) setAlpha(r *http.Request) error {
	var req struct {
		Alpha float64 `json:"alpha"`
	}
	if err := decode(r, &req); err != nil {
		return err
	}
	if req.Alpha < 0 || req.Alpha > 1 {
		return errors.New("alpha must be in [0, 1]")
	}
	s.alpha = req.Alpha
	return s.recompute()
}

func (s *Server) selectItems(r *http.Request) error {
	var req struct {
		Criterion string  `json:"criterion"`
		P1        float64 `json:"p1"`
		P2        float64 `json:"p2"`
	}
	if err := decode(r, &req); err != nil {
		return err
	}
	var idx []int
	switch req.Criterion {
	case "none":
		idx = make([]int, len(s.items))
		for i := range idx {
			idx[i] = i
		}
		LogMessage("selection reset: all intervals")
	case "width":
		idx = SelectWidth(s.left, s.right, req.P1, req.P2)
	case "window":
		idx = SelectWindow(s.left, s.right, req.P1, req.P2)
	default:
		return errors.New("unknown criterion")
	}
	s.selected = make([]bool, len(s.items))
	for _, i := range idx {
		s.selected[i] = true
	}
	s.criterion = req.Criterion
	s.resetClusters()
	return nil
}

func (s *Server) cluster(r *http.Request) error {
	var req struct {
		Method string  `json:"method"`
		K      int     `json:"k"`
		Delta  float64 `json:"delta"`
		Seed   uint64  `json:"seed"`
	}
	if err := decode(r, &req); err != nil {
		return err
	}
	var ids []int
	var l, rr []float64
	for i := range s.items {
		if s.selected[i] {
			ids = append(ids, i)
			l = append(l, s.left[i])
			rr = append(rr, s.right[i])
		}
	}
	var labels []int
	var k, iter int
	var err error
	switch req.Method {
	case "kmeans":
		labels, iter, err = KMeans(l, rr, req.K, req.Seed)
		k = req.K
	case "threshold":
		labels, k, err = Threshold(l, rr, req.Delta)
	default:
		err = errors.New("unknown method")
	}
	if err != nil {
		return err
	}
	s.labels = make([]int, len(s.items))
	for i := range s.labels {
		s.labels[i] = -1
	}
	for j, id := range ids {
		s.labels[id] = labels[j]
	}
	s.clusters = ClusterStats(l, rr, labels, k)
	s.method = req.Method
	s.iter = iter
	for c, info := range s.clusters {
		LogMessage(fmt.Sprintf("cluster %d: count=%d center=[%.4f, %.4f] spread=%.4f", c+1, info.Count, info.Left, info.Right, info.Spread))
	}
	return nil
}

func (s *Server) importCSV(r *http.Request) error {
	if r.Method != http.MethodPost {
		return errors.New("POST required")
	}
	body, err := io.ReadAll(io.LimitReader(r.Body, 8<<20))
	if err != nil {
		return err
	}
	var items []Uniform
	sc := bufio.NewScanner(bytes.NewReader(body))
	line := 0
	for sc.Scan() {
		line++
		t := strings.TrimSpace(sc.Text())
		if t == "" {
			continue
		}
		f := strings.FieldsFunc(t, func(c rune) bool { return c == ',' || c == ';' || c == '\t' || c == ' ' })
		if len(f) < 2 {
			return fmt.Errorf("line %d: expected a,b", line)
		}
		off := 0
		if len(f) >= 3 && strings.EqualFold(f[0], "id") {
			continue
		}
		if len(f) >= 3 {
			off = 1
		}
		a, ea := strconv.ParseFloat(f[off], 64)
		b, eb := strconv.ParseFloat(f[off+1], 64)
		if ea != nil || eb != nil {
			if line == 1 {
				continue
			}
			return fmt.Errorf("line %d: bad number", line)
		}
		if !(a < b) {
			return fmt.Errorf("line %d: need a < b", line)
		}
		items = append(items, Uniform{a, b})
	}
	s.items = items
	LogMessage(fmt.Sprintf("import %d numbers from CSV", len(items)))
	return s.recompute()
}

func (s *Server) exportCSV(w http.ResponseWriter, r *http.Request) {
	s.mu.Lock()
	defer s.mu.Unlock()
	w.Header().Set("Content-Type", "text/csv; charset=utf-8")
	w.Header().Set("Content-Disposition", "attachment; filename=fuzzy_set.csv")
	fmt.Fprintln(w, "id,a,b,A,B,selected,cluster")
	snap := s.snapshot()
	for _, it := range snap.Items {
		cl := ""
		if it.Cluster >= 0 {
			cl = strconv.Itoa(it.Cluster + 1)
		}
		fmt.Fprintf(w, "%d,%.6f,%.6f,%.6f,%.6f,%t,%s\n", it.ID, it.A, it.B, it.Left, it.Right, it.Selected, cl)
	}
	LogMessage(fmt.Sprintf("export %d numbers to CSV", len(snap.Items)))
}

func (s *Server) operate(w http.ResponseWriter, r *http.Request) {
	var req struct {
		X     Uniform `json:"x"`
		Y     Uniform `json:"y"`
		Op    string  `json:"op"`
		Alpha float64 `json:"alpha"`
	}
	if err := decode(r, &req); err != nil {
		writeErr(w, err)
		return
	}
	ops := map[string]int{"add": 0, "sub": 1, "mul": 2, "div": 3}
	op, ok := ops[req.Op]
	if !ok {
		writeErr(w, errors.New("unknown operation"))
		return
	}
	s.mu.Lock()
	res, err := Operate(req.X, req.Y, op, req.Alpha)
	s.mu.Unlock()
	if err != nil {
		writeErr(w, err)
		return
	}
	writeJSON(w, http.StatusOK, res)
}

func (s *Server) journal(w http.ResponseWriter, r *http.Request) {
	data, err := os.ReadFile(s.logPath)
	if err != nil {
		writeErr(w, err)
		return
	}
	const limit = 256 << 10
	if len(data) > limit {
		data = data[len(data)-limit:]
		if i := bytes.IndexByte(data, '\n'); i >= 0 {
			data = data[i+1:]
		}
	}
	w.Header().Set("Content-Type", "text/plain; charset=utf-8")
	w.Write(data)
}
