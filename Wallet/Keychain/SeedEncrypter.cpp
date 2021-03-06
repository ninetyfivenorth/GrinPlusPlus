#include "SeedEncrypter.h"

#include <Crypto/Crypto.h>
#include <Crypto/CryptoException.h>
#include <Common/Util/VectorUtil.h>
#include <Common/Util/HexUtil.h>
#include <Crypto/RandomNumberGenerator.h>

std::optional<SecureVector> SeedEncrypter::DecryptWalletSeed(const EncryptedSeed& encryptedSeed, const SecureString& password) const
{
	try
	{
		SecretKey passwordHash = Crypto::PBKDF(password, encryptedSeed.GetSalt().GetData());
		const SecureVector decrypted = Crypto::AES256_Decrypt(encryptedSeed.GetEncryptedSeedBytes(), passwordHash, encryptedSeed.GetIV());

		SecureVector walletSeed(decrypted.begin(), decrypted.begin() + decrypted.size() - 32);

		const CBigInteger<32> hash256 = Crypto::HMAC_SHA256((const std::vector<unsigned char>&)walletSeed, passwordHash.GetBytes().GetData());
		const CBigInteger<32> hash256Check(&decrypted[walletSeed.size()]);
		if (hash256 == hash256Check)
		{
			return std::make_optional<SecureVector>(std::move(walletSeed));
		}
	}
	catch (CryptoException&)
	{
		return std::nullopt;
	}

	return std::nullopt;
}

EncryptedSeed SeedEncrypter::EncryptWalletSeed(const SecureVector& walletSeed, const SecureString& password) const
{
	CBigInteger<32> randomNumber = RandomNumberGenerator::GenerateRandom32();
	CBigInteger<16> iv = CBigInteger<16>(&randomNumber.GetData()[0]);
	CBigInteger<8> salt(std::vector<unsigned char>(randomNumber.GetData().cbegin() + 16, randomNumber.GetData().cbegin() + 24));

	SecretKey passwordHash = Crypto::PBKDF(password, salt.GetData());

	const CBigInteger<32> hash256 = Crypto::HMAC_SHA256((const std::vector<unsigned char>&)walletSeed, passwordHash.GetBytes().GetData());
	const std::vector<unsigned char>& hash256Bytes = hash256.GetData();

	SecureVector seedPlusHash;
	seedPlusHash.insert(seedPlusHash.begin(), walletSeed.cbegin(), walletSeed.cend());
	seedPlusHash.insert(seedPlusHash.end(), hash256Bytes.cbegin(), hash256Bytes.cend());

	std::vector<unsigned char> encrypted = Crypto::AES256_Encrypt(seedPlusHash, passwordHash, iv);

	return EncryptedSeed(std::move(iv), std::move(salt), std::move(encrypted));
}