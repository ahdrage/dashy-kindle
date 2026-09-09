# Public Netatmo TLS trust anchor

`DigiCertGlobalRootG2.crt` is a public PEM-encoded root certificate, not a private key. It is included as a native asset so the Kindle can verify HTTPS without relying on its old system trust store.

The chain presented by `api.netatmo.com` on 9 September 2026 was:

- Netatmo server certificate
- GeoTrust TLS RSA CA G1
- DigiCert Global Root G2

The root was checked against [DigiCert's published fingerprint](https://knowledge.digicert.com/general-information/digicert-trusted-root-authority-certificates). SHA-256 of its DER encoding:

```text
CB:3C:CB:B7:60:31:E5:E0:13:8F:8D:D3:9A:23:F9:DE:47:FF:C3:5E:43:C1:14:4C:EA:27:D4:6A:5A:B1:CB:5F
```

The build verifies that fingerprint before copying the certificate. BearSSL then verifies the server chain, hostname and validity dates. There is no insecure fallback. The root expires on 15 January 2038, but Netatmo can change its issuing authority earlier; review the actual chain and the authority's published root before updating this asset.
