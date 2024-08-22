import json
import urwid
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
                                                         SelectableText('>', commands, dd),
                                                         None,
                                                         focus_map='selection'
                                                     ))] + contents[row], dividechars=1))
                else:
                    output_zone.append(urwid.Columns(contents[row], dividechars=1))

    return output_zone
