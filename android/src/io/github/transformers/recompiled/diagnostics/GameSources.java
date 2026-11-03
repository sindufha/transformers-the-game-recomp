package io.github.transformers.recompiled.diagnostics;

import android.content.Context;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.system.Os;
import android.system.OsConstants;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashSet;
import org.json.*;

/** Persisted SAF locations. Original assets are read directly; only the index is saved. */
final class GameSources {
    static File indexFile(Context c){return new File(c.getFilesDir(),"direct-sources.json");}
    static String exe(Context c){return c.getSharedPreferences("game_sources",0).getString("exe_uri","");}
    static byte[] bytes(InputStream in,int maximum)throws IOException{
        ByteArrayOutputStream out=new ByteArrayOutputStream();byte[] block=new byte[131072];int n;
        while((n=in.read(block))!=-1){VerifiedFiles.checkInterrupted();if(out.size()+n>maximum)throw new IOException("The source index is too large.");out.write(block,0,n);}
        return out.toByteArray();
    }
    static JSONObject manifest(Context c)throws Exception{
        try(InputStream in=c.getAssets().open("game-assets.json")){
            JSONObject m=new JSONObject(new String(bytes(in,4194304),StandardCharsets.UTF_8));
            if(!BuildInfo.INPUT_SHA256.equals(m.getString("input_sha256")))throw new IOException("The manifest does not match the EXE.");
            return m;
        }
    }
    static void verify(InputStream in,long expected,String hash,VerifiedFiles.Progress progress)throws IOException{
        java.security.MessageDigest digest=VerifiedFiles.sha256();byte[] block=new byte[131072];long total=0;int n;
        while((n=in.read(block))!=-1){VerifiedFiles.checkInterrupted();total+=n;if(total>expected)throw new IOException("File size mismatch.");digest.update(block,0,n);if(progress!=null)progress.bytes(total);}
        if(total!=expected||!hash.equals(VerifiedFiles.hex(digest.digest())))throw new IOException("Source file size or SHA-256 mismatch.");
    }
    static void selectExe(Context c,Uri uri)throws Exception{
        try(InputStream in=c.getContentResolver().openInputStream(uri)){
            if(in==null)throw new IOException("Cannot read the EXE.");
            verify(in,BuildInfo.INPUT_BYTES,BuildInfo.INPUT_SHA256,null);
        }
        if(!c.getSharedPreferences("game_sources",0).edit().putString("exe_uri",uri.toString()).commit())
            throw new IOException("Cannot save the EXE location.");
    }
    static JSONObject load(Context c)throws Exception{
        JSONObject index;
        try(InputStream in=new FileInputStream(indexFile(c))){index=new JSONObject(new String(bytes(in,4194304),StandardCharsets.UTF_8));}
        if(index.getInt("version")!=1||!BuildInfo.INPUT_SHA256.equals(index.getString("input_sha256")))throw new IOException("Select the game folder again.");
        JSONArray expected=manifest(c).getJSONArray("files"),actual=index.getJSONArray("files");
        if(expected.length()!=actual.length())throw new IOException("The source file list is incomplete.");
        HashSet<String> seen=new HashSet<>();
        for(int i=0;i<expected.length();i++){
            JSONObject a=actual.getJSONObject(i),e=expected.getJSONObject(i);
            if(!a.getString("path").equals(e.getString("path"))||a.getLong("bytes")!=e.getLong("bytes")
               ||!a.getString("sha256").equals(e.getString("sha256"))||!seen.add(a.getString("path")))throw new IOException("The source index does not match.");
            if(!"content".equals(Uri.parse(a.getString("uri")).getScheme()))throw new IOException("Invalid source URI.");
        }
        return index;
    }
    static void publish(Context c,JSONObject index)throws Exception{
        byte[] data=index.toString().getBytes(StandardCharsets.UTF_8);
        VerifiedFiles.copy(new ByteArrayInputStream(data),indexFile(c),data.length,VerifiedFiles.hex(VerifiedFiles.sha256().digest(data)),null);
    }
    static boolean hasExe(Context c){
        String value=exe(c);if(value.isEmpty())return false;
        try(ParcelFileDescriptor p=c.getContentResolver().openFileDescriptor(Uri.parse(value),"r")){
            return p!=null&&Os.fstat(p.getFileDescriptor()).st_size==BuildInfo.INPUT_BYTES;
        }catch(Exception error){return false;}
    }
    static int available(Context c){
        try{
            JSONObject index=load(c);
            try(android.database.Cursor cursor=c.getContentResolver().query(android.provider.DocumentsContract.buildDocumentUriUsingTree(Uri.parse(index.getString("tree")),android.provider.DocumentsContract.getTreeDocumentId(Uri.parse(index.getString("tree")))),new String[]{"document_id"},null,null,null)){
                if(cursor==null||!cursor.moveToFirst())return 0;
            }
            return index.getJSONArray("files").length();
        }catch(Exception error){return 0;}
    }
    static boolean regular(File file){
        try{return OsConstants.S_ISREG(Os.lstat(file.getAbsolutePath()).st_mode);}catch(Exception error){return false;}
    }
    static void removeOldCopy(File root,JSONObject item)throws Exception{
        File file=SourcePaths.destination(root,item.getString("path"));
        if(regular(file)&&VerifiedFiles.matches(file,item.getLong("bytes"),item.getString("sha256"),null)){
            if(!file.delete())throw new IOException("Cannot remove the old data copy: "+item.getString("path"));
        }
    }
    /** Owns all descriptors until the native session returns, across Activity recreation. */
    static final class Session implements AutoCloseable {
        private final ArrayList<ParcelFileDescriptor> descriptors=new ArrayList<>();
        private final ArrayList<File> links=new ArrayList<>();
        static Session mount(Context c,File root)throws Exception{
            Session s=new Session();
            try{
                JSONObject index=load(c);JSONArray files=index.getJSONArray("files");
                String exe=exe(c);if(exe.isEmpty())throw new IOException("Select an EXE first.");
                s.add(c,root,"Transformers.exe",Uri.parse(exe),BuildInfo.INPUT_BYTES);
                for(int i=0;i<files.length();i++){
                    JSONObject item=files.getJSONObject(i);
                    s.add(c,root,item.getString("path"),Uri.parse(item.getString("uri")),item.getLong("bytes"));
                }
                return s;
            }catch(Exception error){s.close();throw error;}
        }
        private void add(Context c,File root,String name,Uri uri,long size)throws Exception{
            ParcelFileDescriptor fd=c.getContentResolver().openFileDescriptor(uri,"r");
            if(fd==null)throw new IOException("Cannot open source: "+name);
            descriptors.add(fd);
            if(!OsConstants.S_ISREG(Os.fstat(fd.getFileDescriptor()).st_mode)||Os.fstat(fd.getFileDescriptor()).st_size!=size)
                throw new IOException("The source must be a local file with the expected size: "+name);
            Os.lseek(fd.getFileDescriptor(),0,OsConstants.SEEK_SET);
            File target=SourcePaths.destination(root,name),parent=target.getParentFile();
            if(!parent.isDirectory()&&!parent.mkdirs())throw new IOException("Cannot create the game access folder.");
            if(regular(target)){
                String expected=name.equals("Transformers.exe")?BuildInfo.INPUT_SHA256:null;
                if(expected==null){JSONArray list=manifest(c).getJSONArray("files");for(int i=0;i<list.length();i++){JSONObject v=list.getJSONObject(i);if(name.equals(v.getString("path"))){expected=v.getString("sha256");break;}}}
                if(expected==null||!VerifiedFiles.matches(target,size,expected,null))throw new IOException("The local copy has been modified: "+name);
            }
            try{Os.remove(target.getAbsolutePath());}catch(android.system.ErrnoException error){if(error.errno!=OsConstants.ENOENT)throw error;}
            Os.symlink("/proc/self/fd/"+fd.getFd(),target.getAbsolutePath());links.add(target);
            // This link is a native lookup token only. Reopening a provider FD by
            // pathname can lose its Android grant; native code duplicates the FD
            // and uses positional reads with an independent offset per handle.
        }
        public void close(){
            for(File link:links)try{Os.remove(link.getAbsolutePath());}catch(Exception ignored){}
            links.clear();for(ParcelFileDescriptor fd:descriptors)try{fd.close();}catch(IOException ignored){}descriptors.clear();
        }
    }
}
