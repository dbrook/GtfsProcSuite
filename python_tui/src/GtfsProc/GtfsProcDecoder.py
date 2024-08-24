import json
from datetime import timedelta
from math import floor
import sys

from typing import List

from .GtfsProcSocket import GtfsProcSocket
from .TabularData import TabularData


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
        elif code == 'TRI':
            return 'Trip Information'
        elif code == 'STA':
            return 'Station Information'
        elif code == 'RTE':
            return 'Route List (Static Feed)'
        elif code == 'TSR':
            return 'Trips Operating on Route'
        elif code == 'TSS':
            return 'Trips Servicing a Stop'
        elif code == 'SSR':
            return 'All Stops on Route'
        else:
            return 'UNKNOWN RESPONSE'

    def get_fixed_portion(self, data: dict) -> List[str]:
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

            return [
                "[ Backend ]",
                f"Processed Reqs . . . {data['processed_reqs']}",
                f"Uptime . . . . . . . {up_days}d {up_time}",
                f"Data Load Time . . . {data['dataloadtime_ms']} ms",
                f"Threads  . . . . . . {data['threadpool_count']}",
                f"Overrides  . . . . . {data['overrides']}",
                f"Term Trips . . . . . {term_trips}",
                f"NEX Trips/Rts  . . . {data['nb_nex_trips']}",
                f"RT Date Match  . . . {data['rt_date_match']}",
                f"RT Trip Match  . . . {trip_seq_match}",
                f"System Version . . . {data['application']}",
                "",
                "[ Static Feed Information ]",
                f"Publisher  . . . . . {data['feed_publisher']}",
                f"URL  . . . . . . . . {data['feed_url']}",
                f"Language . . . . . . {data['feed_lang']}",
                f"Valid Time . . . . . {data['feed_valid_start']} - {data['feed_valid_end']}",
                f"Version  . . . . . . {data['feed_version']}",
                f"Records Loaded   . . {data['records']}",
                "",
            ]
        elif message_type == 'TRI':
            ret_list = [
                "[ Trip Information ]",
                f"Trip ID  . . . . . . {data['trip_id']}",
                f"Trip Name  . . . . . {data['short_name']}",
                f"Route Name (S) . . . {data['route_short_name']}",
                f"Route Name (L) . . . {data['route_long_name']}",
            ]
            is_rt = data['real_time']
            if is_rt:
                ret_list.append(f"Vehicle  . . . . . . {data['vehicle']}")
                ret_list.append(f"Start DateTime . . . {data['start_date']} / {data['start_time']}")
            else:
                ret_list.append(f"Service ID . . . . . {data['service_id']}")
                ret_list.append(f"Validity . . . . . . {data['svc_start_date']} - {data['svc_end_date']}")
                ret_list.append(f"Operating Days . . . {data['operate_days']}")
                ret_list.append(f"Additions  . . . . . {data['added_dates']}")
                ret_list.append(f"Exceptions . . . . . {data['exception_dates']}")
                ret_list.append(f"Headsign . . . . . . {data['headsign']}")
            ret_list.append("")
            return ret_list
        elif message_type == 'STA':
            return [
                "[ Stop Information ]",
                f"Stop ID  . . . . . . {data['stop_id']}",
                f"Stop Name  . . . . . {data['stop_name']}",
                f"Parent Station . . . {data['parent_sta']}",
                f"Stop Description . . {data['stop_desc']}",
                f"Stop Location  . . . {data['loc_lat']}, {data['loc_lon']}",
                f"",
            ]
        elif message_type == 'NCF':
            if data['static_data_modif'] != self.rte_date:
                # Update the route-cache if it's outdated
                self.update_route_cache(data['static_data_modif'])
            return [
                "[ Stop Details ]",
                f"Stop ID(s) . . . . . {data['stop_id']}",
                f"Stop Name(s) . . . . {data['stop_name']}",
                f"Stop Description . . {data['stop_desc']}",
                "",
            ]
        elif message_type == 'E2E':
            if data['static_data_modif'] != self.rte_date:
                # Update the route-cache if it's outdated
                self.update_route_cache(data['static_data_modif'])
            return []
        elif message_type == 'RTE':
            return []
        elif message_type == 'TSR':
            return [
                "[ Route Details ]",
                f"Route ID . . . . . . {data['route_id']}",
                f"Short Name . . . . . {data['route_short_name']}",
                f"Long Name  . . . . . {data['route_long_name']}",
                f"Service Date . . . . {data['service_date']}",
                "",
            ]
        elif message_type == 'TSS':
            return [
                "[ Stop Details ]",
                f"Stop ID  . . . . . . {data['stop_id']}",
                f"Stop Name  . . . . . {data['stop_name']}",
                f"Stop Description . . {data['stop_desc']}",
                f"Parent Station . . . {data['parent_sta']}",
                f"Service Date . . . . {data['service_date']}",
                f"",
            ]
        elif message_type == 'SSR':
            return [
                "[ Route Details ]",
                f"Route ID . . . . . . {data['route_id']}",
                f"Short Name . . . . . {data['route_short_name']}",
                f"Long Name  . . . . . {data['route_long_name']}",
                f"Route Type - Descr . {data['route_type']} - {data['route_desc']}",
                "",
            ]
        else:
            return ['Response not yet formattable from GtfsProcDecoder']

    def get_tabular_portion(self, data: dict) -> List[TabularData] | str:
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
            return [TabularData(
                '[ Agencies ]',
                [1, 6, 4, 3],
                ['ID', 'NAME', 'TIME ZONE', 'PHONE'],
                ['left', 'left', 'left', 'left'],
                ret_list,
                [],
                False,
            )]
        elif message_type == 'TRI':
            ret_list = []
            commands = []
            is_rt = data['real_time']
            for stop in data['stops']:
                if is_rt:
                    if stop['skipped']:
                        skip = 'SKIP'
                    else:
                        skip = '    '
                    if stop['arr_based']:
                        a_base = stop['arr_based']
                    else:
                        a_base = ' '
                    if stop['dep_based']:
                        d_base = stop['dep_based']
                    else:
                        d_base = ' '
                    ret_list.append([
                        f"{stop['sequence']}",
                        f"{stop['stop_id']}",
                        stop['stop_name'],
                        stop['arr_time'].ljust(7),
                        a_base,
                        stop['dep_time'].ljust(7),
                        d_base,
                        skip,
                    ])
                else:
                    ret_list.append([
                        f"{stop['sequence']}",
                        f"{stop['stop_id']}",
                        stop['stop_name'],
                        self.get_pickup(stop['pickup_type']),
                        self.get_dropoff(stop['drop_off_type']),
                        "{:7}".format(stop['arr_time']),
                        "{:7}".format(stop['dep_time']),
                    ])
                commands.append([f'STA {stop["stop_id"]}', f'NCF 60 {stop["stop_id"]}', f'NEX 60 {stop["stop_id"]}'])
            if is_rt:
                name = '[ Real-Time Predictions ]'
                cols = [1, 1, 3, None, None, None, None, None]
                headers = ['SEQ', 'STOP ID', 'STOP NAME', 'ARRIVE ', 'A', 'DEPART ', 'D', 'SKIP']
                aligns = ['left', 'left', 'left', 'left', 'left', 'left', 'left', 'left']
            else:
                name = '[ Trip Schedule (Static Dataset) ]'
                cols = [1, 1, 3, None, None, None, None]
                headers = ['SEQ', 'STOP ID', 'STOP NAME', 'P', 'D', 'ARRIVE ', 'DEPART ']
                aligns = ['left', 'left', 'left', 'left', 'left', 'left', 'left']
            return [TabularData(name, cols, headers, aligns, ret_list, commands, True)]
        elif message_type == 'STA':
            routes_list = []
            routes_cmds = []
            for route in data['routes']:
                routes_list.append([
                    f"{route['route_id']}",
                    f"{route['route_short_name']}",
                    f"{route['route_long_name']}",
                ])
                routes_cmds.append([
                    f"SSR {route['route_id']}",
                    f"TSR {route['route_id']}",
                    f"TRD Y {route['route_id']}",
                    f"TRD D {route['route_id']}",
                    f"TRD T {route['route_id']}",
                ])

            shared_list = []
            shared_cmds = []
            for stops in data['stops_sharing_parent']:
                shared_list.append([
                    f"{stops['stop_id']}",
                    f"{stops['stop_name']}",
                    f"{stops['stop_desc']}",
                ])
                shared_cmds.append([
                    f"STA {stops['stop_id']}",
                    f"TSS {stops['stop_id']}",
                    f"TSD Y {stops['stop_id']}",
                    f"TSD D {stops['stop_id']}",
                    f"TSD T {stops['stop_id']}",
                ])

            return [
                TabularData('', [1], ['Shortcuts'], ['left'],
                            [['Parent Station'], ['Upcoming Service'], ['Trips Serving Stop']],
                            [
                                [
                                    f'STA {data["parent_sta"]}',
                                    f'NCF 60 {data["parent_sta"]}',
                                    f'NCF 120 {data["parent_sta"]}',
                                    f'NCF 240 {data["parent_sta"]}',
                                    f'NEX 60 {data["parent_sta"]}',
                                    f'NEX 120 {data["parent_sta"]}',
                                    f'NEX 240 {data["parent_sta"]}',
                                ],
                                [
                                    f'NCF 60 {data["stop_id"]}',
                                    f'NCF 120 {data["stop_id"]}',
                                    f'NCF 240 {data["stop_id"]}',
                                    f'NEX 60 {data["stop_id"]}',
                                    f'NEX 120 {data["stop_id"]}',
                                    f'NEX 240 {data["stop_id"]}',
                                ],
                                [
                                    f'TSS {data["stop_id"]}',
                                    f'TSD Y {data["stop_id"]}',
                                    f'TSD D {data["stop_id"]}',
                                    f'TSD T {data["stop_id"]}',
                                ],
                            ],
                            True,
                            ),
                TabularData(
                    '[ Routes Serving Stop ]',
                    [1, 1, 2],
                    ['ROUTE ID', 'SHORT NAME', 'LONG NAME'],
                    ['left', 'left', 'left'],
                    routes_list,
                    routes_cmds,
                    True,
                ),
                TabularData(
                    '[ Stops Sharing Parent Station ]',
                    [1, 1, 2],
                    ['STOP ID', 'SHORT NAME', 'DESCRIPTION'],
                    ['left', 'left', 'left'],
                    shared_list,
                    shared_cmds,
                    True,
                ),
            ]
        elif message_type == 'NCF':
            ret_list = []
            cmd_list = []
            # Construct trip listing
            for trip in data['trips']:
                route_name_s = self.rte_cache[trip['route_id']]['short_name']
                route_name_l = self.rte_cache[trip['route_id']]['long_name']
                if route_name_s != '':
                    route_name = route_name_s
                else:
                    route_name = route_name_l
                if trip['short_name'] != '':
                    trip_name = trip['short_name']
                else:
                    trip_name = trip['trip_id']
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
                    route_name, trip_name, headsign, start_term,
                    pickup, dropoff, stop_time, minutes, status
                ))
                if 'realtime_data' in trip:
                    cmd_list.append([
                        f'TRI {trip["trip_id"]}',
                        f'RTS {trip["trip_id"]}',
                        f'RTF {trip["trip_id"]}',
                    ])
                else:
                    cmd_list.append([f'TRI {trip["trip_id"]}'])
            return [TabularData(
                '[ Trips ]',
                [2, 1, 2, None, None, None, None, None, None],
                ['ROUTE', 'TRIP ID/NAME', 'HEADSIGN', 'T', 'P', 'D', 'STOP TIME  ', 'MINS', 'STATUS'],
                ['left', 'left', 'left', 'left', 'left', 'left', 'left', 'right', 'right'],
                ret_list,
                cmd_list,
                True,
            )]
        elif message_type == 'E2E':
            ret_list = []
            stops = data['stops']
            trips = data['trips']
            if len(trips) == 0:
                return [TabularData(
                    'There are no trips that satisfy the request within the look-ahead time.',
                    [], [], [], [], [], False
                )]
            for rc in range(len(trips)):
                total_time = floor(
                    (trips[rc][len(trips[rc]) - 1]["wait_time_sec"] - trips[rc][0]["wait_time_sec"]) / 60
                )
                ret_list.append([f'Journey {rc + 1}:  Travel Time: {total_time}m'])
                for st in range(len(trips[rc])):
                    has_rt = 'realtime_data' in trips[rc][st]
                    if st % 2 == 0:
                        # Print the trip's route details first ...
                        route_name_s = self.rte_cache[trips[rc][st]['route_id']]['short_name']
                        route_name_l = self.rte_cache[trips[rc][st]['route_id']]['long_name']
                        if route_name_s != '':
                            route_name = route_name_s
                        else:
                            route_name = route_name_l
                        vehicle = ''
                        if has_rt:
                            vehicle = trips[rc][st]['realtime_data']['vehicle']
                        if vehicle != '':
                            ret_list.append([f'<{route_name}> → {trips[rc][st]["headsign"]} <Vehicle # {vehicle}>'])
                        else:
                            ret_list.append([f'<{route_name}> → {trips[rc][st]["headsign"]}'])

                    stop = stops[trips[rc][st]['stop_id']]
                    if 'stop_desc' in stop and stop['stop_desc'] != '':
                        stop_detail = stop['stop_desc']
                    else:
                        stop_detail = stop['stop_name']
                    # Then the origin stop details
                    wait_time = floor(trips[rc][st]["wait_time_sec"] / 60)
                    status = self.get_status(trips[rc][st])
                    if has_rt:
                        arrival = trips[rc][st]['realtime_data']['actual_arrival']
                        departure = trips[rc][st]['realtime_data']['actual_departure']
                    else:
                        arrival = trips[rc][st]['arr_time']
                        departure = trips[rc][st]['dep_time']
                    ret_list.append([
                        '    [{:4}] [{:6}] {:11} {:11} {}'.format(
                            wait_time, status, arrival, departure, stop_detail
                        )
                    ])
                ret_list.append([''])
            return [TabularData(
                '',
                [1],
                [],
                ['left'],
                ret_list,
                [],
                False,
            )]
        elif message_type == 'RTE':
            route_list = []
            route_cmds = []
            for route in data['routes']:
                route_list.append([
                    f"{route['id']}",
                    f"{route['short_name']}",
                    f"{route['long_name']}",
                    f"{route['type']} - {route['desc']}",
                    f"{route['nb_trips']}",
                ])
                route_cmds.append([
                    f"SSR {route['id']}",
                    f"TSR {route['id']}",
                    f"TRD Y {route['id']}",
                    f"TRD D {route['id']}",
                    f"TRD T {route['id']}",
                ])
            return [TabularData(
                '',
                [1, 1, 3, 2, 1],
                ['ID', 'SHORT NAME', 'LONG NAME', 'TYPE-DESC', '#TRIPS'],
                ['left', 'left', 'left', 'left', 'left'],
                route_list,
                route_cmds,
                True,
            )]
        elif message_type == 'TSR':
            trip_list = []
            trip_cmds = []
            for trip in data['trips']:
                if trip['exceptions_present']:
                    exc = 'E'
                else:
                    exc = ' '
                if trip['supplements_other_days']:
                    sup = 'S'
                else:
                    sup = ' '
                trip_list.append([
                    f"{trip['trip_id']}",
                    f"{trip['headsign']}",
                    f"{trip['operate_days_condensed']}",
                    exc,
                    sup,
                    "{:7} - {:7}".format(trip['first_stop_departure'], trip['last_stop_arrival']),
                ])
                trip_cmds.append([f"TRI {trip['trip_id']}"])
            return [TabularData(
                '',
                [2, 3, None, None, None, None],
                ['TRIP ID', 'HEADSIGN', 'OPERATING DAYS', 'E', 'S', 'START-END        '],
                ['left', 'left', 'left', 'left', 'left', 'left'],
                trip_list,
                trip_cmds,
                True,
            )]
        elif message_type == 'SSR':
            stop_list = []
            stop_cmds = []
            for stop in data['stops']:
                stop_list.append([
                    f"{stop['stop_id']}",
                    f"{stop['stop_name']}",
                    f"{stop['stop_desc']}",
                    f"{stop['trip_count']}",
                ])
                stop_cmds.append([
                    f"STA {stop['stop_id']}",
                    f"NCF 60 {stop['stop_id']}",
                    f"NCF 120 {stop['stop_id']}",
                    f"NCF 240 {stop['stop_id']}",
                    f"NEX 60 {stop['stop_id']}",
                    f"NEX 120 {stop['stop_id']}",
                    f"NEX 240 {stop['stop_id']}",
                ])
            return [TabularData(
                "[ Stops Served by Route ]",
                [2, 3, 4, 1],
                ['STOP ID', 'NAME', 'DESCRIPTION', '#TRIPS'],
                ['left', 'left', 'left', 'left'],
                stop_list,
                stop_cmds,
                True,
            )]
        elif message_type == 'TSS':
            route_list = []
            for route in data['routes']:
                if route['route_short_name']:
                    route_name = route['route_short_name']
                else:
                    route_name = route['route_long_name']
                trip_list = []
                trip_cmds = []
                for trip in route['trips']:
                    if trip['exceptions_present']:
                        exc = 'E'
                    else:
                        exc = ' '
                    if trip['supplements_other_days']:
                        sup = 'S'
                    else:
                        sup = ' '
                    if trip['trip_terminates']:
                        start_term = 'T'
                    elif trip['trip_begins']:
                        start_term = 'S'
                    else:
                        start_term = ' '
                    trip_list.append([
                        f"{trip['trip_id']}",
                        f"{trip['headsign']}",
                        f"{trip['operate_days_condensed']}",
                        exc,
                        sup,
                        start_term,
                        self.get_pickup(trip['pickup_type']),
                        self.get_dropoff(trip['drop_off_type']),
                        "{:7}".format(trip['arr_time']),
                        "{:7}".format(trip['dep_time']),
                    ])
                    trip_cmds.append([f"TRI {trip['trip_id']}"])
                route_list.append(TabularData(
                    f"[ Route: {route_name} ]",
                    [2, 3, None, None, None, None, None, None, None, None],
                    ['TRIP ID', 'HEADSIGN', 'OPERATING DAYS', 'E', 'S', 'T', 'P', 'D', 'ARRIVES', 'DEPARTS'],
                    ['left', 'left', 'left', 'left', 'left', 'left', 'left', 'left', 'left', 'left'],
                    trip_list,
                    trip_cmds,
                    True,
                ))
            return route_list
        else:
            return json.dumps(data)

    def get_stop_time(self, trip: dict):
        # Prefer the departure time if present, otherwise the arrival
        if trip['dep_time'] != '-':
            stop_time = '{:11}'.format(trip['dep_time'])
        elif trip['arr_time'] != '-':
            stop_time = '{:11}'.format(trip['arr_time'])
        else:
            stop_time = 'SCH?'
        if 'realtime_data' in trip:
            if trip['realtime_data']['actual_departure'] != '':
                stop_time = '{:11}'.format(trip['realtime_data']['actual_departure'])
            elif trip['realtime_data']['actual_arrival'] != '':
                stop_time = '{:11}'.format(trip['realtime_data']['actual_arrival'])
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
            return '{:4}'.format(floor(trip["wait_time_sec"] / 60))
        if trip['realtime_data']['status'] in ['RNNG', 'CNCL', 'SKIP']:
            return '{:4}'.format(floor(trip["wait_time_sec"] / 60))
        elif trip['realtime_data']['status'] in ['ARRV', 'BRDG', 'DPRT']:
            return trip['realtime_data']['status']

    def get_status(self, trip: dict) -> str:
        if 'realtime_data' not in trip:
            return '      '
        stop_status = trip['realtime_data']['stop_status']
        if stop_status in ['SPLM', 'SCHD', 'PRED']:
            if stop_status == 'SPLM':
                return 'Extra '
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
                abbr_status = 'Skip  '
            else:
                abbr_status = status_code
            return abbr_status

        offset_sec = trip['realtime_data']['offset_seconds']
        if -60 < offset_sec < 60:
            return 'OT    '
        elif offset_sec <= -60:
            return 'E{:4}m'.format(floor(-1 * offset_sec / 60))
        elif offset_sec >= 60:
            return 'L{:4}m'.format(floor(offset_sec / 60))
        else:
            return '?     '
