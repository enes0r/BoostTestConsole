# BoostTestConsole

Simple tool that is made to ease executing tests built using boost test library in environments where using other more sophisticated tools (IDEs) is not an option or is too much overhead.

# Features

* Executing tests using labels
* Label autocompletion
* History

# Usage 

 * btc.exe [bin_name] - pass test binary as param
 * btc.exe - lounch btc and use '/load [bin_path]' command
 * use /help for printing the help message

#Future work

* Test execution by test name
* Setting lounch options
* Handling multiple test binaries
* Configuration file

# Dependencies

* linenoise-ng - Linenoise Next Generation (https://github.com/arangodb/linenoise-ng)
* boost process & filesystem - http://www.boost.org/

