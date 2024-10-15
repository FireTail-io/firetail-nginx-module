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
import (
	"net/http"
	"strconv"
)

var firetailRequestMiddleware func(next http.Handler) http.Handler
var firetailResponseMiddleware func(next http.Handler) http.Handler

//export ValidateRequestBody
func ValidateRequestBody(
	allowUndefinedRoutes unsafe.Pointer, allowUndefinedRoutesLength C.int,
	bodyCharPtr unsafe.Pointer, bodyLength C.int,
	pathCharPtr unsafe.Pointer, pathLength C.int,
	methodCharPtr unsafe.Pointer, methodLength C.int,
	headersCharPtr unsafe.Pointer, headersLength C.int,
) (C.int, *C.char) {
	log.Println("✅ Validating request body...")
	// Create the middleware if it hasn't already been done
	if firetailRequestMiddleware == nil {
		var err error
		firetailRequestMiddleware, err = firetail.GetMiddleware(&firetail.Options{
			OpenapiSpecPath:          "/etc/nginx/appspec.yml",
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
	}

	// Create a fake handler
	placeholderResponse := []byte{}
	myHandler := &stubHandler{
		responseCode:  200,
		responseBytes: placeholderResponse,
	}

	// Create our middleware instance with the stub handler
	myMiddleware := firetailRequestMiddleware(myHandler)

	// Create a local response writer to record what the middleware says we should respond with
	localResponseWriter := httptest.NewRecorder()

	// Create the go request object we'll pass to the middleware
	mockRequest := httptest.NewRequest(
		string(C.GoBytes(methodCharPtr, methodLength)),
		string(C.GoBytes(pathCharPtr, pathLength)),
		io.NopCloser(bytes.NewBuffer(C.GoBytes(bodyCharPtr, bodyLength))),
	)
	// Add the headers to the mock request
	var headers map[string][]string
	if err := json.Unmarshal(C.GoBytes(headersCharPtr, headersLength), &headers); err != nil {
		panic(err)
	}
	for k, v := range headers {
		// convert value (v) to comma-delimited values. key "k" is still as it is
		mockRequest.Header.Add(k, strings.Join(v[:], ", "))
	}

	// Serve the request to the middlware
	myMiddleware.ServeHTTP(localResponseWriter, mockRequest)

	// Get the response body from the middleware
	middlewareResponseBodyBytes, err := io.ReadAll(localResponseWriter.Body)
	responseCString := C.CString(string(middlewareResponseBodyBytes))
	if err != nil {
		return 1, responseCString // return 1 is error by convention
	}

	// If the body differs after being passed through the middleware then we'll just infer it doesn't match the spec
	if string(middlewareResponseBodyBytes) != string(placeholderResponse) {
		// If allowing undefined routes, then we need to check if the response is a 404 from the middleware. If it is,
		// we return success.
		allowUndefinedRoutesBool, err := strconv.ParseBool(string(C.GoBytes(allowUndefinedRoutes, allowUndefinedRoutesLength)))
		if err != nil {
			return 1, responseCString // return 1 is error by convention
		}
		if allowUndefinedRoutesBool {
			response_json := map[string]interface{}{}
			if err := json.Unmarshal(middlewareResponseBodyBytes, &response_json); err != nil {
				return 1, responseCString // return 1 is error by convention
			}
			if code, ok := response_json["code"]; ok {
				if code_float, ok := code.(float64); ok {
					if code_float == 404 {
						return 0, responseCString // return 0 is success by convention
					}
				}
			}

		}
		return 1, responseCString // return 1 is error by convention
	}

	return 0, responseCString // return 0 is success by convention
}

//export ValidateResponseBody
func ValidateResponseBody(
	urlCharPtr unsafe.Pointer,
	urlLength C.int,
	tokenCharPtr unsafe.Pointer, tokenLength C.int,
	allowUndefinedRoutes unsafe.Pointer, allowUndefinedRoutesLength C.int,
	reqBodyCharPtr unsafe.Pointer, reqBodyLength C.int,
	reqHeadersJsonCharPtr unsafe.Pointer, reqHeadersJsonLength C.int,
	resBodyCharPtr unsafe.Pointer, resBodyLength C.int,
	resHeadersJsonCharPtr unsafe.Pointer, resHeadersJsonLength C.int,
	pathCharPtr unsafe.Pointer, pathLength C.int,
	statusCode C.int,
	methodCharPtr unsafe.Pointer, methodLength C.int,
) (C.int, *C.char) {
	if firetailResponseMiddleware == nil {
		var err error
		firetailResponseMiddleware, err = firetail.GetMiddleware(&firetail.Options{
			OpenapiSpecPath:          "/etc/nginx/appspec.yml",
			LogsApiToken:             strings.TrimSpace(string(C.GoBytes(tokenCharPtr, tokenLength))),
			LogsApiUrl:               strings.TrimSpace(string(C.GoBytes(urlCharPtr, urlLength))),
			DebugErrs:                true,
			EnableRequestValidation:  false,
			EnableResponseValidation: true,
		})
		if err != nil {
			log.Println("Failed to initialise Firetail middleware, err:", err.Error())
			return 0, nil
		}
	}

	// Create a handler returning the response body and status code from nginx
	var responseHeaders map[string]string
	if resHeadersJsonCharPtr != nil {
		if err := json.Unmarshal(C.GoBytes(resHeadersJsonCharPtr, resHeadersJsonLength), &responseHeaders); err != nil {
			panic(err)
		}
	} else {
		responseHeaders = make(map[string]string)
	}
	resBodySlice := C.GoBytes(resBodyCharPtr, resBodyLength)
	myHandler := &stubHandler{
		responseCode:    int(statusCode),
		responseBytes:   resBodySlice,
		responseHeaders: responseHeaders,
	}

	// Create our middleware instance with the stub handler
	myMiddleware := firetailResponseMiddleware(myHandler)

	// Create a local response writer to record what the middleware says we should respond with
	localResponseWriter := httptest.NewRecorder()

	// Create the go request object we'll pass to the middleware
	mockRequest := httptest.NewRequest(
		string(C.GoBytes(methodCharPtr, methodLength)), string(C.GoBytes(pathCharPtr, pathLength)),
		io.NopCloser(bytes.NewBuffer(C.GoBytes(reqBodyCharPtr, reqBodyLength))),
	)
	// Add the headers to the mock request
	if reqHeadersJsonCharPtr != nil {
		var headers map[string][]string
		if err := json.Unmarshal(C.GoBytes(reqHeadersJsonCharPtr, reqHeadersJsonLength), &headers); err != nil {
			panic(err)
		}
		for k, v := range headers {
			// convert value (v) to comma-delimited values. key "k" is still as it is
			mockRequest.Header.Add(k, strings.Join(v[:], ", "))
		}
	}

	// Serve the request to the middlware
	myMiddleware.ServeHTTP(localResponseWriter, mockRequest)

	// for profiling the CPU, uncomment this and run
	// go tool pprof http://localhost:6060/debug/pprof/profile\?seconds\=30
	/*go func() {
		        log.Println(http.ListenAndServe("localhost:6060", nil))
	}() */

	// If the response code or body differs after being passed through the middleware then we'll just infer it doesn't
	// match the spec
	middlewareResponseBodyBytes, err := io.ReadAll(localResponseWriter.Body)
	responseCString := C.CString(string(middlewareResponseBodyBytes))
	if err != nil {
		return 1, responseCString // return 1 is error by convention
	}

	// If the body differs after being passed through the middleware then we'll just infer it doesn't match the spec
	if string(middlewareResponseBodyBytes) != string(resBodySlice) || localResponseWriter.Code != int(statusCode) {
		// If allowing undefined routes, then we need to check if the response is a 404 from the middleware. If it is,
		// we return success.
		allowUndefinedRoutesBool, err := strconv.ParseBool(string(C.GoBytes(allowUndefinedRoutes, allowUndefinedRoutesLength)))
		if err != nil {
			return 1, responseCString // return 1 is error by convention
		}
		if allowUndefinedRoutesBool {
			response_json := map[string]interface{}{}
			if err := json.Unmarshal(middlewareResponseBodyBytes, &response_json); err != nil {
				return 1, responseCString // return 1 is error by convention
			}
			if code, ok := response_json["code"]; ok {
				if code_float, ok := code.(float64); ok {
					if code_float == 404 {
						return 0, C.CString(string(resBodySlice)) // return 0 is success by convention
					}
				}
			}
		}
		return 1, responseCString // return 1 is error by convention
	}

	return 0, C.CString(string(resBodySlice)) // return 0 is success by convention
}

func main() {}
