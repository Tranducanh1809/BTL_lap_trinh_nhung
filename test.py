import tkinter as tk
from tkinter import messagebox
import serial
import threading
import queue
from datetime import datetime

import customtkinter as ctk

# ==================== DESIGN TOKENS ====================
DEFAULT_PORT = "COM12"
BAUDRATE = 115200

ctk.set_appearance_mode("Dark")
ctk.set_default_color_theme("blue")

RADIUS_CARD = 12
RADIUS_CTRL = 8

BG_DARK       = "#0B1220"
CARD_BG       = "#151E2E"
CARD_BG_HOVER = "#1C2740"
CARD_RAISED   = "#1F2A3F"

TEXT_HI   = "#F1F5F9"
TEXT_MID  = "#94A3B8"
TEXT_LO   = "#64748B"

ACCENT    = "#3B82F6"
ACCENT_H  = "#2563EB"

C_OK      = "#22C55E"
C_OK_H    = "#16A34A"
C_WARN    = "#F59E0B"
C_ERR     = "#EF4444"
C_ERR_H   = "#DC2626"

LOG_TX   = "#38BDF8"
LOG_FG   = "#94A3B8"
LOG_OK   = "#34D399"
LOG_ERR  = "#F87171"
LOG_WARN = "#FBBF24"

FONT_MAIN = "Segoe UI"
FONT_MONO = "Consolas"

# ==================== STATE ====================
ser = None
rx_queue = queue.Queue()
running = True
card_list_active = False

prev_door_state = None
pending_open_actor = None
pending_close_actor = None

_pulse = {"i": 0, "running": False}
_PULSE_COLORS = ["#22C55E", "#16A34A", "#15803D", "#16A34A"]

_current_toast = None


# ==================== SERIAL LOGIC ====================
def auto_connect_serial():
    global ser
    try:
        ser = serial.Serial(DEFAULT_PORT, BAUDRATE, timeout=0.2)
        status_conn.configure(text=f"Đã kết nối · {DEFAULT_PORT}", text_color=C_OK)
        threading.Thread(target=read_serial, daemon=True).start()
        start_pulse()
        log_message(f"[HỆ THỐNG] Tự động kết nối thành công {DEFAULT_PORT}", "ok")
        show_toast(f"Đã kết nối {DEFAULT_PORT}", "success")
    except Exception as e:
        ser = None
        status_conn.configure(text=f"Lỗi kết nối · {DEFAULT_PORT}", text_color=C_ERR)
        stop_pulse()
        log_message(f"[HỆ THỐNG] Không thể kết nối {DEFAULT_PORT}: {str(e)}", "err")
        show_toast(f"Không thể kết nối {DEFAULT_PORT}", "error", duration=4000)


def disconnect_serial():
    global ser
    if ser:
        try:
            ser.close()
        except Exception:
            pass
    ser = None
    status_conn.configure(text="Chưa kết nối", text_color=TEXT_MID)
    stop_pulse()
    log_message("[HỆ THỐNG] Đã ngắt kết nối", "warn")


def read_serial():
    while ser and ser.is_open:
        try:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
            if line:
                rx_queue.put(line)
        except Exception:
            break


def send_command(command):
    if not ser or not ser.is_open:
        show_toast(f"Chưa có kết nối {DEFAULT_PORT}", "warning")
        return
    try:
        ser.write((command + "\r\n").encode("utf-8"))
        log_message(f"→ {command}", "tx")
    except Exception as e:
        show_toast(f"Lỗi UART: {e}", "error")


# ==================== PULSE (chấm kết nối nhấp nháy) ====================
def _pulse_step():
    if not _pulse["running"]:
        return
    _pulse["i"] = (_pulse["i"] + 1) % len(_PULSE_COLORS)
    status_dot.configure(text_color=_PULSE_COLORS[_pulse["i"]])
    root.after(600, _pulse_step)


def start_pulse():
    _pulse["running"] = True
    _pulse["i"] = 0
    _pulse_step()


def stop_pulse():
    _pulse["running"] = False
    status_dot.configure(text_color=C_ERR)


# ==================== TOAST ====================
def show_toast(message, kind="info", duration=2400):
    global _current_toast
    if _current_toast is not None:
        try:
            _current_toast.destroy()
        except Exception:
            pass

    colors = {"info": ACCENT, "success": C_OK, "error": C_ERR, "warning": C_WARN}
    icons  = {"info": "ℹ",  "success": "✓", "error": "✕", "warning": "⚠"}

    toast = ctk.CTkFrame(root, corner_radius=RADIUS_CTRL,
                         fg_color=colors.get(kind, ACCENT), height=44)
    toast.place(relx=0.5, rely=1.0, anchor="s", y=-20)
    toast.lift()

    ctk.CTkLabel(
        toast, text=f"{icons.get(kind,'')}   {message}",
        text_color="white", font=(FONT_MAIN, 11, "bold"),
    ).pack(padx=22, pady=10)

    _current_toast = toast
    toast.after(duration, lambda: _safe_destroy(toast))


def _safe_destroy(w):
    try:
        w.destroy()
    except Exception:
        pass


# ==================== CUSTOM CONFIRM ====================
def custom_confirm(title, message, on_yes):
    dlg = ctk.CTkToplevel(root)
    dlg.title(title)
    dlg.configure(fg_color=BG_DARK)
    dlg.resizable(False, False)
    dlg.transient(root)
    dlg.grab_set()

    ctk.CTkLabel(
        dlg, text=message, font=(FONT_MAIN, 12),
        text_color=TEXT_HI, wraplength=340, justify="left",
    ).pack(pady=(24, 20), padx=24)

    row = ctk.CTkFrame(dlg, fg_color="transparent")
    row.pack(pady=(0, 20))

    ctk.CTkButton(
        row, text="Hủy", command=dlg.destroy,
        width=110, height=34, corner_radius=RADIUS_CTRL,
        fg_color=CARD_RAISED, hover_color=CARD_BG_HOVER,
        text_color=TEXT_HI, font=(FONT_MAIN, 11, "bold"),
    ).pack(side="left", padx=6)

    def _yes():
        dlg.destroy()
        on_yes()

    ctk.CTkButton(
        row, text="Xác nhận", command=_yes,
        width=110, height=34, corner_radius=RADIUS_CTRL,
        fg_color=C_ERR, hover_color=C_ERR_H,
        font=(FONT_MAIN, 11, "bold"),
    ).pack(side="left", padx=6)

    dlg.update_idletasks()
    w, h = 380, dlg.winfo_reqheight()
    x = root.winfo_x() + (root.winfo_width()  - w) // 2
    y = root.winfo_y() + (root.winfo_height() - h) // 2
    dlg.geometry(f"{w}x{h}+{x}+{y}")


# ==================== ACTIONS ====================
def set_auto_close_time():
    seconds = auto_close_seconds.get()
    if not 1 <= seconds <= 120:
        show_toast("Thời gian phải trong khoảng 1–120 giây", "warning")
        return
    send_command(f"AUTO_CLOSE:{seconds}")


def start_add_card():
    send_command("ADD_CARD")


def cancel_add_card():
    send_command("CANCEL_ADD")


def refresh_card_list():
    send_command("LIST_CARDS")


def selected_card_uid():
    selection = card_listbox.curselection()
    if selection:
        return card_listbox.get(selection[0]).split("|", 1)[0].strip().upper()
    return uid_entry.get().strip().replace(" ", "").upper()


def save_selected_card_name():
    uid = selected_card_uid()
    name = card_name_entry.get().strip()
    if len(uid) != 8:
        show_toast("Chọn thẻ trong danh sách hoặc nhập UID", "warning")
        return
    try:
        int(uid, 16)
    except ValueError:
        show_toast("UID phải là 8 ký tự hex (vd: 638BCA06)", "warning")
        return
    if not name or len(name.encode("utf-8")) > 19:
        show_toast("Tên không được trống, tối đa 19 byte UTF-8", "warning")
        return
    if any(ch in name for ch in ":\r\n"):
        show_toast("Tên không được chứa dấu ':' hoặc xuống dòng", "warning")
        return
    try:
        name.encode("ascii")
    except UnicodeEncodeError:
        show_toast("Tên chưa hỗ trợ dấu — nhập không dấu (vd: Nguyen Van A)", "warning")
        return
    send_command(f"SET_CARD_NAME:{uid}:{name}")

def toggle_auto_close():
    if auto_close_enabled.get():
        send_command("AUTO_CLOSE_ON")
    else:
        send_command("AUTO_CLOSE_OFF")

def delete_selected_card():
    selection = card_listbox.curselection()
    uid = uid_entry.get().strip().replace(" ", "").upper()
    if not uid and selection:
        uid = card_listbox.get(selection[0]).split("|", 1)[0].strip().upper()
    if len(uid) != 8:
        show_toast("UID phải gồm 8 ký tự hex", "warning")
        return
    try:
        int(uid, 16)
    except ValueError:
        show_toast("UID chỉ chứa ký tự 0-9 và A-F", "warning")
        return
    custom_confirm(
        "Xác nhận xóa",
        f"Bạn có chắc muốn xóa thẻ {uid} khỏi danh sách?",
        lambda: send_command("DELETE_CARD:" + uid),
    )


# ==================== LOG ====================
def log_message(message, tag="rx"):
    ts = datetime.now().strftime("%H:%M:%S")
    prefix = {"tx": "TX ", "rx": "RX ", "ok": "OK ", "err": "ERR",
              "warn": "WRN"}.get(tag, "── ")
    log_box.configure(state="normal")
    log_box.insert("end", f"{ts}  [{prefix}]  {message}\n", tag)
    log_box.see("end")
    log_box.configure(state="disabled")


def clear_log():
    log_box.configure(state="normal")
    log_box.delete("1.0", "end")
    log_box.configure(state="disabled")


# ==================== HISTORY (timeline) ====================
def _scroll_history_bottom():
    try:
        canvas = getattr(history_timeline, "_parent_canvas", None)
        if canvas is not None:
            canvas.yview_moveto(1.0)
    except Exception:
        pass


def add_history_entry(state_text, actor, is_open):
    now = datetime.now().strftime("%H:%M:%S")
    color = C_OK if is_open else C_ERR
    icon = "▶" if is_open else "◀"

    row = ctk.CTkFrame(history_timeline, fg_color="transparent", height=28)
    row.pack(fill="x", pady=1)
    row.pack_propagate(False)

    ctk.CTkLabel(row, text=icon, font=(FONT_MAIN, 12), text_color=color,
                 width=24, anchor="center").pack(side="left")
    ctk.CTkLabel(row, text=now, font=(FONT_MONO, 10), text_color=TEXT_LO,
                 width=76, anchor="w").pack(side="left")
    ctk.CTkLabel(row, text=state_text, font=(FONT_MAIN, 10, "bold"),
                 text_color=color, width=90, anchor="w").pack(side="left")
    ctk.CTkLabel(row, text=actor, font=(FONT_MAIN, 10), text_color=TEXT_MID,
                 anchor="w").pack(side="left", fill="x", expand=True, padx=(0, 8))

    children = history_timeline.winfo_children()
    if len(children) > 100:
        children[0].destroy()

    update_history_empty_state()
    history_timeline.after(20, _scroll_history_bottom)


def clear_history():
    for child in history_timeline.winfo_children():
        child.destroy()
    update_history_empty_state()


def update_history_empty_state():
    if not history_timeline.winfo_children():
        history_empty_label.place(relx=0.5, rely=0.5, anchor="center")
    else:
        history_empty_label.place_forget()


def update_list_empty_state():
    if card_listbox.size() == 0:
        list_empty_label.place(relx=0.5, rely=0.5, anchor="center")
    else:
        list_empty_label.place_forget()


# ==================== DOOR STATE ====================
DOOR_VISUALS = {
    "CLOSED":    ("▣", "Đã đóng",           C_ERR),
    "OPENING":   ("◐", "Đang mở...",        C_WARN),
    "OPEN":      ("▢", "Đang mở hoàn toàn", C_OK),
    "CLOSING":   ("◑", "Đang đóng...",      C_WARN),
    "REOPENING": ("◐", "Đang mở lại...",    C_WARN),
}


def set_door_state(state):
    icon, text, color = DOOR_VISUALS.get(state, ("?", state, TEXT_MID))
    state_icon.configure(text=icon, text_color=color)
    state_label.configure(text=text, text_color=color)


# ==================== RX PROCESSING ====================
def process_received_data():
    global card_list_active, prev_door_state, pending_open_actor, pending_close_actor
    while not rx_queue.empty():
        line = rx_queue.get()

        if line.startswith("ACK:") or line.startswith("CARD:OK") or line.startswith("CARD_OK:"):
            log_message(f"← {line}", "ok")
        elif (line.startswith("ERR:") or line.startswith("WARNING:")
              or line.startswith("CARD:WRONG") or line.startswith("CARD_WRONG:")):
            log_message(f"← {line}", "err")
        else:
            log_message(f"← {line}", "rx")

        # Track actors
        if line == "ACK:OPEN":
            pending_open_actor = "Máy tính (nút mở)"
        elif line == "ACK:CLOSE":
            pending_close_actor = "Máy tính (nút đóng)"
        elif line.startswith("CARD_OK:"):
            parts = line.split(":", 2)
            if len(parts) >= 2:
                uid = parts[1].strip().upper()
                name = parts[2].strip() if len(parts) >= 3 else ""
                if name and name != "Chua dat ten":
                    pending_open_actor = f"Thẻ RFID {uid} — {name}"
                else:
                    pending_open_actor = f"Thẻ RFID {uid}"
        elif line == "WARNING:OBSTACLE":
            pending_open_actor = "Vật cản (đảo chiều an toàn)"
        elif line == "BTN:OPEN":
            pending_open_actor = "Nút nhấn (mở cửa)"
        elif line == "BTN:CLOSE":
            pending_close_actor = "Nút nhấn (đóng cửa)"

        # Door state changes
        if line.startswith("STATE:"):
            state = line.split(":", 1)[1]
            set_door_state(state)

            if prev_door_state is not None:
                if prev_door_state != "OPEN" and state == "OPEN":
                    actor = pending_open_actor or "Tự động (mở lại)"
                    add_history_entry("Cửa mở", actor, True)
                    pending_open_actor = None
                elif prev_door_state != "CLOSED" and state == "CLOSED":
                    actor = pending_close_actor or "Tự động (hết giờ)"
                    add_history_entry("Cửa đóng", actor, False)
                    pending_close_actor = None
            prev_door_state = state

        elif line == "CARD:OK":
            event_label.configure(text="✓  Thẻ RFID hợp lệ", text_color=C_OK)
        elif line == "CARD:WRONG":
            event_label.configure(text="✕  Thẻ RFID không hợp lệ", text_color=C_ERR)
        elif line == "WARNING:OBSTACLE":
            event_label.configure(text="⚠  Cảnh báo: Có vật cản!", text_color=C_ERR)
        elif line == "WARNING:RFID_LOCKED":
            event_label.configure(text="⚠  RFID đang bị khóa", text_color=C_ERR)
        elif line.startswith("ERR:"):
            event_label.configure(text="⚠  Không thể thực hiện lệnh", text_color=C_ERR)
            if line == "ERR:CARD_EXISTS":
                card_manage_status.configure(text="Thẻ này đã có trong danh sách.", text_color=C_WARN)
            elif line == "ERR:CARD_LIST_FULL":
                card_manage_status.configure(text="Danh sách thẻ đã đầy.", text_color=C_ERR)
            elif line == "ERR:CARD_NOT_FOUND":
                card_manage_status.configure(text="Không tìm thấy UID để xóa.", text_color=C_ERR)
            elif line == "ERR:LAST_CARD":
                card_manage_status.configure(text="Không thể xóa thẻ hợp lệ cuối cùng.", text_color=C_ERR)
            elif line == "ERR:INVALID_UID":
                card_manage_status.configure(text="UID không hợp lệ.", text_color=C_ERR)

        elif line == "ADD_MODE:ON":
            card_manage_status.configure(text="Đang chờ quẹt thẻ vào đầu đọc RC522...", text_color=C_WARN)
        elif line == "ADD_MODE:OFF":
            card_manage_status.configure(text="Đã hủy chế độ thêm thẻ.", text_color=TEXT_MID)
        elif line.startswith("CARD_ADDED:"):
            uid = line.split(":", 1)[1].strip().upper()
            card_manage_status.configure(
                text=f"Đã thêm thẻ {uid} — nhập tên rồi bấm Lưu tên.",
                text_color=C_OK,
            )
            refresh_card_list()
        elif line == "CARD_LIST:BEGIN":
            card_list_active = True
            card_listbox.delete(0, "end")
            card_manage_status.configure(text="Đang tải danh sách thẻ...", text_color=TEXT_MID)
        elif line == "CARD_LIST:END":
            card_list_active = False
            card_manage_status.configure(
                text=f"Danh sách có {card_listbox.size()} thẻ.",
                text_color=C_OK,
            )
            update_list_empty_state()
        elif line == "ACK:CARD_NAME_SAVED":
            card_manage_status.configure(text="Đã lưu tên thẻ vào Flash STM32.", text_color=C_OK)
            refresh_card_list()
        elif line == "ERR:FLASH_SAVE_FAILED":
            card_manage_status.configure(
                text="Lỗi lưu Flash STM32 — kiểm tra cấu hình linker.",
                text_color=C_ERR,
            )
        elif line.startswith("CARD:") and line not in ("CARD:OK", "CARD:WRONG"):
            parts = line.split(":", 2)
            uid = parts[1].strip().upper() if len(parts) >= 2 else ""
            if card_list_active and len(uid) == 8:
                name = parts[2].strip() if len(parts) == 3 else "Chua dat ten"
                card_listbox.insert("end", f"{uid}  |  {name}")

    root.after(80, process_received_data)


def on_close():
    global running
    running = False
    disconnect_serial()
    root.destroy()


# ==================== UI BUILD ====================
root = ctk.CTk()
root.title("Smart Door Control Panel")
root.geometry("1020x860")
root.minsize(920, 720)
root.configure(fg_color=BG_DARK)


def make_card(parent, title, icon="▸"):
    card = ctk.CTkFrame(parent, corner_radius=RADIUS_CARD, fg_color=CARD_BG)
    head = ctk.CTkFrame(card, fg_color="transparent", height=28)
    head.pack(fill="x", padx=16, pady=(12, 4))
    head.pack_propagate(False)
    ctk.CTkLabel(
        head, text=f"{icon}  {title.upper()}",
        font=(FONT_MAIN, 10, "bold"), text_color=TEXT_MID,
    ).pack(side="left")
    body = ctk.CTkFrame(card, fg_color="transparent")
    body.pack(fill="both", expand=True, padx=16, pady=(0, 16))
    return card, body


# --- HEADER ---
header = ctk.CTkFrame(root, height=60, corner_radius=0, fg_color=BG_DARK)
header.pack(fill="x", side="top")
header.pack_propagate(False)

title_box = ctk.CTkFrame(header, fg_color="transparent")
title_box.pack(side="left", padx=20)

ctk.CTkLabel(
    title_box, text="SMART DOOR  ·  CONTROL PANEL",
    font=(FONT_MAIN, 15, "bold"), text_color=TEXT_HI,
).pack(anchor="w", pady=(10, 0))

ctk.CTkLabel(
    title_box, text="Giao tiếp STM32 UART — Quản lý truy cập & Thẻ RFID",
    font=(FONT_MAIN, 10), text_color=TEXT_MID,
).pack(anchor="w")

hdr_right = ctk.CTkFrame(header, fg_color="transparent")
hdr_right.pack(side="right", padx=20)

clock_label = ctk.CTkLabel(
    hdr_right, text="", font=(FONT_MONO, 11), text_color=TEXT_MID,
)
clock_label.pack(side="right", padx=(16, 0))

status_conn = ctk.CTkLabel(
    hdr_right, text="Chưa kết nối",
    font=(FONT_MAIN, 11, "bold"), text_color=TEXT_MID,
)
status_conn.pack(side="right", padx=(6, 0))

status_dot = ctk.CTkLabel(
    hdr_right, text="●", font=(FONT_MAIN, 14), text_color=C_ERR,
)
status_dot.pack(side="right")


def update_clock():
    now = datetime.now()
    clock_label.configure(text=now.strftime("%H:%M:%S  ·  %d/%m/%Y"))
    root.after(1000, update_clock)


# --- BODY ---
main_scroll = ctk.CTkScrollableFrame(root, fg_color="transparent")
main_scroll.pack(fill="both", expand=True, padx=16, pady=16)
main_scroll.columnconfigure(0, weight=1, uniform="col")
main_scroll.columnconfigure(1, weight=1, uniform="col")

left_col = ctk.CTkFrame(main_scroll, fg_color="transparent")
left_col.grid(row=0, column=0, sticky="nsew", padx=(0, 8))

right_col = ctk.CTkFrame(main_scroll, fg_color="transparent")
right_col.grid(row=0, column=1, sticky="nsew", padx=(8, 0))

# ==================== LEFT: DOOR ====================
card_door, door_body = make_card(left_col, "Bảng điều khiển cửa")
card_door.pack(fill="x", pady=(0, 12))

state_badge = ctk.CTkFrame(
    door_body, fg_color=CARD_RAISED, corner_radius=RADIUS_CTRL, height=112,
)
state_badge.pack(fill="x", pady=(4, 12))
state_badge.pack_propagate(False)

state_icon = ctk.CTkLabel(
    state_badge, text="◯", font=(FONT_MAIN, 40), text_color=TEXT_LO,
)
state_icon.pack(pady=(14, 0))

state_label = ctk.CTkLabel(
    state_badge, text="Chưa có dữ liệu",
    font=(FONT_MAIN, 12, "bold"), text_color=TEXT_MID,
)
state_label.pack(pady=(0, 14))

btn_grid = ctk.CTkFrame(door_body, fg_color="transparent")
btn_grid.pack(fill="x", pady=(0, 8))
btn_grid.columnconfigure(0, weight=1)
btn_grid.columnconfigure(1, weight=1)

ctk.CTkButton(
    btn_grid, text="MỞ CỬA", command=lambda: send_command("OPEN"),
    height=46, corner_radius=RADIUS_CTRL,
    fg_color=C_OK, hover_color=C_OK_H,
    font=(FONT_MAIN, 12, "bold"),
).grid(row=0, column=0, sticky="ew", padx=(0, 4))

ctk.CTkButton(
    btn_grid, text="ĐÓNG CỬA", command=lambda: send_command("CLOSE"),
    height=46, corner_radius=RADIUS_CTRL,
    fg_color=C_ERR, hover_color=C_ERR_H,
    font=(FONT_MAIN, 12, "bold"),
).grid(row=0, column=1, sticky="ew", padx=(4, 0))

ctk.CTkButton(
    door_body, text="⟳   Cập nhật trạng thái", command=lambda: send_command("STATUS"),
    height=34, corner_radius=RADIUS_CTRL,
    fg_color=CARD_RAISED, hover_color=CARD_BG_HOVER,
    text_color=TEXT_HI, font=(FONT_MAIN, 11),
).pack(fill="x")

# ---- AUTO CLOSE ----
card_auto, auto_body = make_card(left_col, "Tự động đóng cửa")
card_auto.pack(fill="x", pady=(0, 12))

auto_close_enabled = ctk.BooleanVar(value=True)

auto_close_switch = ctk.CTkSwitch(
    auto_body,
    text=" Tự động đóng cửa",
    variable=auto_close_enabled,
    command=toggle_auto_close,
    font=(FONT_MAIN, 11, "bold"),
    progress_color=C_OK,
    text_color=TEXT_HI
)
auto_close_switch.pack(anchor="w", pady=(4, 10))

auto_close_seconds = ctk.IntVar(value=10)

row_time = ctk.CTkFrame(auto_body, fg_color="transparent")
row_time.pack(fill="x", pady=(4, 8))

ctk.CTkLabel(
    row_time, text="Thời gian chờ", font=(FONT_MAIN, 11), text_color=TEXT_MID,
).pack(side="left")

ctk.CTkLabel(
    row_time, text="giây", font=(FONT_MAIN, 10), text_color=TEXT_LO,
).pack(side="right", padx=(0, 8))

ctk.CTkButton(
    row_time, text="+", width=36, height=36, corner_radius=RADIUS_CTRL,
    fg_color=CARD_RAISED, hover_color=CARD_BG_HOVER,
    text_color=TEXT_HI, font=(FONT_MAIN, 16, "bold"),
    command=lambda: auto_close_seconds.set(min(120, auto_close_seconds.get() + 1)),
).pack(side="right")

ctk.CTkLabel(
    row_time, textvariable=auto_close_seconds, width=48,
    font=(FONT_MONO, 14, "bold"), text_color=TEXT_HI,
).pack(side="right", padx=4)

ctk.CTkButton(
    row_time, text="−", width=36, height=36, corner_radius=RADIUS_CTRL,
    fg_color=CARD_RAISED, hover_color=CARD_BG_HOVER,
    text_color=TEXT_HI, font=(FONT_MAIN, 16, "bold"),
    command=lambda: auto_close_seconds.set(max(1, auto_close_seconds.get() - 1)),
).pack(side="right", padx=(4, 0))

ctk.CTkButton(
    auto_body, text="Áp dụng", command=set_auto_close_time,
    height=34, corner_radius=RADIUS_CTRL,
    fg_color=ACCENT, hover_color=ACCENT_H,
    font=(FONT_MAIN, 11, "bold"),
).pack(fill="x")

# ---- EVENT ----
card_event, event_body = make_card(left_col, "Sự kiện gần nhất")
card_event.pack(fill="x")

event_label = ctk.CTkLabel(
    event_body, text="— Chưa có sự kiện —",
    font=(FONT_MAIN, 11, "bold"), text_color=TEXT_MID,
)
event_label.pack(pady=6)

# ==================== RIGHT: RFID ====================
card_rfid, rfid_body = make_card(right_col, "Quản lý thẻ RFID")
card_rfid.pack(fill="x", pady=(0, 12))

rfid_cmds = ctk.CTkFrame(rfid_body, fg_color="transparent")
rfid_cmds.pack(fill="x", pady=(4, 8))

ctk.CTkButton(
    rfid_cmds, text="＋  Thêm thẻ", command=start_add_card,
    height=32, corner_radius=RADIUS_CTRL,
    fg_color=C_OK, hover_color=C_OK_H,
    font=(FONT_MAIN, 10, "bold"),
).pack(side="left", padx=(0, 6))

ctk.CTkButton(
    rfid_cmds, text="Hủy", command=cancel_add_card,
    width=60, height=32, corner_radius=RADIUS_CTRL,
    fg_color=CARD_RAISED, hover_color=CARD_BG_HOVER,
    text_color=TEXT_HI, font=(FONT_MAIN, 10, "bold"),
).pack(side="left")

ctk.CTkButton(
    rfid_cmds, text="Tải danh sách", command=refresh_card_list,
    height=32, corner_radius=RADIUS_CTRL,
    fg_color=ACCENT, hover_color=ACCENT_H,
    font=(FONT_MAIN, 10, "bold"),
).pack(side="right")

card_manage_status = ctk.CTkLabel(
    rfid_body, text="Nhấn “Thêm thẻ”, sau đó quẹt thẻ lên RC522.",
    font=(FONT_MAIN, 10), text_color=TEXT_MID, anchor="w",
)
card_manage_status.pack(fill="x", pady=(0, 8))

list_frame = ctk.CTkFrame(rfid_body, fg_color=BG_DARK, corner_radius=RADIUS_CTRL)
list_frame.pack(fill="x", pady=(0, 8))

card_listbox = tk.Listbox(
    list_frame, height=5, font=(FONT_MONO, 10, "bold"),
    bg=BG_DARK, fg=TEXT_HI,
    selectbackground=ACCENT, selectforeground="#FFFFFF",
    relief="flat", bd=0, highlightthickness=0,
)
card_listbox.pack(side="left", fill="both", expand=True, padx=6, pady=6)

list_empty_label = ctk.CTkLabel(
    list_frame, text="⌀   Chưa có thẻ nào\nNhấn “Thêm thẻ” để bắt đầu",
    font=(FONT_MAIN, 10), text_color=TEXT_LO, justify="center",
)


def _copy_selected_uid(event=None):
    selection = card_listbox.curselection()
    if selection:
        uid = card_listbox.get(selection[0]).split("|", 1)[0].strip()
        uid_entry.delete(0, "end")
        uid_entry.insert(0, uid)


card_listbox.bind("<<ListboxSelect>>", _copy_selected_uid)

uid_row = ctk.CTkFrame(rfid_body, fg_color="transparent")
uid_row.pack(fill="x", pady=(0, 6))
ctk.CTkLabel(
    uid_row, text="UID", font=(FONT_MAIN, 10, "bold"),
    text_color=TEXT_MID, width=40, anchor="w",
).pack(side="left")
uid_entry = ctk.CTkEntry(
    uid_row, font=(FONT_MONO, 10, "bold"), height=30, corner_radius=RADIUS_CTRL,
    placeholder_text="Ví dụ: 638BCA06",
)
uid_entry.pack(side="left", fill="x", expand=True, padx=(0, 6))

name_row = ctk.CTkFrame(rfid_body, fg_color="transparent")
name_row.pack(fill="x", pady=(0, 6))
ctk.CTkLabel(
    name_row, text="Tên", font=(FONT_MAIN, 10, "bold"),
    text_color=TEXT_MID, width=40, anchor="w",
).pack(side="left")
card_name_entry = ctk.CTkEntry(
    name_row, font=(FONT_MAIN, 11), height=30, corner_radius=RADIUS_CTRL,
    placeholder_text="Ví dụ: Nguyen Van A",
)
card_name_entry.pack(side="left", fill="x", expand=True, padx=(0, 6))

action_row = ctk.CTkFrame(rfid_body, fg_color="transparent")
action_row.pack(fill="x")

ctk.CTkButton(
    action_row, text="Lưu tên", command=save_selected_card_name,
    height=32, corner_radius=RADIUS_CTRL,
    fg_color=ACCENT, hover_color=ACCENT_H,
    font=(FONT_MAIN, 10, "bold"),
).pack(side="left", fill="x", expand=True, padx=(0, 4))

ctk.CTkButton(
    action_row, text="Xóa thẻ", command=delete_selected_card,
    height=32, corner_radius=RADIUS_CTRL,
    fg_color=C_ERR, hover_color=C_ERR_H,
    font=(FONT_MAIN, 10, "bold"),
).pack(side="left", fill="x", expand=True, padx=(4, 0))

# ==================== LOG ====================
card_log, log_body = make_card(right_col, "Nhật ký UART")
card_log.pack(fill="both", expand=True)

log_head = ctk.CTkFrame(log_body, fg_color="transparent")
log_head.pack(fill="x", pady=(0, 6))

ctk.CTkButton(
    log_head, text="Xóa log", command=clear_log,
    width=64, height=24, corner_radius=RADIUS_CTRL,
    fg_color=CARD_RAISED, hover_color=CARD_BG_HOVER,
    text_color=TEXT_HI, font=(FONT_MAIN, 9),
).pack(side="right")

log_box = ctk.CTkTextbox(
    log_body, height=160, font=(FONT_MONO, 10),
    fg_color="#060A13", text_color=LOG_FG,
    corner_radius=RADIUS_CTRL, border_width=0,
)
log_box.pack(fill="both", expand=True)

log_box.tag_config("tx", foreground=LOG_TX)
log_box.tag_config("rx", foreground=LOG_FG)
log_box.tag_config("ok", foreground=LOG_OK)
log_box.tag_config("err", foreground=LOG_ERR)
log_box.tag_config("warn", foreground=LOG_WARN)
log_box.configure(state="disabled")

# ==================== HISTORY ====================
card_history, history_body = make_card(main_scroll, "Lịch sử hoạt động cửa")
card_history.grid(row=1, column=0, columnspan=2, sticky="nsew", pady=(12, 0))

hist_ctrl = ctk.CTkFrame(history_body, fg_color="transparent")
hist_ctrl.pack(fill="x", pady=(0, 8))

ctk.CTkLabel(
    hist_ctrl,
    text="Ghi nhận mỗi khi cửa chuyển sang trạng thái MỞ hoặc ĐÓNG",
    font=(FONT_MAIN, 10), text_color=TEXT_MID,
).pack(side="left")

ctk.CTkButton(
    hist_ctrl, text="Xóa lịch sử", command=clear_history,
    width=95, height=24, corner_radius=RADIUS_CTRL,
    fg_color=CARD_RAISED, hover_color=CARD_BG_HOVER,
    text_color=TEXT_HI, font=(FONT_MAIN, 9),
).pack(side="right")

history_timeline = ctk.CTkScrollableFrame(
    history_body, fg_color=BG_DARK, corner_radius=RADIUS_CTRL, height=200,
)
history_timeline.pack(fill="both", expand=True)

history_empty_label = ctk.CTkLabel(
    history_timeline, text="⌀   Chưa có hoạt động nào được ghi nhận",
    font=(FONT_MAIN, 10), text_color=TEXT_LO,
)

# ==================== INITIALIZATION ====================
log_message("[HỆ THỐNG] Đang kết nối mặc định cổng " + DEFAULT_PORT + "...", "warn")
update_clock()
update_history_empty_state()
update_list_empty_state()

# Cho UI render xong rồi mới kết nối
root.after(200, auto_connect_serial)

root.after(80, process_received_data)
root.protocol("WM_DELETE_WINDOW", on_close)

root.mainloop()