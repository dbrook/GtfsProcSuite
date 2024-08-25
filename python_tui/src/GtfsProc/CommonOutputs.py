import json
import urwid
from math import floor
from typing import List


class SelectableText(urwid.SelectableIcon):
    def __init__(self, text, commands: List[str], dd):
        self.commands = commands
        self.dd = dd
        super().__init__(text, 0)

    def keypress(self, size: tuple[int] | tuple[()], key: str) -> str:
        if key in {"enter"}:
            # Open a selection if there's more than 1 command available,
            # otherwise just go straight to the mini-viewer display.
            if len(self.commands) > 0:
                self.dd.draw_qb_select(self.commands)
        return super().keypress(size, key)


def decode(gtfs_proc_deco, dd, resp):
    fixed_area = gtfs_proc_deco.get_fixed_portion(resp)
    output_zone = []
    for fix_text in fixed_area:
        output_zone.append(urwid.Text(fix_text, wrap='ellipsis'))
    tables = gtfs_proc_deco.get_tabular_portion(resp)
    if type(tables) is str:
        # When an unprocessed/unformatted response is received, just dump to scrollable area
        output_zone.append(urwid.AttrMap(
            urwid.Text('Here is a JSON dump of the response:'),
            'error'
        ))
        json_lines = json.dumps(resp, indent=2).split('\n')
        for json_line in json_lines:
            output_zone.append(urwid.Text(json_line))
    else:
        for table in tables:
            # Processed / formattable responses can be dumped into the scrolling view as a table
            name = table.get_table_name()
            if name != '':
                output_zone.append(urwid.Text(name))
            col_heads = table.get_columns_formatted()
            if len(col_heads) > 0:
                output_zone.append(urwid.Columns(col_heads, dividechars=1))
            contents = table.get_table_content()
            for row in range(len(contents)):
                commands = table.get_commands(row)
                if len(commands) > 0:
                    output_zone.append(urwid.Columns([('pack', urwid.AttrMap(
                                                         SelectableText(' ', commands, dd),
                                                         None,
                                                         focus_map='selection'
                                                     ))] + contents[row], dividechars=1))
                else:
                    output_zone.append(urwid.Columns(contents[row], dividechars=1))
            if len(tables) > 1:
                output_zone.append(urwid.Text(u''))

    return output_zone


def get_stop_time(trip: dict):
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


def get_dropoff(dropoff_type: int) -> str:
    if dropoff_type == 1:
        return 'D'
    elif dropoff_type == 2:
        return 'A'
    elif dropoff_type == 3:
        return 'R'
    else:
        return ' '


def get_pickup(pickup_type: int) -> str:
    if pickup_type == 1:
        return 'P'
    elif pickup_type == 2:
        return 'C'
    elif pickup_type == 3:
        return 'F'
    else:
        return ' '


def get_countdown(trip: dict) -> str:
    if 'realtime_data' not in trip:
        return '{:4}'.format(floor(trip["wait_time_sec"] / 60))
    if trip['realtime_data']['status'] in ['RNNG', 'CNCL', 'SKIP']:
        return '{:4}'.format(floor(trip["wait_time_sec"] / 60))
    elif trip['realtime_data']['status'] in ['ARRV', 'BRDG', 'DPRT']:
        return trip['realtime_data']['status']


def get_status(trip: dict) -> str:
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
