# Guide to Creating a Self-Signed Certificate for Testing

All MSIX packages must be digitally signed. For local development and testing, you can create your own **self-signed certificate** for free. This guide walks you through the process using PowerShell on a Windows machine.

> [!WARNING]
> A self-signed certificate is **not trusted** by other computers. You cannot use it for public distribution (e.g., on the Windows Store or winget). For that, you must purchase a certificate from a trusted Certificate Authority (CA).

## Step 1: Create and Export the Certificate

The following PowerShell script will create the self-signed certificate and immediately export it to a password-protected `.pfx` file on your Desktop. This is more reliable than creating and exporting in two separate steps.

The `Subject` name used here is critical: it **must** exactly match the `Publisher` attribute in your `AppxManifest.xml` file.

For this project, the publisher is:
`"CN=Cosmic DNA, O=Cosmic DNA, L=London, C=GB"`

1.  Open PowerShell as an Administrator.
2.  Run the following script. You will be prompted to enter a secure password for the `.pfx` file.

    ```powershell
    # Define the subject for the certificate. This must match the Publisher in AppxManifest.xml
    $subject = "CN=Cosmic DNA, O=Cosmic DNA, L=London, C=GB"

    # Define the output path for the certificate file (your Desktop).
    $pfxPath = "$HOME\Desktop\CosmicDNA-TestCert.pfx"

    # Create the certificate in the user's personal store and capture the output object.
    $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject $subject -CertStoreLocation "Cert:\CurrentUser\My" -KeyAlgorithm RSA -KeyLength 2048 -HashAlgorithm SHA256 -NotAfter (Get-Date).AddYears(5)

    # Check if the certificate was created successfully before proceeding.
    if ($cert) {
        # Prompt for a password and export the newly created certificate to the defined path.
        $pfxPassword = Read-Host -AsSecureString -Prompt "Enter a password to protect the PFX file"
        Export-PfxCertificate -Cert $cert -FilePath $pfxPath -Password $pfxPassword
        Write-Host "`nSuccessfully created and exported certificate to '$pfxPath'."
    } else {
        Write-Error "Failed to create the self-signed certificate. Please ensure you are running PowerShell as an Administrator."
    }
    ```

3.  A `CosmicDNA-TestCert.pfx` file will be created on your Desktop. **Keep this file and its password secure.** You will need it to sign your MSIX package.

## Step 2: Install the Certificate for Testing

Before you can install an MSIX package signed with your new certificate, you must tell Windows to trust it. You only need to do this once on each machine you want to test on.

### Method 1: Using the Certificate Import Wizard (GUI)

1.  Find the `CosmicDNA-TestCert.pfx` file on your Desktop and double-click it.
2.  The Certificate Import Wizard should open. For **Store Location**, choose **`Local Machine`**. You will need administrator rights.
3.  Click `Next`. The file path should already be selected. Click `Next` again.
4.  Enter the password you created in Step 1. Click `Next`.
5.  Select **`Place all certificates in the following store`**.
6.  Click **`Browse...`** and choose the **`Trusted Root Certification Authorities`** store. Click `OK`.
7.  Click `Next`, then `Finish`. You will see a security warning; click `Yes` to install the certificate.

### Method 2: Using the Command Line (Recommended)

If double-clicking the `.pfx` file does not work (due to system policies or file associations), you can use the `certutil` command-line tool. This is a more reliable method.

Run the following command. You will need to set `"$PFX_PASSWORD"` env var with the actual password you set for the `.pfx` file:

```powershell
certutil.exe -f -p "$PFX_PASSWORD" -importpfx CosmicDNA-TestCert.pfx
```

- `-f`: Forces the overwrite of an existing certificate if found.
- `-p "$PFX_PASSWORD"`: Specifies the password for the `.pfx` file.
- `-importpfx`: The command to import the certificate.
- `Root`: Specifies the destination store, which is `Trusted Root Certification Authorities` for the Local Machine.


Once the certificate is installed in the "Trusted Root" store, you can install any MSIX package signed with it without security errors.

## Using the Certificate

You can now use the `CosmicDNA-TestCert.pfx` file with a signing tool (like `SignTool.exe` from the Windows SDK) to sign your MSIX package.

```powershell
# Example of signing with SignTool.exe
SignTool.exe sign /f CosmicDNA-TestCert.pfx /p "$PFX_PASSWORD" /fd SHA256 /td SHA256 /a "C:\path\to\your\app.msix"
```

This completes the process of creating and using a self-signed certificate for testing your application.