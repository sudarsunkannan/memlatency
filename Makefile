all:lat

lat:lat.cpp
	g++ -O0 -fno-strict-overflow -Werror lat.cpp -o lat -lrt

clean:
	rm -rf lat

