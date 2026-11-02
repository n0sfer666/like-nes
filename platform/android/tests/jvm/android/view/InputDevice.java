package android.view;

import android.os.Vibrator;

// Модель платформенного API: настоящий JVM/JNI, без заявления о живом Android.
public final class InputDevice {
    public static volatile int scenario;
    private final int id;
    private InputDevice(int id) { this.id = id; }
    public static int[] getDeviceIds() {
        if (scenario == 1) throw new IllegalStateException("inventory fixture");
        return scenario == 5 ? new int[129] : new int[]{12, 44, 90};
    }
    public static InputDevice getDevice(int id) {
        return scenario == 3 ? null : new InputDevice(id);
    }
    public int getSources() { return id == 90 ? 0x101 : 0x01000411; }
    public int getVendorId() {
        if (scenario == 2) throw new IllegalStateException("vendor fixture");
        return id == 12 ? 0x045e : 0;
    }
    public int getProductId() { return id == 12 ? 0x0b12 : 0; }
    public String getName() { return id == 12 ? "fixture Xbox" : "DualSense \u2603"; }
    public static final class MotionRange {}
    public MotionRange getMotionRange(int axis) {
        return id == 12 && (axis == 11 || axis == 14) ? new MotionRange() : null;
    }
    public Vibrator getVibrator() { return new Vibrator(); }
}
