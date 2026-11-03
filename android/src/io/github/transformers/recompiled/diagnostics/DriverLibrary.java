package io.github.transformers.recompiled.diagnostics;

import java.io.*;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.util.*;

/** Based on the user-supplied DMC3 DriverLibrary reference. ZIP packages are
 * published atomically; importing does not load or execute their libraries. */
final class DriverLibrary {
    private static final long MAX_BYTES=268435456L;
    private final File library;
    static final class Driver {
        final String label;final File file;
        Driver(String label,File file){this.label=label;this.file=file;}
    }
    DriverLibrary(File filesDir){library=new File(filesDir,"drivers/library");}
    List<Driver> list(){
        List<Driver> result=new ArrayList<>();File[] entries=library.listFiles();
        if(entries!=null)for(File entry:entries){
            if(!entry.isDirectory()||!entry.getName().matches("[0-9a-f]{64}"))continue;
            File metadata=new File(entry,"entry.properties");if(!metadata.isFile()||metadata.length()>65536)continue;
            try(InputStream in=new FileInputStream(metadata)){
                Properties p=new Properties();p.load(in);
                File file=DriverZipImporter.inside(entry,p.getProperty("library_file",""));
                String label=p.getProperty("label","");
                if(file.isFile()&&!label.isEmpty()&&p.getProperty("library_sha256","").matches("[0-9a-f]{64}"))result.add(new Driver(label,file));
            }catch(IOException|IllegalArgumentException ignored){}
        }
        Collections.sort(result,new Comparator<Driver>(){public int compare(Driver a,Driver b){int n=a.label.compareToIgnoreCase(b.label);return n!=0?n:a.file.getPath().compareTo(b.file.getPath());}});
        return result;
    }
    Driver require(String path)throws IOException{
        if(path==null||path.isEmpty())throw new IOException("Select a GPU driver first.");
        File selected=new File(path).getCanonicalFile();
        for(Driver d:list())if(d.file.getCanonicalFile().equals(selected)){
            validate(selected);
            File entry=selected;
            while(entry.getParentFile()!=null&&!entry.getParentFile().getCanonicalFile().equals(library.getCanonicalFile()))entry=entry.getParentFile();
            Properties p=new Properties();try(InputStream in=new FileInputStream(new File(entry,"entry.properties"))){p.load(in);}
            if(!sha256(selected).equals(p.getProperty("library_sha256")))throw new IOException("The GPU driver has changed or is damaged. Add the original ZIP again.");
            return d;
        }
        throw new IOException("The selected GPU driver is unavailable. Select another driver.");
    }
    static String sha256(File file)throws IOException{
        try{
            MessageDigest digest=MessageDigest.getInstance("SHA-256");
            try(InputStream in=new FileInputStream(file)){byte[] block=new byte[65536];int n;while((n=in.read(block))!=-1){interrupted();digest.update(block,0,n);}}
            StringBuilder hex=new StringBuilder();for(byte b:digest.digest())hex.append(String.format(Locale.ROOT,"%02x",b&255));return hex.toString();
        }catch(java.security.NoSuchAlgorithmException e){throw new IOException(e);}
    }
    static void interrupted()throws InterruptedIOException{if(Thread.currentThread().isInterrupted())throw new InterruptedIOException("Driver verification canceled.");}
    static void validate(File file)throws IOException{
        if(file.length()<4096||file.length()>MAX_BYTES)throw new IOException("That file is not an arm64 Vulkan driver.");
        try(InputStream in=new FileInputStream(file)){
            byte[] h=new byte[64];int n=0;while(n<h.length){int k=in.read(h,n,h.length-n);if(k<0)throw new IOException("Incomplete GPU driver.");n+=k;}
            if(h[0]!=127||h[1]!='E'||h[2]!='L'||h[3]!='F'||h[4]!=2||h[5]!=1||h[6]!=1||h[16]!=3||h[17]!=0||(h[18]&255)!=183||h[19]!=0)
                throw new IOException("Choose a 64-bit arm64 Android Vulkan shared library.");
        }
        try(InputStream in=new FileInputStream(file)){
            byte[] block=new byte[65536];int keep=0,n;
            while((n=in.read(block,keep,block.length-keep))!=-1){
                interrupted();int length=keep+n;
                for(int i=3;i<length;i++)if(block[i-3]=='H'&&block[i-2]=='M'&&block[i-1]=='I'&&block[i]==0)return;
                keep=Math.min(3,length);System.arraycopy(block,length-keep,block,0,keep);
            }
        }
        throw new IOException("That library is not an Android Vulkan driver (no HMI).");
    }
}
