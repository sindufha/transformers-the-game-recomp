package io.github.transformers.recompiled.diagnostics;

import java.util.Map;
import java.util.TreeMap;

/** Profile-local draft geometry, independent of Android storage and input. */
final class ControlLayout {
    static final float MIN_SIZE=.5f,MAX_SIZE=2f;
    static final class Item {
        final float x,y,size;
        Item(float x,float y,float size){this.x=x;this.y=y;this.size=size;}
    }
    private final TreeMap<String,Item> items=new TreeMap<>();
    Item get(String id){return items.get(id);}
    void clear(){items.clear();}
    void set(String id,float x,float y,float size){
        if(!id.matches("[a-z0-9]{1,40}")||!Float.isFinite(x)||!Float.isFinite(y)||!Float.isFinite(size))return;
        items.put(id,new Item(clamp(x,0,1),clamp(y,0,1),clamp(size,MIN_SIZE,MAX_SIZE)));
    }
    private static float clamp(float v,float low,float high){return Math.max(low,Math.min(high,v));}
    TouchControls.Control apply(TouchControls.Control base,float width,float height){
        Item item=items.get(base.id);float size=item==null?1:item.size;
        float bottom=base.kind==TouchControls.STICK?16:0;
        size=Math.min(size,Math.min(width/(base.halfWidth*2),Math.max(1,height-bottom)/(base.halfHeight*2)));
        float hw=base.halfWidth*size,hh=base.halfHeight*size;
        float x=clamp(item==null?base.x:item.x*width,hw,width-hw);
        float y=clamp(item==null?base.y:item.y*height,hh,height-bottom-hh);
        return new TouchControls.Control(base.id,base.label,base.kind,base.code,x,y,base.radius*size,hw,hh);
    }
    String encode(){
        StringBuilder text=new StringBuilder();
        for(Map.Entry<String,Item> e:items.entrySet()){
            Item i=e.getValue();text.append(e.getKey()).append(',').append(i.x).append(',').append(i.y).append(',').append(i.size).append(';');
        }
        return text.toString();
    }
    static ControlLayout decode(String text){
        ControlLayout layout=new ControlLayout();if(text==null||text.length()>32768)return layout;
        int count=0;
        for(String row:text.split(";")){
            if(++count>64)break;String[] fields=row.split(",",-1);if(fields.length!=4)continue;
            try{layout.set(fields[0],Float.parseFloat(fields[1]),Float.parseFloat(fields[2]),Float.parseFloat(fields[3]));}
            catch(NumberFormatException ignored){}
        }
        return layout;
    }
}
