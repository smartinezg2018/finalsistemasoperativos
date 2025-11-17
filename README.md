Comandos para encriptar y desencriptar
./aes_tool -e -i archivosPrueba/Parcial1SinfoMapaProcesos.txt -o archivosPrueba/Parcial1SinfoMapaProcesos.enc -k AES/keyfile.txt
./aes_tool -u -i archivosPrueba/Parcial1SinfoMapaProcesos.enc -o archivosPrueba/Parcial1SinfoMapaProcesos.dec -k AES/keyfile.txt

./aes_tool -i archivo.txt -o archivo.enc -t 4
