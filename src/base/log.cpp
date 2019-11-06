#include "log.hpp"

#include "base/config.hpp"

#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>


#include <ctime>

namespace
{


std::string dateAsString()
{
    std::time_t rawtime;
    std::tm* timeinfo;
    char buffer[80];
    std::time (&rawtime);
    timeinfo = std::localtime(&rawtime);
    std::strftime(buffer, sizeof(buffer),"%d-%m-%Y %H:%M:%S", timeinfo);
    return buffer;
}

void clearLoggerSettings()
{
    boost::log::core::get()->remove_all_sinks();
}

void setLogLevel(base::LogLevel logLevel)
{
    switch(logLevel) {
        case base::LogLevel::ALL:
            boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::trace);
            break;
        case base::LogLevel::DEBUG:
            boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::debug);
            break;
        case base::LogLevel::INFO:
            boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);
            break;
        case base::LogLevel::WARNING:
            boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::warning);
            break;
        case base::LogLevel::ERROR:
            boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::error);
            break;
    }
}

void formatter(boost::log::record_view const& rec, boost::log::formatting_ostream& strm)
{
    strm << dateAsString() << " | " << rec[boost::log::trivial::severity] << " | "
         << rec[boost::log::expressions::smessage];
}

void setFileSink()
{
    using textFileSink = boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend>;

    boost::shared_ptr<textFileSink> sink(
        new textFileSink(boost::log::keywords::file_name = base::config::LOG_FILE_FORMAT));

    sink->locked_backend()->set_file_collector(
        boost::log::sinks::file::make_collector(boost::log::keywords::target = base::config::LOG_FOLDER,
                                                boost::log::keywords::max_size = base::config::LOG_FILE_MAX_SIZE,
                                                boost::log::keywords::min_free_space = base::config::LOG_FILE_MIN_SPACE,
                                                boost::log::keywords::max_files = base::config::LOG_MAX_FILE_COUNT));

    sink->set_formatter(&formatter);

    boost::log::core::get()->add_sink(sink);
}

void setStdoutSink()
{
    using text_sink = boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend>;

    boost::shared_ptr<text_sink> sink = boost::make_shared<text_sink>();

    boost::shared_ptr<std::ostream> stream(&std::clog, boost::null_deleter());
    sink->locked_backend()->add_stream(stream);

    sink->set_formatter(&formatter);

    boost::log::core::get()->add_sink(sink);
}

void disableLogger()
{
    boost::log::core::get()->set_filter(boost::log::trivial::severity > boost::log::trivial::fatal);
}

} // namespace

namespace base
{

void initLog(base::LogLevel logLevel, size_t mode)
{
    clearLoggerSettings();

    if(!mode) {
        std::cout << "WARNING: LOG OUTPUT IS DISABLED" << std::endl; // TODO: remove later
        disableLogger();
        return;
    }

    setLogLevel(logLevel);

    if(mode & base::Sink::STDOUT) {
        setStdoutSink();
        std::cout << "WARNING: STDOUT LOG OUTPUT INITED" << std::endl; // TODO: remove later
    }
    if(mode & base::Sink::FILE) {
        setFileSink();
        std::cout << "WARNING: FILE LOG OUTPUT INITED" << std::endl; // TODO: remove later
    }

    boost::log::add_common_attributes();
}

} // namespace base
