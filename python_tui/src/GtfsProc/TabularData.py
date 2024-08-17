import urwid
from typing import List, Optional, Tuple, Literal


class TabularData:
    def __init__(
            self,
            table_name: str,
            col_widths: List[int | None],
            header_names: List[str],
            cell_align: List[Literal["left", "center", "right"]],
            table_content: List[List[str]],
            commands: Optional[List[str]],
    ):
        self.table_name = table_name
        self.col_widths = col_widths
        self.header_names = header_names
        self.cell_align = cell_align
        self.table_content = table_content
        self.commands = commands

    def get_table_name(self) -> str:
        return self.table_name

    def get_columns_formatted(self) -> List[Tuple[str, int, any] | Tuple[str, any]]:
        headers = []
        for i in range(len(self.header_names)):
            if self.col_widths[i] is not None:
                headers.append((
                    'weight', self.col_widths[i],
                    urwid.AttrMap(urwid.Text(self.header_names[i]), 'colheads')
                ))
            else:
                headers.append((
                    'pack', urwid.AttrMap(urwid.Text(self.header_names[i]), 'colheads')
                ))
        return headers

    def get_table_content(self) -> List[List[Tuple[str, any] | Tuple[any]]]:
        contents = []
        for i in range(len(self.table_content)):
            sub_contents = []
            for j in range(len(self.table_content[i])):
                if self.col_widths[j] is not None:
                    sub_contents.append((
                        'weight', self.col_widths[j],
                        urwid.Text(self.table_content[i][j], wrap='ellipsis', align=self.cell_align[j]),
                    ))
                else:
                    sub_contents.append((
                        'pack', urwid.Text(self.table_content[i][j], wrap='ellipsis', align=self.cell_align[j]),
                    ))
            contents.append(sub_contents)
        return contents
