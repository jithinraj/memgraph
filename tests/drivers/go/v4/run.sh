#!/bin/bash -e

# check if go is installed
for i in go; do
  if ! which $i >/dev/null; then
    echo "Please install $i!"
    exit 1
  fi
done

go get github.com/neo4j/neo4j-go-driver/neo4j

go run basic.go
go run transactions.go