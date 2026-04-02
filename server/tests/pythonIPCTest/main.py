import win32pipe
import win32file

pipe_name = r'\\.\pipe\mypipe'

handle = win32file.CreateFile(
    pipe_name,
    win32file.GENERIC_READ | win32file.GENERIC_WRITE,
    0,
    None,
    win32file.OPEN_EXISTING,
    0,
    None
)

win32file.WriteFile(handle, b"Hello from Python\n")
_, data = win32file.ReadFile(handle, 4096)