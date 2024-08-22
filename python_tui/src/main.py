# System / Environment Includes
import argparse
import asyncio
import sys

# Typing and project-level imports
from typing import Sequence

import DisplayDriver
from GtfsProc.GtfsProcSocket import GtfsProcSocket
from GtfsProc.GtfsProcDecoder import GtfsProcDecoder


async def main(arguments: Sequence[str]) -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("-n", type=str, dest="host_name",
                        help="Host to connect")
    parser.add_argument("-p", type=int, dest="port_num",
                        help="Port to connect")
    parser.add_argument("-c", type=str, dest="command_str",
                        help="Command to run by default")
    parsed_args = parser.parse_args()

    if parsed_args.host_name is None or parsed_args.port_num is None:
        print('Use -n to specify a host AND -p to specify a port! Exiting ...')
        exit(1)

    print(f'WE GOT: {parsed_args.host_name}:{parsed_args.port_num}, CMD:[{parsed_args.command_str}]')
    gtfs_handler = GtfsProcSocket(parsed_args.host_name, parsed_args.port_num)
    gtfs_decoder = GtfsProcDecoder(gtfs_handler)
    dd = DisplayDriver.DisplayDriver(gtfs_handler, gtfs_decoder, parsed_args.command_str)
    dd.main_draw()


#
# Boilerplate script-as-an-application functions
#
def run() -> None:
    asyncio.run(main(sys.argv[1:]))


if __name__ == "__main__":
    run()
