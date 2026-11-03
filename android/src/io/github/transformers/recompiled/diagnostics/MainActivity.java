package io.github.transformers.recompiled.diagnostics;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.Drawable;
import android.widget.ImageView;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.RadioGroup;
import android.widget.RadioButton;
import android.database.Cursor;
import android.provider.OpenableColumns;
import java.util.List;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import org.json.JSONArray;
import org.json.JSONObject;

/** Game launcher. Imported originals remain read-only; settings use the private game copy. */
public final class MainActivity extends Activity {
    private static final int PICK_IMAGE=10,PICK_ASSETS=11,PICK_DRIVER=12;
    private static final int INK=0xFFF0F4F8,MUTED=0xFF9BAABB,ACCENT=0xFFE6AB59;
    private static final int[] SCALES={50,67,100},WIDTHS={640,858,1280},HEIGHTS={360,482,720};
    private static final String[] RESOLUTIONS={"360p  ·  640 × 360","480p  ·  858 × 482","720p  ·  1280 × 720"};
    private final ExecutorService worker=Executors.newSingleThreadExecutor();
    private final ExecutorService cancellationWorker=Executors.newSingleThreadExecutor();
    private final ArrayList<View> options=new ArrayList<>();
    private volatile AssetImporter importer;
    private boolean busy,ready,syncing;
    private SharedPreferences preferences;
    private TextView state,message,exeState,dataState,resolution;
    private Button play,choose,chooseAssets,cancelImport;
    private ProgressBar progress;
    private RadioGroup driverChoices;
    private TextView driverStatus;
    private Button addDriver;
    private final ArrayList<View> driverOptions=new ArrayList<>();
    private Switch highRes,bloom,shadows,motionBlur,showFps;

    @Override protected void attachBaseContext(android.content.Context base){
        android.content.res.Configuration english=new android.content.res.Configuration(base.getResources().getConfiguration());
        english.setLocale(java.util.Locale.ENGLISH);english.setLayoutDirection(java.util.Locale.ENGLISH);
        super.attachBaseContext(base.createConfigurationContext(english));
    }

    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);preferences=getSharedPreferences("graphics",MODE_PRIVATE);
        getWindow().setStatusBarColor(0xFF0C121B);getWindow().setNavigationBarColor(0xFF0C121B);
        LinearLayout page=new LinearLayout(this);page.setOrientation(LinearLayout.VERTICAL);
        page.setPadding(dp(24),dp(28),dp(24),dp(28));
        page.setBackground(new GradientDrawable(GradientDrawable.Orientation.TL_BR,
            new int[]{0xFF182536,0xFF0C121B,0xFF14202B}));
        LinearLayout banner=new LinearLayout(this);banner.setGravity(Gravity.CENTER_VERTICAL);
        banner.setPadding(dp(18),dp(20),dp(18),dp(20));
        banner.setBackground(background(0xFF182839,0xFF455361));
        ImageView emblem=new ImageView(this);emblem.setImageDrawable(gameLogo());
        emblem.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        LinearLayout.LayoutParams emblemSize=new LinearLayout.LayoutParams(dp(56),dp(56));emblemSize.rightMargin=dp(14);
        banner.addView(emblem,emblemSize);
        TextView brand=text("Transformers : The Game",27,INK);brand.setTypeface(Typeface.DEFAULT,Typeface.BOLD);
        brand.setLineSpacing(dp(3),1f);
        banner.addView(brand,new LinearLayout.LayoutParams(0,-2,1));
        margin(page,banner,0,0,0,20);

        LinearLayout hero=card(page);
        section(hero,"PLAY","play_arrow");
        state=text("Preparing game...",22,INK);state.setTypeface(Typeface.DEFAULT,Typeface.BOLD);
        margin(hero,state,0,8,0,8);
        message=text("",14,MUTED);hero.addView(message);
        progress=new ProgressBar(this,null,android.R.attr.progressBarStyleHorizontal);
        progress.setIndeterminate(true);progress.setIndeterminateTintList(ColorStateList.valueOf(ACCENT));
        margin(hero,progress,0,12,0,4);
        play=button(hero,"PLAY GAME",true,"play_arrow",new View.OnClickListener(){
            @Override public void onClick(View view){launchGame();}
        });

        LinearLayout library=card(page);section(library,"GAME FILES","folder_open");
        exeState=text("Transformers.exe",14,MUTED);decorate(exeState,"description",MUTED,20);margin(library,exeState,0,12,0,0);
        dataState=text("Game data",14,MUTED);decorate(dataState,"folder_open",MUTED,20);margin(library,dataState,0,6,0,12);
        LinearLayout importRow=new LinearLayout(this);importRow.setOrientation(LinearLayout.HORIZONTAL);
        choose=makeButton("Select EXE",false,"description",new View.OnClickListener(){@Override public void onClick(View v){
            Intent picker=new Intent(Intent.ACTION_OPEN_DOCUMENT);picker.setType("*/*");
            picker.addCategory(Intent.CATEGORY_OPENABLE);picker.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION|Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);startActivityForResult(picker,PICK_IMAGE);
        }});
        chooseAssets=makeButton("Select folder",false,"folder_open",new View.OnClickListener(){@Override public void onClick(View v){
            Intent picker=new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);picker.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION|Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
            startActivityForResult(picker,PICK_ASSETS);
        }});
        LinearLayout.LayoutParams first=new LinearLayout.LayoutParams(0,dp(48),1);first.rightMargin=dp(8);
        importRow.addView(choose,first);importRow.addView(chooseAssets,new LinearLayout.LayoutParams(0,dp(48),1));library.addView(importRow);
        cancelImport=button(library,"Cancel verification",false,"close",new View.OnClickListener(){@Override public void onClick(View v){
            cancelImport.setEnabled(false);cancellationWorker.execute(new Runnable(){@Override public void run(){
                AssetImporter current=importer;if(current!=null)current.cancel();
            }});
        }});cancelImport.setVisibility(View.GONE);

        LinearLayout graphics=card(page);section(graphics,"GRAPHICS","tune");
        LinearLayout row=new LinearLayout(this);row.setOrientation(LinearLayout.HORIZONTAL);row.setGravity(Gravity.CENTER_VERTICAL);
        TextView label=text("Internal resolution",15,INK);decorate(label,"aspect_ratio",MUTED,22);row.addView(label,new LinearLayout.LayoutParams(0,dp(56),1));label.setGravity(Gravity.CENTER_VERTICAL);
        resolution=text("",15,ACCENT);resolution.setGravity(Gravity.CENTER_VERTICAL|Gravity.END);
        row.addView(resolution,new LinearLayout.LayoutParams(-2,dp(56)));
        row.setOnClickListener(new View.OnClickListener(){@Override public void onClick(View v){chooseResolution();}});
        graphics.addView(row);options.add(row);divider(graphics);
        highRes=toggle(graphics,"High-resolution textures","Sharper textures with higher memory usage.","high_res_textures");
        bloom=toggle(graphics,"Bloom","Adds a glow around bright lights.","bloom");
        shadows=toggle(graphics,"Shadows","Character and environment shadows.","shadows");
        motionBlur=toggle(graphics,"Motion Blur","Adds blur during fast movement.","motion_blur");
        TextView graphicsNote=text("Settings apply when the game starts.",12,MUTED);margin(graphics,graphicsNote,0,12,0,0);

        LinearLayout drivers=card(page);section(drivers,"GPU DRIVER","memory");
        driverStatus=text("",14,ACCENT);margin(drivers,driverStatus,0,10,0,6);
        driverChoices=new RadioGroup(this);driverChoices.setOrientation(LinearLayout.VERTICAL);drivers.addView(driverChoices);
        addDriver=button(drivers,"Add GPU driver (.zip)",false,"add_circle",new View.OnClickListener(){public void onClick(View v){
            Intent picker=new Intent(Intent.ACTION_OPEN_DOCUMENT);picker.setType("*/*");picker.addCategory(Intent.CATEGORY_OPENABLE);
            picker.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);startActivityForResult(picker,PICK_DRIVER);
        }});
        margin(drivers,text("Driver ZIP packages for ARM64 / Adreno. Changes apply to the next game session.",12,MUTED),0,10,0,0);
        updateDrivers();

        LinearLayout display=card(page);section(display,"DISPLAY","display_settings");
        showFps=toggle(display,"FPS counter","Show the game frame rate at the top center of the screen.","show_fps");

        TextView footer=text("Port By Sinn",11,MUTED);footer.setGravity(Gravity.CENTER);margin(page,footer,0,10,0,0);
        ScrollView scroll=new ScrollView(this);scroll.setFillViewport(true);scroll.setClipToPadding(false);scroll.addView(page);
        setContentView(scroll);updateResolution();refresh();
    }

    @Override protected void onResume(){super.onResume();if(!busy&&importer==null)refresh();}
    private int dp(int value){return Math.round(value*getResources().getDisplayMetrics().density);}
    private TextView text(String value,int size,int color){TextView view=new TextView(this);view.setText(value);view.setTextSize(size);view.setTextColor(color);return view;}
    private void space(LinearLayout parent,int height){parent.addView(new View(this),new LinearLayout.LayoutParams(1,dp(height)));}
    private void margin(LinearLayout parent,View child,int l,int t,int r,int b){
        LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-1,-2);p.setMargins(dp(l),dp(t),dp(r),dp(b));parent.addView(child,p);
    }
    private GradientDrawable background(int color,int stroke){GradientDrawable shape=new GradientDrawable();shape.setColor(color);shape.setCornerRadius(dp(16));shape.setStroke(dp(1),stroke);return shape;}
    private LinearLayout card(LinearLayout parent){
        LinearLayout box=new LinearLayout(this);box.setOrientation(LinearLayout.VERTICAL);box.setPadding(dp(18),dp(18),dp(18),dp(18));
        box.setBackground(background(0xE5121D2A,0xFF2D4052));margin(parent,box,0,0,0,16);return box;
    }
    private Drawable gameLogo(){
        int id=getResources().getIdentifier("ic_launcher","drawable",getPackageName());
        Drawable image=getDrawable(id).mutate();image.setBounds(0,0,dp(56),dp(56));return image;
    }
    private Drawable icon(String name,int color,int size){
        int id=getResources().getIdentifier("ic_"+name,"drawable",getPackageName());
        if(id==0)throw new IllegalStateException("Missing launcher icon: "+name);
        Drawable image=getDrawable(id).mutate();image.setTint(color);image.setBounds(0,0,dp(size),dp(size));return image;
    }
    private void decorate(TextView view,String name,int color,int size){
        view.setCompoundDrawablesRelative(icon(name,color,size),null,null,null);view.setCompoundDrawablePadding(dp(8));
    }
    private void section(LinearLayout parent,String value,String name){TextView view=text(value,12,ACCENT);view.setLetterSpacing(.12f);view.setGravity(Gravity.CENTER_VERTICAL);decorate(view,name,ACCENT,24);parent.addView(view);}
    private void divider(LinearLayout parent){View line=new View(this);line.setBackgroundColor(0xFF29394A);parent.addView(line,new LinearLayout.LayoutParams(-1,dp(1)));}
    private Button makeButton(String value,boolean primary,String name,View.OnClickListener listener){
        Button result=new Button(this);result.setText(value);result.setAllCaps(false);result.setTextSize(primary?17:13);
        result.setTypeface(Typeface.DEFAULT,Typeface.BOLD);result.setTextColor(primary?0xFF14202B:INK);
        result.setPadding(dp(10),0,dp(10),0);decorate(result,name,primary?0xFF14202B:INK,22);
        result.setBackground(background(primary?ACCENT:0xFF233447,primary?ACCENT:0xFF3A5268));
        result.setOnClickListener(listener);return result;
    }
    private Button button(LinearLayout parent,String value,boolean primary,String name,View.OnClickListener click){
        Button result=makeButton(value,primary,name,click);LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-1,dp(primary?56:48));
        p.topMargin=dp(14);parent.addView(result,p);return result;
    }
    private Switch toggle(LinearLayout parent,String title,String description,final String key){
        LinearLayout row=new LinearLayout(this);row.setOrientation(LinearLayout.HORIZONTAL);row.setGravity(Gravity.CENTER_VERTICAL);row.setPadding(0,dp(12),0,dp(12));
        LinearLayout captions=new LinearLayout(this);captions.setOrientation(LinearLayout.VERTICAL);
        String symbol=key.equals("high_res_textures")?"texture":key.equals("bloom")?"flare":key.equals("shadows")?"contrast":key.equals("motion_blur")?"blur_on":"speed";
        ImageView leading=new ImageView(this);leading.setImageDrawable(icon(symbol,MUTED,22));leading.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        LinearLayout.LayoutParams leadingSize=new LinearLayout.LayoutParams(dp(24),dp(24));leadingSize.rightMargin=dp(12);row.addView(leading,leadingSize);
        captions.addView(text(title,15,INK));if(!description.isEmpty()){TextView caption=text(description,12,MUTED);caption.setPadding(0,dp(4),dp(10),0);captions.addView(caption);}
        row.addView(captions,new LinearLayout.LayoutParams(0,-2,1));
        final Switch value=new Switch(this);value.setContentDescription(title);value.setShowText(false);
        int[][] states={{android.R.attr.state_checked},{}};value.setThumbTintList(new ColorStateList(states,new int[]{ACCENT,0xFFACB9C8}));
        value.setTrackTintList(new ColorStateList(states,new int[]{0xFF78613D,0xFF3B4C5E}));
        value.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener(){@Override public void onCheckedChanged(CompoundButton b,boolean checked){
            if(!syncing)preferences.edit().putBoolean(key,checked).apply();
        }});
        row.setOnClickListener(new View.OnClickListener(){@Override public void onClick(View v){value.setChecked(!value.isChecked());}});
        row.addView(value,new LinearLayout.LayoutParams(-2,dp(48)));parent.addView(row);options.add(row);options.add(value);return value;
    }
    private boolean customDriversSupported(){return android.os.Process.is64Bit()&&java.util.Arrays.asList(android.os.Build.SUPPORTED_ABIS).contains("arm64-v8a");}
    private void updateDrivers(){
        driverChoices.setOnCheckedChangeListener(null);driverChoices.removeAllViews();driverOptions.clear();
        final SharedPreferences selected=getSharedPreferences("gpu_driver",MODE_PRIVATE);
        String mode=selected.getString("mode","auto"),path=selected.getString("path","");
        List<DriverLibrary.Driver> saved=new DriverLibrary(getFilesDir()).list();
        ArrayList<String> modes=new ArrayList<>(),paths=new ArrayList<>(),labels=new ArrayList<>();
        modes.add("auto");paths.add("");labels.add("Automatic - bundled Turnip 26.2.5");
        modes.add("system");paths.add("");labels.add("System driver");
        for(DriverLibrary.Driver driver:saved){modes.add("custom");paths.add(driver.file.getAbsolutePath());labels.add(driver.label);}
        int checked=-1;String label="Driver unavailable. Select another driver.";
        for(int i=0;i<labels.size();i++){
            RadioButton choice=new RadioButton(this);choice.setId(View.generateViewId());choice.setText(labels.get(i));choice.setTextColor(INK);choice.setTextSize(14);
            final String chosenMode=modes.get(i),chosenPath=paths.get(i),chosenLabel=labels.get(i);
            choice.setEnabled(!busy&&(!chosenMode.equals("custom")||customDriversSupported()));
            driverChoices.addView(choice);driverOptions.add(choice);
            if(mode.equals(chosenMode)&&(!mode.equals("custom")||path.equals(chosenPath))){checked=choice.getId();label=chosenLabel;}
            choice.setTag(new String[]{chosenMode,chosenPath,chosenLabel});
        }
        if(checked!=-1)driverChoices.check(checked);driverStatus.setText(label);
        driverChoices.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener(){public void onCheckedChanged(RadioGroup group,int id){
            RadioButton button=(RadioButton)group.findViewById(id);if(button==null)return;
            String[] value=(String[])button.getTag();selected.edit().putString("mode",value[0]).putString("path",value[1]).apply();driverStatus.setText(value[2]);
        }});
        addDriver.setEnabled(!busy&&customDriversSupported());
    }
    private void importDriver(final Uri uri){
        if(!customDriversSupported()){report("Driver not supported","GPU driver ZIP packages require Android ARM64 / Adreno.");return;}
        setBusy(true);status("Adding GPU driver...","Saving the ZIP and validating the driver library.");
        worker.execute(new Runnable(){public void run(){
            try{
                String name=null;
                try(Cursor cursor=getContentResolver().query(uri,new String[]{OpenableColumns.DISPLAY_NAME},null,null,null)){
                    if(cursor!=null&&cursor.moveToFirst())name=cursor.getString(0);
                }
                final DriverLibrary.Driver added;
                try(InputStream input=getContentResolver().openInputStream(uri)){
                    if(input==null)throw new java.io.IOException("Cannot read the ZIP file.");
                    added=new DriverZipImporter(getFilesDir()).add(input,name);
                }
                if(!getSharedPreferences("gpu_driver",MODE_PRIVATE).edit().putString("mode","custom").putString("path",added.file.getAbsolutePath()).commit())
                    throw new java.io.IOException("Cannot save the selected driver.");
                runOnUiThread(new Runnable(){public void run(){if(!isFinishing()&&!isDestroyed()){setBusy(false);updateDrivers();status("GPU driver added",added.label+" selected for the next session.");}}});
            }catch(final Exception error){Log.e("TFLauncher","Driver import failed",error);report("Could not add GPU driver",error.getMessage());}
        }});
    }
    private int selectedResolution(){int scale=preferences.getInt("render_scale",50);for(int i=0;i<SCALES.length;++i)if(SCALES[i]==scale)return i;return 0;}
    private void updateResolution(){resolution.setText(RESOLUTIONS[selectedResolution()]+"  ›");}
    private void chooseResolution(){
        if(busy)return;
        new AlertDialog.Builder(this).setTitle("Internal resolution")
            .setSingleChoiceItems(RESOLUTIONS,selectedResolution(),new DialogInterface.OnClickListener(){@Override public void onClick(DialogInterface dialog,int index){
                preferences.edit().putInt("render_scale",SCALES[index]).apply();updateResolution();dialog.dismiss();
            }}).setNegativeButton("Cancel",null).show();
    }
    private void setBusy(boolean value){
        busy=value;choose.setEnabled(!value);chooseAssets.setEnabled(!value);
        play.setEnabled(!value&&ready&&BuildInfo.DXVK_AVAILABLE);play.setAlpha(play.isEnabled()?1f:.45f);
        for(View option:options){option.setEnabled(!value);option.setAlpha(value?.55f:1f);}
        if(addDriver!=null)addDriver.setEnabled(!value&&customDriversSupported());
        for(View option:driverOptions){String[] selected=(String[])option.getTag();option.setEnabled(!value&&(!selected[0].equals("custom")||customDriversSupported()));}
        progress.setVisibility(value?View.VISIBLE:View.GONE);
        cancelImport.setVisibility(importer!=null?View.VISIBLE:View.GONE);cancelImport.setEnabled(importer!=null);
    }
    private File gameDirectory(){return new File(getFilesDir(),"game");}
    private File gameImage(){return new File(gameDirectory(),"Transformers.exe");}
    private void status(String title,String detail){state.setText(title);message.setText(detail);}
    private void report(final String title,final String detail){runOnUiThread(new Runnable(){@Override public void run(){
        if(!isFinishing()&&!isDestroyed()){setBusy(false);status(title,detail);}
    }});}

    private void refresh(){
        if(importer!=null)return;
        setBusy(true);
        worker.execute(new Runnable(){@Override public void run(){
            try {
                File root=gameDirectory();
                final boolean image=GameSources.hasExe(MainActivity.this);
                final int present=GameSources.available(MainActivity.this),total=GameSources.manifest(MainActivity.this).getJSONArray("files").length();
                final GameSettings settings=GameSettings.load(root);
                runOnUiThread(new Runnable(){@Override public void run(){
                    if(isFinishing()||isDestroyed())return;
                    syncing=true;
                    highRes.setChecked(preferences.getBoolean("high_res_textures",settings.highResTextures));
                    bloom.setChecked(preferences.getBoolean("bloom",settings.bloom));
                    shadows.setChecked(preferences.getBoolean("shadows",settings.shadows));
                    motionBlur.setChecked(preferences.getBoolean("motion_blur",settings.motionBlur));
                    showFps.setChecked(preferences.getBoolean("show_fps",false));
                    syncing=false;ready=image&&present==total;
                    exeState.setText(image?"EXE  ·  Ready":"EXE  ·  Not selected");exeState.setTextColor(image?0xFF94D4B2:MUTED);
                    dataState.setText("DATA  ·  "+present+" / "+total+" files in the source folder");dataState.setTextColor(present==total?0xFF94D4B2:MUTED);
                    setBusy(false);
                    status(ready?"Ready to play":"Select your game files",ready?"Game data is read directly from your folder. Adjust graphics, then play.":"Select your game folder and EXE. Files are read directly without copying.");
                }});
            } catch(Exception error){Log.e("TFLauncher","Cannot load game setup",error);report("Settings unavailable",error.getMessage());}
        }});
    }

    private void launchGame(){
        if(busy||!ready)return;
        final int index=selectedResolution();final boolean hires=highRes.isChecked(),useBloom=bloom.isChecked(),useShadows=shadows.isChecked(),blur=motionBlur.isChecked();
        final boolean fpsEnabled=showFps.isChecked();
        setBusy(true);status("Starting game...","Applying graphics settings.");
        worker.execute(new Runnable(){@Override public void run(){
            try {
                final int renderScale=GameSettings.prepareLaunch(gameDirectory(),hires,SCALES[index],useBloom,useShadows,blur);
                Log.i("TFLauncher","Launch resolution="+WIDTHS[index]+"x"+HEIGHTS[index]+" highRes="+hires+" bloom="+useBloom+" shadows="+useShadows+" motionBlur="+blur+" showFps="+fpsEnabled);
                runOnUiThread(new Runnable(){@Override public void run(){
                    if(isFinishing()||isDestroyed())return;
                    setBusy(false);Intent intent=new Intent(MainActivity.this,GameActivity.class);
                    intent.addFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT|Intent.FLAG_ACTIVITY_SINGLE_TOP);
                    // Each launch restores an accepted PC mode before the bridge scales the backbuffer once.
                    intent.putExtra("render_scale",renderScale);intent.putExtra("show_fps",fpsEnabled);startActivity(intent);
                }});
            } catch(Exception error){Log.e("TFLauncher","Cannot apply graphics settings",error);report("Could not start game",error.getMessage());}
        }});
    }

    @Override protected void onActivityResult(int request,int result,Intent data){
        super.onActivityResult(request,result,data);
        if(result!=RESULT_OK||data==null||data.getData()==null)return;
        if(request==PICK_DRIVER){importDriver(data.getData());return;}
        try{getContentResolver().takePersistableUriPermission(data.getData(),Intent.FLAG_GRANT_READ_URI_PERMISSION);}
        catch(SecurityException error){report("Folder permission unavailable","Select a local file location and allow persistent access.");return;}
        if(request==PICK_ASSETS){importAssets(data.getData());return;}
        if(request!=PICK_IMAGE)return;
        final Uri uri=data.getData();setBusy(true);status("Preparing EXE...","Verifying the selected game executable.");
        worker.execute(new Runnable(){@Override public void run(){try{
            GameSources.selectExe(MainActivity.this,uri);
            runOnUiThread(new Runnable(){@Override public void run(){if(!isFinishing()&&!isDestroyed())refresh();}});
        }catch(Exception error){Log.e("TFLauncher","EXE selection failed",error);report("EXE not accepted","Select Transformers.exe from the supported game version.");}}});
    }
    private void importAssets(Uri tree){
        final AssetImporter task=new AssetImporter(getApplicationContext(),tree,gameDirectory(),new AssetImporter.Listener(){
            @Override public void update(final String text){runOnUiThread(new Runnable(){@Override public void run(){
                if(!isFinishing()&&!isDestroyed())status("Preparing game data...",text);
            }});}
        });
        importer=task;setBusy(true);status("Verifying game folder...","Files are read directly from this folder without copying.");
        worker.execute(new Runnable(){@Override public void run(){
            try{task.run();importer=null;runOnUiThread(new Runnable(){@Override public void run(){if(!isFinishing()&&!isDestroyed())refresh();}});}
            catch(Exception error){importer=null;Log.e("TFLauncher","Source verification interrupted",error);report("Verification incomplete",error.getMessage()+"\nSelect your game folder again to verify the source files.");}
        }});
    }
    @Override protected void onDestroy(){
        final AssetImporter task=importer;if(task!=null)cancellationWorker.execute(new Runnable(){@Override public void run(){task.cancel();}});
        worker.shutdownNow();cancellationWorker.shutdown();super.onDestroy();
    }
}
