

echo building Findspot...
echo building PinTool...
echo note: FindSpot directory must be copied to /pin-3.23-xyz/source/tools/
make
echo building cli...
cd findspot-cli
make
cd ..
echo building example...
cd example-1
make
cd ..
echo done!
echo ------------------
echo Usage: run 
echo "     " ../../../pin -t obj-intel64/FindSpot.so -- example-1/example-1
echo in one terminal and 
echo "     " ./findspot-cli/findspot-cli
echo in another!
echo ------------------
