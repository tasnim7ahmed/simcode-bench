void TestIpv6AutoconfigWithPrefix1(Mac48Address macAddress)
{
    Ipv6Address prefix1("2001:1::");
    Ipv6Address ipv6Address = Ipv6Address::MakeAutoconfiguredAddress(macAddress, prefix1);
    
    NS_LOG_INFO("Tested with prefix1, MAC address = " << macAddress << ", Generated address = " << ipv6Address);

    // Check if the address follows the correct format, prefix + MAC address-based suffix
    // We expect the last 64 bits of the address to be derived from the MAC address.
    NS_ASSERT(ipv6Address.GetPrefix() == prefix1);  // Ensure the prefix is correct
}

void TestIpv6AutoconfigWithPrefix1(Mac48Address macAddress)
{
    Ipv6Address prefix1("2001:1::");
    Ipv6Address ipv6Address = Ipv6Address::MakeAutoconfiguredAddress(macAddress, prefix1);
    
    NS_LOG_INFO("Tested with prefix1, MAC address = " << macAddress << ", Generated address = " << ipv6Address);

    // Check if the address follows the correct format, prefix + MAC address-based suffix
    // We expect the last 64 bits of the address to be derived from the MAC address.
    NS_ASSERT(ipv6Address.GetPrefix() == prefix1);  // Ensure the prefix is correct
}

void TestIpv6AutoconfigWithPrefix2(Mac48Address macAddress)
{
    Ipv6Address prefix2("2002:1:1::");
    Ipv6Address ipv6Address = Ipv6Address::MakeAutoconfiguredAddress(macAddress, prefix2);
    
    NS_LOG_INFO("Tested with prefix2, MAC address = " << macAddress << ", Generated address = " << ipv6Address);

    // Check if the address follows the correct format, prefix + MAC address-based suffix
    // We expect the last 64 bits of the address to be derived from the MAC address.
    NS_ASSERT(ipv6Address.GetPrefix() == prefix2);  // Ensure the prefix is correct
}

void TestIpv6AutoconfigWithMultipleMacsPrefix2()
{
    Mac48Address m_addresses[10] = {
        "00:00:00:00:00:01", "00:00:00:00:00:02", "00:00:00:00:00:03", 
        "00:00:00:00:00:04", "00:00:00:00:00:05", "00:00:00:00:00:06", 
        "00:00:00:00:00:07", "00:00:00:00:00:08", "00:00:00:00:00:09", 
        "00:00:00:00:00:10"
    };
    
    Ipv6Address prefix2("2002:1:1::");
    
    for (uint32_t i = 0; i < 10; ++i)
    {
        NS_LOG_INFO("Testing with MAC address = " << m_addresses[i]);
        Ipv6Address ipv6Address = Ipv6Address::MakeAutoconfiguredAddress(m_addresses[i], prefix2);
        NS_LOG_INFO("Generated address = " << ipv6Address);
        NS_ASSERT(ipv6Address.GetPrefix() == prefix2);  // Ensure the prefix is correct
    }
}

void TestIpv6AddressFormat(Mac48Address macAddress, Ipv6Address prefix)
{
    Ipv6Address ipv6Address = Ipv6Address::MakeAutoconfiguredAddress(macAddress, prefix);
    NS_LOG_INFO("Testing address format for MAC address = " << macAddress);
    NS_LOG_INFO("Generated address = " << ipv6Address);
    
    // Check if the suffix is derived correctly (last 64 bits should come from MAC address)
    uint64_t expectedSuffix = macAddress.GetAddress()[5] << 8 | macAddress.GetAddress()[4];
    uint64_t actualSuffix = ipv6Address.GetSuffix();
    
    NS_ASSERT(expectedSuffix == actualSuffix);  // Ensure that the address is derived correctly from the MAC address
}

int main(int argc, char* argv[])
{
    LogComponentEnable("TestIpv6", LOG_LEVEL_ALL);

    // Test 1: Test autoconfiguration with prefix1 and single MAC address
    TestIpv6AutoconfigWithPrefix1(Mac48Address("00:00:00:00:00:01"));

    // Test 2: Test autoconfiguration with prefix2 and single MAC address
    TestIpv6AutoconfigWithPrefix2(Mac48Address("00:00:00:00:00:01"));

    // Test 3: Test autoconfiguration with multiple MAC addresses and prefix1
    TestIpv6AutoconfigWithMultipleMacsPrefix1();

    // Test 4: Test autoconfiguration with multiple MAC addresses and prefix2
    TestIpv6AutoconfigWithMultipleMacsPrefix2();

    // Test 5: Check the address format for autoconfiguration
    TestIpv6AddressFormat(Mac48Address("00:00:00:00:00:01"), Ipv6Address("2001:1::"));

    return 0;
}

