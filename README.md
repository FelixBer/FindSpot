# FindSpot: A Pintool to locate code for specific functionality in binaries using DBI


FindSpot is a Pintool that uses the Intel PIN DBI framework to instrument a binary with the intent to locate
the binary code functions that correspond to a specific functionality of the binary (for example clicking a certain button).





## Concept

>>>TODO


https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-dynamic-binary-instrumentation-tool.html





## Building and Installation


1. Download and extract the Intel Pin Framework 3.23: https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-binary-instrumentation-tool-downloads.html
2. Copy the entire FindSpot folder from this repository into %PINDIR%/source/tools/
3. On Linux: cd to %PINDIR%/source/tools/FindSpot and type 'make'
4. On Windows: open %PINDIR%/source/tools/FindSpot.vcxproj in Visual Studio (tested with VS2019) and hit build
5. The output is a .so/.dll file located in a sub directory. No further installation is required.


## Basic Usage

The process consists of two easy steps, starting pin and attaching a debugger to it.

### Linux

1. cd to %PINDIR%/source/tools/FindSpot
2. do '../../../pin -appdebug -t obj-intel64/FindSpot.so -o findspot.log -- ./targetapp.out' where targetapp.out is the target application
3. you will get this output, with a unique port number:
    Application stopped until continued from debugger.
    Start GDB, then issue this command at the (gdb) prompt:
      target remote :12345
4. in a second terminal, type 'gdb targetapp.out' to start gdb
5. then attach to the pintool with 'target remote :12345' inside gdb
6. if everything went well, you are now attached to the pin-instrumented process and can issue commands (see commands) or continue execution with 'continue'


### Windows

TODO
Currently not supported. Soon!



## Simple Example


    void r1() { std::cout << "wow!"; }
    int main()
    {
        while(1)
        {
            int choice = 0;
            std::cout << "choose: ";
            std::cin.clear();
            std::cin >> choice;
            std::cin.clear();
            if(choice == 5)
                r1();
        }
    }


Above program loops forever, prompting for a number. If the user enters 5, the function r1() is executed, which does something special!
Our task: How can we find which function is executed when we enter 5, without looking at the code or disassembly?


>>>TODO



## Advanced Example

We will now demonstrate a more complex example: Finding the function executed when the "bold" ToolBox button is pressed in Libre Office.

>>>TODO



## Available Commands

Note that all FindSpot commands must be prefixed with "monitor ", as per PIN convention.
Hence the full command to enter in gdb to start collection mode is "monitor mode collect".

    gdb>  monitor help
    help          -- print this help.
    clear         -- clear all collected data.
    show          -- show stats on collected data.
    dump <file>   -- dump current data to file.
    mode collect  -- collect all functions called from now on.
    mode trim     -- remove all functions called from now on.
    mode off      -- dont touch collected data.
    mode          -- show current mode.
    sort chrono   -- sort output in the order the functions were encountered.
    sort hitcount -- sort output by number of times the functions were encountered.
    mod           -- display white/blacklist.
    mod blacklist <mod> -- add module to blacklist.
    mod whitelist <mod> -- add module to whitelist.
    mod blacklist remove <mod> -- remove module from blacklist.
    mod whitelist remove <mod> -- remove module from whitelist.



## Todo

* fix windows support
* attach/detach
* thread whitelist/blacklist

