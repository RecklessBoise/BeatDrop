#include "Keychain.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

namespace beatdrop::Keychain
{

namespace
{
    NSMutableDictionary* makeQuery (const juce::String& service, const juce::String& account)
    {
        return [NSMutableDictionary dictionaryWithDictionary: @{
            (id) kSecClass:       (id) kSecClassGenericPassword,
            (id) kSecAttrService: [NSString stringWithUTF8String: service.toRawUTF8()],
            (id) kSecAttrAccount: [NSString stringWithUTF8String: account.toRawUTF8()]
        }];
    }
}

bool isAvailable()
{
    return true;
}

bool store (const juce::String& service, const juce::String& account, const juce::String& secret)
{
    @autoreleasepool
    {
        auto* query = makeQuery (service, account);
        auto* data = [NSData dataWithBytes: secret.toRawUTF8() length: secret.getNumBytesAsUTF8()];

        const auto updateStatus = SecItemUpdate ((CFDictionaryRef) query,
                                                 (CFDictionaryRef) @{ (id) kSecValueData: data });

        if (updateStatus == errSecSuccess)
            return true;

        if (updateStatus != errSecItemNotFound)
            return false;

        query[(id) kSecValueData] = data;
        query[(id) kSecAttrAccessible] = (id) kSecAttrAccessibleAfterFirstUnlock;
        return SecItemAdd ((CFDictionaryRef) query, nullptr) == errSecSuccess;
    }
}

std::optional<juce::String> load (const juce::String& service, const juce::String& account)
{
    @autoreleasepool
    {
        auto* query = makeQuery (service, account);
        query[(id) kSecReturnData] = @YES;
        query[(id) kSecMatchLimit] = (id) kSecMatchLimitOne;

        CFTypeRef result = nullptr;

        if (SecItemCopyMatching ((CFDictionaryRef) query, &result) != errSecSuccess || result == nullptr)
            return std::nullopt;

        auto* data = (NSData*) result;
        const auto secret = juce::String::fromUTF8 ((const char*) data.bytes, (int) data.length);
        CFRelease (result);
        return secret;
    }
}

bool remove (const juce::String& service, const juce::String& account)
{
    @autoreleasepool
    {
        const auto status = SecItemDelete ((CFDictionaryRef) makeQuery (service, account));
        return status == errSecSuccess || status == errSecItemNotFound;
    }
}

} // namespace beatdrop::Keychain
