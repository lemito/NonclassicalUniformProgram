package main

/*
#cgo CFLAGS: -I${SRCDIR}/../fuzzy/include
#cgo LDFLAGS: ${SRCDIR}/../build/libfuzzy.a -lstdc++ -lm
#include <stdlib.h>
#include "fuzzy.h"
*/
import "C"

import (
	"errors"
	"unsafe"
)

const Levels = 21

type Uniform struct {
	A float64 `json:"a"`
	B float64 `json:"b"`
}

type Cuts struct {
	Alphas   []float64 `json:"alphas"`
	Left     []float64 `json:"left"`
	Right    []float64 `json:"right"`
	Centroid float64   `json:"centroid"`
}

func lastError() error {
	msg := C.GoString(C.fz_last_error())
	if msg == "" {
		msg = "fuzzy library error"
	}
	return errors.New(msg)
}

func LogOpen(path string) bool {
	cs := C.CString(path)
	defer C.free(unsafe.Pointer(cs))
	return C.fz_log_open(cs) != 0
}

func LogClose() {
	C.fz_log_close()
}

func LogMessage(msg string) {
	cs := C.CString(msg)
	defer C.free(unsafe.Pointer(cs))
	C.fz_log_message(cs)
}

func Generate(seed uint64, n int, lo, hi, wmin, wmax float64) ([]Uniform, error) {
	s := C.fz_set_generate(C.uint64_t(seed), C.int(n), C.double(lo), C.double(hi), C.double(wmin), C.double(wmax), Levels)
	if s == nil {
		return nil, lastError()
	}
	defer C.fz_set_free(s)
	cnt := int(C.fz_set_size(s))
	out := make([]Uniform, cnt)
	for i := 0; i < cnt; i++ {
		var a, b C.double
		C.fz_set_get(s, C.int(i), &a, &b)
		out[i] = Uniform{float64(a), float64(b)}
	}
	return out, nil
}

func Intervals(items []Uniform, alpha float64) ([]float64, []float64, error) {
	s := C.fz_set_create(Levels)
	if s == nil {
		return nil, nil, lastError()
	}
	defer C.fz_set_free(s)
	for _, u := range items {
		if C.fz_set_add(s, C.double(u.A), C.double(u.B)) < 0 {
			return nil, nil, lastError()
		}
	}
	n := len(items)
	l := make([]float64, n)
	r := make([]float64, n)
	if n > 0 {
		C.fz_set_intervals(s, C.double(alpha), (*C.double)(&l[0]), (*C.double)(&r[0]))
	}
	return l, r, nil
}

func SelectWidth(l, r []float64, wmin, wmax float64) []int {
	return selectWith(l, r, func(lp, rp *C.double, n C.int, idx *C.int) C.int {
		return C.fz_select_width(lp, rp, n, C.double(wmin), C.double(wmax), idx)
	})
}

func SelectWindow(l, r []float64, x1, x2 float64) []int {
	return selectWith(l, r, func(lp, rp *C.double, n C.int, idx *C.int) C.int {
		return C.fz_select_window(lp, rp, n, C.double(x1), C.double(x2), idx)
	})
}

func selectWith(l, r []float64, f func(*C.double, *C.double, C.int, *C.int) C.int) []int {
	n := len(l)
	if n == 0 {
		return []int{}
	}
	idx := make([]C.int, n)
	c := int(f((*C.double)(&l[0]), (*C.double)(&r[0]), C.int(n), &idx[0]))
	out := make([]int, c)
	for i := 0; i < c; i++ {
		out[i] = int(idx[i])
	}
	return out
}

type ClusterInfo struct {
	Left   float64 `json:"left"`
	Right  float64 `json:"right"`
	Count  int     `json:"count"`
	Spread float64 `json:"spread"`
}

func KMeans(l, r []float64, k int, seed uint64) ([]int, int, error) {
	n := len(l)
	if n == 0 {
		return nil, 0, errors.New("no intervals to cluster")
	}
	labels := make([]C.int, n)
	it := C.fz_kmeans((*C.double)(&l[0]), (*C.double)(&r[0]), C.int(n), C.int(k), C.uint64_t(seed), 300, &labels[0], nil, nil, nil)
	if it < 0 {
		return nil, 0, lastError()
	}
	return toInts(labels), int(it), nil
}

func Threshold(l, r []float64, delta float64) ([]int, int, error) {
	n := len(l)
	if n == 0 {
		return nil, 0, errors.New("no intervals to cluster")
	}
	labels := make([]C.int, n)
	k := C.fz_threshold_cluster((*C.double)(&l[0]), (*C.double)(&r[0]), C.int(n), C.double(delta), &labels[0])
	if k < 0 {
		return nil, 0, lastError()
	}
	return toInts(labels), int(k), nil
}

func ClusterStats(l, r []float64, labels []int, k int) []ClusterInfo {
	n := len(l)
	if n == 0 || k == 0 {
		return []ClusterInfo{}
	}
	cl := make([]C.int, n)
	for i, v := range labels {
		cl[i] = C.int(v)
	}
	ccl := make([]float64, k)
	ccr := make([]float64, k)
	cnt := make([]C.int, k)
	sp := make([]float64, k)
	C.fz_cluster_stats((*C.double)(&l[0]), (*C.double)(&r[0]), C.int(n), &cl[0], C.int(k),
		(*C.double)(&ccl[0]), (*C.double)(&ccr[0]), &cnt[0], (*C.double)(&sp[0]))
	out := make([]ClusterInfo, k)
	for c := 0; c < k; c++ {
		out[c] = ClusterInfo{ccl[c], ccr[c], int(cnt[c]), sp[c]}
	}
	return out
}

func toInts(v []C.int) []int {
	out := make([]int, len(v))
	for i, x := range v {
		out[i] = int(x)
	}
	return out
}

func cutsOf(x *C.fz_number) Cuts {
	m := int(C.fz_levels(x))
	c := Cuts{make([]float64, m), make([]float64, m), make([]float64, m), float64(C.fz_centroid(x))}
	C.fz_cuts(x, (*C.double)(&c.Alphas[0]), (*C.double)(&c.Left[0]), (*C.double)(&c.Right[0]))
	return c
}

type OpResult struct {
	X       Cuts       `json:"x"`
	Y       Cuts       `json:"y"`
	Result  *Cuts      `json:"result,omitempty"`
	Error   string     `json:"error,omitempty"`
	Compare int        `json:"compare"`
	PossXY  float64    `json:"possXgeY"`
	PossYX  float64    `json:"possYgeX"`
	CutX    [2]float64 `json:"cutX"`
	CutY    [2]float64 `json:"cutY"`
	CutR    [2]float64 `json:"cutResult"`
}

func Operate(x, y Uniform, op int, alpha float64) (*OpResult, error) {
	fx := C.fz_create(C.double(x.A), C.double(x.B), Levels)
	if fx == nil {
		return nil, lastError()
	}
	defer C.fz_free(fx)
	fy := C.fz_create(C.double(y.A), C.double(y.B), Levels)
	if fy == nil {
		return nil, lastError()
	}
	defer C.fz_free(fy)
	res := &OpResult{X: cutsOf(fx), Y: cutsOf(fy)}
	var pxy, pyx, l, r C.double
	res.Compare = int(C.fz_compare(fx, fy, &pxy))
	C.fz_compare(fy, fx, &pyx)
	res.PossXY = float64(pxy)
	res.PossYX = float64(pyx)
	C.fz_alpha_cut(fx, C.double(alpha), &l, &r)
	res.CutX = [2]float64{float64(l), float64(r)}
	C.fz_alpha_cut(fy, C.double(alpha), &l, &r)
	res.CutY = [2]float64{float64(l), float64(r)}
	z := C.fz_apply(C.int(op), fx, fy)
	if z == nil {
		res.Error = lastError().Error()
		return res, nil
	}
	defer C.fz_free(z)
	C.fz_alpha_cut(z, C.double(alpha), &l, &r)
	res.CutR = [2]float64{float64(l), float64(r)}
	c := cutsOf(z)
	res.Result = &c
	return res, nil
}

func Version() string {
	return C.GoString(C.fz_version())
}
