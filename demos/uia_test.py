from typing import override

import prism
from win32more.Microsoft.UI.Xaml import RoutedEventArgs, Window
from win32more.Microsoft.UI.Xaml.Controls import Button, StackPanel, TextBox
from win32more.winui3 import XamlApplication


class App(XamlApplication):
    @override
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
        self.win.Activate()
        self.ctx = prism.Context()
        self.backend = self.ctx.create(prism.BackendId.UIA)

    @override
    def on_button_click(self, sender, e: RoutedEventArgs):
        text = self.text_input.Text
        self.backend.output(text)


XamlApplication.Start(App)
