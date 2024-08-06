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
        self.gtfs_rmsg = urwid.Text(u'', wrap='clip')
        self.gtfs_rtag = urwid.Text(u'', wrap='clip')
        self.gtfs_proc = urwid.Text(u'', wrap='clip')
        self.gtfs_time = urwid.Text(u'', wrap='clip')
        self.gtfs_cmmd = urwid.Edit(multiline=False, align='left', wrap='clip')
        self.gtfs_fixd = urwid.Text(u'Fixed Area Text Placeholder')
        self.gtfs_scrh = urwid.Text(u'Scrollable Area Header')
        self.gtfs_scri = urwid.Text(u'(Scroll --)')
        self.gtfs_scra = urwid.Columns([self.gtfs_scrh, ('pack', self.gtfs_scri)])
        self.gtfs_cols = urwid.Columns([urwid.Divider()], dividechars=1)
        ### FIXME: the entire listview implem is kinda broken because it cannot be variable height!
        self.gtfs_scrl = ListView()
        col_rows = urwid.raw_display.Screen().get_cols_rows()
        h = col_rows[1] - 7  # FIXME this is the wrong maths location!
        self.gtfs_scrx = urwid.BoxAdapter(self.gtfs_scrl, height=h)
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
            self.gtfs_fixd.base_widget.set_text(
                f'The GtfsProc server returned the following error: {resp["error"]}'
            )
            self.gtfs_scrh.base_widget.set_text(u' ')
            self.gtfs_cols.contents = []
            self.gtfs_scrl.set_data([])
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
        self.gtfs_fixd.base_widget.set_text(self.gtfs_proc_deco.get_fixed_portion(resp))

        # Scrollable portion of the output
        self.gtfs_scrx.sizing()
        name, head_names, head_widths, align, scrollables = self.gtfs_proc_deco.get_scroll_portion(resp)
        if type(scrollables) is str:
            # When an unprocessed/unformatted response is received, just dump to scrollable area
            self.gtfs_scrh.base_widget.set_text('Here is a JSON dump of the response:')
            headers = [
                (urwid.Text(''), urwid.Columns.options()),
            ]
            self.gtfs_cols.contents = headers
            # self.gtfs_scrl.contents = urwid.SimpleListWalker(urwid.Text(json.dumps(resp, indent=2)))
            json_lines = json.dumps(resp, indent=2).split('\n')
            urwid_line = [urwid.Text(w) for w in json_lines]
            self.gtfs_scrl.set_data(urwid_line)
        else:
            # Processed / formattable responses can be dumped into the scrolling view as a table
            self.gtfs_scrh.base_widget.set_text(name)
            headers = []
            for i in range(len(head_names)):
                headers.append((
                    urwid.Text(head_names[i]),
                    urwid.Columns.options('weight', head_widths[i])
                ))
            self.gtfs_cols.contents = headers
            contents = []
            for i in range(len(scrollables)):
                sub_contents = []
                for j in range(len(scrollables[i])):
                    # sub_contents.append(urwid.Text(scrollables[i][j], wrap='ellipsis'))
                    sub_contents.append((
                        'weight', head_widths[j],
                        urwid.Text(scrollables[i][j], wrap='ellipsis', align=align[j]),
                    ))
                contents.append(urwid.Columns(sub_contents, dividechars=1))
                # contents.append(sub_contents)
            self.gtfs_scrl.set_data(contents)

        # Final screen update after buffered changes
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
        # Header to show the message type and response + system times
        gtfs_head = urwid.Columns([
            self.gtfs_rmsg,
            ('pack', self.gtfs_rtag),
            ('pack', self.gtfs_proc),
            ('pack', self.gtfs_time),
        ], 3)
        gtfs_divi = urwid.Divider(u'─')

        mid_zone = urwid.Pile([
            ('pack', self.gtfs_fixd),
            self.gtfs_scra,
            self.gtfs_cols,
            self.gtfs_scrx,
        ])
        fill = urwid.Filler(mid_zone, 'top')

        gtfs_prmp = urwid.Text(u'Command:', 'left', 'clip')
        gtfs_sbmt = urwid.Button(u'Run')
        urwid.connect_signal(gtfs_sbmt, 'click', self.update_response)
        gtfs_auto = urwid.CheckBox(u'Auto-Refresh')
        urwid.connect_signal(gtfs_auto, 'change', self.auto_update)
        gtfs_quit = urwid.Text(u'│ F8 to Quit')
        gtfs_foot = urwid.Columns([
            ('pack', gtfs_prmp),
            self.gtfs_cmmd,
            ('pack', gtfs_sbmt),
            ('pack', gtfs_auto),
            ('pack', gtfs_quit),
            # ('pack', self.gtfs_scri)
        ], 1)

        # Execute the main-loop until the F8 key is pressed
        gtfs_view = urwid.Pile([
            ('pack', gtfs_head),
            ('pack', gtfs_divi),
            fill,
            ('pack', gtfs_divi),
            ('pack', gtfs_foot),
        ])
        self.loop = urwid.MainLoop(gtfs_view, unhandled_input=self.exit_on_f8)
        self.loop.run()
