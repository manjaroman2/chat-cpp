import enum 
import curses 
import curses.panel 
import curses.ascii
import math
from pynput.keyboard import Key, Listener


class CursorVisibility(enum.Enum):
    INVISIBLE = 0
    NORMAL = 1
    VERY_VISIBLE = 2

    def set(self):
        curses.curs_set(self.value)
    

class Mode(enum.Enum):
    COMMAND = enum.auto()
    CHAT = enum.auto()

    def text(self):
        return f"-- {self.name} --"  
        

class WindowWrapper:
    __ROOT__ = None 
    def __init__(self, method, box=True, height=0, width=0, starty=0, startx=0, xratio=None, yratio=None) -> None:
        if xratio:
            width = math.ceil(curses.COLS * xratio)
            startx = math.floor(curses.COLS * (1 - xratio))
        if yratio:
            height = math.ceil(curses.LINES * yratio)
            starty = math.floor(curses.LINES * (1 - yratio))
        self.win = method(height, width, starty, startx)
        self.win: curses.window = self.win
        self.has_box = box
    
    def __call__(self, title=None, title_color=1, post_clear=None) -> curses.window:
        self.title = title
        self.title_color = title_color
        self.post_clear = post_clear
        self.clear() 
        return self.win

    def clear(self, full_clear=True) -> None:
        if full_clear:
            self.win.clear()
        if self.has_box:
            self.win.box()
        if self.title:
           self.win.addstr(0, 1, f" {self.title} ", curses.color_pair(self.title_color) | curses.A_BOLD)
        if full_clear and self.post_clear:
            self.post_clear(self.win)
    
    @staticmethod 
    def setRoot(root: curses.window) -> curses.window:
        WindowWrapper.__ROOT__ = root
        return root


class WindowWrapperNewwin(WindowWrapper):
    def __init__(self, box=True, height=0, width=0, starty=0, startx=0, xratio=None, yratio=None) -> None:
        super().__init__(curses.newwin, box, height, width, starty, startx, xratio, yratio)
    

class WindowWrapperSubwin(WindowWrapper):
    def __init__(self, box=True, height=0, width=0, starty=0, startx=0, xratio=None, yratio=None) -> None:
        super().__init__(self.__class__.__ROOT__.subwin, box, height, width, starty, startx, xratio, yratio)


class GUI:
    def __init__(self) -> None:
        self.stopped = False
        self.typing_command = False
        self.mode = Mode.COMMAND
        self.command_buffer = []
        self.chat_buffer = []
        self.vratio = 1/3
        self.hratio = 1/4
        self.sidepanel_shown = True 
        self.chat_x = 1
        self.chat_y = 1

        self.is_shift = False
        
    def key_esc(self) -> bool:
        match self.mode:
            case Mode.CHAT:
                self.setmode(Mode.COMMAND)
                self.chat_buffer.clear()
                # self.chatwin_wrapper.title = self.chatwin_wrapper.title.lower()[:-1]
                self.chatwin_wrapper.clear()
                self.chatwin.refresh()
                self.reset_commandline()
                self.commandline.refresh()
                CursorVisibility.INVISIBLE.set()
            case Mode.COMMAND:
                if self.typing_command:
                    self.typing_command = False
                    self.reset_commandline()
                    self.commandline.refresh()
                    self.command_buffer.clear()
                else:
                    return True
        return False
    
    def key_enter(self) -> bool:
        match self.mode:
            case Mode.CHAT:
                if self.is_shift: 
                    self.chatwin.move(self.chatwin.getyx()[0] + 1, 1)
                    self.chat_buffer.append('\n')
                    self.chatwin.refresh()
                else: 
                    # TODO: send chat message
                    # self.mainwin_wrapper.clear()
                    # self.mainwin.addstr(1, 1, f"send chat {''.join(self.chat_buffer)}", curses.color_pair(1))
                    # self.mainwin.refresh()
                    self.chat_buffer.clear()
                    self.chatwin_wrapper.clear()
                    self.chatwin.refresh() # 2 refreshes are unfortunately necessary 
                    self.reset_commandline()
                    self.commandline.refresh() 
                    self.chatwin.move(1, 2) 
                    self.chatwin.refresh()
            case Mode.COMMAND:
                self.mainwin_wrapper.clear()
                self.mainwin.addstr(1, 1, f"process command {''.join(self.command_buffer)}", curses.color_pair(1))
                self.mainwin.refresh()
                self.reset_commandline()
                self.commandline.refresh()
                self.command_buffer.clear()
                self.typing_command = False
        return False

    def key_backspace(self) -> bool:
        match self.mode:
            case Mode.CHAT:
                if self.chat_buffer:
                    self.chat_buffer.pop()
                    y, x  = self.chatwin.getyx()
                    if x - 1 < 0: 
                        self.chatwin.move(y - 1, self.chatwin.getmaxyx()[1] - 1)
                        self.chatwin_wrapper.clear(full_clear=False)
                    else:
                        self.chatwin.delch(y, x - 1) 
                        self.chatwin.insch(' ', curses.color_pair(1))
                    
                    #chatwin_wrapper.clear()
                    #chatwin.addstr(1, 1, ">", curses.color_pair(1))
                    #chatwin.addstr(1, 2, ''.join(chat_buffer), curses.color_pair(1))
                    self.chatwin.refresh()
            case Mode.COMMAND:
                if self.command_buffer:
                    self.command_buffer.pop()
                    self.commandline.deleteln()
                    self.commandline.addstr(0, 0, ''.join(self.command_buffer), curses.color_pair(2))
                    self.commandline.refresh()
        return False

    def key_default(self, ch) -> bool:
        if curses.ascii.isprint(ch):
            ch = chr(ch)
            match self.mode:
                case Mode.CHAT:
                    if self.chatwin.getyx()[1] + 1 >= self.chatwin.getmaxyx()[1]:
                        self.chatwin.move(self.chatwin.getyx()[0] + 1, 1)
                    self.chat_buffer.append(ch)
                    self.chatwin.addch(ch, curses.color_pair(1))
                    self.reset_commandline()
                    self.commandline.refresh()
                    self.chatwin.move(self.chatwin.getyx()[0], self.chatwin.getyx()[1])
                    self.chatwin.refresh()
                case Mode.COMMAND:
                    if not self.typing_command:
                        if ch in self.command_mode_keybinds:
                            self.command_mode_keybinds[ch]()
                        else:
                            self.set_commandline(f"unknown key {ch}", 2)
                            self.commandline.refresh()
                    else:
                        self.command_buffer.append(ch)
                        self.set_commandline(''.join(self.command_buffer), 2)
                        self.commandline.refresh()
        else:
            self.set_commandline(f"unknown key {ch} (no ascii)", 2)
            self.commandline.refresh()
            
    def key_tab(self) -> bool:
        return 0 
    
    def handle_char(self, ch): 
        match ch: 
            case 27: # ESC 
                if self.key_esc():
                    return 
            case 10: # ENTER
                if self.key_enter():
                    return
            case curses.ascii.BS | curses.KEY_BACKSPACE:
                if self.key_backspace():
                    return
            case 9: # TAB 
                if self.key_tab():
                    return 
            case _: 
                if self.key_default(ch):
                    return 
        
    def set_commandline(self, text, color=2):
        self.commandline.deleteln()
        self.commandline.addstr(0, 0, text, curses.color_pair(color))

    def reset_commandline(self):
        self.set_commandline(self.mode.text(), 2)

    def setmode(self, mode):
        self.mode = mode
        self.reset_commandline()
        self.commandline.refresh()
    
    def enter_chatmode(self):
        self.chatwin.refresh()
        self.setmode(Mode.CHAT)
        self.chatwin.move(1, 2)
        self.chatwin.refresh()
        CursorVisibility.NORMAL.set()

    def toggle_sidewin(self):        
        if self.sidepanel_shown:
            self.sidewin_panel.hide()
            self.mainwin.resize(self.thatY, curses.COLS)
        else:
            self.sidewin_panel.show()
            self.mainwin.resize(self.thatY, curses.COLS - self.sidewin.getmaxyx()[1])
        self.sidepanel_shown = not self.sidepanel_shown
        curses.panel.update_panels()
        self.mainwin_wrapper.clear()
        self.mainwin.refresh()

    def start_typing_command(self): 
        self.typing_command = True
        self.command_buffer.append(':')
        self.set_commandline(''.join(self.command_buffer), 2)
        self.commandline.refresh()

    def run(self, root: curses.window):
        self.root = WindowWrapper.setRoot(root)
        self.root.clear()
        curses.set_escdelay(100)
        CursorVisibility.INVISIBLE.set()
        curses.use_default_colors()
        TEXT_COLOR = curses.COLOR_RED
        curses.init_pair(1, TEXT_COLOR, -1) # transparent background
        curses.init_pair(2, TEXT_COLOR, curses.COLOR_WHITE) # white background

        self.root.bkgdset(' ', curses.color_pair(1))
        
        self.chatwin_wrapper = WindowWrapperSubwin(width=curses.COLS, yratio=self.vratio)
        self.chatwin = self.chatwin_wrapper("chat", 1, lambda win: win.addstr(1, 1, ">", curses.color_pair(1)))
        self.thatY = curses.LINES - self.chatwin.getmaxyx()[0]

        self.sidewin_wrapper = WindowWrapperNewwin(height=self.thatY, xratio=self.hratio)
        self.sidewin = self.sidewin_wrapper("side", 1)
        self.sidewin_panel = curses.panel.new_panel(self.sidewin)

        self.mainwin_wrapper = WindowWrapperSubwin(height=self.thatY, width=curses.COLS - self.sidewin.getmaxyx()[1])
        self.mainwin = self.mainwin_wrapper("main", 1)

        self.commandline = curses.newwin(1, curses.COLS, curses.LINES - 1, 0) 
        self.commandline.bkgdset(' ', curses.color_pair(2))
        
        self.reset_commandline()

        curses.panel.update_panels()
        self.root.refresh()
        self.commandline.refresh()

        def on_press(key):
            if key == Key.shift:
                self.is_shift = True

        def on_release(key):
            if key == Key.shift:
                self.is_shift = False

        self.command_mode_keybinds = {
            's': self.toggle_sidewin,
            'c': self.enter_chatmode, 
            ':': self.start_typing_command,
        }
        
        self.command_buffer = []
        self.chat_buffer = []
        with Listener(on_press=on_press, on_release=on_release) as listener:
            while not self.stopped:
                ch = root.getch()
                self.handle_char(ch)
            listener.join()


gui = GUI()
curses.wrapper(gui.run)