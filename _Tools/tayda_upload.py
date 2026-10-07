#!/usr/bin/env python3
"""Put the PG-1 drill template + UV print artwork into YOUR drill.taydakits.com account.

    python3 tayda_upload.py            # asks for your Tayda email + password, then uploads
    python3 tayda_upload.py --dry-run  # only shows what would be sent
    python3 tayda_upload.py --drill-id 12345 --uv-id 12345   # UPDATE existing templates instead of creating new ones
    python3 tayda_upload.py --drill-id 12345                 # update only the drill template

It creates two templates (it does NOT order or pay for anything):
  * drill template "pg-1"      - 15 holes + 2 rectangles (screen window, Seed3 window) (from tayda-drill-holes.csv)
  * UV print template "pg-1 top" - side A, colour layer only (pg1-face-uv-print.pdf)
Then you open the dashboard, check the previews, and create the job/order yourself.

Login is remembered after the first successful run: the password goes into your desktop keyring
(KDE Wallet / GNOME Keyring via `secret-tool`), the email into ~/.config/tayda-upload/email.
If no keyring is available it falls back to ~/.config/tayda-upload/login.json (readable only by you).
    python3 tayda_upload.py --forget   # delete the saved login
This uses the same web API the drill.taydakits.com page uses; if Tayda changes it, enter the
values by hand from 03-Drill-Template/DRILL-ORDER.md instead.
"""
import base64, csv, getpass, json, os, subprocess, sys, urllib.error, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
PEDAL = os.path.join(os.path.dirname(HERE), "PG-Pedal-1")
HOLES_CSV = os.path.join(PEDAL, "03-Drill-Template", "tayda-drill-holes.csv")
UV_PDF = os.path.join(PEDAL, "04-Top-Artwork", "pg1-face-uv-print.pdf")

LOGIN_URL = "https://fo.tayda.com/api/designer/auth/login"
CFG_DIR = os.path.expanduser("~/.config/tayda-upload")
EMAIL_FILE = os.path.join(CFG_DIR, "email")
FALLBACK_FILE = os.path.join(CFG_DIR, "login.json")
KEY_ATTRS = ["service", "tayda-drill"]


def _secret_tool(*args, stdin=None):
    try:
        r = subprocess.run(["secret-tool", *args], input=stdin, capture_output=True, text=True, timeout=20)
        return r.returncode == 0, r.stdout
    except (OSError, subprocess.TimeoutExpired):
        return False, ""


def load_login():
    if os.path.exists(EMAIL_FILE):
        email = open(EMAIL_FILE).read().strip()
        ok, pw = _secret_tool("lookup", *KEY_ATTRS, "user", email)
        if ok and pw:
            return email, pw
    if os.path.exists(FALLBACK_FILE):
        d = json.load(open(FALLBACK_FILE))
        return d.get("email"), d.get("password")
    return None, None


def save_login(email, password):
    os.makedirs(CFG_DIR, mode=0o700, exist_ok=True)
    with open(os.open(EMAIL_FILE, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600), "w") as fh:
        fh.write(email)
    ok, _ = _secret_tool("store", "--label=Tayda drill uploader", *KEY_ATTRS, "user", email, stdin=password)
    if ok:
        print("Login saved in your keyring (KDE Wallet / GNOME Keyring).")
        return
    with open(os.open(FALLBACK_FILE, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600), "w") as fh:
        json.dump({"email": email, "password": password}, fh)
    print(f"No keyring available - login saved to {FALLBACK_FILE} (only your user can read it).")


def forget_login():
    if os.path.exists(EMAIL_FILE):
        email = open(EMAIL_FILE).read().strip()
        _secret_tool("clear", *KEY_ATTRS, "user", email)
        os.remove(EMAIL_FILE)
    if os.path.exists(FALLBACK_FILE):
        os.remove(FALLBACK_FILE)
    print("Saved login removed.")
API = "https://drill.taydakits.com/api/v4"
UA = "Mozilla/5.0 (X11; Linux x86_64) pg-pedal-uploader"


def request(url, body=None, token=None, method=None):
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(url, data=data, method=method or ("POST" if data else "GET"))
    req.add_header("Content-Type", "application/json")
    req.add_header("Accept", "application/json")
    req.add_header("User-Agent", UA)
    req.add_header("Origin", "https://drill.taydakits.com")
    req.add_header("Referer", "https://drill.taydakits.com/")
    if token:
        req.add_header("Authorization", "Bearer " + token)
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            return r.status, json.loads(r.read() or b"{}")
    except urllib.error.HTTPError as e:
        try:
            return e.code, json.loads(e.read() or b"{}")
        except ValueError:
            return e.code, {"message": e.reason}


def drill_payload():
    holes, shapes = [], []
    with open(HOLES_CSV) as fh:
        for r in csv.DictReader(fh):
            if r["type"] == "hole":
                holes.append({"box_side": r["side"], "diameter": r["diameter_mm"],
                              "positionX": r["x_mm"], "positionY": r["y_mm"]})
            else:
                shapes.append({"box_side": r["side"], "shape_type": "Rectangle",
                               "positionX": r["x_mm"], "positionY": r["y_mm"],
                               "width": r["width_mm"], "height": r["height_mm"]})
    return {"name": "pg-1", "enclosure_type": "1590XX", "is_public": 0, "is_archived": 0,
            "holes": holes, "lines": [], "shapes": shapes}


def uv_payload():
    pdf = base64.b64encode(open(UV_PDF, "rb").read()).decode()
    return {"name": "pg-1 top", "enclosure_type": "1590XX", "box_side": "A",
            "color_layer": "Yes", "white_layer": "No", "rdg_white_layer": "No",
            "gloss_layer": "No", "rdg_gloss_layer": "No", "gloss_type": "",
            "upload_file": "data:application/pdf;base64," + pdf, "is_archived": 0}


def arg(name):
    if name in sys.argv:
        i = sys.argv.index(name)
        if i + 1 < len(sys.argv) and sys.argv[i + 1].isdigit():
            return sys.argv[i + 1]
        sys.exit(f"{name} needs a number, e.g. {name} 12345")
    return None


def main():
    drill, uv = drill_payload(), uv_payload()
    drill_id, uv_id = arg("--drill-id"), arg("--uv-id")
    if "--dry-run" in sys.argv:
        print(json.dumps(drill, indent=1))
        print("UV:", {k: (v[:60] + "...") if k == "upload_file" else v for k, v in uv.items()})
        print(f"\n{len(drill['holes'])} holes, {len(drill['shapes'])} rectangle, PDF {len(uv['upload_file'])//1024} KB (base64)")
        return

    if "--forget" in sys.argv:
        forget_login()
        return
    email, password = load_login()
    saved = bool(email and password)
    if saved:
        print(f"Using saved login for {email}")
    else:
        email = input("Tayda / drill.taydakits.com email: ").strip()
        password = getpass.getpass("Password (not shown): ")
    code, res = request(LOGIN_URL, {"email": email, "password": password})
    token = res.get("access_token")
    if not token:
        if saved:
            print("Saved login was rejected (password changed?). Run with --forget, then again.")
        sys.exit(f"Login failed ({code}): {res.get('message') or res.get('error') or res}")
    print("Logged in.")
    if not saved:
        if input("Remember this login for next time? [Y/n] ").strip().lower() in ("", "y", "yes"):
            save_login(email, password)
    password = None

    code, res = request(API + "/box_designs" + (f"/{drill_id}" if drill_id else ""), drill, token,
                        "PUT" if drill_id else "POST")
    if "box_design" not in res:
        sys.exit(f"Drill template not saved ({code}): {res.get('message') or res}\n"
                 "Tip: if the name 'pg-1' already exists, rename/delete it on the dashboard and run again.")
    did = res["box_design"]["id"]
    print(f"Drill template saved: https://drill.taydakits.com/box-designs/edit?id={did}")

    if drill_id and not uv_id:
        print("UV print left as is (pass --uv-id N to update it too).")
        return
    code, res = request(API + "/box_uv_designs" + (f"/{uv_id}" if uv_id else ""), uv, token,
                        "PUT" if uv_id else "POST")
    if "box_uv_design" not in res:
        sys.exit(f"UV template not saved ({code}): {res.get('message') or res}\n"
                 "Upload 04-Top-Artwork/pg1-face-uv-print.pdf by hand (see UV-PRINT.md).")
    uid = res["box_uv_design"]["id"]
    print(f"UV print template saved: https://drill.taydakits.com/box-uv-designs/edit?id={uid}")
    print("\nNext: open https://drill.taydakits.com/dashboard, check both previews "
          "(the 'in' hole must be above the LEFT half of the face), then create the job/order.")


if __name__ == "__main__":
    main()
