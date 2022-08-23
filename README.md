# FindSpot: A Pintool to locate code for specific functionality in binaries using DBI


FindSpot is a [Pintool](https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-dynamic-binary-instrumentation-tool.html
) that uses the Intel PIN DBI framework to instrument a binary with the intent to locate
the binary code functions that correspond to a specific functionality of the binary (for example clicking on a specific button).





## Basic Usage

The process consists of two easy steps, starting pin and attaching the findspot-cli controller to it. Once attached the target is suspended. It can be continued with unfreeze command.

Once the application is running, FindSpot can be switched into collection mode (see above).
The goal is now to capture the behavior of interest (button click, menu click, ...) and as little
as possible "noise". Right after switching into collection mode, return to the application and
perform the intended action. The binary code executed as a consequence is now logged in FindSpot, but so is lots of noise.


Immediately switch FindSpot into trim mode (see above).
We will now chizzle away at that noise. Any code executed now will be *removed* from the internal log. Take your time and perform various actions in the application, **except** the one of interest.

The current state of the log can be inspected with the `show` command.
Continue removing noise until candidates for the code of interest are reduced sufficiently.


**Hint**: The log can be sorted by hit-count and chronologically. See help.

**Hint**: It might be useful to perform the action of interest X times and then look for code executed X times.



## Available Commands

    findspot>help
    help          -- print this help.
    kill          -- kill program (not available with -appdebug).
    clear         -- clear all collected data.
    freeze        -- freeze target program (all threads).
    unfreeze      -- unfreeze target program.
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

### Modes

FindSpot has three modes:

* collect

    This mode records any binary functions executed by the target into an internal list.

* trim

    This mode *removes* any binary functions executed by the target from the internal list.

* off (default)

   This mode doesnt touch the internal list at all.



## Simple Example


    void r1() { std::cout << "wow!"; }
    int main()
    {
        while(1)
        {
            std::cout << "choose: ";
            int choice = 0;
            std::cin >> choice;
            if(choice == 5)
                r1();
            r2();
        }
    }


Above program loops forever, prompting for a number. If the user enters 5, the function `r1()` is executed, which does something special!
Our task: How can we find which function is executed when we enter `5`, without looking at the code or disassembly?


1. Download and extract the Intel Pin Framework 3.23: https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-binary-instrumentation-tool-downloads.html

2. Download a FindSpot binary release and extract it.

3. Navigate to the pin folder and in one terminal start FindSpot: `pin -t FindSpot.so -d debug.log -- ./example-1`

    Note: `-t` takes the full path to the FindSpot pin tool (FindSpot.dll on Windows).

4. In a second terminal, start the FindSpot controller `./findspot-cli` (findspot-cli.exe on Windows)

5. The controller should attach to the pin tool on the default port automatically:

        using port 8022
        hello from findspot 1.0
        target frozen, type unfreeze to continue execution
    Note: 8022 is the hardcoded default port. This can be changed using `-p <port>`.

6. The target program has been frozen close to the entry point. Switch mode now or continue execution with `unfreeze`.

        findspot>unfreeze
        target resumed
    Hint: Type `help` to see all available commands.

7. We can play around with the application a bit at this point. Once we get a feel for everything,  it's time to start the actual work. In the findspot controller, type `mode collect` to switch into collection mode (see modes). Freezing the application is not necessary.


8. Switching back to the target, enter `1,2,3,4` followed by `5` six times.
    As we are in collection mode, any function executed by the program, including r1(), r2() and any OS-API and C++ runtime, was logged by FindSpot!  
    Verify this by again switching to findspot-cli and entering `show`.

    A total of 44 functions were executed on my system, most of them in libc and libstdc++.
    You can dump all of them to a file with `dump take1.log` for inspection.


9. Now it's time to break out the chizzle!  
    Switch FindSpot to trim mode with `mode trim`. Again, freezing the target is not necessary.
    In this mode, we will *remove* any functions executed from the list of 44 functions we collected so far.  
    Remember we are looking to find the function called when we enter the magic number `5`.  
    Hence, our aim is now to generate as much traffic as possible without hitting the `5`-execution path. To accomplish this just enter any number but `5` a couple of times.

10. Then check out the results with `monitor show`:

        gdb> monitor show
        #            Address       Hits               Module Symbol 
        0       55b8bcad2271          6            example-1 _Z2r1v 
        1       7f030e38c020          1       libstdc++.so.6 .plt 
        Total Count: 2

    Only two functions remain! `_Z2r1v` in `example-1` is indeed the decorated name of `r1()` that we were looking for.
    Note how it was executed exactly six times.  
    Generally, we can rule out the second function here 1) because it was called one time only and 2) because it is located in the c++ runtime.


11. We have now located which function is executed for the magical `5` case without looking at any assembly at all!



## Advanced Example

We will now demonstrate a more complex example: Finding the function executed when the "bold" ToolBox button is pressed in Libre Office.

>>>TODO with mod whitelisting and sorting



## Limitations

PIN considers a function/routine only if it is a named location.
Hence FindSpot is only useful with full smybols right now, especially on Windows.
A mechanism to define custom functions (which can be exported from olly/ida) exists and will be part of a future release...


The commands kill, clear and dynamic changing of whitelist/blacklist are currently broken.


## Usage via debugger (Linux only)


2. Do `pin -appdebug -t FindSpot.so -d debug.log -- ./targetapp.out`.

    Note the additional `-appdebug ` switch.

3. You will get this output, with a unique port number:

        Application stopped until continued from debugger.
        Start GDB, then issue this command at the (gdb) prompt:
            target remote :12345

4. In a second terminal, type `gdb targetapp.out` to start gdb.

5. Then attach to the pintool with `target remote :12345` inside gdb.

6. If everything went well, you are now attached to the pin-instrumented process and can issue commands (see commands) or continue execution with `continue`.

7. Note that commands issued to the pin tool (FindSpot) need to be prefixed with `monitor` (e.g. `monitor help`), otherwise the command is handled by gdb.


Note: -appdebug is only available on Linux.



<details>
  <summary>Click here to see the same example using -appdebug on Linux</summary>

1. In one terminal, start FindSpot: `../../../pin -appdebug -t obj-intel64/FindSpot.so -o findspot.log -- ./choice.out`.

2. In a second terminal, start a gdb session: `gdb choice.out`.

3. Attach gdb to the pintool session, as instructed in terminal 1: `target remote :12345`.

4. Gdb should break immediately:

        [#0] Id 1, stopped 0x7f0322c9e100 in _start (), reason: SIGINT
5. Continue execution in gdb with `continue`

        gdb>  continuue
        Continuing.

6. The application continues execution and waits for an input:

        %PIN%/source/tools/FindSpot$ ../../../pin -appdebug -t obj-intel64/FindSpot.so -- ./choice.out
        Application stopped until continued from debugger.
        Start GDB, then issue this command at the (gdb) prompt:
        target remote :12345
        choose:

7. We can play around with the application a bit at this point. Once we get a feel for everything,  it's time to start the actual work. Break into gdb by hitting `ctrl+c`.

8. Any commands issued in gdb to FindSpot have to be prefixed with `monitor`. To see and overview of FindSpot commands, type `monitor help`.

9. Type `monitor show`:

        gdb>  monitor show
        #            Address       Hits               Module Symbol 
        Total Count: 0
    There is nothing, because no data has been collected yet. Time to change that by switching FindSpot into collection mode with `monitor mode collect`.  
    We can print the current mode we are in with `monitor mode`.  
    Finally, continue execution of the program with `continue`.

10. Switching back the program, enter `5` six times, followed by `0` one time.  
    Since we are in collection mode, any function executed by the program, including r1(), r2() and any OS-API and C++ runtime, was logged by FindSpot!  
    Verify this by again hitting `ctrl+c` and breaking into gdb.

        gdb>  monitor show
        #        Address   Hits        Module Symbol 
        28   7f030e0e7420     49     libc.so.6 getc 
        5    7f030e400e30     37     libstdc++.so.6 _ZN9__gnu_cxx18st
        27   7f030e0e0f30     37     libc.so.6 _IO_ungetc 
        41   7f030e0ee900     36     libc.so.6 _IO_sputbackc 
        <...>
        Total Count: 44

    A total of 44 functions were executed, most of them in libc and libstdc++.
    You can dump all of them to a file with `dump take1.log` for inspection.
    Now it's time to break out the chizzle!  
    Switch FindSpot to trim mode with `monitor mode trim`.
    In this mode, we will *remove* any functions executed from the list of 44 functions we collected so far.  
    Remember we are looking to find the function called when we enter the magic number `5`.  
    Hence, our aim is now to generate as much traffic as possible without hitting the `5`-execution path. To accomplish this, `continue` gdb, and just enter any number but `5` a couple of times.

11. Hit `ctrl+c` again to break into gdb and check out the results with `monitor show`:

        gdb> monitor show
        #            Address       Hits               Module Symbol 
        0       55b8bcad2271          6           choice.out _Z2r1v 
        1       7f030e38c020          1       libstdc++.so.6 .plt 
        Total Count: 2

    Only two functions remain! `_Z2r1v` in `choice.out` is indeed the decorated name of `r1()` that we were looking for.
    Note how it was executed exactly six times.  
    Generally, we can rule out the second function here 1) because it was called one time only and 2) because it is located in the c++ runtime.

12. We have now located which function is executed for the magical `5` case without looking at any assembly at all!

</details>


## Building


1. Download and extract the Intel Pin Framework 3.23: https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-binary-instrumentation-tool-downloads.html.

2. Copy the entire FindSpot directory from this repository into `%PINDIR%/source/tools/`.

3. On Linux: cd to `%PINDIR%/source/tools/FindSpot` and type 'make'.

4. On Windows: open `%PINDIR%/source/tools/FindSpot/FindSpot.vcxproj` in Visual Studio (tested with VS2019) and hit build.

5. Now build the controller and optionally the example, cd to `%PINDIR%/source/tools/FindSpot/findspot-cli` and run `make` (Linux) or build the findspot-cli.vcxproj (Windows).


Note: Building can be a bit of a hassle on Windows. Make sure FindSpot is located in /source/tools/ and try `build->clean + build->rebuild` in Visual Studio. Building has been tested with Visual Studio 2019 only. Alternatively use the binary release.




## Todo

* clear command is broken
* kill command broken
* dynamic changes to whitelist/blacklist broken
* import user functions
* attach/detach
* thread whitelist/blacklist

