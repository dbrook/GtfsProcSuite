import urwid
import json
from typing import List, Hashable, Callable, Any

from QuickBrowser import QuickBrowser
from GtfsProc import GtfsProcSocket, GtfsProcDecoder, CommonOutputs


class DisplayDriver:
    def __init__(
            self,
            gtfs_handler: GtfsProcSocket,
            gtfs_decoder: GtfsProcDecoder,
            start_cmd: str,
    ):
        self.gtfs_proc_sock = gtfs_handler
        self.gtfs_proc_deco = gtfs_decoder
        self.gtfs_rmsg = urwid.Text(u'Welcome to GtfsProc\'s Python Urwid UI!', wrap='clip')
        self.gtfs_rtag = urwid.Text(u'', wrap='clip')
        self.gtfs_proc = urwid.Text(u'', wrap='clip')
        self.gtfs_time = urwid.Text(u'', wrap='clip')
        self.gtfs_cmmd = urwid.Edit(multiline=False, align='left', wrap='clip')
        self.fill_zone = urwid.SimpleListWalker([urwid.Text('Enter a command, then press F5', wrap='clip')])
        self.scrl_zone = urwid.ListBox(self.fill_zone)
        self.gtfs_auto = None
        self.gtfs_view = None
        self.tmp_alarm = None
        # QuickBrowser variables
        self.qb_select = None
        self.quick_brw = None
        self.start_cmd = start_cmd
        self.qb_command = ''

    def fkey_handler(self, key: str | tuple[str, int, int, int]) -> None:
        if key in {"f8"}:
            raise urwid.ExitMainLoop()
        elif key in {"f5"}:
            self.update_response()
        elif key in {"f2"}:
            if self.gtfs_view.focus_position == 'footer':
                self.gtfs_view.focus_position = 'body'
            else:
                self.gtfs_view.focus_position = 'footer'
        elif key in {"f7"}:
            self.gtfs_cmmd.set_edit_text(self.qb_command)
            self.loop.widget = self.gtfs_view
            self.update_response()
        elif key in {"esc"}:
            self.loop.widget = self.gtfs_view

    def update_response(self) -> None:
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

        output_zone = CommonOutputs.decode(self.gtfs_proc_deco, self, resp)
        self.fill_zone.contents[:] = output_zone
        self.loop.draw_screen()

    def wrap_auto_update(self, foo, bar):
        self.update_response()
        self.tmp_alarm = self.loop.set_alarm_in(20, self.wrap_auto_update)

    def auto_update(self, checkbox, checked: bool) -> None:
        if checked:
            self.wrap_auto_update(None, None)
        else:
            self.loop.remove_alarm(self.tmp_alarm)

    def draw_qb_select(self, commands: List[str]) -> None:
        self.gtfs_auto.set_state(False)
        menu_items = []
        for cmd in commands:
            menu_items.append(
                urwid.AttrMap(urwid.Button(cmd, on_press=self.draw_qb), None, focus_map="selection")
            )
        tmp_list_box = urwid.ListBox(urwid.SimpleListWalker(menu_items))
        list_layout = (urwid.Frame(
            tmp_list_box,
            header=urwid.AttrMap(urwid.Text('QuickBrowser Option'), 'colheads'),
            footer=urwid.Columns(
                [
                    ('weight', 1, urwid.Text(' ')),
                    ('pack', urwid.AttrMap(urwid.Text('↑↓: Select'), 'keys')),
                    ('pack', urwid.AttrMap(urwid.Text('ENT: Run'), 'keys')),
                    ('pack', urwid.AttrMap(urwid.Text('ESC: Close'), 'keys')),
                ],
                dividechars=1,
            ),
            focus_part='body'
        ))
        self.qb_select = urwid.Overlay(
            urwid.LineBox(list_layout),
            self.gtfs_view,
            align=urwid.CENTER,
            width=(urwid.RELATIVE, 75),
            valign=urwid.MIDDLE,
            height=(urwid.RELATIVE, 50),
            min_width=20,
            min_height=8,
        )
        self.loop.widget = self.qb_select

    def draw_qb(self, button) -> None:
        self.qb_command = button.get_label()
        qb = QuickBrowser(self.gtfs_proc_sock, self.gtfs_proc_deco, self.qb_command)
        self.quick_brw = qb.get_panel(self.gtfs_view, self)
        self.loop.widget = self.quick_brw

    def main_draw(self) -> None:
        palette = [
            # ('footerbar', 'white', 'black'),
            ('command', 'black', 'dark cyan'),
            ('control', 'black', 'dark cyan'),
            ('keys', 'light gray', 'dark blue'),
            ('error', 'white', 'dark red'),
            ('colheads', 'black', 'light gray'),
            ('indicator', 'brown', 'default'),
            ('selection', 'light red', 'dark blue'),
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
        self.gtfs_auto = urwid.CheckBox(u'Auto')
        urwid.connect_signal(self.gtfs_auto, 'change', self.auto_update)
        gtfs_foot = urwid.Pile([
            urwid.Divider(),
            urwid.Columns([
                ('pack', gtfs_prmp),
                urwid.AttrMap(self.gtfs_cmmd, 'command'),
                # ('pack', urwid.AttrMap(gtfs_sbmt, 'control')),
                ('pack', urwid.AttrMap(self.gtfs_auto, 'control')),
                ('pack', urwid.AttrMap(urwid.Text(u'F2: Focus'), 'keys')),
                ('pack', urwid.AttrMap(urwid.Text(u'F5: Run'), 'keys')),
                ('pack', urwid.AttrMap(urwid.Text(u'F8: Quit'), 'keys')),
            ], 1),
        ])

        # Execute the main-loop until the F8 key is pressed
        self.gtfs_view = urwid.Frame(self.scrl_zone, header=gtfs_head, footer=gtfs_foot)
        self.gtfs_view.focus_position = 'footer'
        self.loop = urwid.MainLoop(self.gtfs_view, palette, unhandled_input=self.fkey_handler)

        if self.start_cmd:
            self.gtfs_cmmd.set_edit_text(self.start_cmd)
            # self.update_response(None)

        self.loop.run()
