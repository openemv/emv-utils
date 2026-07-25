#!/bin/bash

# This script is inspired by these guides:
# - https://docs.github.com/en/actions/deployment/deploying-xcode-applications/installing-an-apple-certificate-on-macos-runners-for-xcode-development
# - https://localazy.com/blog/how-to-automatically-sign-macos-apps-using-github-actions
# - https://federicoterzi.com/blog/automatic-code-signing-and-notarization-for-macos-apps-using-github-actions/
# - https://github.com/lando/code-sign-action/

# This script assumes that these environment variables are provided:
# - RUNNER_TEMP (temporary directory provided by Github Actions runner)
# - OPENEMV_MACOS_CERT_BASE64
# - OPENEMV_MACOS_CERT_PWD
# - KEYCHAIN_PASSWORD

# Temporary paths
KEYCHAIN_FILE=$RUNNER_TEMP/app-signing.keychain-db
OPENEMV_MACOS_CERT_P12=$RUNNER_TEMP/certificate.p12
OPENEMV_MACOS_CERT_PEM=$RUNNER_TEMP/certificate.pem

# Create temporary keychain
security create-keychain -p "$KEYCHAIN_PASSWORD" $KEYCHAIN_FILE
security set-keychain-settings -lut 21600 $KEYCHAIN_FILE
security unlock-keychain -p "$KEYCHAIN_PASSWORD" $KEYCHAIN_FILE

# Decode and import signing certificate
echo -n "$OPENEMV_MACOS_CERT_BASE64" | base64 --decode > $OPENEMV_MACOS_CERT_P12
security import $OPENEMV_MACOS_CERT_P12 -P "$OPENEMV_MACOS_CERT_PWD" -A -t cert -f pkcs12 -k $KEYCHAIN_FILE
security list-keychains -d user -s $KEYCHAIN_FILE

# Trust signing certificate
security find-certificate -p -c openemv.org $KEYCHAIN_FILE > $OPENEMV_MACOS_CERT_PEM
sudo security add-trusted-cert -d -r trustRoot $OPENEMV_MACOS_CERT_PEM

# Allow codesign application to use signing key
security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$KEYCHAIN_PASSWORD" $KEYCHAIN_FILE

# Cleanup
rm $OPENEMV_MACOS_CERT_P12 $OPENEMV_MACOS_CERT_PEM

# Verify that codesigning identity is ready
if ! security find-identity -v -p codesigning $KEYCHAIN_FILE \
        | grep -q "openemv.org"; then
    echo "ERROR: openemv.org codesigning identity not present" >&2
    exit 1
fi
