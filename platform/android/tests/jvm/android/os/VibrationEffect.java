package android.os;

public final class VibrationEffect {
    public final long duration;
    public final int amplitude;
    private VibrationEffect(long duration, int amplitude) {
        this.duration = duration; this.amplitude = amplitude;
    }
    public static VibrationEffect createOneShot(long duration, int amplitude) {
        if (duration <= 0 || amplitude < 1 || amplitude > 255) throw new IllegalArgumentException("effect fixture");
        return new VibrationEffect(duration, amplitude);
    }
}
