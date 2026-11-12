#!/usr/bin/env bash
# Creates a self-signed code-signing identity "ScreenCam Development" in your login keychain.
#
# Why: macOS remembers Camera / Microphone / Screen Recording permission per *code signature*.
# An ad-hoc signature is a hash of the binary, so every rebuild looks like a brand-new app and
# the permission you granted is silently gone. Signing every build with one fixed certificate
# makes macOS recognise ScreenCam across rebuilds. CMake picks this identity up automatically.
#
# Usage: scripts/create-macos-signing-identity.sh   (run once; macOS asks for your password
#        to trust the certificate for code signing)
set -euo pipefail

NAME="ScreenCam Development"
KEYCHAIN="$HOME/Library/Keychains/login.keychain-db"
OPENSSL=/usr/bin/openssl  # macOS's LibreSSL: its .p12 format is the one `security import` reads

[[ "$(uname)" == "Darwin" ]] || { echo "macOS only"; exit 1; }

if security find-identity -v -p codesigning | grep -q "\"$NAME\""; then
    echo "'$NAME' already exists and is valid for code signing — nothing to do."
    exit 0
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# A certificate that may only be used for code signing, valid for 10 years.
cat > "$TMP/cert.cnf" <<CNF
[ req ]
distinguished_name = dn
x509_extensions = ext
prompt = no
[ dn ]
CN = $NAME
[ ext ]
basicConstraints = critical, CA:false
keyUsage = critical, digitalSignature
extendedKeyUsage = critical, codeSigning
CNF

echo "==> Creating the certificate"
"$OPENSSL" req -x509 -newkey rsa:2048 -nodes -days 3650 \
    -config "$TMP/cert.cnf" -keyout "$TMP/key.pem" -out "$TMP/cert.pem"
"$OPENSSL" pkcs12 -export -inkey "$TMP/key.pem" -in "$TMP/cert.pem" \
    -name "$NAME" -out "$TMP/identity.p12" -passout pass:screencam

echo "==> Importing it into your login keychain (codesign may use the key without asking)"
security import "$TMP/identity.p12" -k "$KEYCHAIN" -P screencam -T /usr/bin/codesign

echo "==> Trusting it for code signing (macOS will ask for your password)"
security add-trusted-cert -p codeSign -k "$KEYCHAIN" "$TMP/cert.pem"

echo "==> Checking"
security find-identity -v -p codesigning | grep "\"$NAME\"" ||
    { echo "The identity is not valid for code signing — see the messages above."; exit 1; }

cat <<MSG

Done. Next:
  1. Reconfigure + rebuild ScreenCam (rm -rf build, then your usual build) — CMake now signs
     with "$NAME".
  2. Clear the stale permission entries once:  tccutil reset ScreenCapture com.hsley13.screencam
  3. Open the app with:  open build/ScreenCam.app   and allow Screen Recording when asked,
     then quit (Cmd+Q) and reopen it.
MSG
