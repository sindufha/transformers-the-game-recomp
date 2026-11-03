package io.github.transformers.recompiled.diagnostics;

/** Relative camera motion, independent of weapon buttons and pointer ordering. */
final class CameraDrag {
    private int pointer = -1;
    private float x, y, remainderX, remainderY;
    int dx, dy;

    int pointer() { return pointer; }
    void begin(int id, float nextX, float nextY) {
        pointer=id; x=nextX; y=nextY; remainderX=remainderY=0; dx=dy=0;
    }
    boolean move(int id, float nextX, float nextY) {
        dx=dy=0;
        if (pointer<0 || id!=pointer) return false;
        float relativeX=nextX-x+remainderX;
        float relativeY=nextY-y+remainderY;
        dx=Math.round(relativeX); dy=Math.round(relativeY);
        remainderX=relativeX-dx; remainderY=relativeY-dy;
        x=nextX; y=nextY;
        return dx!=0 || dy!=0;
    }
    void end() { pointer=-1; remainderX=remainderY=0; dx=dy=0; }
}
