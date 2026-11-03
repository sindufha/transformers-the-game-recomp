package io.github.transformers.recompiled.diagnostics;

/** Rates successful game presents, independent of Android's UI refresh rate. */
final class FrameRateCounter {
    private boolean initialized;
    private int lastFrames;
    private long lastTime;

    void reset() { initialized=false; }

    float sample(int frames,long now) {
        if(!initialized||now<=lastTime) {
            initialized=true;lastFrames=frames;lastTime=now;
            return Float.NaN;
        }
        long elapsed=now-lastTime;
        if(elapsed<500000000L)return Float.NaN;
        long presented=Integer.toUnsignedLong(frames-lastFrames);
        lastFrames=frames;lastTime=now;
        return (float)(presented*1e9/(double)elapsed);
    }
}
