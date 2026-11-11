"""VirtualBox 窗口输入辅助：在无 Guest Additions 的虚拟机上做鼠标操作。

用法：
  python vmmouse.py info
  python vmmouse.py activate
  python vmmouse.py click <x> <y>        # 相对窗口客户区的宿主坐标
  python vmmouse.py rel <dx> <dy>        # 相对移动（虚拟机捕获后用）
  python vmmouse.py clamp                # 把客户机指针推到左上角 (0,0)
"""
import ctypes
import ctypes.wintypes as wt
import sys
import time

u = ctypes.windll.user32

MOUSEEVENTF_MOVE = 0x0001
MOUSEEVENTF_LEFTDOWN = 0x0002
MOUSEEVENTF_LEFTUP = 0x0004

user32 = u
EnumWindowsProc = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)


def find_vm_window(substr="HaikuOS"):
    found = []

    def cb(hwnd, _):
        if not user32.IsWindowVisible(hwnd):
            return True
        n = user32.GetWindowTextLengthW(hwnd)
        if n == 0:
            return True
        buf = ctypes.create_unicode_buffer(n + 1)
        user32.GetWindowTextW(hwnd, buf, n + 1)
        if substr.lower() in buf.value.lower():
            found.append((hwnd, buf.value))
        return True

    user32.EnumWindows(EnumWindowsProc(cb), 0)
    return found


def rects(hwnd):
    r = wt.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(r))
    c = wt.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(c))
    return (r.left, r.top, r.right, r.bottom), (c.right, c.bottom)


def rel(dx, dy):
    user32.mouse_event(MOUSEEVENTF_MOVE, int(dx), int(dy), 0, 0)


def left_click():
    user32.mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
    time.sleep(0.05)
    user32.mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)


def move_steps(dx, dy, step=1, delay=0.002):
    """小步移动，避免宿主/客户机指针加速造成的偏差。"""
    sx = 1 if dx > 0 else -1
    sy = 1 if dy > 0 else -1
    for _ in range(abs(int(dx))):
        rel(sx, 0)
        if delay:
            time.sleep(delay)
    for _ in range(abs(int(dy))):
        rel(0, sy)
        if delay:
            time.sleep(delay)


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "info"
    wins = find_vm_window()
    if not wins:
        print("找不到标题含 HaikuOS 的窗口")
        return 1
    hwnd, title = wins[0]

    if cmd == "info":
        wr, cs = rects(hwnd)
        print(f"hwnd={hwnd} title={title!r}")
        print(f"window_rect={wr}  client_size={cs[0]}x{cs[1]}")
        print(f"foreground={user32.GetForegroundWindow() == hwnd}")
        return 0

    if cmd == "activate":
        user32.ShowWindow(hwnd, 9)  # SW_RESTORE
        user32.SetForegroundWindow(hwnd)
        time.sleep(0.4)
        print("activated, foreground =", user32.GetForegroundWindow() == hwnd)
        return 0

    if cmd == "rel":
        dx, dy = int(sys.argv[2]), int(sys.argv[3])
        move_steps(dx, dy)
        print(f"moved {dx},{dy}")
        return 0

    if cmd == "clamp":
        for _ in range(30):
            rel(-300, -300)
            time.sleep(0.01)
        print("clamped to top-left")
        return 0

    if cmd == "click":
        x, y = int(sys.argv[2]), int(sys.argv[3])
        user32.SetCursorPos(x, y)
        time.sleep(0.1)
        left_click()
        print(f"clicked at {x},{y}")
        return 0

    print("未知命令:", cmd)
    return 2


if __name__ == "__main__":
    sys.exit(main())
