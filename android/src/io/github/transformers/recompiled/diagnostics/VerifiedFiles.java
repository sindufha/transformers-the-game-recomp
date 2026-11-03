package io.github.transformers.recompiled.diagnostics;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InterruptedIOException;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

/** Streaming verification shared by EXE/data import. No Android dependencies. */
final class VerifiedFiles {
    interface Progress { void bytes(long count) throws IOException; }

    static File destination(File root, String relative) throws IOException {
        if (relative.length() == 0 || relative.indexOf('\\') >= 0 || relative.indexOf(':') >= 0)
            throw new IOException("Invalid filename.");
        for (String part : relative.split("/", -1)) {
            if (part.length() == 0 || part.equals(".") || part.equals(".."))
                throw new IOException("Invalid file path.");
        }
        File result = new File(root, relative);
        if (!result.getCanonicalPath().startsWith(root.getCanonicalPath() + File.separator))
            throw new IOException("The file path leaves the game folder.");
        return result;
    }

    static MessageDigest sha256() {
        try { return MessageDigest.getInstance("SHA-256"); }
        catch (NoSuchAlgorithmException error) { throw new AssertionError(error); }
    }

    static String hex(byte[] data) {
        char[] out = new char[data.length * 2];
        final char[] alphabet = "0123456789abcdef".toCharArray();
        for (int i = 0; i < data.length; ++i) {
            out[i * 2] = alphabet[(data[i] & 255) >>> 4]; out[i * 2 + 1] = alphabet[data[i] & 15];
        }
        return new String(out);
    }

    static void checkInterrupted() throws InterruptedIOException {
        if (Thread.currentThread().isInterrupted()) throw new InterruptedIOException("Import canceled.");
    }

    static boolean matches(File file, long expectedBytes, String expectedHash, Progress progress) throws IOException {
        if (!file.isFile() || file.length() != expectedBytes) return false;
        MessageDigest digest = sha256(); long total = 0;
        try (InputStream input = new FileInputStream(file)) {
            byte[] block = new byte[131072]; int count;
            while ((count = input.read(block)) != -1) {
                checkInterrupted(); total += count; digest.update(block, 0, count);
                if (progress != null) progress.bytes(total);
            }
        }
        return total == expectedBytes && expectedHash.equals(hex(digest.digest()));
    }

    /** Retain the previous valid file until the whole replacement verifies. */
    static void copy(InputStream input, File target, long expectedBytes, String expectedHash,
                     Progress progress) throws IOException {
        File parent = target.getParentFile();
        if (!parent.isDirectory() && !parent.mkdirs()) throw new IOException("Cannot create the destination folder.");
        File temporary = new File(parent, target.getName() + ".import-pending");
        try {
            MessageDigest digest = sha256(); long total = 0;
            try (FileOutputStream output = new FileOutputStream(temporary)) {
                byte[] block = new byte[131072]; int count;
                while ((count = input.read(block)) != -1) {
                    checkInterrupted(); total += count;
                    if (total > expectedBytes) throw new IOException("File size mismatch: " + target.getName());
                    digest.update(block, 0, count); output.write(block, 0, count);
                    if (progress != null) progress.bytes(total);
                }
                if (total != expectedBytes || !expectedHash.equals(hex(digest.digest())))
                    throw new IOException("SHA-256 or file size mismatch: " + target.getName());
                checkInterrupted(); output.getFD().sync();
            }
            // A pending file lives on the same filesystem. Atomic publication
            // prevents an interrupted copy from replacing a valid asset.
            Files.move(temporary.toPath(), target.toPath(), StandardCopyOption.ATOMIC_MOVE,
                StandardCopyOption.REPLACE_EXISTING);
        } finally { temporary.delete(); }
    }
}
