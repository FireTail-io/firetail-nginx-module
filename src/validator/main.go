package main

import (
	"C"
	"encoding/json"
	"log"
	"strings"
	"unsafe"

	"bytes"
	"io"
	"net/http/httptest"
	_ "net/http/pprof"

	firetail "github.com/FireTail-io/firetail-go-lib/middlewares/http"
)

//export ValidateRequestBody
func ValidateRequestBody(specBytes unsafe.Pointer, specLength C.int,
	bodyCharPtr unsafe.Pointer, bodyLength C.int,
	pathCharPtr unsafe.Pointer, pathLength C.int,
	methodCharPtr unsafe.Pointer, methodLength C.int,
	headersCharPtr unsafe.Pointer, headersLength C.int) (C.int, *C.char) {

	specSlice := C.GoBytes(specBytes, specLength)

	firetailMiddleware, err := firetail.GetMiddleware(&firetail.Options{
		OpenapiBytes:             specSlice,
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
	headersSlice := C.GoBytes(headersCharPtr, headersLength)

	var headers map[string][]string
	if err := json.Unmarshal(headersSlice, &headers); err != nil {
		panic(err)
	}

	// Create a fake handler
	placeholderResponse := []byte{}
	myHandler := &stubHandler{
		responseCode:  200,
		responseBytes: placeholderResponse,
	}

	// Create our middleware instance with the stub handler
	myMiddleware := firetailMiddleware(myHandler)

	// Create a local response writer to record what the middleware says we should respond with
	localResponseWriter := httptest.NewRecorder()

	mockRequest := httptest.NewRequest(
		string(methodSlice), string(pathSlice),
		io.NopCloser(bytes.NewBuffer(bodySlice)))

	for k, v := range headers {
		// convert value (v) to comma-delimited values
		// key "k" is still as it is
		mockRequest.Header.Add(k, strings.Join(v[:], ", "))
	}

	// Serve the request to the middlware
	myMiddleware.ServeHTTP(localResponseWriter, mockRequest)

	// If the body differs after being passed through the middleware then we'll just infer it doesn't
	// match the spec
	middlewareResponseBodyBytes, err := io.ReadAll(localResponseWriter.Body)
	response := C.CString(string(middlewareResponseBodyBytes))

	if err != nil {
		log.Println("Failed to read request body bytes from middleware, err:", err.Error())
		// return 1 is error by convention
		return 1, response
	}
	if string(middlewareResponseBodyBytes) != string(placeholderResponse) {
		log.Printf("Middleware altered response body, original: %s, new: %s", string(placeholderResponse), string(middlewareResponseBodyBytes))
		// return 1 is error by convention
		return 1, response
	}

	// return 0 is success by convention
	return 0, response
}

//export ValidateResponseBody
func ValidateResponseBody(urlCharPtr unsafe.Pointer,
        urlLength C.int,
	reqBodyCharPtr unsafe.Pointer, reqBodyLength C.int,
        tokenCharPtr unsafe.Pointer, tokenLength C.int,
        specBytes unsafe.Pointer, specLength C.int,
	bodyCharPtr unsafe.Pointer, bodyLength C.int,
	pathCharPtr unsafe.Pointer, pathLength C.int,
	statusCode C.int,
	methodCharPtr unsafe.Pointer, methodLength C.int) (C.int, *C.char) {

	specSlice := C.GoBytes(specBytes, specLength)
	tokenSlice := C.GoBytes(tokenCharPtr, tokenLength)
	urlSlice := C.GoBytes(urlCharPtr, urlLength)

	log.Println("URL: ", string(urlSlice))

	firetailMiddleware, err := firetail.GetMiddleware(&firetail.Options{
		OpenapiBytes:             specSlice,
		LogsApiToken:             string(tokenSlice),
		LogsApiUrl:               string(urlSlice),
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
		io.NopCloser(bytes.NewBuffer(C.GoBytes(reqBodyCharPtr, reqBodyLength))),
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

func main() {}
