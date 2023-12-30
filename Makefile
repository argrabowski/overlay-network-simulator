all: overlay

nodes: overlay
	parallel cp ./overlay ::: ./nodes/1/ ./nodes/2/ ./nodes/3/ ./nodes/4/ ./nodes/5/ ./nodes/6/
	parallel cp ./config/config.txt ::: ./nodes/1/ ./nodes/2/ ./nodes/3/ ./nodes/4/ ./nodes/5/ ./nodes/6/

overlay: cpp/overlay.cpp cpp/cs3516sock.h
	g++ -o overlay cpp/overlay.cpp

.PHONY: clean

clean:
	rm -f overlay *.o
