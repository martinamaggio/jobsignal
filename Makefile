all: clean compile

compile: clean
	cp build/autobuild.sh .
	./autobuild.sh
	rm autobuild.sh

clean:
	rm -rf bin
