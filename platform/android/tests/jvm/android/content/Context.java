package android.content;

import android.os.Vibrator;
import android.view.InputDevice;

public final class Context {
    public Object getSystemService(String name) {
        if (InputDevice.scenario == 6) throw new IllegalStateException("service fixture");
        return name.equals("vibrator") ? new Vibrator() : null;
    }
}
