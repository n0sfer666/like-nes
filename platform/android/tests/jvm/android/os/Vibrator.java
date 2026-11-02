package android.os;

import android.view.InputDevice;

public final class Vibrator {
    public static int amplitude, cancels;
    public static long duration;
    public boolean hasVibrator() { return InputDevice.scenario != 4; }
    public void vibrate(VibrationEffect effect) {
        amplitude = effect.amplitude; duration = effect.duration;
    }
    public void cancel() { ++cancels; }
}
