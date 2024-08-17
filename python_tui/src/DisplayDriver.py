import urwid
import json

from GtfsProc import GtfsProcSocket, GtfsProcDecoder


class ListBoxIndicatorCB(urwid.ListBox):
    def __init__(self, body, on_focus_change=None):
        super().__init__(body)

        self.on_focus_change = on_focus_change

    def change_focus(self, size, position, offset_inset=0, coming_from=None, cursor_coords=None, snap_rows=None):
        super().change_focus(size, position, offset_inset, coming_from, cursor_coords, snap_rows)
        if self.on_focus_change is not None:
            self.on_focus_change(size, position, offset_inset, coming_from, cursor_coords, snap_rows)


class SelectableText(urwid.SelectableIcon):
    def __init__(self, text, cback):
        self.cback = cback
        super().__init__(text, 0)

    def keypress(self, size: tuple[int] | tuple[()], key: str) -> str:
        if key in {"enter"}:
            self.cback.base_widget.set_text('BOOPY')
        return super().keypress(size, key)


class DisplayDriver:
    def __init__(self, gtfs_handler: GtfsProcSocket, gtfs_decoder: GtfsProcDecoder):
        self.gtfs_proc_sock = gtfs_handler
        self.gtfs_proc_deco = gtfs_decoder
        self.gtfs_rmsg = urwid.Text(u'Welcome to GtfsProc\'s Python Urwid UI!', wrap='clip')
        self.gtfs_rtag = urwid.Text(u'', wrap='clip')
        self.gtfs_proc = urwid.Text(u'', wrap='clip')
        self.gtfs_time = urwid.Text(u'', wrap='clip')
        self.gtfs_cmmd = urwid.Edit(multiline=False, align='left', wrap='clip')
        self.fill_zone = urwid.SimpleListWalker([urwid.Text('Enter a command, then press F5', wrap='clip')])
        self.scrl_zone = ListBoxIndicatorCB(self.fill_zone, self.update_scroll_indicator)
        self.scrl_indi = urwid.Text('--')
        self.gtfs_view = None
        self.tmp_alarm = None

    def fkey_handler(self, key: str | tuple[str, int, int, int]) -> None:
        if key in {"f8"}:
            raise urwid.ExitMainLoop()
        elif key in {"f5"}:
            self.update_response(None)
        elif key in {"f2"}:
            if self.gtfs_view.focus_position == 'footer':
                self.gtfs_view.focus_position = 'body'
            else:
                self.gtfs_view.focus_position = 'footer'

    def update_response(self, button) -> None:
        self.gtfs_view.focus_position = 'body'
        self.gtfs_rmsg.base_widget.set_text(('indicator', u'Data Acquisition ...'))
        self.loop.draw_screen()

        command = self.gtfs_cmmd.base_widget.get_edit_text()
        resp = self.gtfs_proc_sock.req_resp(command)

        if resp['error'] != 0:
            self.gtfs_rmsg.base_widget.set_text(('error', u'ERROR'))
            self.gtfs_time.base_widget.set_text(u' ')
            self.gtfs_proc.base_widget.set_text(u' ')
            self.gtfs_rtag.base_widget.set_text(u' ')
            error_text = urwid.AttrMap(
                urwid.Text(f'The GtfsProc server returned the following error: {resp["error"]}'),
                'error'
            )
            self.fill_zone.contents[:] = [error_text]
            self.loop.draw_screen()
            return

        # Header updates
        self.gtfs_rmsg.base_widget.set_text(self.gtfs_proc_deco.get_module_code(resp['message_type']))
        if 'message_time' in resp:
            self.gtfs_time.base_widget.set_text(resp['message_time'])
        else:
            self.gtfs_time.base_widget.set_text(u' ')
        if 'proc_time_ms' in resp:
            self.gtfs_proc.base_widget.set_text(f'Proc: {resp["proc_time_ms"]} ms')
        else:
            self.gtfs_proc.base_widget.set_text(u' ')
        if 'realtime_age_sec' in resp:
            self.gtfs_rtag.base_widget.set_text(f'RT: {resp["realtime_age_sec"]} s')
        else:
            self.gtfs_rtag.base_widget.set_text(u' ')

        # Fixed portion of the output
        fixed_area = self.gtfs_proc_deco.get_fixed_portion(resp)
        output_zone = []
        for fix_text in fixed_area:
            output_zone.append(urwid.Text(fix_text, wrap='ellipsis'))

        # Scrollable portion of the output
        tables = self.gtfs_proc_deco.get_scroll_portion(resp)
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
                    output_zone.append(urwid.Columns([('pack', urwid.Text(' '))] + col_heads, dividechars=1))
                contents = table.get_table_content()
                if len(contents) > 0:
                    for row in contents:
                        output_zone.append(urwid.Columns([('pack', SelectableText('>', self.gtfs_rmsg))] + row, dividechars=1))

        # Final screen update after buffered changes
        self.fill_zone.contents[:] = output_zone
        self.loop.draw_screen()

    def wrap_auto_update(self, foo, bar):
        self.update_response(None)
        self.tmp_alarm = self.loop.set_alarm_in(20, self.wrap_auto_update)

    def auto_update(self, checkbox, checked: bool) -> None:
        if checked:
            self.wrap_auto_update(None, None)
        else:
            self.loop.remove_alarm(self.tmp_alarm)

    def update_scroll_indicator(self, size, position, offset_inset, coming_from, cursor_coords, snap_rows):
        # FIXME: this doesn't seem to work all that great... (unreliable updates)
        result = self.scrl_zone.ends_visible(size)
        if "top" in result:
            indic = '-'
        else:
            indic = '↑'
        if "bottom" in result:
            indic = indic + '-'
        else:
            indic = indic + '↓'
        self.scrl_indi.set_text(indic)


    def main_draw(self) -> None:
        palette = [
            # ('footerbar', 'white', 'black'),
            ('command', 'black', 'dark cyan'),
            ('control', 'black', 'dark cyan'),
            ('keys', 'light gray', 'dark blue'),
            ('error', 'white', 'dark red'),
            ('colheads', 'black', 'light gray'),
            ('indicator', 'brown', 'default'),
        ]
        # Header to show the message type and response + system times
        gtfs_head = urwid.Pile([
            urwid.Columns([
                self.gtfs_rmsg,
                ('pack', self.gtfs_rtag),
                ('pack', self.gtfs_proc),
                ('pack', self.gtfs_time),
            ], 3),
            urwid.Divider()
        ])

        # Footer Generation
        gtfs_prmp = urwid.Text(u'Command:', 'left', 'clip')
        gtfs_auto = urwid.CheckBox(u'Auto')
        urwid.connect_signal(gtfs_auto, 'change', self.auto_update)
        gtfs_foot = urwid.Pile([
            urwid.Divider(),
            urwid.Columns([
                ('pack', gtfs_prmp),
                urwid.AttrMap(self.gtfs_cmmd, 'command'),
                # ('pack', urwid.AttrMap(gtfs_sbmt, 'control')),
                ('pack', urwid.AttrMap(gtfs_auto, 'control')),
                ('pack', urwid.AttrMap(urwid.Text(u'F2: Focus'), 'keys')),
                ('pack', urwid.AttrMap(urwid.Text(u'F5: Run'), 'keys')),
                ('pack', urwid.AttrMap(urwid.Text(u'F8: Quit'), 'keys')),
                ('pack', urwid.AttrMap(self.scrl_indi, 'colheads')),
            ], 1),
        ])

        # Execute the main-loop until the F8 key is pressed
        self.gtfs_view = urwid.Frame(self.scrl_zone, header=gtfs_head, footer=gtfs_foot)
        self.gtfs_view.focus_position = 'footer'
        self.loop = urwid.MainLoop(self.gtfs_view, palette, unhandled_input=self.fkey_handler)
        self.loop.run()
