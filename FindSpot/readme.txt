FindSpot
~~~~~~~~

For more information, see https://github.com/FelixBer/FindSpot/


1. Download Intel Pin here: https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-binary-instrumentation-tool-downloads.html
FindSpot has been tested with version 3.23.

2. run
	 pin -t FindSpot.dll -- example-1.exe
in one terminal and 
	findspot-cli.exe
in another.

3. If everything went well you should see something like this in the second terminal:
	findspot-cli.exe>
	using port 8022
	hello from findspot 1.0
	target frozen, type unfreeze to continue execution
	findspot>unfreeze
	target resumed
	findspot>help
	...


Type "help" to see a list of available commands and options.

