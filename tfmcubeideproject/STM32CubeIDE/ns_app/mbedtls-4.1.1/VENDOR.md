Vendored from the official `mbedtls-4.1.1.tar.bz2` (not the GitHub auto zip; that tarball omits the `tf-psa-crypto` tree).

Kept: `include/`, `library/`, `tf-psa-crypto` headers plus ASN.1/PK/PEM/platform sources, `LICENSE`.

Stripped for this NS PSA-client port: tests, programs, docs trees, CMake/CI, unused crypto drivers (`pqcp`, `everest`, `p256-m`), and the software AES/SHA/ECP/PSA-core `.c` files. Headers under `core/` and `drivers/builtin/` remain. AES/SHA stay in the TF-M Crypto partition.
