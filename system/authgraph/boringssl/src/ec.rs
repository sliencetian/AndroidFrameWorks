// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////

//! BoringSSL-based implementation for the AuthGraph `Ec` trait.

use alloc::borrow::Cow;
use alloc::vec::Vec;
use authgraph_core::{
    ag_err, ag_verr,
    error::Error,
    key::{
        check_cose_key_params, EcExchangeKey, EcExchangeKeyPriv, EcExchangeKeyPub, EcSignKey,
        EcVerifyKey, EcdhSecret, CURVE25519_PRIV_KEY_LEN,
    },
    traits, try_to_vec,
};
use authgraph_wire::ErrorCode;
use core::ops::DerefMut;
use coset::{cbor::Value, iana, CoseKey, Label};

/// Initial byte of SEC1 public key encoding that indicates an uncompressed point.
pub const SEC1_UNCOMPRESSED_PREFIX: u8 = 0x04;

/// The struct implementing the Authgraph [`EcDh`] trait.
pub struct BoringEcDh;

impl traits::EcDh for BoringEcDh {
    fn generate_key(&self) -> Result<EcExchangeKey, Error> {
        let (priv_key, pub_key) = create_p256_key_pair(iana::Algorithm::ECDH_ES_HKDF_256)?;

        Ok(EcExchangeKey {
            priv_key: EcExchangeKeyPriv(priv_key),
            pub_key: EcExchangeKeyPub(pub_key),
        })
    }

    fn compute_shared_secret(
        &self,
        own_key: &EcExchangeKeyPriv,
        peer_key: &EcExchangeKeyPub,
    ) -> Result<EcdhSecret, Error> {
        let peer_key = p256_ecdh_pkey_from_cose(&peer_key.0)?;
        let group =
            ossl!(openssl::ec::EcGroup::from_curve_name(openssl::nid::Nid::X9_62_PRIME256V1))?;
        // This method is an Android modification to the rust-openssl crate.
        let ec_key = ossl!(openssl::ec::EcKey::private_key_from_der_for_group(&own_key.0, &group))?;
        let pkey = ossl!(openssl::pkey::PKey::from_ec_key(ec_key))?;
        let mut deriver = ossl!(openssl::derive::Deriver::new(&pkey))?;
        ossl!(deriver.set_peer(&peer_key))
            .map_err(|e| ag_err!(InvalidPeerKeKey, "peer key invalid: {:?}", e))?;
        let derived = ossl!(deriver.derive_to_vec())?;
        Ok(EcdhSecret(derived))
    }
}

/// The struct implementing the Authgraph [`EcDsa`] trait.
pub struct BoringEcDsa;

impl traits::EcDsa for BoringEcDsa {
    fn generate_key(&self, curve: iana::EllipticCurve) -> Result<(EcSignKey, EcVerifyKey), Error> {
        match curve {
            iana::EllipticCurve::P_256 => create_p256_key_pair(iana::Algorithm::ES256)
                .map(|(priv_key, pub_key)| (EcSignKey::P256(priv_key), EcVerifyKey::P256(pub_key))),
            iana::EllipticCurve::P_384 => create_p384_key_pair(iana::Algorithm::ES384)
                .map(|(priv_key, pub_key)| (EcSignKey::P384(priv_key), EcVerifyKey::P384(pub_key))),
            iana::EllipticCurve::Ed25519 => create_ed25519_key_pair().map(|(priv_key, pub_key)| {
                (EcSignKey::Ed25519(priv_key), EcVerifyKey::Ed25519(pub_key))
            }),
            _ => Err(ag_err!(Unimplemented, "unexpected curve {curve:?} for ECDSA keygen")),
        }
    }
    fn sign(&self, sign_key: &EcSignKey, data: &[u8]) -> Result<Vec<u8>, Error> {
        let pkey;
        let mut signer = match sign_key {
            EcSignKey::Ed25519(key) => {
                pkey = ossl!(openssl::pkey::PKey::private_key_from_raw_bytes(
                    key,
                    openssl::pkey::Id::ED25519
                ))?;
                // Ed25519 has an internal digest so needs no external digest.
                ossl!(openssl::sign::Signer::new_without_digest(&pkey))
            }
            EcSignKey::P256(key) => {
                let group = ossl!(openssl::ec::EcGroup::from_curve_name(
                    openssl::nid::Nid::X9_62_PRIME256V1
                ))?;
                let digest = openssl::hash::MessageDigest::sha256();
                let ec_key =
                    ossl!(openssl::ec::EcKey::private_key_from_der_for_group(key, &group))?;
                pkey = ossl!(openssl::pkey::PKey::from_ec_key(ec_key))?;
                ossl!(openssl::sign::Signer::new(digest, &pkey))
            }
            EcSignKey::P384(key) => {
                let group =
                    ossl!(openssl::ec::EcGroup::from_curve_name(openssl::nid::Nid::SECP384R1))?;
                // Note that the CDDL for PubKeyECDSA384 specifies SHA-384 as the digest.
                let digest = openssl::hash::MessageDigest::sha384();
                let ec_key =
                    ossl!(openssl::ec::EcKey::private_key_from_der_for_group(key, &group))?;
                pkey = ossl!(openssl::pkey::PKey::from_ec_key(ec_key))?;
                ossl!(openssl::sign::Signer::new(digest, &pkey))
            }
        }?;
        let sig = ossl!(signer.sign_oneshot_to_vec(data))?;
        match sign_key {
            EcSignKey::Ed25519(_) => Ok(sig),
            // BoringSSL emits signatures encoded in an `Ecdsa-Sig-Value` DER structure, as per
            // RFC 3279 section 2.2.3.  Convert this to R | S form.
            EcSignKey::P256(_) => ec_der_signature_to_cose(32, &sig),
            EcSignKey::P384(_) => ec_der_signature_to_cose(48, &sig),
        }
    }

    fn verify_signature(
        &self,
        verify_key: &EcVerifyKey,
        data: &[u8],
        signature: &[u8],
    ) -> Result<(), Error> {
        let signature = match verify_key {
            EcVerifyKey::Ed25519(_) => Cow::Borrowed(signature),
            // For NIST curves, convert the R|S format signature to the DER-encoded form that
            // BoringSSL expects.
            EcVerifyKey::P256(_) => Cow::Owned(ec_cose_signature_to_der(32, signature)?),
            EcVerifyKey::P384(_) => Cow::Owned(ec_cose_signature_to_der(48, signature)?),
        };

        let pkey;
        let mut verifier = match verify_key {
            EcVerifyKey::Ed25519(key) => {
                pkey = ed25519_ecdsa_pkey_from_cose(key)?;
                ossl!(openssl::sign::Verifier::new_without_digest(&pkey))
            }
            EcVerifyKey::P256(key) => {
                pkey = p256_ecdsa_pkey_from_cose(key)?;
                let digest = openssl::hash::MessageDigest::sha256();
                ossl!(openssl::sign::Verifier::new(digest, &pkey))
            }
            EcVerifyKey::P384(key) => {
                pkey = p384_ecdsa_pkey_from_cose(key)?;
                let digest = openssl::hash::MessageDigest::sha384();
                ossl!(openssl::sign::Verifier::new(digest, &pkey))
            }
        }?;
        if ossl!(verifier.verify_oneshot(&signature, data))? {
            Ok(())
        } else {
            Err(ag_err!(InvalidSignature, "signature verification failed"))
        }
    }
}

/// Convert a R | S format NIST signature to a DER-encoded form.
fn ec_cose_signature_to_der(coord_len: usize, signature: &[u8]) -> Result<Vec<u8>, Error> {
    if signature.len() != 2 * coord_len {
        return Err(ag_err!(
            InvalidSignature,
            "expected signature of length {0}+{0}, got {1}",
            coord_len,
            signature.len()
        ));
    }
    let r = ossl!(openssl::bn::BigNum::from_slice(&signature[..coord_len]))?;
    let s = ossl!(openssl::bn::BigNum::from_slice(&signature[coord_len..]))?;
    let signature = ossl!(openssl::ecdsa::EcdsaSig::from_private_components(r, s))?;
    ossl!(signature.to_der())
}

/// Convert a DER-encoded EC NIST signature to R | S form
fn ec_der_signature_to_cose(coord_len: usize, signature: &[u8]) -> Result<Vec<u8>, Error> {
    let coord_len: i32 = coord_len
        .try_into()
        .map_err(|_e| ag_err!(InvalidSignature, "invalid coord len {coord_len}"))?;
    let signature = ossl!(openssl::ecdsa::EcdsaSig::from_der(signature))?;
    let mut sig = ossl!(signature.r().to_vec_padded(coord_len))?;
    sig.extend_from_slice(&ossl!(signature.s().to_vec_padded(coord_len))?);
    Ok(sig)
}

/// Create an EC key pair on P-256 curve and return a tuple consisting of the private key bytes
/// and the public key as a `CoseKey`.  The `algorithm` parameter indicates the intended use
/// for the keypair (ECDH or ECDSA).
pub fn create_p256_key_pair(algorithm: iana::Algorithm) -> Result<(Vec<u8>, CoseKey), Error> {
    let group = ossl!(openssl::ec::EcGroup::from_curve_name(openssl::nid::Nid::X9_62_PRIME256V1))?;
    create_nist_key_pair(algorithm, iana::EllipticCurve::P_256, group, 32)
}

/// Create an EC key pair on P-384 curve and return a tuple consisting of the private key bytes
/// and the public key as a `CoseKey`.  The `algorithm` parameter indicates the intended use
/// for the keypair (ECDH or ECDSA).
pub fn create_p384_key_pair(algorithm: iana::Algorithm) -> Result<(Vec<u8>, CoseKey), Error> {
    let group = ossl!(openssl::ec::EcGroup::from_curve_name(openssl::nid::Nid::SECP384R1))?;
    create_nist_key_pair(algorithm, iana::EllipticCurve::P_384, group, 48)
}

fn create_nist_key_pair(
    algorithm: iana::Algorithm,
    curve: iana::EllipticCurve,
    group: openssl::ec::EcGroup,
    coord_len: usize,
) -> Result<(Vec<u8>, CoseKey), Error> {
    let ec_priv_key = ossl!(openssl::ec::EcKey::<openssl::pkey::Private>::generate(&group))?;
    // Convert the private key to DER-encoded `ECPrivateKey` as per RFC5915 s3, which is
    // our choice of private key encoding.
    let priv_key = ossl!(ec_priv_key.private_key_to_der())?;

    // Get the public key point, and convert to (x, y) coordinates.
    let pub_key_pt = ec_priv_key.public_key();
    let mut bn_ctx = ossl!(openssl::bn::BigNumContext::new())?;
    let pub_key_sec1 = ossl!(pub_key_pt.to_bytes(
        group.as_ref(),
        openssl::ec::PointConversionForm::UNCOMPRESSED,
        bn_ctx.deref_mut()
    ))?;
    let (x, y) = crate::ec::coords_from_nist_pub_key(&pub_key_sec1, coord_len)?;
    let pub_key = coset::CoseKeyBuilder::new_ec2_pub_key(curve, x, y).algorithm(algorithm).build();
    Ok((priv_key, pub_key))
}

fn create_ed25519_key_pair() -> Result<([u8; CURVE25519_PRIV_KEY_LEN], CoseKey), Error> {
    let pkey = ossl!(openssl::pkey::PKey::generate_ed25519())?;
    let priv_key = ossl!(pkey.raw_private_key())?;
    let priv_key: [u8; CURVE25519_PRIV_KEY_LEN] = priv_key
        .try_into()
        .map_err(|e| ag_err!(InternalError, "generated Ed25519 key of unexpected size: {:?}", e))?;
    let raw_pub_key = ossl!(pkey.raw_public_key())?;
    let pub_key = coset::CoseKeyBuilder::new_okp_key()
        .param(iana::OkpKeyParameter::Crv as i64, Value::from(iana::EllipticCurve::Ed25519 as u64))
        .param(iana::OkpKeyParameter::X as i64, Value::from(raw_pub_key))
        .algorithm(coset::iana::Algorithm::EdDSA)
        .build();
    Ok((priv_key, pub_key))
}

/// Helper function to return the (x,y) coordinates of a P-256 public key, as a SEC-1 encoded
/// uncompressed point. 0x04: uncompressed, followed by x || y coordinates.
pub fn coords_from_p256_pub_key(pub_key: &[u8]) -> Result<(Vec<u8>, Vec<u8>), Error> {
    coords_from_nist_pub_key(pub_key, 32) // For P-256
}

/// Helper function to return the (x,y) coordinates of a P-384 public key, as a SEC-1 encoded
/// uncompressed point. 0x04: uncompressed, followed by x || y coordinates.
pub fn coords_from_p384_pub_key(pub_key: &[u8]) -> Result<(Vec<u8>, Vec<u8>), Error> {
    coords_from_nist_pub_key(pub_key, 48) // For P-384
}

/// Helper function to return the (x,y) coordinates, given the public key as a SEC-1 encoded
/// uncompressed point. 0x04: uncompressed, followed by x || y coordinates.
pub fn coords_from_nist_pub_key(
    pub_key: &[u8],
    coord_len: usize,
) -> Result<(Vec<u8>, Vec<u8>), Error> {
    if pub_key.len() != (1 + 2 * coord_len) {
        return Err(ag_err!(
            InternalError,
            "unexpected SEC1 pubkey len of {} for coord_len {coord_len}",
            pub_key.len(),
        ));
    }
    if pub_key[0] != SEC1_UNCOMPRESSED_PREFIX {
        return Err(ag_err!(
            InternalError,
            "unexpected SEC1 pubkey initial byte {} for coord_len {coord_len}",
            pub_key[0],
        ));
    }
    Ok((try_to_vec(&pub_key[1..1 + coord_len])?, try_to_vec(&pub_key[1 + coord_len..])?))
}

/// Convert an ECDH P-256 `COSE_Key` into an [`openssl::pkey::PKey`].
fn p256_ecdh_pkey_from_cose(
    cose_key: &coset::CoseKey,
) -> Result<openssl::pkey::PKey<openssl::pkey::Public>, Error> {
    nist_pkey_from_cose(
        cose_key,
        iana::KeyType::EC2,
        iana::Algorithm::ECDH_ES_HKDF_256, // ECDH
        iana::EllipticCurve::P_256,
        openssl::nid::Nid::X9_62_PRIME256V1,
        ErrorCode::InvalidPeerKeKey,
    )
}

/// Convert an ECDSA P-256 `COSE_Key` into an [`openssl::pkey::PKey`].
fn p256_ecdsa_pkey_from_cose(
    cose_key: &coset::CoseKey,
) -> Result<openssl::pkey::PKey<openssl::pkey::Public>, Error> {
    nist_pkey_from_cose(
        cose_key,
        iana::KeyType::EC2,
        iana::Algorithm::ES256, // ECDSA
        iana::EllipticCurve::P_256,
        openssl::nid::Nid::X9_62_PRIME256V1,
        ErrorCode::InvalidCertChain,
    )
}

/// Convert an ECDSA P-384 `COSE_Key` into an [`openssl::pkey::PKey`].
fn p384_ecdsa_pkey_from_cose(
    cose_key: &coset::CoseKey,
) -> Result<openssl::pkey::PKey<openssl::pkey::Public>, Error> {
    nist_pkey_from_cose(
        cose_key,
        iana::KeyType::EC2,
        iana::Algorithm::ES384,
        iana::EllipticCurve::P_384,
        openssl::nid::Nid::SECP384R1,
        ErrorCode::InvalidCertChain,
    )
}

fn nist_pkey_from_cose(
    cose_key: &coset::CoseKey,
    want_kty: iana::KeyType,
    want_alg: iana::Algorithm,
    want_curve: iana::EllipticCurve,
    nid: openssl::nid::Nid,
    err_code: ErrorCode,
) -> Result<openssl::pkey::PKey<openssl::pkey::Public>, Error> {
    // First check that the COSE_Key looks sensible.
    check_cose_key_params(cose_key, want_kty, want_alg, want_curve, err_code)?;
    // Extract (x, y) coordinates (big-endian).
    let x = cose_key
        .params
        .iter()
        .find_map(|(l, v)| match (l, v) {
            (Label::Int(l), Value::Bytes(data)) if *l == iana::Ec2KeyParameter::X as i64 => {
                Some(data)
            }
            _ => None,
        })
        .ok_or_else(|| ag_verr!(err_code, "no x coord"))?;
    let y = cose_key
        .params
        .iter()
        .find_map(|(l, v)| match (l, v) {
            (Label::Int(l), Value::Bytes(data)) if *l == iana::Ec2KeyParameter::Y as i64 => {
                Some(data)
            }
            _ => None,
        })
        .ok_or_else(|| ag_verr!(err_code, "no y coord"))?;

    let x = ossl!(openssl::bn::BigNum::from_slice(x))?;
    let y = ossl!(openssl::bn::BigNum::from_slice(y))?;
    let group = ossl!(openssl::ec::EcGroup::from_curve_name(nid))?;
    let ec_key = ossl!(openssl::ec::EcKey::from_public_key_affine_coordinates(&group, &x, &y))?;
    let pkey = ossl!(openssl::pkey::PKey::from_ec_key(ec_key))?;
    Ok(pkey)
}

/// Convert an EdDSA Ed25519 `COSE_Key` into an [`openssl::pkey::PKey`].
fn ed25519_ecdsa_pkey_from_cose(
    cose_key: &coset::CoseKey,
) -> Result<openssl::pkey::PKey<openssl::pkey::Public>, Error> {
    // First check that the COSE_Key looks sensible.
    check_cose_key_params(
        cose_key,
        iana::KeyType::OKP,
        iana::Algorithm::EdDSA,
        iana::EllipticCurve::Ed25519,
        ErrorCode::InvalidCertChain,
    )?;
    // Extract x coordinate (little-endian).
    let x = cose_key
        .params
        .iter()
        .find_map(|(l, v)| match (l, v) {
            (Label::Int(l), Value::Bytes(data)) if *l == iana::OkpKeyParameter::X as i64 => {
                Some(data)
            }
            _ => None,
        })
        .ok_or_else(|| ag_err!(InvalidCertChain, "no x coord"))?;

    let pkey =
        ossl!(openssl::pkey::PKey::public_key_from_raw_bytes(x, openssl::pkey::Id::ED25519))?;
    Ok(pkey)
}

#[cfg(test)]
mod tests {
    use super::*;
    use authgraph_core::traits::EcDsa;

    #[test]
    fn test_p256_keygen() {
        let ecdsa = BoringEcDsa;
        let (priv_key, pub_key) = ecdsa.generate_key(iana::EllipticCurve::P_256).unwrap();
        assert!(matches!(priv_key, EcSignKey::P256(_)));
        let cose_key = if let EcVerifyKey::P256(cose_key) = pub_key {
            cose_key
        } else {
            panic!("Unexpected pub key variant from generate_key()");
        };
        assert!(p256_ecdsa_pkey_from_cose(&cose_key).is_ok());
    }

    #[test]
    fn test_p384_keygen() {
        let ecdsa = BoringEcDsa;
        let (priv_key, pub_key) = ecdsa.generate_key(iana::EllipticCurve::P_384).unwrap();
        assert!(matches!(priv_key, EcSignKey::P384(_)));
        let cose_key = if let EcVerifyKey::P384(cose_key) = pub_key {
            cose_key
        } else {
            panic!("Unexpected pub key variant from generate_key()");
        };
        assert!(p384_ecdsa_pkey_from_cose(&cose_key).is_ok());
    }

    #[test]
    fn test_ed25519_keygen() {
        let ecdsa = BoringEcDsa;
        let (priv_key, pub_key) = ecdsa.generate_key(iana::EllipticCurve::Ed25519).unwrap();
        assert!(matches!(priv_key, EcSignKey::Ed25519(_)));
        let cose_key = if let EcVerifyKey::Ed25519(cose_key) = pub_key {
            cose_key
        } else {
            panic!("Unexpected pub key variant from generate_key()");
        };
        assert!(ed25519_ecdsa_pkey_from_cose(&cose_key).is_ok());
    }

    #[test]
    fn test_signature_convert() {
        let tests = [
            (32,
             concat!(
                 "3045", // SEQUENCE
                 "0220", // INTEGER len x20
                 "2ba3a8be6b94d5ec80a6d9d1190a436effe50d85a1eee859b8cc6af9bd5c2e18",
                 "0221", // INTEGER len x21
                 "00",   // leading zero to avoid appearing negative
                 "b329f479a2bbd0a5c384ee1493b1f5186a87139cac5df4087c134b49156847db",
             ),
             concat!(
                 "2ba3a8be6b94d5ec80a6d9d1190a436effe50d85a1eee859b8cc6af9bd5c2e18",
                 "b329f479a2bbd0a5c384ee1493b1f5186a87139cac5df4087c134b49156847db",
             ),
            ),
            (48,
             concat!(
                 "3065",  // SEQUENCE
                 "0230",  // INTEGER len x30
                 "12b30abef6b5476fe6b612ae557c0425661e26b44b1bfe19daf2ca28e3113083ba8e4ae4cc45a0320abd3394f1c548d7",
                 "0231",  // INTEGER len x31
                 "00",    // leading zero
                 "e7bf25603e2d07076ff30b7a2abec473da8b11c572b35fc631991d5de62ddca7525aaba89325dfd04fecc47bff426f82",
                 ),
             concat!(
                 "12b30abef6b5476fe6b612ae557c0425661e26b44b1bfe19daf2ca28e3113083ba8e4ae4cc45a0320abd3394f1c548d7",
                 "e7bf25603e2d07076ff30b7a2abec473da8b11c572b35fc631991d5de62ddca7525aaba89325dfd04fecc47bff426f82",
              ),
            ),
            (32,
             concat!(
                 "3044", // SEQUENCE
                 "021f", // INTEGER len x1f
                 "2ba8be6b94d5ec80a6d9d1190a436effe50d85a1eee859b8cc6af9bd5c2e18",
                 "0221", // INTEGER len x21
                 "00",   // leading zero to avoid appearing negative
                 "b329f479a2bbd0a5c384ee1493b1f5186a87139cac5df4087c134b49156847db",
             ),
             concat!(
                 "002ba8be6b94d5ec80a6d9d1190a436effe50d85a1eee859b8cc6af9bd5c2e18",
                 "b329f479a2bbd0a5c384ee1493b1f5186a87139cac5df4087c134b49156847db",
             ),
            ),
        ];
        for (coord_len, der_sig, cose_sig) in tests {
            let sig = hex::decode(der_sig).unwrap();
            let got = ec_der_signature_to_cose(coord_len, &sig).unwrap();
            assert_eq!(hex::encode(&got), cose_sig, "DER->COSE mismatch for {der_sig}");

            let got = ec_cose_signature_to_der(coord_len, &got).unwrap();
            assert_eq!(hex::encode(&got), der_sig, "COSE->DER mismatch for {cose_sig}");
        }
    }

    #[test]
    fn test_der_signature_convert_fail() {
        let tests = [
            (
                32,
                concat!(
                    "3044", // SEQUENCE **wrong len**
                    "0220", // INTEGER len x20
                    "2ba3a8be6b94d5ec80a6d9d1190a436effe50d85a1eee859b8cc6af9bd5c2e18",
                    "0221", // INTEGER len x21
                    "00",   // leading zero to avoid appearing negative
                    "b329f479a2bbd0a5c384ee1493b1f5186a87139cac5df4087c134b49156847db",
                ),
            ),
            (
                48,
                concat!(
                    "3065",  // SEQUENCE
                    "0231",  // INTEGER len x30  **wrong len**
                    "12b30abef6b5476fe6b612ae557c0425661e26b44b1bfe19daf2ca28e3113083ba8e4ae4cc45a0320abd3394f1c548d7",
                    "0231",  // INTEGER len x31
                    "00",    // leading zero
                    "e7bf25603e2d07076ff30b7a2abec473da8b11c572b35fc631991d5de62ddca7525aaba89325dfd04fecc47bff426f82",
                ),
            ),
            (
                32,
                concat!(
                    "3044", // SEQUENCE
                    "021f", // INTEGER len x1f **wrong len**
                    "002ba8be6b94d5ec80a6d9d1190a436effe50d85a1eee859b8cc6af9bd5c2e18",
                    "0221", // INTEGER len x21
                    "00",   // leading zero to avoid appearing negative
                    "b329f479a2bbd0a5c384ee1493b1f5186a87139cac5df4087c134b49156847db",
                ),
            ),
            (
                32,
                concat!(
                    "3044", // SEQUENCE
                    "0220", // INTEGER len x20  **unnecessary leading 00**
                    "002ba8be6b94d5ec80a6d9d1190a436effe50d85a1eee859b8cc6af9bd5c2e18",
                    "0221", // INTEGER len x21
                    "00",   // leading zero to avoid appearing negative
                    "b329f479a2bbd0a5c384ee1493b1f5186a87139cac5df4087c134b49156847db",
                ),
            ),
            (
                1 + i32::MAX as usize,
                concat!(
                    "3045", // SEQUENCE
                    "0220", // INTEGER len x20
                    "2ba3a8be6b94d5ec80a6d9d1190a436effe50d85a1eee859b8cc6af9bd5c2e18",
                    "0221", // INTEGER len x21
                    "00",   // leading zero to avoid appearing negative
                    "b329f479a2bbd0a5c384ee1493b1f5186a87139cac5df4087c134b49156847db",
                ),
            ),
        ];
        for (coord_len, der_sig) in tests {
            let sig = hex::decode(der_sig).unwrap();
            assert!(
                ec_der_signature_to_cose(coord_len, &sig).is_err(),
                "expected DER decode error for {der_sig}"
            );
        }
    }

    #[test]
    fn test_cose_signature_convert_fail() {
        let tests = [
            (
                32,  // data too long
                concat!(
                    "112ba3a8be6b94d5ec80a6d9d1190a436effe50d85a1eee859b8cc6af9bd5c2e18",
                    "11b329f479a2bbd0a5c384ee1493b1f5186a87139cac5df4087c134b49156847db",
                ),
            ),
            (
                32, // data too short
                concat!(
                    "2ba3a8be6b94d5ec80a6d9d1190a436effe50d85a1eee859b8cc6af9bd5c2e18",
                    "29f479a2bbd0a5c384ee1493b1f5186a87139cac5df4087c134b49156847db",
                ),
            ),
            (
                48, // data too long
                concat!(
                    "b30abef6b5476fe6b612ae557c0425661e26b44b1bfe19daf2ca28e3113083ba8e4ae4cc45a0320abd3394f1c548d7",
                    "e7bf25603e2d07076ff30b7a2abec473da8b11c572b35fc631991d5de62ddca7525aaba89325dfd04fecc47bff426f82",
                ),
            ),
        ];
        for (coord_len, cose_sig) in tests {
            let sig = hex::decode(cose_sig).unwrap();
            assert!(
                ec_cose_signature_to_der(coord_len, &sig).is_err(),
                "expected COSE decode error for {cose_sig}"
            );
        }
    }
}
