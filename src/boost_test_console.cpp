#include <iostream>
#include <memory>
#include <vector>
#include <regex>

#include <boost/process.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/trim_all.hpp>

#include <gsl/gsl>

#include "linenoise.h"

namespace bf = boost::filesystem;
namespace bp = boost::process;
namespace ba = boost::algorithm;

typedef void (*cmd_handler) (const std::string&);

struct cmd_info_t {
    std::string _cmd;
    std::string _cmd_short;
    cmd_handler _handler;
    std::string _description;
};

void print_help         (const std::string&);
void quit               (const std::string&);
void default_handler    (const std::string&);
void print_content      (const std::string&);
void print_labels       (const std::string&);
void open_last_full_log (const std::string&);
void load_bin           (const std::string& path);

const char* g_hist_file = "./btc_history.hist";

bool g_bin_loaded = false;

std::string g_content;
std::string g_labels;
std::string g_bin_name;
std::string g_prompt;
std::string g_default_prompt = "test console >";

bf::path g_logs_path;
bf::path g_bin_path;

std::vector<bf::path> full_logs_vector;
std::vector<std::string> g_label_vector;
std::vector<std::string> g_test_name_vector;
std::vector<cmd_info_t> g_command_info_vector =
    {
        {"/quit",           "/q",     quit           ,      "Exit the application"},
        {"/last-full-log",  "/lfl",   open_last_full_log,   "Show latest full test log in notepad"},
        {"/last-log",       "/llg",   default_handler,      "Show latest normal test log in notepad"},
        {"/list-labels",    "/ll",    print_labels,         "List all found labels"},
        {"/list-tests",     "/lt",    print_content,        "List all found tests"},
        {"/view-log",       "/vl",    default_handler,      ""},
        {"/view-full-log",  "/vfl",   default_handler,      ""},
        {"/help",           "/h",     print_help,           "Print help message"},
        {"/view-test",      "/vt",    default_handler,      ""},
        {"/load",           "/l",     load_bin,             "Load provided test binary"},
        {"/select",         "/s",     default_handler,      ""},
    };

void
print_help (const std::string&)
{
    std::cout << "@<name> - start test using label <name>\n";
    std::cout << "<name> - start test using its name <name>\n";
    std::cout << '\n';
    std::cout << "Possible commands:\n\n";
    for (const auto& cmd_info : g_command_info_vector) {
        std::cout << cmd_info._cmd << " [" << cmd_info._cmd_short << "] - " << cmd_info._description << '\n';
    }
}

void
quit (const std::string&)
{
    linenoiseHistorySave (g_hist_file);
    linenoiseHistoryFree ();

    exit (0);
}

void
default_handler (const std::string&)
{
    std::cout << "Not implemented\n";
}

void
print_content (const std::string&)
{
    std::cout << g_content << '\n';
}

void
print_labels (const std::string&)
{
    std::cout << g_labels << '\n';
}

void
open_last_full_log (const std::string&)
{
    for (const auto& dir_entry : bf::directory_iterator (g_logs_path)) {
        if (!bf::is_regular_file (dir_entry))
            continue;

        if (dir_entry.path ().extension ().string () != ".log")
            continue;

        if (dir_entry.path ().filename ().string ().substr (0, 3) == "ful")
            full_logs_vector.push_back (dir_entry.path ());
    }

    auto sort_files_new2old =
        [] (const bf::path& lhs, const bf::path& rhs)
        {
            return bf::last_write_time (rhs) < bf::last_write_time (lhs);
        };

    std::sort (
        full_logs_vector.begin (),
        full_logs_vector.end (),
        sort_files_new2old);

    bp::spawn ("notepad " + full_logs_vector.begin ()->string ());
}

bool
exec_cmd (const std::string& cmd)
{
    const auto& cmd_info_it
        = std::find_if (
            g_command_info_vector.begin (),
            g_command_info_vector.end (),
            [&cmd] (const cmd_info_t& c_info)
            {
                if (0 == c_info._cmd.compare (0, c_info._cmd.size (), cmd.c_str (), c_info._cmd.size ()))
                    return true;

                return false;
            });

    if (cmd_info_it == g_command_info_vector.end ())
        return false;

    auto params = cmd.substr (cmd_info_it->_cmd.size ());
    ba::trim (params);

    cmd_info_it->_handler (params);

    return true;
}

void
label_completion_hook (
    const char* prefix,
    linenoiseCompletions* lc)
{
    std::string p_string (prefix);
    std::string reg_str = "";
    for (const auto& c : p_string) {
        reg_str += c;
        reg_str += ".*";
    }

    std::regex reg (reg_str);
    for (const auto& label : g_label_vector)
        if (std::regex_match (label.c_str (), reg))
            linenoiseAddCompletion (lc, label.c_str ());
}

void
cmd_completion_hook (
    const char* prefix,
    linenoiseCompletions* lc)
{
    std::string p_string (prefix);
    std::string reg_str = "";
    for (const auto& c : p_string) {
        reg_str += c;
        reg_str += ".*";
    }

    std::regex reg (reg_str);
    for (const auto& cmd_info : g_command_info_vector) {
        const auto& cmd_long = cmd_info._cmd;
        if (std::regex_match (cmd_long.c_str (), reg))
            linenoiseAddCompletion (lc, cmd_long.c_str ());
    }
}

void
completion_hook (
    const char* prefix,
    linenoiseCompletions* lc)
{
    if (!strlen (prefix))
        return;

    switch (prefix[0]) {
        case '@': label_completion_hook (prefix, lc);   return;
        case '/': cmd_completion_hook   (prefix, lc);   return;
    }
}

void
load_bin (
    const std::string& path)
{
    const bf::path& bin_path (path);
    g_prompt = g_default_prompt;
    g_bin_loaded = false;

    if (!bf::exists (bin_path)) {
        std::cout << "File not found: " << bin_path.filename () << '\n';
        return;
    }

    if (!bf::is_regular_file (bin_path)) {
        std::cout << "File: '" << bin_path.filename () << "' is not a valid file type\n";
        return;
    }

    g_logs_path = bin_path.parent_path () / "logs";

    // @todo: add some proper error handling
    bp::ipstream list_content;
    bp::child exec_list_content (bin_path, "--list_content", bp::std_out > list_content, bp::std_err > list_content);

    std::string line;

    std::string test_full_name;
    while (list_content && std::getline (list_content, line)) {
        line.erase (
            std::remove_if (
                line.begin (),
                line.end (),
                [](char x) { return x == '\r'; }),
            line.end ());

        g_content += line;
        g_content += '\n';
    }

    exec_list_content.wait ();
    if (exec_list_content.exit_code ()) {
        std::cout << "Failed to list bin content: '" << bin_path.filename () << "'. Not a valid boost test binary?\n";
        return;
    }

    bp::ipstream list_labels;
    bp::child exec_list_labels (bin_path, "--list_labels", bp::std_out > list_labels, bp::std_err > list_labels);

    while (list_labels && std::getline (list_labels, line)) {
        std::string label;
        ba::trim_all (line);
        label += '@';
        label += line;
        g_label_vector.push_back (label);
        g_labels += label;
        g_labels += '\n';
    }

    exec_list_labels.wait ();

    g_bin_name = bin_path.stem ().string ();
    g_bin_path = bin_path;

    const std::string pre_prompt = "\x1b[1;32m";
    const std::string post_prompt = "\x1b[0m > ";
    auto prompt = pre_prompt;
    prompt += g_bin_name;
    prompt += post_prompt;

    g_prompt = prompt;
    g_bin_loaded = true;
}

int
main (int argc, char** argv)
{
    if (argc > 2) {
        std::cout << "Usage:\n";
        std::cout << argv[0] << " [boost-test-binary]\n";
        exit (-1);
    }

    if (2 == argc) {
        load_bin (argv[1]);
    }

    linenoiseInstallWindowChangeHandler ();
    linenoiseHistoryLoad (g_hist_file);

    linenoiseSetCompletionCallback (completion_hook);

    while (true) {
        std::unique_ptr <char[]> result (linenoise (g_prompt.c_str ()));
        const std::string res_str (result.get ());
        bool add_to_hist = false;

        if (!res_str.length ())
            continue;

        linenoiseHistoryAdd (result.get ());

        if (res_str[0] == '/') {
            if (!exec_cmd (res_str))
                std::cout << "Unknown command\n";

            continue;
        }

        if (!g_bin_loaded) {
            std::cout << "No test binary loaded!\n";
            continue;
        }

        std::string test_to_run;
        switch (res_str[0]) {
            case '/':
            {
                if (!exec_cmd (res_str))
                    std::cout << "Unknown command\n";

                continue;
            }
            break;
            case '@':
            {
                auto l_it = std::find (g_label_vector.begin (), g_label_vector.end (), res_str);
                if (l_it != g_label_vector.end ())
                    test_to_run = *l_it;
            }
            break;
            default: break;
        }

        if (test_to_run.length ()) {
            bp::child exec_test (g_bin_path, std::string ("--run_test=") + test_to_run, bp::std_out > stdout, bp::std_err > stdout);
            exec_test.wait ();
        }
    }


    return 0;
}
