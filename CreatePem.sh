#!/bin/sh

if [ "$1" = "" ]; then
	echo CreatePem.sh file.pem
	exit 1
fi

log () {
	echo [`date`] $*
}

log Creating Random File for key seed
dd if=/dev/urandom of=blah-1234.txt bs=1024k count=10 >/dev/null 2>&1

openssl genrsa -rand blah-1234.txt -out ${1}.key
openssl req -new -key ${1}.key -out ${1}.csr
openssl x509 -req -days 365 -in ${1}.csr -signkey ${1}.key -out ${1}.crt

log Cleaning up
rm -f blah-1234.txt ${1}.csr
log done

cat ${1}.key > $1
cat ${1}.crt >> $1

rm ${1}.key ${1}.crt

echo "Created $1"
