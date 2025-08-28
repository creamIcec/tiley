import tkinter as tk
from tkinter import ttk, messagebox
import json
from pathlib import Path
import subprocess
from ttkthemes import ThemedTk

CONFIG_FILE = Path.home() / ".config" / "tiley" / "hotkey.json"
IMPORTANT_ACTIONS = {
    "launch_terminal": "Launch Terminal",
    "launch_app_launcher": "Launch Application Launcher",
    "close_window": "Close Current Window",
    "quit_compositor": "Quit Tiley (Logout)",
    "lock_screen": "Lock Screen",
    "toggle_floating": "Toggle Window Floating/Tiling",
    "fullscreen_toggle": "Toggle Window Fullscreen",
    "change_wallpaper": "Change Wallpaper",
    "screenshot": "Take Screenshot",
    "focus_up": "Focus Up",
    "focus_down": "Focus Down",
    "focus_left": "Focus Left",
    "focus_right": "Focus Right",
}

class KeybindingEditor:
    def __init__(self, root):
        self.root = root
        self.root.title("Tiley Keyboard Shortcuts Configuration")
        self.root.minsize(500, 600)

        self.all_bindings = {}
        self.key_vars = {}
        self.entry_widgets = {}
        self.is_recording = False
        self.recording_action = None

        self.load_bindings()
        self.create_widgets()
        
        self.root.bind("<KeyPress>", self.on_key_press)
        self.root.bind("<Button-1>", self.on_window_click)

    def load_bindings(self):
        try:
            with open(CONFIG_FILE, 'r') as f:
                loaded_json = json.load(f)
                self.all_bindings = {v: k for k, v in loaded_json.items()}
        except (FileNotFoundError, json.JSONDecodeError):
            self.all_bindings = {}

    def create_widgets(self):
        main_frame = ttk.Frame(self.root, padding="15 20")
        main_frame.pack(expand=True, fill="both")
        main_frame.columnconfigure(1, weight=1)

        title_label = ttk.Label(main_frame, text="Tiley Core Shortcuts", font=("Helvetica", 18, "bold"))
        title_label.grid(row=0, column=0, columnspan=2, pady=(0, 20), sticky="w")
        
        help_label = ttk.Label(main_frame, text="Click input field to start recording shortcut, press ESC to cancel", 
                              font=("Helvetica", 9), foreground="gray")
        help_label.grid(row=1, column=0, columnspan=2, pady=(0, 10), sticky="w")

        self.style = ttk.Style()
        try:
            self.style.configure("Recording.TEntry", fieldbackground="#e8f4fd")
        except:
            pass

        row_index = 2
        for action, description in IMPORTANT_ACTIONS.items():
            label = ttk.Label(main_frame, text=f"{description}:", font=("Helvetica", 11))
            label.grid(row=row_index, column=0, sticky="w", padx=(0, 10), pady=8)

            key_var = tk.StringVar()
            current_key = self.all_bindings.get(action, "Not bound")
            key_var.set(current_key)
            self.key_vars[action] = key_var

            entry = ttk.Entry(main_frame, textvariable=key_var, font=("Helvetica", 10))
            entry.grid(row=row_index, column=1, sticky="ew", pady=8)
            entry.config(state="readonly")
            
            entry.bind("<Button-1>", lambda e, act=action: self.on_entry_click(e, act))
            
            self.entry_widgets[action] = entry
            
            row_index += 1

        button_frame = ttk.Frame(main_frame)
        button_frame.grid(row=row_index, column=0, columnspan=2, pady=(30, 0), sticky="e")
        
        save_button = ttk.Button(button_frame, text="Save & Apply", command=self.on_save, style="Accent.TButton")
        save_button.pack(side="right", padx=(10, 0))

        quit_button = ttk.Button(button_frame, text="Exit", command=self.root.quit)
        quit_button.pack(side="right")

    def on_entry_click(self, event, action):
        self.start_recording(action)
        return "break"

    def start_recording(self, action):
        if self.is_recording:
            self.stop_recording(restore_original=True)

        self.is_recording = True
        self.recording_action = action
        
        entry = self.entry_widgets[action]
        
        entry.config(state="normal")
        entry.delete(0, tk.END)
        entry.insert(0, "Press a key...")
        entry.config(state="readonly")
        
        try:
            entry.configure(style="Recording.TEntry")
        except:
            pass
        
        self.root.grab_set()
        entry.focus_set()

    def stop_recording(self, restore_original=False):
        if not self.is_recording:
            return

        action = self.recording_action
        entry = self.entry_widgets[action]
        key_var = self.key_vars[action]
        
        if restore_original:
            original_value = self.all_bindings.get(action, "Not bound")
            key_var.set(original_value)
        
        display_value = key_var.get()
        
        if not display_value or display_value.strip() == "":
            display_value = "Not bound"
            key_var.set(display_value)
        
        entry.config(state="normal")
        entry.delete(0, tk.END)
        entry.insert(0, display_value)
        entry.config(state="readonly")
        
        try:
            entry.configure(style="TEntry")
        except:
            pass

        self.is_recording = False
        self.recording_action = None
        self.root.grab_release()

    def on_key_press(self, event):
        if not self.is_recording:
            return

        if event.keysym in ('Control_L', 'Control_R', 'Alt_L', 'Alt_R', 'Shift_L', 'Shift_R', 'Super_L', 'Super_R'):
            return

        if event.keysym == 'Escape':
            self.stop_recording(restore_original=True)
            return "break"

        key_string = self.format_key_event(event)
        
        if not key_string or key_string.strip() == "":
            self.stop_recording(restore_original=True)
            return "break"
        
        self.key_vars[self.recording_action].set(key_string)
        self.stop_recording(restore_original=False)
        
        return "break"

    def on_window_click(self, event):
        if self.is_recording and event.widget not in self.entry_widgets.values():
            self.stop_recording(restore_original=True)

    def format_key_event(self, event):
        modifiers = []
        
        if event.state & 0x4: modifiers.append("ctrl")
        if event.state & 0x1: modifiers.append("shift")
        if event.state & 0x8 or event.state & 0x80: modifiers.append("alt")
        if event.state & 0x40: modifiers.append("super")

        key = event.keysym
        
        key_mapping = {
            'space': 'Space',
            'Return': 'Return',
            'Tab': 'Tab',
            'BackSpace': 'BackSpace',
            'Delete': 'Delete',
            'Insert': 'Insert',
            'Home': 'Home',
            'End': 'End',
            'Page_Up': 'Page_Up',
            'Page_Down': 'Page_Down',
            'Up': 'Up',
            'Down': 'Down',
            'Left': 'Left',
            'Right': 'Right',
            'F1': 'F1', 'F2': 'F2', 'F3': 'F3', 'F4': 'F4',
            'F5': 'F5', 'F6': 'F6', 'F7': 'F7', 'F8': 'F8',
            'F9': 'F9', 'F10': 'F10', 'F11': 'F11', 'F12': 'F12'
        }
        
        if key in key_mapping:
            key = key_mapping[key]
        elif key.endswith(("_L", "_R")):
            key = key[:-2]
        elif len(key) == 1:
            pass
        elif len(key) > 1:
            key = key.capitalize()
        
        if not key or key in ('', 'NoSymbol'):
            return ""
        
        if modifiers:
            result = "+".join(modifiers + [key])
        else:
            result = key
            
        return result

    def on_save(self):
        final_json_output = {}
        
        for action, key in self.all_bindings.items():
            if action not in IMPORTANT_ACTIONS:
                final_json_output[key] = action

        for action, key_var in self.key_vars.items():
            new_key = key_var.get().strip()
            if new_key and new_key != "Not bound":
                final_json_output[new_key] = action
        
        try:
            CONFIG_FILE.parent.mkdir(parents=True, exist_ok=True)
            with open(CONFIG_FILE, 'w') as f:
                json.dump(final_json_output, f, indent=2, sort_keys=True)
            
            messagebox.showinfo("Success", f"Configuration saved successfully to\n{CONFIG_FILE}")
            self.notify_tiley()
        except Exception as e:
            messagebox.showerror("Error", f"Failed to save file: {e}")

    def notify_tiley(self):
        try:
            result = subprocess.run(["pkill", "-SIGHUP", "-f", "tiley"], check=False, capture_output=True)
            if result.returncode == 0:
                messagebox.showinfo("Applied Successfully", "Tiley has been notified and reloaded the shortcuts!")
            else:
                messagebox.showwarning("Notice", "No running Tiley process found.\nConfiguration saved, will take effect when Tiley is next started.")
        except Exception as e:
            messagebox.showerror("Error", f"Failed to notify Tiley: {e}")

if __name__ == "__main__":
    root = ThemedTk(theme="arc")
    
    style = ttk.Style()
    style.configure("Accent.TButton", font=("Helvetica", 10, "bold"))

    app = KeybindingEditor(root)
    root.mainloop()
