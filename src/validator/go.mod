module firetail-validator

go 1.20

require github.com/FireTail-io/firetail-go-lib v0.0.0

require (
	github.com/getkin/kin-openapi v0.110.0 // indirect
	github.com/go-openapi/jsonpointer v0.19.5 // indirect
	github.com/go-openapi/swag v0.22.3 // indirect
	github.com/gorilla/mux v1.8.0 // indirect
	github.com/invopop/yaml v0.2.0 // indirect
	github.com/josharian/intern v1.0.0 // indirect
	github.com/mailru/easyjson v0.7.7 // indirect
	github.com/mohae/deepcopy v0.0.0-20170929034955-c48cc78d4826 // indirect
	gopkg.in/yaml.v3 v3.0.1 // indirect
)

//replace github.com/FireTail-io/firetail-go-lib => github.com/FireTail-io/firetail-go-lib v0.1.0
replace github.com/FireTail-io/firetail-go-lib => github.com/muhammadn/firetail-go-lib v0.0.0-20240223085919-e878127c5c1c
