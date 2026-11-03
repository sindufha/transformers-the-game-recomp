package io.github.transformers.recompiled.diagnostics;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.widget.FrameLayout;
import android.widget.TextView;
import java.io.File;
import java.io.InputStream;
import java.io.ByteArrayOutputStream;
import java.util.Locale;
import org.json.JSONArray;
import org.json.JSONObject;

/** The graphics/runtime process is isolated from the import/diagnostics UI. */
public final class GameActivity extends Activity implements SurfaceHolder.Callback {
    private static volatile GameActivity currentActivity;
    private static volatile String lastNativeMessage="";
    private static volatile boolean sessionEnded;
    private static boolean sessionStarted;
    private final Handler ui = new Handler(Looper.getMainLooper());
    private boolean started;
    private boolean loaded;
    private boolean resumed,surfaceAttached;
    private TouchControlsView controls;
    private TouchUiPanel touchUi;
    private float audioGain=1f;
    private TextView status;
    private TextView fpsLabel;
    private boolean showFps;
    private final FrameRateCounter frameRate=new FrameRateCounter();
    private AudioManager audioManager;
    private AudioFocusRequest audioFocus;
    private native int nativeRun(Surface surface, String directory, String logPath, boolean smoke, int renderScale, String driverMode, String driverPath);
    private native void nativeKey(int virtualKey,boolean down);
    private native void nativeMouse(int dx,int dy,int wheel,int buttons);
    private native void nativeAudioGain(float gain);
    private native int nativeControlProfile();
    private native int nativePresentedFrames();
    private native void nativeSetForeground(boolean foreground);
    private native void nativeSetSurface(Surface surface,boolean nativeResolution);
    private final Runnable controlPoll=new Runnable(){
        @Override public void run(){
            if(!resumed||!loaded||isFinishing()||isDestroyed())return;
            if(controls!=null)controls.setProfile(nativeControlProfile());
            ui.postDelayed(this,16);
        }
    };
    private final Runnable fpsPoll=new Runnable(){
        @Override public void run(){
            if(!resumed||!showFps||!loaded||sessionEnded||!hasWindowFocus()||(touchUi!=null&&touchUi.isOpen())||isFinishing()||isDestroyed())return;
            float fps=frameRate.sample(nativePresentedFrames(),SystemClock.elapsedRealtimeNanos());
            if(!Float.isNaN(fps))fpsLabel.setText(String.format(Locale.US,"FPS %.1f",fps));
            ui.postDelayed(this,500);
        }
    };

    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        currentActivity=this;started=sessionStarted;
        showFps=saved!=null?saved.getBoolean("show_fps",false):getIntent().getBooleanExtra("show_fps",false);
        setVolumeControlStream(AudioManager.STREAM_MUSIC);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON|WindowManager.LayoutParams.FLAG_FULLSCREEN);
        WindowManager.LayoutParams attributes=getWindow().getAttributes();
        attributes.layoutInDisplayCutoutMode=WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        getWindow().setAttributes(attributes);
        FrameLayout content = new FrameLayout(this);
        SurfaceView surface = new SurfaceView(this); surface.getHolder().addCallback(this);
        surface.setFocusable(true); surface.setFocusableInTouchMode(true); surface.requestFocus();
        content.addView(surface,new FrameLayout.LayoutParams(-1,-1));
        status = new TextView(this); status.setTextColor(Color.WHITE);
        status.setBackgroundColor(0x9912161d); status.setPadding(16,12,16,12);
        status.setText("Starting game...");
        content.addView(status,new FrameLayout.LayoutParams(-1,-2,Gravity.TOP));
        if (!getIntent().getBooleanExtra("smoke",false)) {
            controls=new TouchControlsView(this,new TouchControls.Output() {
                @Override public void key(int key,boolean down) { if(loaded)nativeKey(key,down); }
                @Override public void mouse(int dx,int dy,int wheel,int buttons) {
                    if(loaded)nativeMouse(dx,dy,wheel,buttons);
                }
                @Override public void close() { finish(); }
                @Override public void menu() { if(loaded&&touchUi!=null)touchUi.openSidebar(); }
            });
            controls.setEnabled(false);
            content.addView(controls,new FrameLayout.LayoutParams(-1,-1));
        }
        fpsLabel=new TextView(this);fpsLabel.setText("FPS --");fpsLabel.setTextSize(13);
        fpsLabel.setTextColor(0xFFE3F8EF);fpsLabel.setTypeface(Typeface.MONOSPACE,Typeface.BOLD);
        fpsLabel.setGravity(Gravity.CENTER);fpsLabel.setIncludeFontPadding(false);
        fpsLabel.setPadding(dp(10),dp(5),dp(10),dp(5));
        GradientDrawable fpsBackground=new GradientDrawable();fpsBackground.setColor(0xAA0C1821);
        fpsBackground.setCornerRadius(dp(7));fpsBackground.setStroke(dp(1),0x557CB8BC);fpsLabel.setBackground(fpsBackground);
        fpsLabel.setClickable(false);fpsLabel.setFocusable(false);
        fpsLabel.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        fpsLabel.setVisibility(View.GONE);
        FrameLayout.LayoutParams fpsPosition=new FrameLayout.LayoutParams(-2,-2,Gravity.TOP|Gravity.CENTER_HORIZONTAL);
        fpsPosition.topMargin=dp(82);content.addView(fpsLabel,fpsPosition);
        if(controls!=null)controls.setFpsCounter(fpsLabel);
        if(controls!=null)touchUi=new TouchUiPanel(this,content,controls,new Runnable(){@Override public void run(){
            updateSessionForeground();refreshFpsCounter();
        }});
        setContentView(content);
        immersive();
        try { System.loadLibrary("transformers_game"); loaded=true; if(controls!=null)controls.setEnabled(true); }
        catch (UnsatisfiedLinkError error) {
            started=true; status.setText("The DXVK runtime is unavailable for this ABI.\n" + error.getMessage());
        }
        if(sessionStarted){if(sessionEnded)status.setText(lastNativeMessage);else status.setVisibility(View.GONE);}
        Log.i("TFGame","Activity created; existingSession="+sessionStarted);
    }

    private int dp(int value){return Math.round(value*getResources().getDisplayMetrics().density);}
    private void updateSessionForeground(){
        if(!loaded)return;
        boolean active=resumed&&hasWindowFocus()&&(touchUi==null||!touchUi.isOpen());
        nativeSetForeground(active);nativeAudioGain(active?audioGain:0f);
    }
    private void refreshFpsCounter(){
        ui.removeCallbacks(fpsPoll);frameRate.reset();fpsLabel.setText("FPS --");
        fpsLabel.setVisibility(showFps&&loaded&&!sessionEnded&&(touchUi==null||!touchUi.isOpen())?View.VISIBLE:View.GONE);
        if(showFps&&loaded&&resumed&&hasWindowFocus()&&!sessionEnded)ui.post(fpsPoll);
    }
    @Override protected void onNewIntent(Intent intent){
        super.onNewIntent(intent);
        if(intent.hasExtra("show_fps"))showFps=intent.getBooleanExtra("show_fps",false);
        intent.putExtra("show_fps",showFps);setIntent(intent);refreshFpsCounter();
        Log.i("TFGame","FPS counter enabled="+showFps);
    }
    @Override protected void onSaveInstanceState(Bundle saved){
        saved.putBoolean("show_fps",showFps);super.onSaveInstanceState(saved);
    }

    private void immersive(){
        if(Build.VERSION.SDK_INT>=30){
            getWindow().setDecorFitsSystemWindows(false);
            WindowInsetsController controller=getWindow().getInsetsController();
            if(controller!=null){
                controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                controller.hide(WindowInsets.Type.systemBars());
            }
        }else getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            |View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN|View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            |View.SYSTEM_UI_FLAG_FULLSCREEN|View.SYSTEM_UI_FLAG_HIDE_NAVIGATION|View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
    }

    private int virtualKey(int key) {
        if (key>=KeyEvent.KEYCODE_A && key<=KeyEvent.KEYCODE_Z) return 65+key-KeyEvent.KEYCODE_A;
        if (key>=KeyEvent.KEYCODE_0 && key<=KeyEvent.KEYCODE_9) return 48+key-KeyEvent.KEYCODE_0;
        if (key>=KeyEvent.KEYCODE_F1 && key<=KeyEvent.KEYCODE_F12) return 112+key-KeyEvent.KEYCODE_F1;
        switch(key) {
            case KeyEvent.KEYCODE_ENTER:return 13;case KeyEvent.KEYCODE_ESCAPE:case KeyEvent.KEYCODE_BACK:return 27;
            case KeyEvent.KEYCODE_SPACE:return 32;case KeyEvent.KEYCODE_TAB:return 9;
            case KeyEvent.KEYCODE_DEL:return 8;case KeyEvent.KEYCODE_FORWARD_DEL:return 46;
            case KeyEvent.KEYCODE_DPAD_LEFT:return 37;case KeyEvent.KEYCODE_DPAD_UP:return 38;
            case KeyEvent.KEYCODE_DPAD_RIGHT:return 39;case KeyEvent.KEYCODE_DPAD_DOWN:return 40;
            case KeyEvent.KEYCODE_SHIFT_LEFT:return 160;case KeyEvent.KEYCODE_SHIFT_RIGHT:return 161;
            case KeyEvent.KEYCODE_CTRL_LEFT:return 162;case KeyEvent.KEYCODE_CTRL_RIGHT:return 163;
            case KeyEvent.KEYCODE_ALT_LEFT:return 164;case KeyEvent.KEYCODE_ALT_RIGHT:return 165;
            default:return 0;
        }
    }
    @Override public boolean onKeyDown(int key,KeyEvent event) {
        if(touchUi!=null&&touchUi.isOpen()){
            if(key==KeyEvent.KEYCODE_BACK||key==KeyEvent.KEYCODE_ESCAPE){if(event.getRepeatCount()==0)touchUi.back();return true;}
            if(virtualKey(key)!=0)return true;
        }
        int virtualKey=virtualKey(key);
        if (loaded && virtualKey!=0) { if(controls!=null)controls.input.hardwareKey(virtualKey,true); else nativeKey(virtualKey,true); return true; }
        return super.onKeyDown(key,event);
    }
    @Override public boolean onKeyUp(int key,KeyEvent event) {
        if(touchUi!=null&&touchUi.isOpen()&&virtualKey(key)!=0)return true;
        int virtualKey=virtualKey(key);
        if (loaded && virtualKey!=0) { if(controls!=null)controls.input.hardwareKey(virtualKey,false); else nativeKey(virtualKey,false); return true; }
        return super.onKeyUp(key,event);
    }

    @Override public void surfaceCreated(SurfaceHolder holder) { }
    private void startOnSurfaceReady(final SurfaceHolder holder) {
        if(!loaded)return;
        nativeSetSurface(holder.getSurface(),new File(getFilesDir(),"game/gpu-native-resolution").isFile());
        surfaceAttached=true;
        if (started) return;
        started=sessionStarted=true;
        startForegroundService(new Intent(this,GameSessionService.class));
        final boolean smoke = getIntent().getBooleanExtra("smoke",false);
        new Thread(new Runnable() { @Override public void run() {
            try {
                File directory = new File(getFilesDir(),"game");
                if (!directory.isDirectory() && !directory.mkdirs()) throw new Exception("Cannot create the game folder.");
                android.content.SharedPreferences driver=getSharedPreferences("gpu_driver",MODE_PRIVATE);
                String driverMode=driver.getString("mode","auto"),driverPath="";
                if(driverMode.equals("custom")){
                    if(!android.os.Process.is64Bit()||!java.util.Arrays.asList(Build.SUPPORTED_ABIS).contains("arm64-v8a"))throw new Exception("Custom GPU driver requires ARM64.");
                    driverPath=new DriverLibrary(getFilesDir()).require(driver.getString("path","")).file.getCanonicalPath();
                }else if(!driverMode.equals("auto")&&!driverMode.equals("system"))throw new Exception("Invalid GPU driver selection.");
                Log.i("TFGame","GPU driver mode="+driverMode+" path="+driverPath);
                GameSources.Session sourceSession=null;
                try {
                if (!smoke) {
                    sourceSession=GameSources.Session.mount(getApplicationContext(),directory);
                    installShaderCache(directory);
                    Log.i("TFGame","Direct SAF sources mounted; original game data not copied");
                }
                final int result = nativeRun(holder.getSurface(),directory.getAbsolutePath(),
                    new File(getFilesDir(),"dxvk-runtime.log").getAbsolutePath(),smoke,
                    getIntent().getIntExtra("render_scale",100),driverMode,driverPath);
                Log.i("TFGame","Game finished with code "+result);
                sessionEnded=true;
                onNativeEvent(result==0?"Game session ended.":"The game stopped before loading completed.");
                } finally {if(sourceSession!=null)sourceSession.close();}
            } catch (Exception error) {sessionEnded=true;Log.e("TFGame","Game startup failed",error);onNativeEvent("Could not start game: "+error.getMessage());}
        }},"transformers-game").start();
    }

    private void installShaderCache(File directory) throws Exception {
        String[] files=getAssets().list("shader-cache");
        if (files==null || files.length==0) return;
        ByteArrayOutputStream bytes=new ByteArrayOutputStream();
        try (InputStream input=getAssets().open("shader-cache/index.json")) {
            byte[] block=new byte[8192];int count;
            while ((count=input.read(block))!=-1) {
                if (bytes.size()+count>1048576) throw new Exception("Indeks shader terlalu besar.");
                bytes.write(block,0,count);
            }
        }
        JSONArray index=new JSONArray(bytes.toString("UTF-8"));
        File root=new File(directory,"shader-cache");
        for (int i=0;i<index.length();++i) {
            JSONObject item=index.getJSONObject(i);String path=item.getString("path");
            File target=VerifiedFiles.destination(root,path);long size=item.getLong("bytes");String hash=item.getString("sha256");
            if (VerifiedFiles.matches(target,size,hash,null)) continue;
            try (InputStream input=getAssets().open("shader-cache/"+path)) {VerifiedFiles.copy(input,target,size,hash,null);}
        }
    }

    // Called on the Java worker thread by JNI; all View access stays on main.
    private void onNativeEvent(final String message) {
        Log.i("TFGame",message);
        lastNativeMessage=message;
        GameActivity activity=currentActivity;
        if(activity!=null)activity.showNativeEvent(message);
    }
    private void showNativeEvent(final String message){
        final String visible=message.startsWith("Menjalankan startup kode game")?"Loading game...":
            message.startsWith("Memeriksa EXE")?"Loading game data...":message;
        ui.post(new Runnable() { @Override public void run() {
            if (!isFinishing() && !isDestroyed()) {
                if(sessionEnded)stopService(new Intent(GameActivity.this,GameSessionService.class));
                if(sessionEnded)refreshFpsCounter();
                status.setText(visible);status.setVisibility(View.VISIBLE);
                if (message.startsWith("Menjalankan startup kode game")) ui.postDelayed(new Runnable() {
                    @Override public void run() {
                        if (!isFinishing() && !isDestroyed() && visible.contentEquals(status.getText())) status.setVisibility(View.GONE);
                    }
                },2000);
            }
        }});
    }

    @Override public void surfaceChanged(SurfaceHolder holder,int format,int width,int height) {
        // Android guarantees an initial size callback after creation. Attach
        // only once dimensions are known, including a recreated resume surface.
        if(width>0&&height>0&&!surfaceAttached)startOnSurfaceReady(holder);
    }
    @Override public void surfaceDestroyed(SurfaceHolder holder) {
        if(loaded&&surfaceAttached&&currentActivity==this){nativeSetSurface(null,false);surfaceAttached=false;}
    }
    @Override public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused);
        if(!focused&&controls!=null)controls.releaseAll();
        if(focused)immersive();
        updateSessionForeground();
        refreshFpsCounter();
    }
    @Override protected void onPause(){
        resumed=false;ui.removeCallbacks(controlPoll);
        ui.removeCallbacks(fpsPoll);frameRate.reset();
        if(controls!=null)controls.releaseAll();
        if(loaded){nativeSetForeground(false);nativeAudioGain(0f);}
        if(audioManager!=null&&audioFocus!=null)audioManager.abandonAudioFocusRequest(audioFocus);
        Log.i("TFGame","Activity paused; session retained");super.onPause();
    }
    @Override protected void onStop() {
        ui.removeCallbacks(controlPoll);
        ui.removeCallbacks(fpsPoll);
        if(controls!=null)controls.releaseAll();
        Log.i("TFGame","Activity stopped; session retained");super.onStop();
    }
    @Override protected void onDestroy() {
        if(audioManager!=null&&audioFocus!=null)audioManager.abandonAudioFocusRequest(audioFocus);
        if(currentActivity==this){
            currentActivity=null;
            if(loaded){nativeSetForeground(false);if(surfaceAttached)nativeSetSurface(null,false);nativeAudioGain(0f);}
        }
        loaded=false;
        ui.removeCallbacksAndMessages(null);
        super.onDestroy();
        if(isFinishing()){
            stopService(new Intent(this,GameSessionService.class));
            android.os.Process.killProcess(android.os.Process.myPid());
        }
    }

    @Override protected void onResume() {
        super.onResume();
        resumed=true;immersive();
        if(!loaded)return;
        updateSessionForeground();
        refreshFpsCounter();
        Log.i("TFGame","FPS counter enabled="+showFps);
        Log.i("TFGame","Activity resumed; existingSession="+sessionStarted);
        ui.removeCallbacks(controlPoll);ui.post(controlPoll);
        audioManager=(AudioManager)getSystemService(AUDIO_SERVICE);
        if(audioManager==null)return;
        if(audioFocus==null)audioFocus=new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
            .setAudioAttributes(new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME)
                .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build())
            .setOnAudioFocusChangeListener(new AudioManager.OnAudioFocusChangeListener(){
                @Override public void onAudioFocusChange(int focus){
                    audioGain=focus==AudioManager.AUDIOFOCUS_GAIN?1f:focus==AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK?.25f:0f;
                    updateSessionForeground();
                }
            },ui).build();
        audioGain=audioManager.requestAudioFocus(audioFocus)==AudioManager.AUDIOFOCUS_REQUEST_GRANTED?1f:0f;updateSessionForeground();
    }
}
