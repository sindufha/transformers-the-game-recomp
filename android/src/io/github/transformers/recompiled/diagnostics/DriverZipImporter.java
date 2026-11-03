// Adapted from the user-supplied DMC3 v028-original DriverZipImporter reference.
package io.github.transformers.recompiled.diagnostics;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Properties;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.zip.CRC32;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/** Imports a driver ZIP once; the unchanged native loader uses its extracted library. */
final class DriverZipImporter {
    private static final long MAX_BYTES = 268435456L;
    private static final int MAX_ENTRIES = 256;
    private static final Pattern LIBRARY_NAME = Pattern.compile("(?:^|[,{])\\s*\"libraryName\"\\s*:\\s*\"((?:[^\"\\\\]|\\\\.)*)\"");
    private final File library;
    private final DriverLibrary catalog;

    DriverZipImporter(File filesDir) {
        library = new File(filesDir, "drivers/library");
        catalog = new DriverLibrary(filesDir);
    }

    DriverLibrary.Driver add(InputStream input, String displayName) throws IOException {
        if (displayName == null || displayName.length()>240 || !displayName.toLowerCase(Locale.ROOT).endsWith(".zip")) {
            throw new IOException("Choose a GPU driver ZIP package.");
        }
        for(int i=0;i<displayName.length();i++)if(Character.isISOControl(displayName.charAt(i))||displayName.charAt(i)=='/'||displayName.charAt(i)=='\\')throw new IOException("Invalid driver ZIP name.");
        if (!library.isDirectory() && !library.mkdirs()) throw new IOException("Cannot create the driver library.");
        File work = Files.createTempDirectory(library.toPath(), "zip-import-").toFile();
        try {
            File archive = new File(work, "source.zip");
            String hash = copyArchive(input, archive);
            File destination = new File(library, hash);
            List<DriverLibrary.Driver> saved = catalog.list();
            String destinationPrefix = destination.getAbsolutePath() + File.separator;
            for (DriverLibrary.Driver driver : saved) {
                if (new File(destination, "source.zip").isFile() && driver.file.getAbsolutePath().startsWith(destinationPrefix)) return driver;
            }
            File payload = new File(work, "package");
            if (!payload.mkdir()) throw new IOException("Cannot prepare the driver package.");
            List<File> files = extract(archive, payload);
            File driver = selectLibrary(payload, files);
            String relative = "package/" + payload.toPath().relativize(driver.toPath()).toString().replace(File.separatorChar, '/');
            String originalName = displayName;
            String label = originalName;
            for (int suffix = 2; hasLabel(saved, label); suffix++) label = originalName + " (" + suffix + ")";
            Properties properties = new Properties();
            properties.setProperty("label", label);
            properties.setProperty("archive_name", originalName);
            properties.setProperty("library_file", relative);
            properties.setProperty("library_sha256", DriverLibrary.sha256(driver));
            try (FileOutputStream out = new FileOutputStream(new File(work, "entry.properties"))) {
                properties.store(out, "Transformers imported driver ZIP");
                out.getFD().sync();
            }
            if (destination.exists()) throw new IOException("This saved driver package is incomplete.");
            // Publish the archive, extraction and metadata together. A partial package is never listed.
            Files.move(work.toPath(), destination.toPath(), StandardCopyOption.ATOMIC_MOVE);
            return new DriverLibrary.Driver(label, inside(destination, relative));
        } finally {
            removeWork(work);
        }
    }

    private static String copyArchive(InputStream input, File archive) throws IOException {
        MessageDigest digest;
        try {
            digest = MessageDigest.getInstance("SHA-256");
        } catch (NoSuchAlgorithmException e) {
            throw new IOException(e);
        }
        try (FileOutputStream out = new FileOutputStream(archive)) {
            byte[] buffer = new byte[65536];
            long count = 0;
            int read;
            while ((read = input.read(buffer)) != -1) {
                DriverLibrary.interrupted();
                count += read;
                if (count > MAX_BYTES) throw new IOException("The driver ZIP is too large (maximum 256 MiB).");
                out.write(buffer, 0, read);
                digest.update(buffer, 0, read);
            }
            out.getFD().sync();
        }
        StringBuilder hash = new StringBuilder();
        for (byte value : digest.digest()) hash.append(String.format(Locale.ROOT, "%02x", value & 255));
        return hash.toString();
    }

    private static List<File> extract(File archive, File payload) throws IOException {
        List<File> files = new ArrayList<>();
        Set<String> names = new HashSet<>();
        long total = 0;
        int count = 0;
        try (ZipFile zip = new ZipFile(archive)) {
            Enumeration<? extends ZipEntry> entries = zip.entries();
            while (entries.hasMoreElements()) {
                ZipEntry entry = entries.nextElement();
                if (++count > MAX_ENTRIES) throw new IOException("The driver ZIP contains too many files.");
                File output = inside(payload, entry.getName());
                if (!names.add(output.getCanonicalPath())) throw new IOException("The driver ZIP contains duplicate paths.");
                if (entry.isDirectory()) {
                    if (!output.isDirectory() && !output.mkdirs()) throw new IOException("Cannot extract a driver folder.");
                    continue;
                }
                if (entry.getSize() > MAX_BYTES || (entry.getSize() >= 0 && entry.getSize() > MAX_BYTES - total)) {
                    throw new IOException("The extracted driver package is too large.");
                }
                File parent = output.getParentFile();
                if (!parent.isDirectory() && !parent.mkdirs()) throw new IOException("Cannot extract a driver folder.");
                CRC32 crc = new CRC32();
                long size = 0;
                try (InputStream in = zip.getInputStream(entry); FileOutputStream out = new FileOutputStream(output)) {
                    byte[] buffer = new byte[65536];
                    int read;
                    while ((read = in.read(buffer)) != -1) {
                        DriverLibrary.interrupted();
                        total += read;
                        size += read;
                        if (total > MAX_BYTES) throw new IOException("The extracted driver package is too large.");
                        out.write(buffer, 0, read);
                        crc.update(buffer, 0, read);
                    }
                    out.getFD().sync();
                }
                if ((entry.getSize() >= 0 && size != entry.getSize()) || (entry.getCrc() >= 0 && crc.getValue() != entry.getCrc())) {
                    throw new IOException("The driver ZIP is damaged. Import it again from a complete download.");
                }
                files.add(output);
            }
        }
        return files;
    }

    /** Resolve only ordinary relative paths, both for extraction and saved metadata. */
    static File inside(File parent, String name) throws IOException {
        if (name == null || name.isEmpty() || name.indexOf('\0') >= 0) throw new IOException("The driver package has an invalid path.");
        String path = name.replace('\\', '/');
        if (path.startsWith("/") || path.indexOf(':') >= 0) throw new IOException("The driver package contains an absolute path.");
        for (String part : path.split("/")) {
            if (part.equals(".") || part.equals("..")) throw new IOException("The driver package contains an unsafe path.");
        }
        File result = new File(parent, path).getCanonicalFile();
        if (!result.getPath().startsWith(parent.getCanonicalPath() + File.separator)) throw new IOException("The driver package path leaves its folder.");
        return result;
    }

    private static File selectLibrary(File payload, List<File> files) throws IOException {
        List<File> metadata = new ArrayList<>();
        for (File file : files) if (file.getName().equals("meta.json")) metadata.add(file);
        File meta = new File(payload, "meta.json");
        if (!meta.isFile()) meta = metadata.size() == 1 ? metadata.get(0) : null;
        if (meta != null) {
            if (meta.length() > 65536) throw new IOException("The driver package metadata is too large.");
            String json = new String(Files.readAllBytes(meta.toPath()), StandardCharsets.UTF_8);
            Matcher matcher = LIBRARY_NAME.matcher(json);
            if (matcher.find()) {
                String name = decodeJsonString(matcher.group(1));
                if (matcher.find()) throw new IOException("The driver package declares more than one libraryName.");
                File selected = inside(meta.getParentFile(), name);
                if (!selected.isFile()) throw new IOException("The driver library named in meta.json is missing.");
                DriverLibrary.validate(selected);
                return selected;
            }
        }
        List<File> candidates = new ArrayList<>();
        for (File file : files) {
            if (!file.getName().toLowerCase(Locale.ROOT).endsWith(".so")) continue;
            try {
                DriverLibrary.validate(file);
                candidates.add(file);
            } catch (IOException ignored) { }
        }
        if (candidates.isEmpty()) throw new IOException("This ZIP has no supported arm64 Android Vulkan driver.");
        if (candidates.size() != 1) throw new IOException("This ZIP has multiple drivers; meta.json must identify libraryName.");
        return candidates.get(0);
    }

    private static String decodeJsonString(String encoded) throws IOException {
        StringBuilder result = new StringBuilder();
        for (int i = 0; i < encoded.length(); i++) {
            char value = encoded.charAt(i);
            if (value == '\\') {
                if (++i >= encoded.length()) throw new IOException("Invalid libraryName in meta.json.");
                value = encoded.charAt(i);
                if (value == 'u') {
                    if (i + 4 >= encoded.length()) throw new IOException("Invalid libraryName in meta.json.");
                    try { value = (char) Integer.parseInt(encoded.substring(i + 1, i + 5), 16); }
                    catch (NumberFormatException e) { throw new IOException("Invalid libraryName in meta.json."); }
                    i += 4;
                } else if (value != '\\' && value != '/' && value != '"') {
                    throw new IOException("Invalid libraryName in meta.json.");
                }
            }
            if (Character.isISOControl(value)) throw new IOException("Invalid libraryName in meta.json.");
            result.append(value);
        }
        return result.toString();
    }

    private static boolean hasLabel(List<DriverLibrary.Driver> drivers, String label) {
        for (DriverLibrary.Driver driver : drivers) if (driver.label.equalsIgnoreCase(label)) return true;
        return false;
    }

    private void removeWork(File work) throws IOException {
        if (!work.exists()) return;
        String path = work.getCanonicalPath();
        if (!path.startsWith(library.getCanonicalPath() + File.separator) || !work.getName().startsWith("zip-import-")) {
            throw new IOException("Refusing to clean a path outside the driver import folder.");
        }
        removeTree(work);
    }

    private static void removeTree(File file) {
        File[] children = file.listFiles();
        if (children != null) for (File child : children) removeTree(child);
        file.delete();
    }
}
