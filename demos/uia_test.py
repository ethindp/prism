import ctypes

import prism
from win32more import Guid
from win32more.Microsoft.UI.Xaml import RoutedEventArgs, Window
from win32more.Microsoft.UI.Xaml.Controls import Button, StackPanel, TextBox
from win32more.Windows.Win32.Foundation import HRESULT, HWND
from win32more.Windows.Win32.System.Com import IUnknown
from win32more.winui3 import XamlApplication


class IWindowNative(IUnknown):
    _iid_ = Guid("{eecdbf0e-bae9-4cb6-a68e-9598e1cb57bb}")


def get_hwnd_from_native(native_obj):
    GetWindowHandleType = ctypes.WINFUNCTYPE(
        HRESULT,
        ctypes.c_void_p,
        ctypes.POINTER(HWND),
    )
    lpVtbl = ctypes.cast(
        native_obj.value,
        ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p)),
    )
    func_addr = lpVtbl.contents[3]
    get_window_handle = GetWindowHandleType(func_addr)
    hwnd_out = HWND()
    hr = get_window_handle(native_obj.value, ctypes.byref(hwnd_out))
    if hr != 0:
        raise OSError(f"Failed to get WindowHandle. HRESULT: {hr}")
    return hwnd_out.value


class App(XamlApplication):
    def OnLaunched(self, args):
        self.win = Window()
        self.win.Title = "Prism UIA test"
        panel = StackPanel()
        panel.Spacing = 10
        self.text_input = TextBox()
        self.text_input.PlaceholderText = "Type something here..."
        btn = Button()
        btn.Content = "Speak"
        btn.Click += self.on_button_click
        panel.Children.Append(self.text_input)
        panel.Children.Append(btn)
        self.win.Content = panel
        native_window = self.win.as_(IWindowNative)
        self.ctx = prism.Context(get_hwnd_from_native(native_window))
        self.backend = self.ctx.create(prism.BackendId.UIA)
        self.win.Activate()

    def on_button_click(self, sender, e: RoutedEventArgs):
        text = self.text_input.Text
        self.backend.output(text)


XamlApplication.Start(App)
