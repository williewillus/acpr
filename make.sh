# TODO make this a real makefile/cmake file

osr=$(cat /etc/os-release)
if [[ $osr =~ .*ubuntu.* ]] 
then
	# Lab computers do not install the necessary boost runtime libraries, so I ship them in this folder
	bfs=./libboost_filesystem.a
	bsys=./libboost_system.a
else
	# If on my local machine, use proper libraries
	bfs=/usr/lib/libboost_filesystem.a
	bsys=/usr/lib/libboost_system.a
fi

g++ -g -O2 -std=c++14 -Wall -flto *.cpp *.h $bfs $bsys -o acpr
