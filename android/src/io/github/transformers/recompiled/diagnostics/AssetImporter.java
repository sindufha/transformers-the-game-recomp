package io.github.transformers.recompiled.diagnostics;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.CancellationSignal;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;
import java.io.*;
import java.util.*;
import org.json.*;

/** Verifies an explicitly selected SAF folder and saves locations, never asset bytes. */
final class AssetImporter {
    interface Listener { void update(String message); }
    private final Context context;
    private final Uri tree;
    private final File root;
    private final Listener listener;
    private final CancellationSignal cancellation=new CancellationSignal();
    private volatile boolean cancelled;
    private volatile InputStream activeInput;
    private final Map<String,Map<String,Entry>> directories=new HashMap<>();
    AssetImporter(Context c,Uri t,File r,Listener l){context=c;tree=t;root=r;listener=l;}
    void cancel(){cancelled=true;cancellation.cancel();InputStream in=activeInput;if(in!=null)try{in.close();}catch(IOException ignored){}}
    private void check()throws IOException{VerifiedFiles.checkInterrupted();if(cancelled)throw new InterruptedIOException("Verification canceled.");}
    private static final class Entry {
        final Uri uri;final boolean directory;final long size;
        Entry(Uri u,boolean d,long n){uri=u;directory=d;size=n;}
    }
    private Map<String,Entry> children(String id)throws Exception{
        check();if(directories.containsKey(id))return directories.get(id);
        Map<String,Entry> entries=new HashMap<>();
        Uri uri=DocumentsContract.buildChildDocumentsUriUsingTree(tree,id);
        try(Cursor cursor=context.getContentResolver().query(uri,new String[]{Document.COLUMN_DOCUMENT_ID,Document.COLUMN_DISPLAY_NAME,Document.COLUMN_MIME_TYPE,Document.COLUMN_SIZE},null,null,null,cancellation)){
            if(cursor==null)throw new IOException("Cannot read the folder.");int rows=0;
            while(cursor.moveToNext()){
                check();if(++rows>20000)throw new IOException("The folder contains too many files.");
                String name=cursor.getString(1);if(name==null)continue;String key=name.toLowerCase(Locale.ROOT);
                if(entries.containsKey(key))throw new IOException("Ambiguous filename: "+name);
                entries.put(key,new Entry(DocumentsContract.buildDocumentUriUsingTree(tree,cursor.getString(0)),Document.MIME_TYPE_DIR.equals(cursor.getString(2)),cursor.isNull(3)?-1:cursor.getLong(3)));
            }
        }
        directories.put(id,entries);return entries;
    }
    private Entry resolve(String path)throws Exception{
        String id=DocumentsContract.getTreeDocumentId(tree);String[] parts=path.split("/");Entry e=null;
        for(int i=0;i<parts.length;i++){
            e=children(id).get(parts[i].toLowerCase(Locale.ROOT));
            if(e==null||e.directory!=(i<parts.length-1))throw new IOException("File not found: "+path);
            id=DocumentsContract.getDocumentId(e.uri);
        }
        return e;
    }
    String run()throws Exception{
        JSONObject manifest=GameSources.manifest(context);JSONArray expected=manifest.getJSONArray("files"),sources=new JSONArray();
        for(int i=0;i<expected.length();i++){
            check();JSONObject item=expected.getJSONObject(i);String path=item.getString("path");
            SourcePaths.destination(root,path);Entry entry=resolve(path);long size=item.getLong("bytes");
            if(entry.size>=0&&entry.size!=size)throw new IOException("Source file size mismatch: "+path);
            listener.update("Verifying source "+(i+1)+" / "+expected.length()+"\n"+path+"\nFiles are read directly without copying.");
            try(InputStream in=context.getContentResolver().openInputStream(entry.uri)){
                if(in==null)throw new IOException("Cannot read file: "+path);
                activeInput=in;GameSources.verify(in,size,item.getString("sha256"),new VerifiedFiles.Progress(){public void bytes(long n)throws IOException{check();}});
            }finally{activeInput=null;}
            JSONObject record=new JSONObject(item.toString());record.put("uri",entry.uri.toString());sources.put(record);
        }
        check();
        JSONObject index=new JSONObject().put("version",1).put("input_sha256",BuildInfo.INPUT_SHA256).put("tree",tree.toString()).put("files",sources);
        // Also accept the supported EXE from the selected game folder when present.
        Entry exe=children(DocumentsContract.getTreeDocumentId(tree)).get("transformers.exe");
        if(exe!=null&&!exe.directory)GameSources.selectExe(context,exe.uri);
        check();GameSources.publish(context,index);
        // Remove only verified private duplicates after all originals are accessible.
        boolean running=false;
        android.app.ActivityManager am=(android.app.ActivityManager)context.getSystemService(Context.ACTIVITY_SERVICE);
        List<android.app.ActivityManager.RunningAppProcessInfo> processes=am==null?null:am.getRunningAppProcesses();
        if(processes!=null)for(android.app.ActivityManager.RunningAppProcessInfo p:processes)if((context.getPackageName()+":game").equals(p.processName))running=true;
        if(!running){
            for(int i=0;i<expected.length();i++){check();listener.update("Removing verified old data copies... "+(i+1)+" / "+expected.length());GameSources.removeOldCopy(root,expected.getJSONObject(i));}
            if(!GameSources.exe(context).isEmpty())GameSources.removeOldCopy(root,new JSONObject().put("path","Transformers.exe").put("bytes",BuildInfo.INPUT_BYTES).put("sha256",BuildInfo.INPUT_SHA256));
        }
        return "Game folder verified. Files are read directly from the selected location.";
    }
}
