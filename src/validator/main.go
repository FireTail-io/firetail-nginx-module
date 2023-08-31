package main

import (
	"C"
	"log"
	"net/http"
	"unsafe"

	"bytes"
	"io"
	"net/http/httptest"

	firetail "github.com/FireTail-io/firetail-go-lib/middlewares/http"
)

// Will hold our Firetail middleware in Go memory
var firetailMiddleware func(next http.Handler) http.Handler

//export DemoPrint
func DemoPrint(specLocationBytes unsafe.Pointer, specLocationLength C.int) C.int {
        log.Println("Running demoprint")

        specLocationSlice := C.GoBytes(specLocationBytes, specLocationLength)
        specLocation := string(specLocationSlice)
        log.Println("spec data:", specLocation)

	return 1
}

// Creates our middleware instance with the provided OpenAPI spec and Firetail API key
//
//export CreateMiddleware
func CreateMiddleware(specLocationBytes unsafe.Pointer, specLocationLength C.int) C.int {
	log.Println("Loading OpenAPI spec in Go...")

	specLocationSlice := C.GoBytes(specLocationBytes, specLocationLength)
	specLocation := string(specLocationSlice)
	log.Println("OpenAPI spec location:", specLocation)

	var err error
	firetailMiddleware, err = firetail.GetMiddleware(&firetail.Options{
		OpenapiSpecPath:          specLocation,
		LogsApiToken:             "",
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
func ValidateResponseBody(specLocationBytes unsafe.Pointer, specLocationLength C.int, bodyCharPtr unsafe.Pointer, bodyLength C.int, pathCharPtr unsafe.Pointer, pathLength C.int, statusCode C.int) (C.int, *C.char) {
        log.Println("Running ValidResponseBody...")

        specLocationSlice := C.GoBytes(specLocationBytes, specLocationLength)
        specLocation := string(specLocationSlice)

        var err error
        firetailMiddleware, err = firetail.GetMiddleware(&firetail.Options{
                OpenapiSpecData:          specLocation,
                LogsApiToken:             "",
                LogsApiUrl:               "",
                DebugErrs:                true,
                EnableRequestValidation:  true,
                EnableResponseValidation: true,
        })
        if err != nil {
                log.Println("Failed to initialise Firetail middleware, err:", err.Error())
                return 0, nil
        }

	bodySlice := C.GoBytes(bodyCharPtr, bodyLength)
	pathSlice := C.GoBytes(pathCharPtr, pathLength)

	// Create a handler returning the response body and status code from nginx
	myHandler := &stubHandler{
		responseCode:  int(statusCode),
		responseBytes: bodySlice,
	}

	// Create our middleware instance with the stub handler
	myMiddleware := firetailMiddleware(myHandler)

	// Create a local response writer to record what the middleware says we should respond with
	localResponseWriter := httptest.NewRecorder()

	// Serve the request to the middlware
	myMiddleware.ServeHTTP(localResponseWriter, httptest.NewRequest(
		"GET", string(pathSlice),
		io.NopCloser(bytes.NewBuffer([]byte{})),
	))

	// If the response code or body differs after being passed through the middleware then we'll just infer it doesn't
	// match the spec
	middlewareResponseBodyBytes, err := io.ReadAll(localResponseWriter.Body)
	response := C.CString(string(middlewareResponseBodyBytes))
	if err != nil {
		log.Println("Failed to read response body bytes from middleware, err:", err.Error())
		return 0, response
	}
	if localResponseWriter.Code != int(statusCode) {
		log.Printf("Middleware altered status code from %d to %d", statusCode, localResponseWriter.Code)
		return 0, response
	}
	if string(middlewareResponseBodyBytes) != string(bodySlice) {
		log.Printf("Middleware altered response body, original: %s, new: %s", string(bodySlice), string(middlewareResponseBodyBytes))
		return 0, response
	}
	return 1, response
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
