import urwid

from GtfsProc import GtfsProcSocket, GtfsProcDecoder, CommonOutputs


class QuickBrowser:
    def __init__(self, gtfs_handler: GtfsProcSocket, gtfs_decoder: GtfsProcDecoder, command: str):
        self.gtfs_handler = gtfs_handler
        self.gtfs_decoder = gtfs_decoder
        self.command = command

    def get_panel(self, parent_widget, dd) -> urwid.Overlay:
        try:
            resp = self.gtfs_handler.req_resp(self.command)
        except Exception as ex:
            tmp_list_box = urwid.Filler(urwid.Text(f"Unexpected {ex=}, {type(ex)=})]"), "top")
            output_panel = (urwid.Frame(
                tmp_list_box,
                header=urwid.AttrMap(urwid.Text('EXCEPTION ENCOUNTERED'), 'error'),
                footer=urwid.Columns(
                    [
                        ('weight', 1, urwid.Text(' ')),
                        ('pack', urwid.AttrMap(urwid.Text('F7: To Main'), 'keys')),
                        ('pack', urwid.AttrMap(urwid.Text('ESC: Close'), 'keys')),
                    ],
                    dividechars=1,
                ),
                focus_part='body'
            ))
            qb_panel = urwid.Overlay(
                urwid.LineBox(output_panel),
                parent_widget,
                align=urwid.CENTER,
                width=(urwid.RELATIVE, 50),
                valign=urwid.MIDDLE,
                height=(urwid.RELATIVE, 20),
            )
            return qb_panel
        if resp['error'] != 0:
            # Error response needs some padding and help to render unlike a ListBox
            tmp_list_box = urwid.Filler(urwid.AttrMap(urwid.Text(
                f'GtfsProc error {resp["error"]}:\n{CommonOutputs.decode_error_number(resp["error"])}'
            ), 'error'), "top")
            command = 'ERROR'
        else:
            # Making the layout from the output
            output_zone = CommonOutputs.decode(self.gtfs_decoder, dd, resp)
            tmp_list_box = urwid.ListBox(urwid.SimpleListWalker(output_zone))
            command = resp['message_type']

        output_panel = (urwid.Frame(
            tmp_list_box,
            header=urwid.AttrMap(urwid.Text(
                f'QB // {command} // Proc: {resp["proc_time_ms"]} ms // {resp["message_time"]}'
            ), 'colheads'),
            footer=urwid.Columns(
                [
                    ('weight', 1, urwid.Text(' ')),
                    ('pack', urwid.AttrMap(urwid.Text('↑↓: Select'), 'keys')),
                    ('pack', urwid.AttrMap(urwid.Text('ENT: Get'), 'keys')),
                    ('pack', urwid.AttrMap(urwid.Text('F7: To Main'), 'keys')),
                    ('pack', urwid.AttrMap(urwid.Text('ESC: Close'), 'keys')),
                ],
                dividechars=1,
            ),
            focus_part='body'
        ))
        qb_panel = urwid.Overlay(
            urwid.LineBox(output_panel),
            parent_widget,
            align=urwid.CENTER,
            width=(urwid.RELATIVE, 90),
            valign=urwid.MIDDLE,
            height=(urwid.RELATIVE, 90),
        )
        return qb_panel
