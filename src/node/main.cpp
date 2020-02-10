#include "soft_config.hpp"
#include "hard_config.hpp"

#include "base/program_options.hpp"
#include "base/config.hpp"
#include "base/log.hpp"
#include "base/assert.hpp"
#include "node/node.hpp"

#ifdef CONFIG_OS_FAMILY_UNIX
#include <cstring>
#endif

#include <boost/stacktrace.hpp>

#include <iostream>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <thread>
#include <filesystem>

namespace
{

extern "C" void signalHandler(int signal)
{
    if(signal == SIGINT) {
        LOG_INFO << "SIGINT caught. Exit.";
        std::abort(); // TODO: a clean exit
    }
    else {
        LOG_INFO << "Signal caught: " << signal
#ifdef CONFIG_OS_FAMILY_UNIX
                 << " (" << strsignal(signal) << ")"
#endif
                 << '\n'
                 << boost::stacktrace::stacktrace();
        std::abort();
    }
}


void atExitHandler()
{
    LOG_INFO << "Node shutdown";
}

} // namespace

int main(int argc, char** argv)
{
    try {
        base::initLog(base::LogLevel::ALL, base::Sink::STDOUT | base::Sink::FILE);
        LOG_INFO << "Node startup";

        // set up options parser
        base::ProgramOptionsParser parser;
        parser.addOption<std::string>("config,c", config::CONFIG_PATH, "Path to config file");

        // process options
        parser.process(argc, argv);
        if(parser.hasOption("help")) {
            std::cout << parser.helpMessage() << std::endl;
            return base::config::EXIT_OK;
        }

        auto config_file_path = parser.getValue<std::string>("config");

        if(!std::filesystem::exists(config_file_path)) {
            LOG_ERROR << "[config file is not exists] input file path: " << parser.getValue<std::string>("config");
            return base::config::EXIT_FAIL;
        }
        else {
            LOG_INFO << "Found config file by path: " << config_file_path;
        }

        // handlers initialization

        // setup handler for all signal types defined in Standard, expect SIGABRT. Not all POSIX signals
        for(auto signal_code: {SIGTERM, SIGSEGV, SIGINT, SIGILL, SIGFPE}) {
            [[maybe_unused]] auto result = std::signal(signal_code, signalHandler);
            ASSERT_SOFT(result != SIG_ERR);
        }

        {
            [[maybe_unused]] auto result = std::atexit(atExitHandler);
            ASSERT_SOFT(result == 0);
        }

        //=====================
        SoftConfig exe_config(config_file_path);
        Node node(exe_config);
        node.run();
        //=====================
        std::this_thread::sleep_for(std::chrono::seconds(4500));

        return base::config::EXIT_OK;
    }
    catch(const std::exception& error) {
        LOG_ERROR << "[exception caught in main] " << error.what();
        return base::config::EXIT_FAIL;
    }
    catch(...) {
        LOG_ERROR << "[unknown exception caught]";
        return base::config::EXIT_FAIL;
    }
}
