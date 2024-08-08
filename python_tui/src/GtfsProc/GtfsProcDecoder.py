import json
from datetime import timedelta
from math import ceil, floor

from typing import List, Tuple

from .GtfsProcSocket import GtfsProcSocket

import sys


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


class GtfsProcDecoder:
    def __init__(self, socket: GtfsProcSocket):
        self.socket = socket
        self.rte_cache = {}
        self.rte_date = ''

    def update_route_cache(self, date: str) -> None:
        self.rte_cache = {}
        new_routes = self.socket.req_resp('RTE')
        self.rte_date = date
        for route in new_routes['routes']:
            self.rte_cache[route['id']] = route

    def get_module_code(self, code: str):
        if code == 'SDS':
            return 'System and Data Status'
        elif code == 'NCF':
            return 'Upcoming Trips at Stop'
        elif code == 'E2E':
            return 'End-to-End Connections'
        else:
            return 'UNKNOWN RESPONSE'

    def get_fixed_portion(self, data: dict) -> str:
        message_type = data['message_type']
        if message_type == 'SDS':
            if data['hide_terminating']:
                term_trips = 'Hidden'
            else:
                term_trips = 'Shown'
            if data['rt_trip_seq_match']:
                trip_seq_match = 'Stop Sequence Index'
            else:
                trip_seq_match = 'Stop ID Only'
            up_days = floor(data['appuptime_sec'] / 43200)
            up_time = str(timedelta(seconds=data['appuptime_sec'] % 43200))

            return f'''[ Backend ]
Processed Reqs . . . {data['processed_reqs']}
Uptime . . . . . . . {up_days}d {up_time}
Data Load Time . . . {data['dataloadtime_ms']} ms
Threads  . . . . . . {data['threadpool_count']}
Overrides  . . . . . {data['overrides']}
Term Trips . . . . . {term_trips}
NEX Trips/Rts  . . . {data['nb_nex_trips']}
RT Date Match  . . . {data['rt_date_match']}
RT Trip Match  . . . {trip_seq_match}
System Version . . . {data['application']}

[ Static Feed Information ]
Publisher  . . . . . {data['feed_publisher']}
URL  . . . . . . . . {data['feed_url']}
Language . . . . . . {data['feed_lang']}
Valid Time . . . . . {data['feed_valid_start']} - {data['feed_valid_end']}
Version  . . . . . . {data['feed_version']}
Records Loaded   . . {data['records']}
'''
        elif message_type == 'NCF':
            if data['static_data_modif'] != self.rte_date:
                # Update the route-cache if it's outdated
                self.update_route_cache(data['static_data_modif'])
            return f'''[ Stop Details ]
Stop ID(s) . . . . . {data['stop_id']}
Stop Name(s) . . . . {data['stop_name']}
Stop Description . . {data['stop_desc']}
'''
        elif message_type == 'E2E':
            if data['static_data_modif'] != self.rte_date:
                # Update the route-cache if it's outdated
                self.update_route_cache(data['static_data_modif'])
            return ''
        else:
            return 'Response not yet formattable from GtfsProcDecoder'

    def get_scroll_portion(self, data: dict) -> Tuple[str, List[str], List[int], List[str], List[List[str]] | str]:
        message_type = data['message_type']
        if message_type == 'SDS':
            ret_list = []
            for agency in data['agencies']:
                ret_list.append([
                    agency['id'],
                    agency['name'],
                    agency['tz'],
                    agency['phone'],
                ])
            return (
                '[ Agencies ]',
                ['ID', 'NAME', 'TIME-ZONE', 'PHONE'],
                [1, 6, 4, 3],
                ['left', 'left', 'left', 'left'],
                ret_list,
            )
        elif message_type == 'NCF':
            ret_list = []

            # Construct trip listing
            for trip in data['trips']:
                route_name_s = self.rte_cache[trip['route_id']]['short_name']
                route_name_l = self.rte_cache[trip['route_id']]['long_name']
                if route_name_s != '':
                    route_name = route_name_s
                else:
                    route_name = route_name_l
                trip_id = trip['trip_id']
                trip_name = trip['short_name']
                headsign = trip['headsign']
                if trip['trip_terminates']:
                    start_term = 'T'
                elif trip['trip_begins']:
                    start_term = 'S'
                else:
                    start_term = ' '
                pickup = self.get_pickup(trip['pickup_type'])
                dropoff = self.get_dropoff(trip['drop_off_type'])
                stop_time = self.get_stop_time(trip)
                minutes = self.get_countdown(trip)
                status = self.get_status(trip)
                ret_list.append((
                    route_name, trip_id, trip_name, headsign, start_term,
                    pickup, dropoff, stop_time, minutes, status
                ))

            return (
                '[ Trips ]',
                ['ROUTE', 'TRIP-ID', 'NAME', 'HEADSIGN', 'T', 'P', 'D', 'STOP-TIME', 'MINS', 'STATUS'],
                [6, 10, 4, 12, 1, 1, 1, 7, 3, 4],
                ['left', 'left', 'left', 'left', 'left', 'left', 'left', 'left', 'right', 'right'],
                ret_list,
            )
        elif message_type == 'E2E':
            ret_list = []
            stops = data['stops']
            trips = data['trips']
            for rc in range(len(trips)):
                if rc == 4:
                    break
                if rc == 0:
                    ret_list.append([' '])
                    ret_list.append(['FIRST BOARD IN:'])
                    ret_list.append([' '])
                    for st in range(len(trips[rc])):
                        if st % 2 == 0:
                            stop_name = stops[trips[rc][st]['stop_id']]['stop_name']
                            ret_list.append([stop_name])
                            ret_list.append([' '])
                            ret_list.append([' '])
                        else:
                            stop_name = stops[trips[rc][st]['stop_id']]['stop_name']
                            ret_list.append([stop_name])
                            ret_list.append([' '])
                            if st == len(trips[rc]) - 1:
                                ret_list.append(['TOTAL TRIP TIME:'])
                            else:
                                ret_list.append(['CONNECT TIME:'])
                            ret_list.append([' '])
                ret_list[1].append(f'{floor(trips[rc][0]["wait_time_sec"] / 60)} m')
                for st in range(len(trips[rc])):
                    if st % 2 == 0:
                        row_start = floor((st / 2)) * 7 + 3
                        if 'realtime_data' in trips[rc][st]:
                            is_rt = '*'
                        else:
                            is_rt = ''
                        ret_list[row_start].append(f'{is_rt}{self.get_stop_time(trips[rc][st])}')
                        route_name_s = self.rte_cache[trips[rc][st]['route_id']]['short_name']
                        route_name_l = self.rte_cache[trips[rc][st]['route_id']]['long_name']
                        if route_name_s != '':
                            route_name = route_name_s
                        else:
                            route_name = route_name_l
                        ret_list[row_start + 1].append(route_name)
                        ret_list[row_start + 2].append(trips[rc][st]['headsign'])
                    else:
                        row_start = floor((st / 2)) * 7 + 6
                        if 'realtime_data' in trips[rc][st]:
                            is_rt = '*'
                        else:
                            is_rt = ''
                        ret_list[row_start].append(f'{is_rt}{self.get_stop_time(trips[rc][st])}')
                        if st == len(trips[rc]) - 1:
                            total_time = floor(
                                (trips[rc][st]["wait_time_sec"] - trips[rc][0]["wait_time_sec"])
                                / 60)
                            ret_list[row_start + 2].append(f'{total_time} m')
                        else:
                            conn_time = trips[rc][st + 1]['wait_time_sec'] - trips[rc][st]['wait_time_sec']
                            ret_list[row_start + 2].append(f'{floor(conn_time / 60)} m')
            # eprint(json.dumps(ret_list))
            return (
                '[ Travel Options ]',
                ['STOPS/CONNECTIONS'],
                [2, 1, 1, 1, 1],
                ['left', 'left', 'left', 'left', 'left'],
                ret_list,
                # [],
            )
        else:
            return '', [], [], [], json.dumps(data)

    def get_stop_time(self, trip: dict):
        # Prefer the departure time if present, otherwise the arrival
        if trip['dep_time'] != '-':
            stop_time = trip['dep_time']
        elif trip['arr_time'] != '-':
            stop_time = trip['arr_time']
        else:
            stop_time = 'SCH?'
        if 'realtime_data' in trip:
            if trip['realtime_data']['actual_departure'] != '':
                stop_time = trip['realtime_data']['actual_departure']
            elif trip['realtime_data']['actual_arrival'] != '':
                stop_time = trip['realtime_data']['actual_arrival']
        return stop_time

    def get_dropoff(self, dropoff_type: int) -> str:
        if dropoff_type == 1:
            return 'D'
        elif dropoff_type == 2:
            return 'A'
        elif dropoff_type == 3:
            return 'R'
        else:
            return ' '

    def get_pickup(self, pickup_type: int) -> str:
        if pickup_type == 1:
            return 'P'
        elif pickup_type == 2:
            return 'C'
        elif pickup_type == 3:
            return 'F'
        else:
            return ' '

    def get_countdown(self, trip: dict) -> str:
        if 'realtime_data' not in trip:
            return f'{floor(trip["wait_time_sec"] / 60)}'
        if trip['realtime_data']['status'] in ['RNNG', 'CNCL', 'SKIP']:
            return f'{floor(trip["wait_time_sec"] / 60)}'
        elif trip['realtime_data']['status'] in ['ARRV', 'BRDG', 'DPRT']:
            return trip['realtime_data']['status']

    def get_status(self, trip: dict) -> str:
        if 'realtime_data' not in trip:
            return ''
        stop_status = trip['realtime_data']['stop_status']
        if stop_status in ['SPLM', 'SCHD', 'PRED']:
            if stop_status == 'SPLM':
                return 'Extra'
            elif stop_status == 'SCHD':
                return 'NoData'
            elif stop_status == 'PRED':
                return 'Predic'
            else:
                return stop_status
        status_code = trip['realtime_data']['status']
        if status_code in ['CNCL', 'SKIP']:
            if status_code == 'CNCL':
                abbr_status = 'Cancel'
            elif status_code == 'SKIP':
                abbr_status = 'Skip'
            else:
                abbr_status = status_code
            return abbr_status

        offset_sec = trip['realtime_data']['offset_seconds']
        if -60 < offset_sec < 60:
            return 'OnTime'
        elif offset_sec <= -60:
            return 'E{:4}m'.format(floor(-1 * offset_sec / 60))
        elif offset_sec >= 60:
            return 'L{:4}m'.format(floor(offset_sec / 60))
        else:
            return '??????'
