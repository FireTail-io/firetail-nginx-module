package main

import (
	"net/http"
	"C"
	"unsafe"
	"bytes"
	"log"
	_ "io"
)

//export HttpClient
func HttpClient(body unsafe.Pointer, bodyLength C.int,
                apiToken unsafe.Pointer, apiTokenLength C.int) C.int {
	log.Println("HttpClient is running!")

        bodySlice := C.GoBytes(body, bodyLength)
	apiTokenSlice := C.GoBytes(apiToken, apiTokenLength)
	buf := []byte(string(bodySlice))

        req, err := http.NewRequest("POST", "https://api.logging.eu-west-1.prod.firetail.app/logs/bulk", bytes.NewBuffer(buf))
	if err != nil {
          panic(err)
	}

	req.Header.Add("Content-Type", "application/nd-json")
	req.Header.Add("x-ft-api-key", string(apiTokenSlice))

	go func() {
	        client := &http.Client{}
		res, err := client.Do(req)

                client.Do(req)
                if err != nil {
                        panic(err)
                }
                defer res.Body.Close() 

		/* debug code
                log.Println("HTTP status code: %d", res.StatusCode)

                b, err := io.ReadAll(res.Body)
                if err != nil {
                        log.Println("Error reading response body: %s", err)
                }

                log.Println("HTTP response body: %s", string(b))
		*/
	}() 
	return 0;
}

func main() {}
