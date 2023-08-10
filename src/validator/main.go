package main

import (
	"C"
	"log"
	"net/http"
	"unsafe"

	firetail "github.com/FireTail-io/firetail-go-lib/middlewares/http"
)

// Will hold our Firetail middleware in Go memory
var firetailMiddleware func(next http.Handler) http.Handler

// Creates our middleware instance with the provided OpenAPI spec and Firetail API key
//
//export CreateMiddleware
func CreateMiddleware(specLocationBytes unsafe.Pointer, specLocationLength C.int, apiTokenBytes unsafe.Pointer, apiTokenLength C.int) C.int {
	log.Println("Loading OpenAPI spec in Go...")

	specLocationSlice := C.GoBytes(specLocationBytes, specLocationLength)
	specLocation := string(specLocationSlice)
	log.Println("OpenAPI spec location:", specLocation)

	apiTokenSlice := C.GoBytes(apiTokenBytes, apiTokenLength)
	apiToken := string(apiTokenSlice)
	log.Println("Received API token of length", len(apiToken))

	var err error
	firetailMiddleware, err = firetail.GetMiddleware(&firetail.Options{
		OpenapiSpecPath:          specLocation,
		LogsApiToken:             apiToken,
		LogsApiUrl:               "",
		DebugErrs:                true,
		EnableRequestValidation:  true,
		EnableResponseValidation: true,
	})
	if err != nil {
		log.Println("Failed to initialise Firetail middleware, err:", err.Error())
		return 0
	}

	return 1
}

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
