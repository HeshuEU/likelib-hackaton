import argparse
import os

from tester import run_registered_test_cases

import test_communication_base
import test_common_process
import test_contract
import test_auction_contract

import test_bad_network
import test_simple_network
# import test_real_network
import test_contract_functions
import test_multi_transfer

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Test execution params')
    parser.add_argument('-b', '--bin', type=str, help='path to folder with dependencies')
    parser.add_argument('-t', '--tests', type=str, default="",
                        help='pattern for test execution')
    parser.add_argument('-d', '--dir', type=str, default=os.getcwd(), help='directory for evaluate tests')

    args = parser.parse_args()

    return_code = run_registered_test_cases(args.tests, args.bin, args.dir)
    exit(return_code)
