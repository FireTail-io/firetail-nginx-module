package main

import (
	"log"
	"net/http"
)

type stubHandler struct {
	responseCode  int
	responseBytes []byte
}

func (m *stubHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	w.WriteHeader(m.responseCode)
	w.Header().Add("Content-Type", "application/json")
	_, err := w.Write(m.responseBytes)
	if err != nil {
		log.Println("Mock backend failed to write bytes to ResponseWriter, err:", err.Error())
	}
}
