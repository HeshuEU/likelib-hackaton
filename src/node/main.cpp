#include "soft_config.hpp"
#include "hard_config.hpp"

#include "base/config.hpp"
#include "base/log.hpp"
#include "base/assert.hpp"
#include "base/network/manager.hpp"
#include "base/network/network_address.hpp"

#ifdef CONFIG_OS_FAMILY_UNIX
#include <cstring>
#endif

#include <csignal>
#include <cstdlib>
#include <exception>
#include <thread>


namespace
{


extern "C" void signalHandler(int signal)
{
    LOG_INFO << "Signal caught: " << signal
#ifdef CONFIG_OS_FAMILY_UNIX
             << " (" << strsignal(signal) << ")"
#endif
#ifdef CONFIG_IS_DEBUG
             << '\n'
             << boost::stacktrace::stacktrace()
#endif
        ;
    std::exit(base::config::EXIT_FAIL);
}

void atExitHandler()
{
    LOG_INFO << "atExitHandler called";
}

} // namespace

int main(int argc, char** argv)
{
    try {
        base::initLog(base::LogLevel::ALL, base::Sink::STDOUT | base::Sink::FILE);
        LOG_INFO << "Node startup";

        // handlers initialization

        // setup handler for all signal types defined in Standard. Not all POSIX signals
        for(auto signal_code: {SIGTERM, SIGSEGV, SIGINT, SIGILL, SIGABRT, SIGFPE}) {
            ASSERT_SOFT(std::signal(signal_code, signalHandler) != SIG_ERR);
        }

        ASSERT_SOFT(std::atexit(atExitHandler) == 0);

        //=====================

        SoftConfig exe_config(config::CONFIG_PATH);

        base::network::Manager manager;
        manager.acceptClients(base::network::NetworkAddress{exe_config.get<std::string>("listen_address")});
        manager.run();

        std::this_thread::sleep_for(std::chrono::seconds(15));

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