set(TEST_SOURCES
        main.cpp
        base/big_integer.cpp
        base/bytes.cpp
        base/crypto.cpp
        base/database.cpp
        base/hash.cpp
        base/json.cpp
        base/program_options.cpp
        base/serialization.cpp
        base/time.cpp
        base/timer.cpp
        core/address.cpp
        core/block.cpp
        core/consensus.cpp
        core/transaction.cpp
        core/transactions_set.cpp
        net/endpoint.cpp
        vm/vm.cpp
        vm/tools.cpp
        )

add_executable(run_tests ${TEST_SOURCES})

target_link_libraries(run_tests base core net websocket vm Boost::unit_test_framework dl)


