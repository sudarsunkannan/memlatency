all:lat

lat:lat.cpp
	g++ -O0  lat.cpp -o lat -lrt

clean:
	rm -rf lat

