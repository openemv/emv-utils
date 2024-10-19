#!/bin/bash

# This script implements the cleanup recommended by:
# https://docs.github.com/en/actions/deployment/deploying-xcode-applications/installing-an-apple-certificate-on-macos-runners-for-xcode-development

# Remove temporary keychain
security delete-keychain $RUNNER_TEMP/app-signing.keychain-db

