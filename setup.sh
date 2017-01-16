

echo script starting..

pkill -f tcpnode

./tcpnode 23220 itchy 23221 &
./tcpnode 23221 itchy 23222 &
./tcpnode 23222 itchy 23223 &
./tcpnode 23223 itchy 23224 &
./tcpnode 23224 itchy 23225 &
./tcpnode 23225 itchy 23226 &
./tcpnode 23226 itchy 23227 &
./tcpnode 23227 itchy 23228 &
./tcpnode 23228 itchy 23229 &
./tcpnode 23229 itchy 23230 &
./tcpnode 23230 itchy 23231 &
./tcpnode 23231 itchy 23220 &

