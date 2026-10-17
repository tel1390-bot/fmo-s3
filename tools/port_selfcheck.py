import re
import sys

def check(path):
    s = open(path, encoding="utf-8").read()
    s2 = re.sub(r"/\*.*?\*/", "", s, flags=re.S)
    s2 = re.sub(r"//.*", "", s2)
    s2 = re.sub(r'"(?:\\.|[^"\\])*"', '""', s2)
    ok = True
    for a, b, name in [("{", "}", "brace"), ("(", ")", "paren"), ("[", "]", "bracket")]:
        n1, n2 = s2.count(a), s2.count(b)
        state = "OK" if n1 == n2 else "MISMATCH"
        if n1 != n2:
            ok = False
        print(f"{name}: {n1}/{n2} {state}")
    return ok

def scan_c3(root):
    import pathlib
    hits = []
    for p in pathlib.Path(root).rglob("*"):
        if not p.is_file():
            continue
        fp = str(p).replace("\\", "/")
        if "/src/" in fp or "/.git/" in fp or "dependencies.lock" in fp or "/build/" in fp:
            continue
        try:
            text = p.read_text(encoding="utf-8", errors="ignore")
        except Exception:
            continue
        for pat in ("esp32c3", "esp32-c3", "esp_codec_dev", "es8311"):
            if pat in text:
                for line in text.splitlines():
                    if pat in line:
                        hits.append(f"{p}: {line.strip()[:100]}")
                        break
    return hits

if __name__ == "__main__":
    ok = check(r"G:\esp\fmo\fmo-s3\main\fmo_ui.c")
    print("---- C3/ES8311 residual scan ----")
    hits = scan_c3(r"G:\esp\fmo\fmo-s3")
    if hits:
        for h in hits[:25]:
            print(h)
    else:
        print("none")
    sys.exit(0 if ok else 1)
