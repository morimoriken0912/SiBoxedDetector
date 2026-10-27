cd ../build/
make -j$(nproc)
./exampleB1 run.mac 10000
cd ../src/
g++ -O2 makeSiViewer.cc -o makeSiViewer $(root-config --cflags --libs)
./makeSiViewer ../build/SiDetector.root SiViewer.html
explorer.exe SiViewer.html
