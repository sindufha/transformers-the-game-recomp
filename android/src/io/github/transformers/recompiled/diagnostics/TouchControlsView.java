package io.github.transformers.recompiled.diagnostics;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Insets;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.content.SharedPreferences;
import android.util.Log;
import android.view.MotionEvent;
import android.view.DisplayCutout;
import android.view.View;
import android.view.WindowInsets;
import android.view.Gravity;
import android.widget.FrameLayout;
import android.widget.TextView;
import java.util.ArrayList;
import java.util.HashMap;

/** One transparent overlay owns all fingers, including movement and camera drag. */
final class TouchControlsView extends View {
    final TouchControls input;
    private final TouchControlsArtwork artwork;
    private final int[] pointerIds=new int[32];
    private float scale=1, left=0, top=0, right=0, bottom=0;
    private final SharedPreferences layouts;
    private final ArrayList<TouchControls.Control> defaults=new ArrayList<>();
    private final ControlLayout[] drafts=new ControlLayout[4];
    private final HashMap<Integer,EditorFinger> editorFingers=new HashMap<>();
    private final Paint selectionPaint=new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint fpsPaint=new Paint(Paint.ANTI_ALIAS_FLAG);
    private TextView fpsCounter;
    private boolean editing;
    private int actualProfile=TouchControls.HIDDEN;
    private String selected;
    private Runnable selectionChanged;
    private static final class EditorFinger {
        final String id;final float dx,dy;
        EditorFinger(String id,float dx,float dy){this.id=id;this.dx=dx;this.dy=dy;}
    }

    TouchControlsView(Context context,TouchControls.Output output) {
        super(context);input=new TouchControls(output);artwork=new TouchControlsArtwork(context);
        layouts=context.getSharedPreferences("touch_layouts",Context.MODE_PRIVATE);
        setContentDescription("Transformers controls: robot movement stick, vehicle steering and pedals, camera, weapons, block and transform");
        setFocusable(false);
    }
    private void layoutControls() {
        float w=Math.max(1,getWidth()-left-right),h=Math.max(1,getHeight()-top-bottom);
        scale=Math.max(.1f,Math.min(getResources().getDisplayMetrics().density,Math.min(w/840f,h/400f)));
        input.layout(w/scale,h/scale,scale);
        defaults.clear();defaults.addAll(input.controls);
        if(input.profile!=TouchControls.HIDDEN)defaults.add(new TouchControls.Control("fps","FPS",TouchControls.HUD,0,w/scale/2,96,14,46,14));
        editorFingers.clear();applyLayout();invalidate();
    }
    private String key(int profile){return "profile_"+profile;}
    private float logicalWidth(){return Math.max(1,getWidth()-left-right)/scale;}
    private float logicalHeight(){return Math.max(1,getHeight()-top-bottom)/scale;}
    private ControlLayout currentLayout(){return editing?drafts[input.profile]:ControlLayout.decode(layouts.getString(key(input.profile),""));}
    private void applyLayout(){
        ControlLayout layout=currentLayout();input.controls.clear();
        for(TouchControls.Control base:defaults)input.controls.add(layout.apply(base,logicalWidth(),logicalHeight()));
        updateFpsPosition();
    }
    void setFpsCounter(TextView counter){fpsCounter=counter;updateFpsPosition();}
    private void updateFpsPosition(){
        TouchControls.Control fps=input.find("fps");if(fpsCounter==null||fps==null||getWidth()==0)return;
        float size=fps.radius/14;
        fpsCounter.setTextSize(android.util.TypedValue.COMPLEX_UNIT_PX,13*scale*size);
        fpsCounter.setPadding(Math.round(8*scale*size),0,Math.round(8*scale*size),0);
        FrameLayout.LayoutParams position=new FrameLayout.LayoutParams(Math.round(fps.halfWidth*2*scale),Math.round(fps.halfHeight*2*scale),Gravity.TOP|Gravity.LEFT);
        position.leftMargin=Math.round(left+(fps.x-fps.halfWidth)*scale);position.topMargin=Math.round(top+(fps.y-fps.halfHeight)*scale);
        fpsCounter.setLayoutParams(position);
    }
    void setSelectionChanged(Runnable listener){selectionChanged=listener;}
    boolean isEditing(){return editing;}
    int editorProfile(){return input.profile;}
    String selectedId(){return selected;}
    float selectedSize(){
        if(!editing||selected==null)return 1;
        ControlLayout.Item item=drafts[input.profile].get(selected);return item==null?1:item.size;
    }
    void selectControl(String id){
        if(!editing||input.find(id)==null)return;selected=id;notifySelection();invalidate();
    }
    private void notifySelection(){if(selectionChanged!=null)selectionChanged.run();}
    void beginEditing(int profile){
        releaseAll();editing=true;
        for(int p=1;p<=3;p++)drafts[p]=ControlLayout.decode(layouts.getString(key(p),""));
        editProfile(profile);
    }
    void editProfile(int profile){
        if(!editing||profile<1||profile>3)return;
        releaseAll();input.setProfile(profile);layoutControls();
        selected=profile==TouchControls.ROBOT?"melee":profile==TouchControls.CAR?"gas":"accept";
        notifySelection();invalidate();
    }
    void resetEditorProfile(){
        if(!editing)return;drafts[input.profile].clear();editorFingers.clear();applyLayout();notifySelection();invalidate();
    }
    boolean saveEditing(){
        if(!editing)return true;
        String[] previous=new String[4];
        for(int p=1;p<=3;p++)previous[p]=layouts.getString(key(p),"");
        SharedPreferences.Editor change=layouts.edit();
        for(int p=1;p<=3;p++)change.putString(key(p),drafts[p].encode());
        if(!change.commit()){
            SharedPreferences.Editor rollback=layouts.edit();for(int p=1;p<=3;p++)rollback.putString(key(p),previous[p]);rollback.commit();return false;
        }
        endEditing();return true;
    }
    void cancelEditing(){if(editing)endEditing();}
    private void endEditing(){
        releaseAll();editing=false;selected=null;editorFingers.clear();
        input.setProfile(actualProfile);layoutControls();
    }
    private TouchControls.Control base(String id){for(TouchControls.Control c:defaults)if(c.id.equals(id))return c;return null;}
    private void place(String id,float x,float y,float size){
        TouchControls.Control base=base(id);if(base==null)return;
        ControlLayout layout=drafts[input.profile];layout.set(id,x/logicalWidth(),y/logicalHeight(),size);
        TouchControls.Control clamped=layout.apply(base,logicalWidth(),logicalHeight());
        layout.set(id,clamped.x/logicalWidth(),clamped.y/logicalHeight(),size);
        applyLayout();invalidate();
    }
    void resizeSelected(float size){
        if(!editing||selected==null)return;TouchControls.Control c=input.find(selected);if(c==null)return;
        place(selected,c.x,c.y,size);notifySelection();
    }
    @Override protected void onSizeChanged(int w,int h,int oldw,int oldh) {layoutControls();}
    @Override public WindowInsets onApplyWindowInsets(WindowInsets insets) {
        Insets gestures=insets.getSystemGestureInsets();
        left=gestures.left;right=gestures.right;top=gestures.top;bottom=gestures.bottom;
        DisplayCutout cutout=insets.getDisplayCutout();
        if(cutout!=null){
            left=Math.max(left,cutout.getSafeInsetLeft());right=Math.max(right,cutout.getSafeInsetRight());
            top=Math.max(top,cutout.getSafeInsetTop());bottom=Math.max(bottom,cutout.getSafeInsetBottom());
        }
        layoutControls();return insets;
    }
    void cancelEditorFingers(){editorFingers.clear();}
    void releaseAll() {input.releaseAll();editorFingers.clear();invalidate();}
    void setProfile(int profile){actualProfile=profile;if(!editing&&input.setProfile(profile)){
        layoutControls();Log.i("TFControls","profile="+input.profile+" controls="+input.controls.size());invalidate();
    }}
    @Override protected void onDetachedFromWindow() {releaseAll();super.onDetachedFromWindow();}

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);canvas.save();canvas.translate(left,top);canvas.scale(scale,scale);
        artwork.draw(canvas,input,(getWidth()-left-right)/scale,(getHeight()-top-bottom)/scale);
        if(editing){
            TouchControls.Control fps=input.find("fps");
            if(fps!=null){
                fpsPaint.setStyle(Paint.Style.FILL);fpsPaint.setColor(0xDD0C1821);
                canvas.drawRoundRect(fps.x-fps.halfWidth,fps.y-fps.halfHeight,fps.x+fps.halfWidth,fps.y+fps.halfHeight,7,7,fpsPaint);
                fpsPaint.setColor(0xFFE3F8EF);fpsPaint.setTypeface(Typeface.MONOSPACE);fpsPaint.setTextSize(13*fps.radius/14);fpsPaint.setTextAlign(Paint.Align.CENTER);
                Paint.FontMetrics metrics=fpsPaint.getFontMetrics();canvas.drawText("FPS 60.0",fps.x,fps.y-(metrics.ascent+metrics.descent)/2,fpsPaint);
            }
        }
        if(editing&&selected!=null){
            TouchControls.Control c=input.find(selected);
            if(c!=null){
                selectionPaint.setColor(0xFFFFD59B);selectionPaint.setStyle(Paint.Style.STROKE);selectionPaint.setStrokeWidth(2.5f);
                if(c.kind==TouchControls.PAD||c.kind==TouchControls.PEDAL||c.kind==TouchControls.BUTTON||c.kind==TouchControls.HUD)
                    canvas.drawRoundRect(c.x-c.halfWidth-3,c.y-c.halfHeight-3,c.x+c.halfWidth+3,c.y+c.halfHeight+3,20,20,selectionPaint);
                else canvas.drawCircle(c.x,c.y,c.radius+3,selectionPaint);
            }
        }
        canvas.restore();
    }
    @Override public boolean onTouchEvent(MotionEvent event) {
        if(!isEnabled())return true;
        if(editing)return editorTouch(event);
        int action=event.getActionMasked(),index=event.getActionIndex();
        if(action==MotionEvent.ACTION_CANCEL){input.releaseTouches();invalidate();return true;}
        if(action==MotionEvent.ACTION_DOWN)input.releaseTouches();
        int count=Math.min(pointerIds.length,event.getPointerCount());
        for(int i=0;i<count;++i)pointerIds[i]=event.getPointerId(i);
        input.cancelMissing(pointerIds,count);
        if(action==MotionEvent.ACTION_DOWN||action==MotionEvent.ACTION_POINTER_DOWN)
            input.down(event.getPointerId(index),(event.getX(index)-left)/scale,(event.getY(index)-top)/scale);
        else if(action==MotionEvent.ACTION_MOVE)
            for(int i=0;i<event.getPointerCount();++i)
                input.move(event.getPointerId(i),(event.getX(i)-left)/scale,(event.getY(i)-top)/scale);
        else if(action==MotionEvent.ACTION_UP||action==MotionEvent.ACTION_POINTER_UP) {
            input.up(event.getPointerId(index),(event.getX(index)-left)/scale,(event.getY(index)-top)/scale,false);
            if(action==MotionEvent.ACTION_UP){input.releaseTouches();performClick();}
        }
        invalidate();return true;
    }
    private boolean editorTouch(MotionEvent event){
        int action=event.getActionMasked(),index=event.getActionIndex();
        if(action==MotionEvent.ACTION_CANCEL){editorFingers.clear();return true;}
        if(action==MotionEvent.ACTION_DOWN)editorFingers.clear();
        java.util.Iterator<Integer> missing=editorFingers.keySet().iterator();
        while(missing.hasNext())if(event.findPointerIndex(missing.next())<0)missing.remove();
        if(action==MotionEvent.ACTION_DOWN||action==MotionEvent.ACTION_POINTER_DOWN){
            float x=(event.getX(index)-left)/scale,y=(event.getY(index)-top)/scale;
            TouchControls.Control hit=null;float distance=Float.MAX_VALUE;
            for(TouchControls.Control c:input.controls)if(c.contains(x,y,1.08f)){
                float dx=(x-c.x)/c.radius,dy=(y-c.y)/c.radius,d=dx*dx+dy*dy;
                if(d<distance){hit=c;distance=d;}
            }
            if(hit==null&&selected!=null)hit=input.find(selected);
            if(hit!=null){
                boolean owned=false;for(EditorFinger f:editorFingers.values())if(f.id.equals(hit.id))owned=true;
                if(!owned){selected=hit.id;editorFingers.put(event.getPointerId(index),new EditorFinger(hit.id,x-hit.x,y-hit.y));notifySelection();}
            }
        }else if(action==MotionEvent.ACTION_MOVE){
            for(int i=0;i<event.getPointerCount();i++){
                EditorFinger finger=editorFingers.get(event.getPointerId(i));if(finger==null)continue;
                ControlLayout.Item item=drafts[input.profile].get(finger.id);
                place(finger.id,(event.getX(i)-left)/scale-finger.dx,(event.getY(i)-top)/scale-finger.dy,item==null?1:item.size);
            }
        }else if(action==MotionEvent.ACTION_UP||action==MotionEvent.ACTION_POINTER_UP){
            EditorFinger finger=editorFingers.get(event.getPointerId(index));
            if(finger!=null){ControlLayout.Item item=drafts[input.profile].get(finger.id);place(finger.id,(event.getX(index)-left)/scale-finger.dx,(event.getY(index)-top)/scale-finger.dy,item==null?1:item.size);}
            editorFingers.remove(event.getPointerId(index));if(action==MotionEvent.ACTION_UP){editorFingers.clear();performClick();}
        }
        invalidate();return true;
    }
    @Override public boolean performClick() {super.performClick();return true;}
}
