import urwid
import json

from GtfsProc import GtfsProcSocket, GtfsProcDecoder


class ListView(urwid.WidgetWrap):
    def __init__(self):
        self.walker = urwid.SimpleListWalker([])
        list_box = urwid.ListBox(self.walker)
        urwid.WidgetWrap.__init__(self, list_box)

    def set_data(self, response_array):
        self.walker[:] = response_array


class DisplayDriver:
    def __init__(self, gtfs_handler: GtfsProcSocket, gtfs_decoder: GtfsProcDecoder):
        self.gtfs_proc_sock = gtfs_handler
        self.gtfs_proc_deco = gtfs_decoder
        self.gtfs_rmsg = urwid.Text(u'Welcome to GtfsProc\'s Python Urwid UI!', wrap='clip')
        self.gtfs_rtag = urwid.Text(u'', wrap='clip')
        self.gtfs_proc = urwid.Text(u'', wrap='clip')
        self.gtfs_time = urwid.Text(u'', wrap='clip')
        self.gtfs_cmmd = urwid.Edit(multiline=False, align='left', wrap='clip')
        self.fill_zone = urwid.SimpleListWalker([urwid.Text('WE CAN HAZ TEST TEXT?', wrap='clip')])
        self.scrl_zone = urwid.ListBox(self.fill_zone)
        self.tmp_alarm = None

    def exit_on_f8(self, key: str | tuple[str, int, int, int]) -> None:
        if key in {"f8"}:
            raise urwid.ExitMainLoop()

    def update_response(self, button) -> None:
        self.gtfs_rmsg.base_widget.set_text(u'Working on it ...')
        self.loop.draw_screen()

        command = self.gtfs_cmmd.base_widget.get_edit_text()
        resp = self.gtfs_proc_sock.req_resp(command)

        if resp['error'] != 0:
            self.gtfs_rmsg.base_widget.set_text(u'ERROR')
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
        if fixed_area is not None:
            output_zone.append(urwid.Text(fixed_area))

        # Scrollable portion of the output
        name, head_names, head_widths, align, scrollables = self.gtfs_proc_deco.get_scroll_portion(resp)
        if type(scrollables) is str:
            # When an unprocessed/unformatted response is received, just dump to scrollable area
            output_zone.append(urwid.Text('Here is a JSON dump of the response:'))
            json_lines = json.dumps(resp, indent=2).split('\n')
            for json_line in json_lines:
                output_zone.append(urwid.Text(json_line))
        else:
            # Processed / formattable responses can be dumped into the scrolling view as a table
            if name != '':
                output_zone.append(urwid.Text(name))
            headers = []
            for i in range(len(head_names)):
                headers.append(('weight', head_widths[i], urwid.Text(head_names[i])))
            if len(headers) != 0:
                output_zone.append(urwid.Columns(headers, dividechars=1))
            for i in range(len(scrollables)):
                sub_contents = []
                for j in range(len(scrollables[i])):
                    # sub_contents.append(urwid.Text(scrollables[i][j], wrap='ellipsis'))
                    sub_contents.append((
                        'weight', head_widths[j],
                        urwid.Text(scrollables[i][j], wrap='ellipsis', align=align[j]),
                    ))
                output_zone.append(urwid.Columns(sub_contents, dividechars=1))

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

    def main_draw(self) -> None:
        palette = [
            ('footerbar', 'light gray', 'black'),
            ('command', 'black', 'light gray'),
            ('control', 'black', 'dark cyan'),
            ('exit', 'black', 'brown'),
            ('error', 'white', 'dark red'),
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
        gtfs_sbmt = urwid.Button(u'Run')
        urwid.connect_signal(gtfs_sbmt, 'click', self.update_response)
        gtfs_auto = urwid.CheckBox(u'Auto-Ref')
        urwid.connect_signal(gtfs_auto, 'change', self.auto_update)
        gtfs_quit = urwid.Text(u'F8 to Quit')
        gtfs_foot = urwid.Pile([
            urwid.Divider(),
            urwid.AttrMap(
                urwid.Columns([
                    ('pack', gtfs_prmp),
                    urwid.AttrMap(self.gtfs_cmmd, 'command'),
                    ('pack', urwid.AttrMap(gtfs_sbmt, 'control')),
                    ('pack', urwid.AttrMap(gtfs_auto, 'control')),
                    ('pack', urwid.AttrMap(gtfs_quit, 'exit')),
                    # ('pack', self.gtfs_scri)
                ], 1),
                'footerbar'
            )
        ])

        # Execute the main-loop until the F8 key is pressed
        gtfs_view = urwid.Frame(self.scrl_zone, header=gtfs_head, footer=gtfs_foot)
        gtfs_view.focus_position = 'footer'
        self.loop = urwid.MainLoop(gtfs_view, palette, unhandled_input=self.exit_on_f8)
        self.loop.run()
