package io.github.transformers.recompiled.diagnostics;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.nio.file.AtomicMoveNotSupportedException;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Edits the original game's configuration before its process starts. */
public final class GameSettings {
    public static final String CONFIG_RELATIVE_PATH = "documents/ACTIVISION/Transformers/pcconfig.txt";
    private static final int MAX_CONFIG_BYTES = 1024 * 1024;
    public final boolean highResTextures;
    public final boolean bloom, shadows, motionBlur;
    public final int width, height;

    private GameSettings(boolean highResTextures, int width, int height,
                         boolean bloom, boolean shadows, boolean motionBlur) {
        this.highResTextures = highResTextures;
        this.width = width;
        this.height = height;
        this.bloom = bloom;
        this.shadows = shadows;
        this.motionBlur = motionBlur;
    }

    public static GameSettings load(File gameRoot) throws IOException {
        String text = read(configFile(gameRoot));
        // The PC game applies the pending texture value when loading pcconfig.
        int active = value(text, "TransformersUseHires", 1);
        return new GameSettings(value(text, "TransformersUseHiresPending", active) > 0,
                positive(value(text, "ScreenWidth", 1280), 1280),
                positive(value(text, "ScreenHeight", 720), 720),
                value(text, "BloomEnabled", 1) > 0,
                value(text, "ShadowsEnabled", 1) > 0,
                value(text, "SpeedBlurEnabled", 1) > 0);
    }

    /**
     * Sets requested PC-game dimensions, which remain subject to its mode filters.
     * The launcher writes a supported 1280x720 base before every run, then applies
     * the user's 50/67/100 render scale through the existing native bridge.
     */
    public static void applyOptions(File gameRoot, boolean highRes, int width, int height) throws IOException {
        saveOptions(gameRoot, highRes, width, height, null, null, null);
    }

    public static void applyOptions(File gameRoot, boolean highRes, int width, int height,
                                    boolean bloom, boolean shadows, boolean motionBlur) throws IOException {
        saveOptions(gameRoot, highRes, width, height, bloom, shadows, motionBlur);
    }

    /** Restores an accepted PC mode so persisted reduced dimensions never get scaled twice. */
    public static int prepareLaunch(File gameRoot, boolean highRes, int renderScale,
                                    boolean bloom, boolean shadows, boolean motionBlur) throws IOException {
        if (renderScale != 50 && renderScale != 67 && renderScale != 100)
            throw new IllegalArgumentException("Unsupported render scale");
        applyOptions(gameRoot, highRes, 1280, 720, bloom, shadows, motionBlur);
        return renderScale;
    }

    private static void saveOptions(File gameRoot, boolean highRes, int width, int height,
                                    Boolean bloom, Boolean shadows, Boolean motionBlur) throws IOException {
        if (width < 2 || height < 2 || width > 16384 || height > 16384)
            throw new IllegalArgumentException("Resolution must be between 2 and 16384 pixels");
        File target = configFile(gameRoot);
        boolean existed = target.isFile();
        String original = read(target);
        String text = original;
        if (!existed) text = "FileVersion                    1\r\nFSAA                           0\r\n";
        text = set(text, "ScreenWidth", width);
        text = set(text, "ScreenHeight", height);
        text = set(text, "TransformersUseHires", highRes ? 1 : 0);
        text = set(text, "TransformersUseHiresPending", highRes ? 1 : 0);
        if (bloom != null) text = set(text, "BloomEnabled", bloom ? 1 : 0);
        if (shadows != null) text = set(text, "ShadowsEnabled", shadows ? 1 : 0);
        if (motionBlur != null) text = set(text, "SpeedBlurEnabled", motionBlur ? 1 : 0);
        if (text.equals(original)) return;
        File parent = target.getParentFile();
        if (!parent.isDirectory() && !parent.mkdirs()) throw new IOException("Cannot create game settings directory");
        File temporary = File.createTempFile("pcconfig-", ".tmp", parent);
        try {
            try (FileOutputStream output = new FileOutputStream(temporary)) {
                output.write(text.getBytes(StandardCharsets.ISO_8859_1));
                output.getFD().sync();
            }
            try {
                Files.move(temporary.toPath(), target.toPath(), StandardCopyOption.ATOMIC_MOVE,
                        StandardCopyOption.REPLACE_EXISTING);
            } catch (AtomicMoveNotSupportedException unsupported) {
                Files.move(temporary.toPath(), target.toPath(), StandardCopyOption.REPLACE_EXISTING);
            }
        } finally {
            if (temporary.exists()) temporary.delete();
        }
    }

    private static File configFile(File root) throws IOException {
        File canonicalRoot = root.getCanonicalFile();
        File config = new File(canonicalRoot, CONFIG_RELATIVE_PATH).getCanonicalFile();
        if (!config.getPath().startsWith(canonicalRoot.getPath() + File.separator))
            throw new IOException("Game settings path leaves imported game directory");
        return config;
    }

    private static String read(File file) throws IOException {
        if (!file.exists()) return "";
        if (!file.isFile() || file.length() > MAX_CONFIG_BYTES) throw new IOException("Invalid game settings file");
        byte[] bytes = new byte[(int)file.length()];
        try (FileInputStream input = new FileInputStream(file)) {
            int offset = 0;
            while (offset < bytes.length) {
                int count = input.read(bytes, offset, bytes.length - offset);
                if (count < 0) throw new IOException("Game settings changed while reading");
                offset += count;
            }
            if (input.read() != -1) throw new IOException("Game settings changed while reading");
        }
        for (byte b : bytes) if (b == 0) throw new IOException("Unsupported game settings encoding");
        // Byte-preserving conversion keeps comments, unknown keys and line endings intact.
        return new String(bytes, StandardCharsets.ISO_8859_1);
    }

    private static Pattern entry(String key) {
        return Pattern.compile("(?m)^([ \\t]*" + Pattern.quote(key) + "[ \\t]+)([^ \\t\\r\\n]+)");
    }

    private static int value(String text, String key, int fallback) {
        Matcher matcher = entry(key).matcher(text);
        int result = fallback;
        while (matcher.find()) {
            try { result = Integer.parseInt(matcher.group(2)); }
            catch (NumberFormatException invalid) { result = fallback; }
        }
        return result;
    }

    private static int positive(int value, int fallback) { return value > 0 ? value : fallback; }

    private static String set(String text, String key, int value) {
        Matcher matcher = entry(key).matcher(text);
        StringBuffer output = new StringBuffer();
        boolean found = false;
        while (matcher.find()) {
            found = true;
            matcher.appendReplacement(output, Matcher.quoteReplacement(matcher.group(1) + value));
        }
        matcher.appendTail(output);
        if (found) return output.toString();
        String newline = text.contains("\r\n") ? "\r\n" : text.contains("\n") ? "\n" : "\r\n";
        if (!text.isEmpty() && !text.endsWith("\n") && !text.endsWith("\r")) text += newline;
        return text + String.format(java.util.Locale.ROOT, "%-30s %d", key, value) + newline;
    }
}
