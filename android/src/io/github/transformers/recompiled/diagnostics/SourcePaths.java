package io.github.transformers.recompiled.diagnostics;

import java.io.File;
import java.io.IOException;

/** Allows the final source link, but never follows a linked parent directory. */
final class SourcePaths {
    static File destination(File root,String relative) throws IOException {
        if(relative.isEmpty()||relative.indexOf('\\')>=0||relative.indexOf(':')>=0)
            throw new IOException("Invalid source path.");
        for(String part:relative.split("/",-1))
            if(part.isEmpty()||part.equals(".")||part.equals(".."))
                throw new IOException("Invalid source path.");
        File target=new File(root,relative);
        String base=root.getCanonicalPath(),parent=target.getParentFile().getCanonicalPath();
        if(!parent.equals(base)&&!parent.startsWith(base+File.separator))
            throw new IOException("The source folder leaves the application directory.");
        return target;
    }
}
