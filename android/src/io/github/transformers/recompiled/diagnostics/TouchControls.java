package io.github.transformers.recompiled.diagnostics;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/** Platform-independent touch ownership, layout and keyboard/mouse aggregation. */
final class TouchControls {
    private static final int[] MENU_KEYS={38,40,37,39}, GAME_KEYS={87,83,65,68};
    private static final int[] NO_POINTERS={};
    interface Output {
        void key(int key, boolean down);
        void mouse(int dx, int dy, int wheel, int buttons);
        void close();
        default void menu(){}
    }
    static final int HIDDEN=0, MENU=1, ROBOT=2, CAR=3;
    static final int KEY=0, MOUSE=1, WHEEL=2, STICK=3, MORE=5, CLOSE=6, PAD=8, PEDAL=9, BUTTON=10, HUD=11;
    static final class Control {
        final String id, label;
        final int kind, code;
        final float x, y, radius, halfWidth, halfHeight;
        int held;
        Control(String id, String label, int kind, int code, float x, float y, float radius) {
            this(id,label,kind,code,x,y,radius,radius,kind==PEDAL?radius*1.2f:radius);
        }
        Control(String id,String label,int kind,int code,float x,float y,float radius,float halfWidth,float halfHeight) {
            this.id=id; this.label=label; this.kind=kind; this.code=code;
            this.x=x; this.y=y; this.radius=radius;
            this.halfWidth=halfWidth;this.halfHeight=halfHeight;
        }
        boolean contains(float px, float py, float slack) {
            float dx=px-x,dy=py-y,r=radius*slack;
            if(kind==PAD||kind==PEDAL||kind==BUTTON||kind==HUD)return Math.abs(dx)<=halfWidth*slack&&Math.abs(dy)<=halfHeight*slack;
            return dx*dx+dy*dy<=r*r;
        }
    }
    private static final class Finger {
        final Control control;
        boolean held;
        Finger(Control control) { this.control=control; }
    }
    final ArrayList<Control> controls=new ArrayList<>();
    final CameraDrag camera=new CameraDrag();
    private final Output output;
    private final HashMap<Integer,Finger> fingers=new HashMap<>();
    private final int[] keyHolds=new int[256], mouseHolds=new int[8];
    private final boolean[] hardwareKeys=new boolean[256];
    private int stickPointer=-1, directionMask, mouseButtons;
    private float width, height, pixelScale=1;
    float stickX, stickY;
    boolean menu=true, driving;
    int profile=HIDDEN;

    TouchControls(Output output) { this.output=output; }
    boolean setProfile(int next){
        if(next<HIDDEN||next>CAR)next=HIDDEN;
        if(next==profile)return false;
        releaseTouches();profile=next;menu=next==MENU;driving=next==CAR;
        layout(width,height,pixelScale);return true;
    }
    private void add(String id,String label,int kind,int code,float x,float y,float r) {
        controls.add(new Control(id,label,kind,code,x,y,r));
    }
    private void rectangle(String id,String label,int kind,int code,float x,float y,float hw,float hh) {
        controls.add(new Control(id,label,kind,code,x,y,Math.min(hw,hh),hw,hh));
    }
    void layout(float w,float h,float scale) {
        releaseTouches(); width=w; height=h; pixelScale=scale;
        controls.clear();
        if(w<1||h<1||profile==HIDDEN)return;
        add("more","...",MORE,0,w/2-40,42,24);
        add("exit","EXIT",CLOSE,0,w/2+34,42,26);
        if(menu) {
            // Separate direction pads, with the confirmation bank on the right.
            rectangle("up","UP",PAD,38,184,h*.66f-62,30,27);
            rectangle("down","DOWN",PAD,40,184,h*.66f+62,30,27);
            rectangle("left","LEFT",PAD,37,100,h*.66f,34,27);
            rectangle("right","RIGHT",PAD,39,268,h*.66f,34,27);
            rectangle("accept","ENTER",BUTTON,13,w-150,h*.68f,58,44);
            rectangle("back","BACK",BUTTON,8,w-150,h*.43f,58,28);
        } else {
            add("pause","PAUSE",KEY,27,44,42,27);
            add("transform","TRANSFORM",WHEEL,120,driving?90:52,h*(driving?.35f:.37f),30);
            if(driving){
                rectangle("block","BOOST",BUTTON,160,w-145,h*.53f,99,25);
                rectangle("left","LEFT",PAD,65,124,h*.77f,48,36);
                rectangle("right","RIGHT",PAD,68,244,h*.77f,48,36);
                add("guns","GUNS",MOUSE,2,w-76,h*.35f,30);
                add("rocket","ROCKET",MOUSE,1,w-160,h*.35f,30);
                rectangle("gas","GAS",PEDAL,87,w-86,h*.74f,40,44);
                rectangle("brake","BRAKE",PEDAL,83,w-204,h*.74f,40,44);
                add("jump","H-BRAKE",KEY,32,w-304,h*.77f,25);
                add("action","HORN",KEY,81,184,h*.35f,25);
            }else{
                add("stick","MOVE",STICK,0,156,h*.70f,68);
                add("lock","LOCK",MOUSE,4,146,h*.37f,30);
                add("melee","MELEE",KEY,69,w-156,h*.73f,40);
                add("block","BLOCK",KEY,160,w-58,h*.74f,27);
                add("guns","GUNS",MOUSE,2,w-278,h*.80f,29);
                add("rocket","ROCKET",MOUSE,1,w-244,h*.60f,29);
                add("jump","JUMP",KEY,32,w-84,h*.47f,29);
                add("action","PICK UP",KEY,81,w-168,h*.50f,29);
            }
        }
    }
    Control find(String id) {
        for(Control control:controls)if(control.id.equals(id))return control;
        return null;
    }
    private Control hit(float x,float y) {
        Control best=null;float distance=Float.MAX_VALUE;
        for(Control c:controls)if(c.kind!=HUD&&c.contains(x,y,1.08f)) {
            float dx=(x-c.x)/c.radius,dy=(y-c.y)/c.radius,d=dx*dx+dy*dy;
            if(d<distance){distance=d;best=c;}
        }
        return best;
    }
    private void key(int code,boolean down) {
        int before=keyHolds[code];
        keyHolds[code]=Math.max(0,before+(down?1:-1));
        if((before==0)!=(keyHolds[code]==0))output.key(code,keyHolds[code]!=0);
    }
    void hardwareKey(int code,boolean down) {
        if(code<=0||code>=256||hardwareKeys[code]==down)return;
        hardwareKeys[code]=down;key(code,down);
    }
    private void hold(Control c,boolean down) {
        if(c==null)return;
        int before=c.held;c.held=Math.max(0,before+(down?1:-1));
        if((before==0)==(c.held==0))return;
        boolean pressed=c.held>0;
        if(c.kind==KEY||c.kind==PAD||c.kind==PEDAL||c.kind==BUTTON)key(c.code,pressed);
        else if(c.kind==MOUSE) {
            int bit=Integer.numberOfTrailingZeros(c.code);
            mouseHolds[bit]=Math.max(0,mouseHolds[bit]+(pressed?1:-1));
            int buttons=0;
            for(int i=0;i<mouseHolds.length;++i)if(mouseHolds[i]>0)buttons|=1<<i;
            mouseButtons=buttons;output.mouse(0,0,0,buttons);
        }
    }
    void down(int id,float x,float y) {
        if(profile==HIDDEN)return;
        if(fingers.containsKey(id))up(id,x,y,true);
        Control c=hit(x,y);Finger f=new Finger(c);fingers.put(id,f);
        if(c!=null&&c.kind==STICK) {
            if(stickPointer<0){stickPointer=id;c.held=1;moveStick(x,y);}
        } else if(c!=null) {
            if(c.kind==WHEEL&&c.held==0)output.mouse(0,0,c.code,mouseButtons);
            f.held=true;hold(c,true);
        }
        else if(!menu&&camera.pointer()<0&&x>width*.30f&&y>76)
            camera.begin(id,x*pixelScale,y*pixelScale);
    }
    void move(int id,float x,float y) {
        Finger f=fingers.get(id);if(f==null)return;
        if(id==stickPointer)moveStick(x,y);
        else if(id==camera.pointer()) {
            if(camera.move(id,x*pixelScale,y*pixelScale))output.mouse(camera.dx,camera.dy,0,mouseButtons);
        } else if(f.control!=null&&f.control.kind!=STICK) {
            boolean held=f.control.contains(x,y,1.25f);
            if(f.held!=held){f.held=held;hold(f.control,held);}
        }
    }
    private void moveStick(float x,float y) {
        Control c=find("stick");if(c==null)return;
        float dx=(x-c.x)/(c.radius*.72f),dy=(y-c.y)/(c.radius*.72f);
        float length=(float)Math.sqrt(dx*dx+dy*dy);
        if(length>1){dx/=length;dy/=length;}
        stickX=dx;stickY=dy;
        int next=0;
        for(int i=0;i<4;++i) {
            float axis=i==0?-dy:i==1?dy:i==2?-dx:dx;
            if(axis>((directionMask&(1<<i))!=0?.18f:.28f))next|=1<<i;
        }
        setDirections(next);
    }
    private void resetStick(){
        setDirections(0);stickPointer=-1;stickX=stickY=0;
    }
    private void setDirections(int next) {
        int[] keys=menu?MENU_KEYS:GAME_KEYS;
        for(int i=0;i<4;++i)if(((directionMask^next)&(1<<i))!=0)key(keys[i],(next&(1<<i))!=0);
        directionMask=next;
    }
    void up(int id,float x,float y,boolean cancelled) {
        Finger f=fingers.remove(id);if(f==null)return;
        if(id==stickPointer) {
            resetStick();f.control.held=0;
        } else if(id==camera.pointer()) {
            if(!cancelled&&camera.move(id,x*pixelScale,y*pixelScale))
                output.mouse(camera.dx,camera.dy,0,mouseButtons);
            camera.end();
        } else if(f.control!=null&&f.held) {
            hold(f.control,false);
            if(!cancelled&&f.control.contains(x,y,1.25f)) {
                if(f.control.kind==MORE){releaseAll();output.menu();}
                else if(f.control.kind==CLOSE){releaseAll();output.close();}
            }
        }
    }
    void cancelMissing(int[] ids,int count) {
        Iterator<Map.Entry<Integer,Finger>> iterator=fingers.entrySet().iterator();
        while(iterator.hasNext()) {
            Map.Entry<Integer,Finger> entry=iterator.next();boolean exists=false;
            for(int i=0;i<count;++i)if(ids[i]==entry.getKey()){exists=true;break;}
            if(!exists) {
                int id=entry.getKey();Finger f=entry.getValue();iterator.remove();
                if(id==stickPointer){resetStick();f.control.held=0;}
                else if(id==camera.pointer())camera.end();
                else if(f.held)hold(f.control,false);
            }
        }
    }
    void releaseTouches() {cancelMissing(NO_POINTERS,0);}
    void releaseAll() {
        for(int i=0;i<256;++i){if(keyHolds[i]>0)output.key(i,false);keyHolds[i]=0;hardwareKeys[i]=false;}
        for(int i=0;i<8;++i)mouseHolds[i]=0;
        if(mouseButtons!=0)output.mouse(0,0,0,0);
        mouseButtons=directionMask=0;stickPointer=-1;stickX=stickY=0;
        fingers.clear();camera.end();for(Control c:controls)c.held=0;
    }
}
