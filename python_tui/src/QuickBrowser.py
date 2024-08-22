import urwid

from GtfsProc import GtfsProcSocket, GtfsProcDecoder, CommonOutputs


class QuickBrowser:
    def __init__(self, gtfs_handler: GtfsProcSocket, gtfs_decoder: GtfsProcDecoder):
        self.gtfs_handler = gtfs_handler
        self.gtfs_decoder = gtfs_decoder

    def get_panel(self, command, parent_widget, dd) -> urwid.Overlay:
        resp = self.gtfs_handler.req_resp(command)
        if resp['error'] != 0:
            # Error response needs some padding and help to render unlike a ListBox
            tmp_list_box = urwid.Filler(
                urwid.AttrMap(urwid.Text(f'Error returned from GtfsProc: {resp["error"]}'), 'error'),
                "top"
            )
        else:
            # Making the layout from the output
            output_zone = CommonOutputs.decode(self.gtfs_decoder, dd, resp)
            tmp_list_box = urwid.ListBox(urwid.SimpleListWalker(output_zone))
        output_panel = (urwid.Frame(
            tmp_list_box,
            header=urwid.AttrMap(urwid.Text('QuickBrowser'), 'colheads'),
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
        qb_panel = urwid.Overlay(
            urwid.LineBox(output_panel),
            parent_widget,
            align=urwid.CENTER,
            width=(urwid.RELATIVE, 90),
            valign=urwid.MIDDLE,
            height=(urwid.RELATIVE, 90),
        )
        return qb_panel
