package main

import (
	"C"
	"log"
	"net/http"
	"unsafe"

	"bytes"
	"io"
	"net/http/httptest"
	_ "net/http/pprof"
	"strings"

	firetail "github.com/FireTail-io/firetail-go-lib/middlewares/http"
)

// Will hold our Firetail middleware in Go memory
var firetailMiddleware func(next http.Handler) http.Handler

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
		EnableRequestValidation:  false,
		EnableResponseValidation: false,
	})
	if err != nil {
		log.Println("Failed to initialise Firetail middleware, err:", err.Error())
	        // return 1 is error by convention
		return 1
	}

	// return 0 is success by convention
	return 0
}

//export ValidateRequestBody
func ValidateRequestBody(specBytes unsafe.Pointer, specLength C.int,
	bodyCharPtr unsafe.Pointer, bodyLength C.int,
	pathCharPtr unsafe.Pointer, pathLength C.int,
        methodCharPtr unsafe.Pointer, methodLength C.int) (C.int, *C.char) {

	// remove this later
	log.Println("Hello from Go's ValidateRequestBody!")

	specSlice := C.GoBytes(specBytes, specLength)
	spec := string(specSlice)

	var err error
	firetailMiddleware, err = firetail.GetMiddleware(&firetail.Options{
		OpenapiSpecData:          spec,
		LogsApiToken:             "",
		LogsApiUrl:               "",
		DebugErrs:                true,
		EnableRequestValidation:  true,
		EnableResponseValidation: false,
	})

	if err != nil {
		log.Println("Failed to initialise Firetail middleware, err:", err.Error())
		// return 1 is error by convention
		return 1, nil
	}

	pathSlice := C.GoBytes(pathCharPtr, pathLength)
	bodySlice := C.GoBytes(bodyCharPtr, bodyLength)
	methodSlice := C.GoBytes(methodCharPtr, methodLength)

	// Create a fake handler
	myHandler := &stubHandler{
		responseCode:  200,
		responseBytes: []byte{},
	}

	// Create our middleware instance with the stub handler
	myMiddleware := firetailMiddleware(myHandler)

	// Create a local response writer to record what the middleware says we should respond with
	localResponseWriter := httptest.NewRecorder()

	// Serve the request to the middlware
	myMiddleware.ServeHTTP(localResponseWriter, httptest.NewRequest(
		string(methodSlice), string(pathSlice),
		io.NopCloser(bytes.NewBuffer(bodySlice)),
	))

	// If the body differs after being passed through the middleware then we'll just infer it doesn't
	// match the spec
	//middlewareRequestBodyBytes, err := io.ReadAll(localResponseWriter.Body)
	localRequest := strings.NewReader(string(bodySlice))
	middlewareRequestBodyBytes, err := io.ReadAll(localRequest)

	request := C.CString(string(middlewareRequestBodyBytes))

	// just logging, can remove later
	bodyString := string(bodySlice)
	log.Println("Request body length:", bodyLength)
	log.Println("Request body in Go:", bodyString)

	if err != nil {
		log.Println("Failed to read request body bytes from middleware, err:", err.Error())
		// return 1 is error by convention
		return 1, request
	}

	if string(middlewareRequestBodyBytes) != string(bodySlice) {
		log.Printf("Middleware altered request body, original: %s, new: %s", string(bodySlice), string(middlewareRequestBodyBytes))
	        // return 1 is error by convention
		return 1, request
	}

        // return 0 is success by convention
	return 0, request
}

//export ValidateResponseBody
func ValidateResponseBody(specBytes unsafe.Pointer, specLength C.int,
	bodyCharPtr unsafe.Pointer, bodyLength C.int,
	pathCharPtr unsafe.Pointer, pathLength C.int,
	statusCode C.int,
	methodCharPtr unsafe.Pointer, methodLength C.int) (C.int, *C.char) {

	log.Println("Running ValidateResponseBody...")

	specSlice := C.GoBytes(specBytes, specLength)
	spec := string(specSlice)

	var err error
	firetailMiddleware, err = firetail.GetMiddleware(&firetail.Options{
		OpenapiSpecData:          spec,
		LogsApiToken:             "",
		LogsApiUrl:               "",
		DebugErrs:                true,
		EnableRequestValidation:  false,
		EnableResponseValidation: true,
	})

	if err != nil {
		log.Println("Failed to initialise Firetail middleware, err:", err.Error())
		return 0, nil
	}

	bodySlice := C.GoBytes(bodyCharPtr, bodyLength)
	pathSlice := C.GoBytes(pathCharPtr, pathLength)
	methodSlice := C.GoBytes(methodCharPtr, methodLength)

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
		string(methodSlice), string(pathSlice),
                io.NopCloser(bytes.NewBuffer([]byte{})),
	))

	// for profiling the CPU, uncomment this and run
	// go tool pprof http://localhost:6060/debug/pprof/profile\?seconds\=30
	/* go func() {
		        log.Println(http.ListenAndServe("localhost:6060", nil))
	   }() */

	// If the response code or body differs after being passed through the middleware then we'll just infer it doesn't
	// match the spec
	middlewareResponseBodyBytes, err := io.ReadAll(localResponseWriter.Body)
	response := C.CString(string(middlewareResponseBodyBytes))

	if err != nil {
		log.Println("Failed to read response body bytes from middleware, err:", err.Error())
	        // return 1 is error by convention
		return 1, response
	}
	if localResponseWriter.Code != int(statusCode) {
		log.Printf("Middleware altered status code from %d to %d", statusCode, localResponseWriter.Code)
	        // return 1 is error by convention
		return 1, response
	}
	if string(middlewareResponseBodyBytes) != string(bodySlice) {
		log.Printf("Middleware altered response body, original: %s, new: %s", string(bodySlice), string(middlewareResponseBodyBytes))
	        // return 1 is error by convention
		return 1, response
	}
        // return 0 is success by convention
	return 0, response
}

//export ValidateRequestHeaders
func ValidateRequestHeaders(headersCharPtr unsafe.Pointer, headersLength C.int) C.int {
	log.Println("Hello from Go's ValidateRequestHeaders!")
	slice := C.GoBytes(headersCharPtr, headersLength)
	headersString := string(slice)
	log.Println("Request headers length:", headersLength)
	log.Println("Request headers in Go:", headersString)
        // return 0 is success by convention
	return 0
}

//export ValidateResponseHeaders
func ValidateResponseHeaders(headersCharPtr unsafe.Pointer, headersLength C.int) C.int {
	log.Println("Hello from Go's ValidateResponseHeaders!")
	slice := C.GoBytes(headersCharPtr, headersLength)
	headersString := string(slice)
	log.Println("Response headers length:", headersLength)
	log.Println("Response headers in Go:", headersString)
        // return 0 is success by convention
	return 0
}

func main() {}
