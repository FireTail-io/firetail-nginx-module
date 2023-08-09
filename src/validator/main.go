package main

import (
	"C"
	"log"
	"unsafe"
)

//export ValidateRequestBody
func ValidateRequestBody(bodyCharPtr unsafe.Pointer, bodyLength C.int) C.int {
	log.Println("Hello from Go's ValidateRequestBody!")
	slice := C.GoBytes(bodyCharPtr, bodyLength)
	bodyString := string(slice)
	log.Println("Request body length:", bodyLength)
	log.Println("Request body in Go:", bodyString)
	return 1
}

//export ValidateResponseBody
func ValidateResponseBody(bodyCharPtr unsafe.Pointer, bodyLength C.int) C.int {
	log.Println("Hello from Go's ValidateResponseBody!")
	slice := C.GoBytes(bodyCharPtr, bodyLength)
	bodyString := string(slice)
	log.Println("Response body length:", bodyLength)
	log.Println("Response body in Go:", bodyString)
	return 1
}

//export ValidateRequestHeaders
func ValidateRequestHeaders(headersCharPtr unsafe.Pointer, headersLength C.int) C.int {
	log.Println("Hello from Go's ValidateRequestHeaders!")
	slice := C.GoBytes(headersCharPtr, headersLength)
	headersString := string(slice)
	log.Println("Request headers length:", headersLength)
	log.Println("Request headers in Go:", headersString)
	return 1
}

//export ValidateResponseHeaders
func ValidateResponseHeaders(headersCharPtr unsafe.Pointer, headersLength C.int) C.int {
	log.Println("Hello from Go's ValidateResponseHeaders!")
	slice := C.GoBytes(headersCharPtr, headersLength)
	headersString := string(slice)
	log.Println("Response headers length:", headersLength)
	log.Println("Response headers in Go:", headersString)
	return 1
}

func main() {}
