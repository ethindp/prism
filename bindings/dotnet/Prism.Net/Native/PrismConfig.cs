using System.Runtime.InteropServices;

namespace Prism.Net.NativeInterop
{
    [StructLayout(LayoutKind.Sequential)]
    public partial struct PrismConfig
    {
        public byte version;

        // Platform-specific handle (HWND on Windows, JNIEnv* on Android). Ignored on other platforms.
        public IntPtr platformPointer;
    }
}
