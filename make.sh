# TODO make this a real makefile/cmake file

osr=$(cat /etc/os-release)
if [[ $osr =~ .*ubuntu.* ]] 
then
	# Ubuntu systems (lab) install boost into a different directory
	bfs=/usr/lib/x86_64-linux-gnu/libboost_filesystem.a
	bsys=/usr/lib/x86_64-linux-gnu/libboost_system.a
else
	# If on my local Arch Linux, use the standard directory
	bfs=/usr/lib/libboost_filesystem.a
	bsys=/usr/lib/libboost_system.a
fi

g++ -g -O2 -std=c++14 -Wall -flto *.cpp *.h $bfs $bsys -o acpr
