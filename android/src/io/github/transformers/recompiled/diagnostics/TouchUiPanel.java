package io.github.transformers.recompiled.diagnostics;

import android.app.Activity;
import android.graphics.Insets;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.ColorDrawable;
import android.view.Gravity;
import android.view.View;
import android.view.WindowInsets;
import android.view.animation.DecelerateInterpolator;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;
import java.util.ArrayList;

/** In-process sidebar/editor over the retained, paused game surface. */
final class TouchUiPanel {
    private static final int WHITE=0xFFF5F6F8,MUTED=0xFFC0CBD9,AMBER=0xFFE6AB59;
    private final Activity activity;
    private final TouchControlsView controls;
    private final Runnable stateChanged;
    private final FrameLayout sidebar,editor;
    private final FrameLayout sidebarSheet;
    private final LinearLayout toolbar,sizePanel;
    private final Button showTools;
    private final Button[] tabs=new Button[3];
    private final TextView title,sizeLabel;
    private final Spinner selectedButton;
    private final SeekBar sizeSlider;
    private final ArrayList<String> ids=new ArrayList<>();
    private boolean syncing;
    private boolean sidebarClosing;
    private int sidebarTransition;

    TouchUiPanel(Activity activity,FrameLayout root,TouchControlsView controls,Runnable changed){
        this.activity=activity;this.controls=controls;stateChanged=changed;
        sidebar=new FrameLayout(activity);sidebar.setBackgroundColor(0x8810161E);sidebar.setVisibility(View.GONE);
        sidebar.setClickable(true);sidebar.setOnClickListener(new View.OnClickListener(){@Override public void onClick(View v){closeSidebar();}});
        sidebarSheet=new FrameLayout(activity);sidebarSheet.setBackgroundColor(0xFF131C28);sidebarSheet.setClickable(true);
        LinearLayout menu=column();menu.setClickable(true);
        TextView heading=text("GAME MENU",22,WHITE);heading.setTypeface(Typeface.DEFAULT,Typeface.BOLD);menu.addView(heading);
        add(menu,button("Resume game",true,new View.OnClickListener(){@Override public void onClick(View v){closeSidebar();}}),12);
        TextView custom=text("Custom UI",19,WHITE);custom.setTypeface(Typeface.DEFAULT,Typeface.BOLD);add(menu,custom,24);
        add(menu,text("Choose a layout to edit",13,MUTED),8);
        String[] profiles={"Menu","Robot","Car"};
        for(int i=0;i<3;i++){
            final int profile=i+1;add(menu,button(profiles[i],false,new View.OnClickListener(){@Override public void onClick(View v){openEditor(profile);}}),10);
        }
        add(menu,text("Your game stays paused while the sidebar or editor is open.",13,MUTED),20);
        final ScrollView scroll=new ScrollView(activity);
        scroll.setFillViewport(true);scroll.setPadding(0,0,0,0);scroll.setClipToPadding(false);
        scroll.setVerticalFadingEdgeEnabled(false);scroll.setFadingEdgeLength(0);
        scroll.setOverScrollMode(View.OVER_SCROLL_NEVER);
        scroll.setScrollBarStyle(View.SCROLLBARS_INSIDE_OVERLAY);scroll.setScrollbarFadingEnabled(false);
        scroll.setVerticalScrollbarPosition(View.SCROLLBAR_POSITION_RIGHT);scroll.setScrollBarSize(dp(3));
        scroll.setVerticalScrollbarTrackDrawable(new ColorDrawable(0x245B6A7D));
        GradientDrawable thumb=new GradientDrawable();thumb.setColor(AMBER);thumb.setCornerRadius(dp(2));scroll.setVerticalScrollbarThumbDrawable(thumb);
        scroll.addView(menu,new ScrollView.LayoutParams(-1,-2));
        sidebarSheet.addView(scroll,new FrameLayout.LayoutParams(-1,-1,Gravity.TOP|Gravity.LEFT));
        FrameLayout.LayoutParams menuPosition=new FrameLayout.LayoutParams(dp(280),-1,Gravity.TOP|Gravity.RIGHT);sidebar.addView(sidebarSheet,menuPosition);
        // The viewport and scrollbar span the entire sheet. Insets belong to
        // the scrolling child, not to the ScrollView's clipping rectangle.
        sidebar.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener(){@Override public WindowInsets onApplyWindowInsets(View view,WindowInsets insets){
            Insets gestures=insets.getSystemGestureInsets();int t=gestures.top,r=gestures.right,b=gestures.bottom;
            android.view.DisplayCutout cutout=insets.getDisplayCutout();if(cutout!=null){t=Math.max(t,cutout.getSafeInsetTop());r=Math.max(r,cutout.getSafeInsetRight());b=Math.max(b,cutout.getSafeInsetBottom());}
            menu.setPadding(dp(16),t+dp(12),r+dp(16),b+dp(12));return insets;
        }});
        root.addView(sidebar,new FrameLayout.LayoutParams(-1,-1));

        editor=new FrameLayout(activity);editor.setVisibility(View.GONE);safeInsets(editor);
        toolbar=column();toolbar.setPadding(dp(8),dp(5),dp(8),dp(5));toolbar.setBackground(background(0xF0131C28));toolbar.setClickable(true);
        LinearLayout actions=new LinearLayout(activity);actions.setGravity(Gravity.CENTER_VERTICAL);
        title=text("Custom UI",11,WHITE);title.setTypeface(Typeface.DEFAULT,Typeface.BOLD);
        actions.addView(title,new LinearLayout.LayoutParams(0,-2,1));
        addAction(actions,"Save",true,new View.OnClickListener(){@Override public void onClick(View v){save();}});addAction(actions,"Reset",false,new View.OnClickListener(){@Override public void onClick(View v){controls.resetEditorProfile();}});
        addAction(actions,"Cancel",false,new View.OnClickListener(){@Override public void onClick(View v){cancelEditor();}});toolbar.addView(actions);
        LinearLayout tabRow=new LinearLayout(activity);
        for(int i=0;i<3;i++){
            final int profile=i+1;tabs[i]=smallButton(profiles[i],false,new View.OnClickListener(){@Override public void onClick(View v){controls.editProfile(profile);animateEditorTools();}});
            LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(0,dp(32),1);lp.setMargins(dp(2),dp(3),dp(2),0);tabRow.addView(tabs[i],lp);
        }
        toolbar.addView(tabRow);
        FrameLayout.LayoutParams top=new FrameLayout.LayoutParams(dp(430),-2,Gravity.TOP|Gravity.CENTER_HORIZONTAL);top.setMargins(dp(8),dp(6),dp(8),0);editor.addView(toolbar,top);

        sizePanel=column();sizePanel.setPadding(dp(8),dp(5),dp(8),dp(5));sizePanel.setBackground(background(0xF0131C28));sizePanel.setClickable(true);
        LinearLayout selector=new LinearLayout(activity);selector.setGravity(Gravity.CENTER_VERTICAL);
        selectedButton=new Spinner(activity);selector.addView(selectedButton,new LinearLayout.LayoutParams(0,dp(32),1));
        sizeLabel=text("100%",11,AMBER);sizeLabel.setGravity(Gravity.CENTER);selector.addView(sizeLabel,new LinearLayout.LayoutParams(dp(44),dp(32)));
        Button hide=smallButton("Hide",false,new View.OnClickListener(){@Override public void onClick(View v){setToolsVisible(false);}});selector.addView(hide,new LinearLayout.LayoutParams(dp(48),dp(32)));sizePanel.addView(selector);
        sizeSlider=new SeekBar(activity);sizeSlider.setMax(150);sizePanel.addView(sizeSlider,new LinearLayout.LayoutParams(-1,dp(32)));
        sizeSlider.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){
            @Override public void onProgressChanged(SeekBar bar,int value,boolean fromUser){if(fromUser&&!syncing)controls.resizeSelected((50+value)/100f);}
            @Override public void onStartTrackingTouch(SeekBar bar){controls.cancelEditorFingers();}
            @Override public void onStopTrackingTouch(SeekBar bar){}
        });
        FrameLayout.LayoutParams bottom=new FrameLayout.LayoutParams(dp(300),-2,Gravity.BOTTOM|Gravity.CENTER_HORIZONTAL);
        bottom.setMargins(dp(8),0,dp(8),dp(6));editor.addView(sizePanel,bottom);
        showTools=smallButton("Show tools",true,new View.OnClickListener(){@Override public void onClick(View v){setToolsVisible(true);}});showTools.setVisibility(View.GONE);
        FrameLayout.LayoutParams show=new FrameLayout.LayoutParams(dp(94),dp(32),Gravity.TOP|Gravity.CENTER_HORIZONTAL);show.topMargin=dp(6);editor.addView(showTools,show);
        root.addView(editor,new FrameLayout.LayoutParams(-1,-1));
        selectedButton.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener(){
            @Override public void onItemSelected(AdapterView<?> parent,View v,int position,long id){
                if(!syncing&&position>=0&&position<ids.size()&&!ids.get(position).equals(controls.selectedId()))controls.selectControl(ids.get(position));
            }
            @Override public void onNothingSelected(AdapterView<?> parent){}
        });
        controls.setSelectionChanged(new Runnable(){@Override public void run(){refreshSelection();}});
    }

    boolean isOpen(){return sidebar.getVisibility()==View.VISIBLE||controls.isEditing();}
    void openSidebar(){
        if(isOpen())return;controls.releaseAll();sidebarClosing=false;sidebarTransition++;
        sidebar.animate().cancel();sidebarSheet.animate().cancel();sidebar.setAlpha(0f);
        sidebarSheet.setTranslationX(sidebarSheet.getWidth()>0?sidebarSheet.getWidth():dp(280));
        sidebar.setVisibility(View.VISIBLE);sidebar.requestApplyInsets();stateChanged.run();
        sidebar.animate().alpha(1f).setDuration(200).start();
        sidebarSheet.animate().translationX(0).setDuration(240).setInterpolator(new DecelerateInterpolator()).start();
    }
    private void closeSidebar(){
        if(sidebarClosing||sidebar.getVisibility()!=View.VISIBLE)return;
        sidebarClosing=true;final int transition=++sidebarTransition;controls.releaseAll();
        sidebar.animate().cancel();sidebarSheet.animate().cancel();
        sidebarSheet.animate().translationX(sidebarSheet.getWidth()).setDuration(180).start();
        sidebar.animate().alpha(0f).setDuration(180).withEndAction(new Runnable(){@Override public void run(){
            if(transition!=sidebarTransition)return;
            sidebar.setVisibility(View.GONE);sidebarClosing=false;stateChanged.run();
        }}).start();
    }
    private void openEditor(int profile){
        if(sidebarClosing||controls.isEditing())return;
        // Keep the pause owner continuously active across the transition.
        controls.beginEditing(profile);editor.setVisibility(View.VISIBLE);editor.setAlpha(0f);
        closeSidebar();
        editor.requestApplyInsets();setToolsVisible(true);refreshSelection();stateChanged.run();
        editor.animate().alpha(1f).setDuration(200).start();
    }
    private void save(){
        if(sidebarClosing)return;
        if(!controls.saveEditing()){Toast.makeText(activity,"Could not save your layout. Please try again.",Toast.LENGTH_LONG).show();return;}
        hideEditor();stateChanged.run();
    }
    private void hideEditor(){editor.animate().cancel();toolbar.animate().cancel();sizePanel.animate().cancel();editor.setVisibility(View.GONE);}
    private void cancelEditor(){controls.cancelEditing();hideEditor();stateChanged.run();}
    boolean back(){
        if(controls.isEditing()){cancelEditor();return true;}
        if(sidebar.getVisibility()==View.VISIBLE){closeSidebar();return true;}return false;
    }
    private void setToolsVisible(boolean visible){
        controls.cancelEditorFingers();toolbar.setVisibility(visible?View.VISIBLE:View.GONE);
        sizePanel.setVisibility(visible?View.VISIBLE:View.GONE);showTools.setVisibility(visible?View.GONE:View.VISIBLE);
        toolbar.animate().cancel();sizePanel.animate().cancel();if(visible)animateEditorTools();
    }
    private void animateEditorTools(){
        toolbar.animate().cancel();sizePanel.animate().cancel();
        toolbar.setAlpha(0f);toolbar.setTranslationY(-dp(8));toolbar.setScaleX(.97f);toolbar.setScaleY(.97f);
        sizePanel.setAlpha(0f);sizePanel.setTranslationY(dp(8));
        toolbar.animate().alpha(1f).translationY(0).scaleX(1f).scaleY(1f).setDuration(180).setInterpolator(new DecelerateInterpolator()).start();
        sizePanel.animate().alpha(1f).translationY(0).setDuration(180).setInterpolator(new DecelerateInterpolator()).start();
    }
    private void refreshSelection(){
        if(!controls.isEditing())return;syncing=true;
        String[] names={"Menu","Robot","Car"};int profile=controls.editorProfile();title.setText("Custom UI · "+names[profile-1]);
        for(int i=0;i<3;i++)tabs[i].setTextColor(i+1==profile?AMBER:WHITE);
        ArrayList<String> next=new ArrayList<>(),labels=new ArrayList<>();
        for(TouchControls.Control c:controls.input.controls){next.add(c.id);labels.add(c.id.equals("fps")?"FPS counter":c.id.equals("more")?"Sidebar":TouchControlsArtwork.labelFor(c,profile==TouchControls.CAR));}
        if(!ids.equals(next)){
            ids.clear();ids.addAll(next);
            ArrayAdapter<String> adapter=new ArrayAdapter<String>(activity,android.R.layout.simple_spinner_item,labels){
                @Override public View getView(int position,View convertView,android.view.ViewGroup parent){
                    TextView view=(TextView)super.getView(position,convertView,parent);view.setTextSize(12);view.setPadding(dp(2),0,dp(2),0);return view;
                }
            };
            adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);selectedButton.setAdapter(adapter);
        }
        int index=ids.indexOf(controls.selectedId());if(index>=0)selectedButton.setSelection(index);
        int percent=Math.round(controls.selectedSize()*100);sizeLabel.setText(percent+"%");sizeSlider.setProgress(percent-50);
        sizeSlider.setEnabled(index>=0);syncing=false;
    }
    private int dp(int value){return Math.round(value*activity.getResources().getDisplayMetrics().density);}
    private LinearLayout column(){LinearLayout view=new LinearLayout(activity);view.setOrientation(LinearLayout.VERTICAL);return view;}
    private TextView text(String value,int size,int color){TextView view=new TextView(activity);view.setText(value);view.setTextColor(color);view.setTextSize(size);return view;}
    private GradientDrawable background(int color){GradientDrawable shape=new GradientDrawable();shape.setColor(color);shape.setCornerRadius(dp(16));shape.setStroke(dp(1),0x605B6A7D);return shape;}
    private Button button(String title,boolean primary,View.OnClickListener listener){
        Button button=new Button(activity);button.setText(title);button.setAllCaps(false);button.setTextSize(14);
        button.setTextColor(primary?0xFF141B24:WHITE);button.setBackground(background(primary?AMBER:0xFF253244));
        button.setPadding(dp(8),0,dp(8),0);button.setOnClickListener(listener);return button;
    }
    private Button smallButton(String title,boolean primary,View.OnClickListener listener){
        Button button=button(title,primary,listener);button.setTextSize(12);button.setMinHeight(0);button.setMinimumHeight(0);button.setPadding(dp(4),0,dp(4),0);return button;
    }
    private void add(LinearLayout parent,View view,int margin){
        LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,view instanceof Button?dp(48):-2);lp.topMargin=dp(margin);parent.addView(view,lp);
    }
    private void addAction(LinearLayout parent,String title,boolean primary,View.OnClickListener listener){
        LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(dp(58),dp(32));lp.leftMargin=dp(4);parent.addView(smallButton(title,primary,listener),lp);
    }
    private void safeInsets(FrameLayout layer){
        layer.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener(){@Override public WindowInsets onApplyWindowInsets(View view,WindowInsets insets){
            Insets gestures=insets.getSystemGestureInsets();int l=gestures.left,t=gestures.top,r=gestures.right,b=gestures.bottom;
            android.view.DisplayCutout cutout=insets.getDisplayCutout();if(cutout!=null){l=Math.max(l,cutout.getSafeInsetLeft());t=Math.max(t,cutout.getSafeInsetTop());r=Math.max(r,cutout.getSafeInsetRight());b=Math.max(b,cutout.getSafeInsetBottom());}
            view.setPadding(l,t,r,b);return insets;
        }});
    }
}
