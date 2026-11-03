package io.github.transformers.recompiled.diagnostics;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import java.util.HashMap;

/** Presentation only. Reads control state without changing bindings or geometry. */
final class TouchControlsArtwork {
    private static final int WHITE=0xFFF5F6F8, MUTED=0xD4D7DEE8;
    private static final int AMBER=0xFFE6AB59, DARK=0xFF151B23;
    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
    private final HashMap<String,Drawable> icons=new HashMap<>();

    TouchControlsArtwork(Context context){
        paint.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));
        String[] names={"sports_mma","shield","arrow_upward","my_location","rocket_launch",
            "back_hand","directions_car","smart_toy","bolt","pause","arrow_back","play_arrow",
            "close","more_horiz","keyboard_arrow_up","keyboard_arrow_down","keyboard_arrow_left",
            "keyboard_arrow_right","campaign","pan_tool","filter_center_focus","gas","brake"};
        for(String name:names){
            int id=context.getResources().getIdentifier("hud_"+name,"drawable",context.getPackageName());
            if(id==0)throw new IllegalStateException("Missing control artwork: "+name);
            icons.put(name,context.getDrawable(id).mutate());
        }
    }

    private void fill(int color){paint.setStyle(Paint.Style.FILL);paint.setColor(color);}
    private void stroke(int color,float width){paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(width);paint.setColor(color);}
    private void text(Canvas canvas,String value,float x,float baseline,float size,int color){
        fill(color);paint.setTextSize(size);paint.setTextAlign(Paint.Align.CENTER);canvas.drawText(value,x,baseline,paint);
    }
    private void symbol(Canvas canvas,String name,float x,float y,float size,int color){
        Drawable icon=icons.get(name);icon.setTint(color);
        // Translate in float coordinates so icons do not snap as the stick moves.
        canvas.save();canvas.translate(x-size/2,y-size/2);
        int edge=Math.max(1,Math.round(size));icon.setBounds(0,0,edge,edge);icon.draw(canvas);canvas.restore();
    }

    void draw(Canvas canvas,TouchControls input,float width,float height){
        if(input.profile==TouchControls.HIDDEN)return;
        for(TouchControls.Control control:input.controls)drawControl(canvas,control,input);
        String hint=input.menu?"Choose an option, then Continue":input.driving?"Steer left or right · Hold a pedal":"Drag an empty area to look around";
        paint.setTextSize(10);float half=paint.measureText(hint)/2+12,cx=width*.53f;
        fill(0xB310161E);canvas.drawRoundRect(cx-half,height-37,cx+half,height-13,12,12,paint);
        text(canvas,hint,cx,height-21,10,MUTED);
    }

    private void drawControl(Canvas canvas,TouchControls.Control c,TouchControls input){
        if(c.kind==TouchControls.HUD)return;
        float x=c.x,y=c.y,r=c.radius;boolean pressed=c.held>0;
        if(c.kind==TouchControls.STICK){
            fill(0xA6141B25);canvas.drawCircle(x,y,r,paint);
            stroke(pressed?0xE6E6AB59:0x80E0E5EE,1.6f);canvas.drawCircle(x,y,r-1,paint);
            stroke(0x25FFFFFF,1);canvas.drawCircle(x,y,r*.63f,paint);
            float kx=x+input.stickX*r*.55f,ky=y+input.stickY*r*.55f;
            fill(pressed?AMBER:0xDDCCD5E1);canvas.drawCircle(kx,ky,r*.34f,paint);
            fill(pressed?0xFFEDBE7C:0xFFF1F4F8);canvas.drawCircle(kx-r*.045f,ky-r*.045f,r*.20f,paint);
            text(canvas,"MOVE",x,y+r+15,10.5f,MUTED);return;
        }

        boolean rectangle=c.kind==TouchControls.PAD||c.kind==TouchControls.PEDAL||c.kind==TouchControls.BUTTON;
        boolean primary=c.id.equals("melee")||c.id.equals("accept")||c.id.equals("gas");
        int background=pressed?0xEBE6AB59:primary?0xC4242B35:0xB318202B;
        int border=pressed?0xFFFFD59B:primary?0xC9E6AB59:0x73D6DFEB;
        int ink=pressed?DARK:WHITE;
        if(rectangle){
            float hw=c.halfWidth,hh=c.halfHeight,corner=Math.min(20,hh*.62f);
            fill(background);canvas.drawRoundRect(x-hw,y-hh,x+hw,y+hh,corner,corner,paint);
            stroke(border,pressed?2:1.25f);canvas.drawRoundRect(x-hw+1,y-hh+1,x+hw-1,y+hh-1,corner,corner,paint);
        }else{
            fill(background);canvas.drawCircle(x,y,r,paint);
            stroke(border,pressed?2:1.25f);canvas.drawCircle(x,y,r-1,paint);
        }
        if(c.id.startsWith("extra")){
            text(canvas,c.label,x,y+4,c.label.length()>4?10.5f:13,ink);return;
        }
        if(c.id.equals("more")){
            symbol(canvas,"more_horiz",x,y,28,ink);return;
        }

        String icon=iconFor(c,input.driving),label=labelFor(c,input.driving);
        float halfHeight=rectangle?c.halfHeight:r;
        float size=rectangle?Math.min(36,Math.min(c.halfWidth*1.05f,c.halfHeight*.88f)):r*.96f;
        float iconY=y-halfHeight*.20f;
        float font=halfHeight>=40?11.5f:9.5f;
        float available=(rectangle?c.halfWidth:r)*1.64f;
        paint.setTextSize(font);float measured=paint.measureText(label);
        if(measured>available)font*=available/measured;
        symbol(canvas,icon,x,iconY,size,ink);
        text(canvas,label,x,y+halfHeight*.60f,font,ink);
    }

    private String iconFor(TouchControls.Control c,boolean driving){
        switch(c.id){
            case "up":return "keyboard_arrow_up";
            case "down":return "keyboard_arrow_down";
            case "left":return "keyboard_arrow_left";
            case "right":return "keyboard_arrow_right";
            case "accept":return "play_arrow";
            case "back":case "delete":return "arrow_back";
            case "pause":return "pause";
            case "exit":return "close";
            case "transform":return driving?"smart_toy":"directions_car";
            case "lock":return "filter_center_focus";
            case "guns":return "my_location";
            case "rocket":return "rocket_launch";
            case "melee":return "sports_mma";
            case "block":return driving?"bolt":"shield";
            case "jump":return driving?"pan_tool":"arrow_upward";
            case "action":return driving?"campaign":"back_hand";
            case "gas":return "gas";
            case "brake":return "brake";
            default:return "more_horiz";
        }
    }
    static String labelFor(TouchControls.Control c,boolean driving){
        switch(c.id){
            case "accept":return "CONTINUE";
            case "melee":return "ATTACK";
            case "lock":return "AIM";
            case "guns":return "FIRE";
            case "action":return driving?"HORN":"GRAB";
            default:return c.label;
        }
    }
}
