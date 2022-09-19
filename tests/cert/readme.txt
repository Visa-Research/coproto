
See the genCert.sh for an automated script.

=====================================
Making a normal key pair
=====================================

1) Create a client SK: 
   openssl genrsa -out client-0.key.pem 2048

2) View the client SK:
    openssl rsa -in client-0.key.pem -noout -text    

=====================================
Option 1, creating a CSR (w/o .cnf)
=====================================
1) Create a CSR:
	openssl req -new -key ./client-0.key.pem -out client-0.csr.pem

Let i = 0,1,2,..., then make sure that the CommonName has one of the following forms:
	client-i
	server-i
	broker

2) View the CSR:
	openssl req -in client-0.csr.pem -noout -text


=====================================
Option 2, creating a CSR (w/ .cnf)
=====================================
1) Copy the included csr.cnf file to temp.cnf. This has some defaults set in it.
You will need to add "CN=myCommonName" to then end of temp.cnf. Let i = 0,1,2,..., 
then make sure that the myCommonName has one of the following forms:
	client-i
	server-i
	broker
For exmaple, add "CN=client-2134" but without the quotes.

2) To create a CSR with the provided config:
	openssl req -new -config temp.cnf -out client-0.csr.pem -key client-0.key.pem



3) View the CSR:
	openssl req -in client-0.csr.pem -noout -text

=====================================
Cearting a CA key pair
=====================================
1) run:
	mkdir private
	mkdir cert

1) Create a new key pair:
	openssl genrsa -out private/ca.key.pem 2048

2) Create self signed certificate for the CA:
	openssl req -new -x509 -key private/ca.key.pem -out cert/ca.cert.pem

The fields can be filled out as follows:
	Country Name (2 letter code) [AU]:US
	State or Province Name (full name) [Some-State]:California
	Locality Name (eg, city) []:Palo Alto
	Organization Name (eg, company) [Internet Widgits Pty Ltd]:coproto
	Organizational Unit Name (eg, section) []:coproto
	Common Name (e.g. server FQDN or YOUR name) []:coproto-CA


=====================================
Option 1, Creating a Certificate (w/o .cnf)
=====================================
0) WARNING, this will not include the expentions that are
specified in the temp.cnf file.

1) sign a CSR with:
	openssl x509 -req -in client-0.csr.pem -CA cert/ca.cert.pem -CAkey private/ca.key.pem -CAcreateserial -out client-0.cert.pem -days 9999

2) Note that this created the serial number file cert/ca.cert.srl

3) Note that the expiration date on client-0.cert.pem is in 9999 days.

4) View the cert:
	openssl x509 -in client-0.cert.pem -text -noout


=====================================
Option 2, Creating a Certificate (w/ .cnf)
=====================================
0) Note the ca.cnf file. This has some defaults set in it. 
Secondly note the ca.extension.cnf. This has the extensions 
that the CA will actually apply. The CSR has a section for extensions
but these are ignored. You must manually copy any speficied extension
from the CSR into the ca.extension.cnf file. 

1) Run the following:
	mkdir ./newcerts
	touch index.txt
	echo '01' > serial

2) Sign a CSR with:
	openssl ca -config ca.cnf -extfile ca.extension.cnf -out client-0.cert.pem -infiles client-0.csr.pem
