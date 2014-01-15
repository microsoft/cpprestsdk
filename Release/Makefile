MODE = $(shell getconf LONG_BIT)

debug:
	mkdir -vp build.debug$(MODE)
	cd build.debug$(MODE) && CXX=g++-4.8 cmake .. -DCMAKE_BUILD_TYPE=Debug && cmake --build .

release:
	mkdir -vp build.release$(MODE)
	cd build.release$(MODE) && CXX=g++-4.8 cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build .

clean:
	@[ -e src ] # sanity check directory
	@find . -iname *.so -exec rm '{}' \;
	@find . -iname *.o -exec rm '{}' \;
	@find . -iname *.d -exec rm '{}' \;
	@find ../Binaries -iname *.so -exec rm '{}' \;
	@find ../Binaries -iname *.txt -exec rm '{}' \;
	@find ../Binaries -iname *.d -exec rm '{}' \;
	@find ../Binaries -iname SearchFile -exec rm '{}' \;
	@find ../Binaries -iname BingRequest -exec rm '{}' \;
	@find ../Binaries -iname syncdir -exec rm '{}' \;
	@find ../Binaries -iname test_runner -exec rm '{}' \;
	@rm -rf build.debug* build.release*


all: debug release

.PHONY: all clean release debug
