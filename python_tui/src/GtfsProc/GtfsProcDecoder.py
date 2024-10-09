import json
from datetime import timedelta
from math import floor

from typing import List, Tuple

from .GtfsProcSocket import GtfsProcSocket
from .TabularData import TabularData
from .CommonOutputs import get_pickup, get_dropoff, get_status, get_countdown, get_stop_time, get_vehicle


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
        elif code == 'RDS':
            return 'Real-Time Data Status'
        elif code in ['NCF', 'NEX']:
            return 'Upcoming Trips at Stop'
        elif code == 'E2E':
            return 'End-to-End Connections'
        elif code == 'TRI':
            return 'Single Trip Information'
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
        elif code == 'TRR':
            return 'Trips with Real-Time Info'
        elif code == 'RTI':
            return 'Real-Time Data Analysis'
        elif code == 'SBS':
            return 'Scheduled Service Between Stops'
        elif code == 'SNT':
            return 'Stops Without Any Trips'
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
        elif message_type == 'RDS':
            if data['active_side'] in ['A', 'B']:
                return [
                    f"Mutex Side . . . . . {data['active_side']}",
                    f"Data Generation  . . {data['active_feed_time']}",
                    f"GTFS-Realtime Ver  . {data['active_rt_version']}",
                    f"Fetch Time . . . . . {data['active_download_ms']} ms",
                    f"Integration Time . . {data['active_integration_ms']} ms",
                    f"Feed Age . . . . . . {data['active_age_sec']} s",
                    f"Next Fetch In  . . . {data['seconds_to_next_fetch']} s",
                    f"Latest RT Txn  . . . {data['last_realtime_query']}",
                ]
            else:
                return ["Real-time data collection is disabled or idle."]
        elif message_type == 'TRI':
            ret_list = [
                f"Trip ID  . . . . . . {data['trip_id']}",
                f"Trip Name  . . . . . {data['short_name']}",
                f"Route Name Short . . {data['route_short_name']}",
                f"Route Name Long  . . {data['route_long_name']}",
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
        elif message_type in ['NCF', 'NEX']:
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
        elif message_type == 'TRR':
            return []
        elif message_type == 'RTI':
            if 'realtime_age_sec' not in data:
                return ["Real-time data collection is disabled or idle."]

            if data['static_data_modif'] != self.rte_date:
                # Update the route-cache if it's outdated
                self.update_route_cache(data['static_data_modif'])
            return []
        elif message_type == 'SBS':
            return [
                f"Origin Stop  . . . . {data['ori_stop_name']}",
                f"Origin Stop Desc . . {data['ori_stop_desc']}",
                f"Destin Stop  . . . . {data['des_stop_name']}",
                f"Destin Stop Desc . . {data['des_stop_desc']}",
                f"Service Date . . . . {data['service_date']}",
            ]
        elif message_type == 'SNT':
            return [f"Total Stops: {len(data['stops'])}"]
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
        elif message_type == 'RDS':
            return []
        elif message_type == 'TRI':
            ret_list = []
            commands = []
            is_rt = data['real_time']
            for stop in data['stops']:
                if is_rt:
                    if stop['skipped']:
                        skip = 'S'
                    else:
                        skip = ' '
                    if stop['arr_based']:
                        a_base = stop['arr_based']
                    else:
                        a_base = ' '
                    if stop['dep_based']:
                        d_base = stop['dep_based']
                    else:
                        d_base = ' '
                    # TODO: get the after-midnight stuff? (Does it make sense for an RTS/RTF/RTI message?)
                    ret_list.append([
                        f"{stop['sequence']}",
                        f"{stop['stop_id']}",
                        stop['stop_name'],
                        "{:1}{:>7}".format(' ', stop['arr_time']),
                        a_base,
                        "{:1}{:>7}".format(' ', stop['dep_time']),
                        d_base,
                        skip,
                    ])
                else:
                    if stop['interp']:
                        interpolated = 'i'
                    else:
                        interpolated = ' '
                    if stop['arr_next_day']:
                        arr_next_day = '+'
                    else:
                        arr_next_day = ' '
                    if stop['dep_next_day']:
                        dep_next_day = '+'
                    else:
                        dep_next_day = ' '
                    ret_list.append([
                        f"{stop['sequence']}",
                        f"{stop['stop_id']}",
                        stop['stop_name'],
                        "{:1}{:>7}".format(arr_next_day, stop['arr_time']),
                        "{:1}{:>7}".format(dep_next_day, stop['dep_time']),
                        interpolated,
                        get_pickup(stop['pickup_type']),
                        get_dropoff(stop['drop_off_type']),
                    ])
                commands.append([
                    f'STA {stop["stop_id"]}',
                    f'TSS {stop["stop_id"]}',
                    f'TSD D {stop["stop_id"]}',
                    f'NCF 60 {stop["stop_id"]}',
                    f'NCF 120 {stop["stop_id"]}',
                    f'NCF 240 {stop["stop_id"]}',
                    f'NEX 60 {stop["stop_id"]}',
                    f'NEX 120 {stop["stop_id"]}',
                    f'NEX 240 {stop["stop_id"]}',
                ])
            if is_rt:
                name = '[ Real-Time Predictions ]'
                cols = [1, 2, 3, None, None, None, None, None]
                headers = ['SEQ', 'STOP ID', 'STOP NAME', 'ARRIVE  ', 'A', 'DEPART  ', 'D', 'S']
                aligns = ['left', 'left', 'left', 'left', 'left', 'left', 'left', 'left']
            else:
                name = '[ Trip Schedule (Static Dataset) ]'
                cols = [1, 2, 3, None, None, None, None, None]
                headers = ['SEQ', 'STOP ID', 'STOP NAME', 'ARRIVE  ', 'DEPART  ', 'I', 'P', 'D']
                aligns = ['left', 'left', 'left', 'left', 'left', 'left', 'left', 'left']
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
                    f"NCF 60 {stops['stop_id']}",
                    f"NCF 120 {stops['stop_id']}",
                    f"NCF 240 {stops['stop_id']}",
                    f"NEX 60 {stops['stop_id']}",
                    f"NEX 120 {stops['stop_id']}",
                    f"NEX 240 {stops['stop_id']}",
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
            ret_list, cmd_list = self.get_arriving_trips(data['trips'], False)
            return [TabularData(
                '[ Trips ]',
                [2, 1, 2, None, None, None, None, None, None],
                ['ROUTE', 'TRIP ID/NAME', 'HEADSIGN', 'MINS', 'STATUS', 'STOP TIME  ', 'T', 'P', 'D'],
                ['left', 'left', 'left', 'right', 'right', 'left', 'left', 'left', 'left'],
                ret_list,
                cmd_list,
                True,
            )]
        elif message_type == 'NEX':
            route_list = []
            for route in data['routes']:
                ret_list, cmd_list = self.get_arriving_trips(route['trips'], True)
                route_name = self.route_name_from_id_cache(route['route_id'])
                route_list.append(TabularData(
                    f"[ {route_name} ]",
                    [2, 3, 1, None, None, None, None, None, None],
                    ['TRIP ID/NAME', 'HEADSIGN', 'VEHICLE', 'MINS', 'STATUS', 'STOP TIME  ', 'T', 'P', 'D'],
                    ['left', 'left', 'left', 'left', 'left', 'left', 'left', 'right', 'right'],
                    ret_list,
                    cmd_list,
                    True,
                ))
            return route_list
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
                        route_name = self.route_name_from_id_cache(trips[rc][st]['route_id'])
                        vehicle = ''
                        if has_rt:
                            vehicle = trips[rc][st]['realtime_data']['vehicle']
                        if vehicle != '':
                            ret_list.append([f'{route_name} → {trips[rc][st]["headsign"]} <Vehicle # {vehicle}>'])
                        else:
                            ret_list.append([f'{route_name} → {trips[rc][st]["headsign"]}'])

                    stop = stops[trips[rc][st]['stop_id']]
                    if 'stop_desc' in stop and stop['stop_desc'] != '':
                        stop_detail = stop['stop_desc']
                    else:
                        stop_detail = stop['stop_name']
                    # Then the origin stop details
                    wait_time = floor(trips[rc][st]["wait_time_sec"] / 60)
                    status = get_status(trips[rc][st])
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
                    f"TRR {route['id']}"
                ])
            return [TabularData(
                '',
                [1, 1, 3, 2, 1],
                ['ID', 'SHORT NAME', 'LONG NAME', 'TYPE-DESC', '#TRIPS'],
                ['left', 'left', 'left', 'left', 'right'],
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
                if trip['first_stop_next_day']:
                    first_stop_next_day = '+'
                else:
                    first_stop_next_day = ' '
                if trip['last_stop_next_day']:
                    last_stop_next_day = '+'
                else:
                    last_stop_next_day = ' '
                trip_list.append([
                    f"{trip['trip_id']}",
                    f"{trip['headsign']}",
                    f"{trip['operate_days_condensed']}",
                    exc,
                    sup,
                    "{:1}{:>7} - {:1}{:>7}".format(first_stop_next_day,
                                                   trip['first_stop_departure'],
                                                   last_stop_next_day,
                                                   trip['last_stop_arrival']),
                ])
                trip_cmds.append([f"TRI {trip['trip_id']}"])
            return [
                TabularData(
                    '', [1], ['Shortcuts'], ['left'],
                    [['Stops Served by Route'], ['Real-Time Trips on Route']],
                    [[f'SSR {data["route_id"]}'], [f'TRR {data["route_id"]}']],
                    True,
                ),
                TabularData(
                    '',
                    [2, 3, None, None, None, None],
                    ['TRIP ID', 'HEADSIGN', 'OPERATING DAYS', 'E', 'S', 'START-END          '],
                    ['left', 'left', 'left', 'left', 'left', 'left'],
                    trip_list,
                    trip_cmds,
                    True,
                )
            ]
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
                    f'TSS {stop["stop_id"]}',
                    f'TSD D {stop["stop_id"]}',
                    f"NCF 60 {stop['stop_id']}",
                    f"NCF 120 {stop['stop_id']}",
                    f"NCF 240 {stop['stop_id']}",
                    f"NEX 60 {stop['stop_id']}",
                    f"NEX 120 {stop['stop_id']}",
                    f"NEX 240 {stop['stop_id']}",
                ])
            return [
                TabularData(
                    '', [1], ['Shortcuts'], ['left'],
                    [['Trips Serving Route'], ['Real-Time Trips on Route']],
                    [
                        [
                            f'TSR {data["route_id"]}',
                            f'TRD Y {data["route_id"]}',
                            f'TRD D {data["route_id"]}',
                            f'TRD T {data["route_id"]}',
                        ],
                        [
                            f'TRR {data["route_id"]}',
                        ],
                    ],
                    True,
                ),
                TabularData(
                    "[ Stops Served by Route ]",
                    [2, 3, 4, 1],
                    ['STOP ID', 'NAME', 'DESCRIPTION', '#TRIPS'],
                    ['left', 'left', 'left', 'right'],
                    stop_list,
                    stop_cmds,
                    True,
                )
            ]
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
                    elif trip['interp']:
                        start_term = 'i'
                    else:
                        start_term = ' '
                    if trip['arr_next_day']:
                        arr_next_day = '+'
                    else:
                        arr_next_day = ' '
                    if trip['dep_next_day']:
                        dep_next_day = '+'
                    else:
                        dep_next_day = ' '
                    trip_list.append([
                        f"{trip['trip_id']}",
                        f"{trip['headsign']}",
                        f"{trip['operate_days_condensed']}",
                        exc,
                        sup,
                        "{:1}{:>7}".format(arr_next_day, trip['arr_time']),
                        "{:1}{:>7}".format(dep_next_day, trip['dep_time']),
                        start_term,
                        get_pickup(trip['pickup_type']),
                        get_dropoff(trip['drop_off_type']),
                    ])
                    trip_cmds.append([f"TRI {trip['trip_id']}"])
                route_list.append(TabularData(
                    f"[ Route: {route_name} ]",
                    [2, 3, None, None, None, None, None, None, None, None],
                    ['TRIP ID', 'HEADSIGN', 'OPERATING DAYS', 'E', 'S', 'ARRIVE  ', 'DEPART  ', 'T', 'P', 'D'],
                    ['left', 'left', 'left', 'left', 'left', 'left', 'left', 'left', 'left', 'left'],
                    trip_list,
                    trip_cmds,
                    True,
                ))
            return route_list
        elif message_type == 'TRR':
            route_list = []
            for route in data['routes']:
                if route['route_short_name']:
                    route_name = route['route_short_name']
                else:
                    route_name = route['route_long_name']
                trip_list = []
                trip_cmds = []
                for trip in route['trips']:
                    if trip['skipped']:
                        skip = 'S'
                    else:
                        skip = ' '
                    if 'pickup_type' in trip:
                        pickup = get_pickup(trip['pickup_type'])
                    else:
                        pickup = '-'
                    if 'drop_off_type' in trip:
                        dropoff = get_dropoff(trip['drop_off_type'])
                    else:
                        dropoff = '-'
                    trip_list.append([
                        f"{trip['trip_id']}",
                        f"{trip['headsign']}",
                        f"{trip['next_stop_name']}",
                        f"{trip['vehicle']}",
                        "{:11}".format(trip['arrive']),
                        "{:11}".format(trip['depart']),
                        pickup,
                        dropoff,
                        skip,
                    ])
                    trip_cmds.append([
                        f"STA {trip['next_stop_id']}",
                        f"TRI {trip['trip_id']}",
                        f"RTS {trip['trip_id']}",
                        f"RTF {trip['trip_id']}",
                        f"NCF 60 {trip['next_stop_id']}",
                        f"NCF 120 {trip['next_stop_id']}",
                        f"NCF 240 {trip['next_stop_id']}",
                        f"NEX 60 {trip['next_stop_id']}",
                        f"NEX 120 {trip['next_stop_id']}",
                        f"NEX 240 {trip['next_stop_id']}",
                    ])
                route_list.append(TabularData(
                    f"[ Route: {route_name} ]",
                    [3, 3, 2, 1, None, None, None, None, None],
                    ['TRIP ID', 'HEADSIGN', 'NEXT STOP', 'VEHICLE', 'ARRIVE     ', 'DEPART     ', 'P', 'D', 'S'],
                    ['left', 'left', 'left', 'left', 'left', 'left', 'left', 'left', 'left'],
                    trip_list,
                    trip_cmds,
                    True,
                ))
            return route_list
        elif message_type == 'RTI':
            if 'realtime_age_sec' not in data:
                return []

            mismatch_list, mismatch_cmds = self.get_tri_nest(data, 'mismatch_trips')
            canceled_list, canceled_cmds = self.get_tri_nest(data, 'canceled_trips')
            added_list, added_cmds = self.get_tri_nest(data, 'added_trips')
            running_list, running_cmds = self.get_tri_nest(data, 'active_trips')
            orphan_list = []
            orphan_cmds = []
            for orphan in data['orphaned_trips']:
                orphan_list.append([f"{orphan}"])
                orphan_cmds.append([f"RTF {orphan}"])
            duplicate_list = []
            duplicate_cmds = []
            for dupe in data['duplicate_trips']:
                route = self.route_name_from_id_cache(dupe)
                duplicate_list.append([f"{route}"])
                duplicate_cmds.append([f"TRR {dupe}", f"SSR {dupe}", f"TSR {dupe}"])
                for trip in data['duplicate_trips'][dupe]:
                    duplicate_list.append([f"   {trip}"])
                    inner_cmds = []
                    for rtIdx in trip:
                        inner_cmds.append(f"RTT {rtIdx}")
                    duplicate_cmds.append(inner_cmds)
            return [
                TabularData("", [1], ['MISMATCHED REAL-TIME STOPS/SEQS'],
                            ['left'], mismatch_list, mismatch_cmds, True),
                TabularData("", [1], ['ORPHANED TRIPS'],
                            ['left'], orphan_list, orphan_cmds, True),
                TabularData("", [1], ['DUPLICATE TRIPS'],
                            ['left'], duplicate_list, duplicate_cmds, True),
                TabularData("", [1], ['CANCELED TRIPS'],
                            ['left'], canceled_list, canceled_cmds, True),
                TabularData("", [1], ['SUPPLEMENTAL TRIPS'],
                            ['left'], added_list, added_cmds, True),
                TabularData("", [1], ['ACTIVE SCHEDULED TRIPS'],
                            ['left'], running_list, running_cmds, True),
            ]
        elif message_type == 'SBS':
            trip_list = []
            trip_cmds = []
            for trip in data['trips']:
                if trip['route_short_name']:
                    route_name = trip['route_short_name']
                else:
                    route_name = trip['route_long_name']
                if trip['trip_short_name']:
                    trip_name = trip['trip_short_name']
                else:
                    trip_name = trip['trip_id']
                trip_list.append([
                    f"{route_name}",
                    f"{trip_name}",
                    f"{trip['headsign']}",
                    "{:11}".format(trip['ori_depature']),
                    get_pickup(trip['ori_pick_up']),
                    "{:11}".format(trip['des_arrival']),
                    get_dropoff(trip['des_drop_off']),
                    trip['duration'],
                ])
                trip_cmds.append([f"TRI {trip['trip_id']}"])
            return [TabularData(
                "",
                [1, 1, 2, None, None, None, None, None],
                ['ROUTE', 'TRIP', 'HEADSIGN', 'DEP TIME   ', 'P', 'ARR TIME   ', 'D', 'DUR  '],
                ['left', 'left', 'left', 'left', 'left', 'left', 'left', 'left'],
                trip_list,
                trip_cmds,
                True,
            )]
        elif message_type == 'SNT':
            stop_list = []
            for stop in data['stops']:
                stop_list.append([
                    stop['stop_id'],
                    stop['stop_name'],
                    stop['stop_desc'],
                    '{:>21}'.format(f"{stop['loc_lat'][:10]},{stop['loc_lon'][:10]}"),
                ])
            return [TabularData(
                "",
                [1, 2, 2, None],
                ['ID', 'NAME', 'DESC', 'LOC                  '],
                ['left', 'left', 'left', 'right'],
                stop_list,
                [],
                False,
            )]
        else:
            return json.dumps(data)

    def route_name_from_id_cache(self, route_id) -> str:
        if route_id not in self.rte_cache:
            return '#!ROUTE ID?#'
        route_name_s = self.rte_cache[route_id]['short_name']
        route_name_l = self.rte_cache[route_id]['long_name']
        if route_name_s != '':
            return route_name_s
        else:
            return route_name_l

    def get_arriving_trips(self, trips, skip_route) -> Tuple[List, List]:
        ret_list = []
        cmd_list = []
        # Construct trip listing
        for trip in trips:
            route_name = self.route_name_from_id_cache(trip['route_id'])
            if trip['short_name'] != '':
                trip_name = trip['short_name']
            else:
                trip_name = trip['trip_id']
            headsign = trip['headsign']
            if trip['trip_terminates']:
                start_term = 'T'
            elif trip['trip_begins']:
                start_term = 'S'
            elif trip['interp']:
                start_term = 'i'
            else:
                start_term = ' '
            pickup = get_pickup(trip['pickup_type'])
            dropoff = get_dropoff(trip['drop_off_type'])
            stop_time = get_stop_time(trip)
            minutes = get_countdown(trip)
            status = get_status(trip)
            vehicle = get_vehicle(trip)
            if skip_route:
                ret_list.append([
                    trip_name, headsign, vehicle, minutes, status, stop_time, start_term, pickup, dropoff
                ])
            else:
                ret_list.append([
                    route_name, trip_name, headsign, minutes, status, stop_time, start_term, pickup, dropoff
                ])
            if 'realtime_data' in trip:
                cmd_list.append([
                    f'TRI {trip["trip_id"]}',
                    f'RTS {trip["trip_id"]}',
                    f'RTF {trip["trip_id"]}',
                ])
            else:
                cmd_list.append([f'TRI {trip["trip_id"]}'])
        return ret_list, cmd_list

    def get_tri_nest(self, data: dict, key: str) -> Tuple[List[List[str]], List[List[str]]]:
        list_items = []
        commands = []
        for route_id in data[key].keys():
            route = self.route_name_from_id_cache(route_id)
            list_items.append([f"{route}"])
            commands.append([f"TRR {route_id}", f"SSR {route_id}", f"TSR {route_id}"])
            for trip in data[key][route_id]:
                list_items.append([f"   {trip}"])
                commands.append([f"TRI {trip}", f"RTS {trip}", f"RTF {trip}"])
        return list_items, commands
