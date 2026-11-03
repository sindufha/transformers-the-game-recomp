"""Android 10 native ABI targets; a universal package contains each ABI."""

TARGETS = {
    "arm64-v8a": ("aarch64-linux-android", 2, 183),
    "armeabi-v7a": ("armv7a-linux-androideabi", 1, 40),
    "x86": ("i686-linux-android", 1, 3),
    "x86_64": ("x86_64-linux-android", 2, 62),
}


def target_triple(abi: str, api: int) -> str:
    if api < 29:
        raise ValueError("This project targets Android 10 / API 29 or newer")
    return TARGETS[abi][0] + str(api)


def validate_elf(data: bytes, abi: str) -> None:
    _, elf_class, machine = TARGETS[abi]
    if data[:4] != b"\x7fELF" or data[4:6] != bytes((elf_class, 1)) or int.from_bytes(data[18:20], "little") != machine:
        raise ValueError(f"Output is not the expected little-endian ELF for {abi}")
