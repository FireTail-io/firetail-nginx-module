package main

import (
	"log"
	"net/http"
)

type stubHandler struct {
	responseCode    int
	responseBytes   []byte
	responseHeaders map[string]string
}

func (m *stubHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	w.WriteHeader(m.responseCode)
	for k, v := range m.responseHeaders {
		w.Header().Add(k, v)
	}
	_, err := w.Write(m.responseBytes)
	if err != nil {
		log.Println("Mock backend failed to write bytes to ResponseWriter, err:", err.Error())
	}
}
