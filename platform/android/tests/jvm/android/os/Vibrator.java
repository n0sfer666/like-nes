package android.os;

import android.view.InputDevice;

public final class Vibrator {
    public static int amplitude, cancels;
    public static long duration;
    public boolean hasVibrator() {
        if (InputDevice.scenario == 7) throw new IllegalStateException("capability fixture");
        return InputDevice.scenario != 4;
    }
    public void vibrate(VibrationEffect effect) {
        if (InputDevice.scenario == 8) throw new IllegalStateException("vibration fixture");
        amplitude = effect.amplitude; duration = effect.duration;
    }
    public void cancel() { ++cancels; }
}
