


usage () {
   echo "build-and-copy.sh <dest-host>"
   echo "    dest-host      destination IP or host used directly by scp command"
   exit
}

if [ "$#" -ne 1 ]; then
    usage
fi

reset
docker run -v$(pwd):/home/doppel jdolanacces/enet-builder /bin/bash -c "make clean ; make -C apcilib clean ; make && make -C apcilib"

if [ ! -f apci.ko ]; then
  echo "DRIVER NOT BUILT"
  exit
fi

if [ ! -f apcilib/check_dma ]; then
  echo "CHECK_DMA NOT BUILT"
  exit
fi

scp apci.ko apcilib/check_dma $1: