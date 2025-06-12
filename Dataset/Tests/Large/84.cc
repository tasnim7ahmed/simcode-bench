#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

class UdpTraceClientServerTestSuite : public TestCase
{
public:
    UdpTraceClientServerTestSuite() : TestCase("UdpTraceClientServer Test Suite") {}

    virtual void DoRun() override
    {
        // Run each test function.
        TestNodeCreation();
        TestIpv4AddressAssignment();
        TestIpv6AddressAssignment();
        TestUdpClientServerCommunication();
        TestCsmaChannelSetup();
        TestUdpTraceClientSetup();
    }

private:
    // Test if nodes are created correctly
    void TestNodeCreation()
    {
        NodeContainer n;
        n.Create(2);
        NS_TEST_ASSERT_MSG_EQ(n.GetN(), 2, "Node creation failed.");
        std::cout << "Node creation test passed." << std::endl;
    }

    // Test if IPv4 addresses are assigned correctly
    void TestIpv4AddressAssignment()
    {
        NodeContainer n;
        n.Create(2);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
        csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
        csma.SetDeviceAttribute("Mtu", UintegerValue(1500));
        NetDeviceContainer d = csma.Install(n);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer i = ipv4.Assign(d);

        // Check if the IP address assignment for node 1 is correct
        Ipv4Address address = i.GetAddress(1);
        NS_TEST_ASSERT_MSG_EQ(address, Ipv4Address("10.1.1.1"), "IPv4 address assignment failed.");
        std::cout << "IPv4 address assignment test passed." << std::endl;
    }

    // Test if IPv6 addresses are assigned correctly
    void TestIpv6AddressAssignment()
    {
        NodeContainer n;
        n.Create(2);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
        csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
        csma.SetDeviceAttribute("Mtu", UintegerValue(1500));
        NetDeviceContainer d = csma.Install(n);

        Ipv6AddressHelper ipv6;
        ipv6.SetBase("2001:0000:f00d:cafe::", Ipv6Prefix(64));
        Ipv6InterfaceContainer i6 = ipv6.Assign(d);

        // Check if the IPv6 address assignment for node 1 is correct
        Ipv6Address address = i6.GetAddress(1, 1);
        NS_TEST_ASSERT_MSG_EQ(address, Ipv6Address("2001:0000:f00d:cafe::1"), "IPv6 address assignment failed.");
        std::cout << "IPv6 address assignment test passed." << std::endl;
    }

    // Test UDP Echo Client-Server communication using trace file
    void TestUdpClientServerCommunication()
    {
        NodeContainer n;
        n.Create(2);

        InternetStackHelper internet;
        internet.Install(n);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
        csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
        csma.SetDeviceAttribute("Mtu", UintegerValue(1500));
        NetDeviceContainer d = csma.Install(n);

        // IPv4 Address Assignment for UDP communication
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer i = ipv4.Assign(d);
        Address serverAddress(i.GetAddress(1));

        uint16_t port = 4000;
        UdpServerHelper server(port);
        ApplicationContainer apps = server.Install(n.Get(1));
        apps.Start(Seconds(1.0));
        apps.Stop(Seconds(10.0));

        uint32_t maxPacketSize = 1472; // Back off 20 (IP) + 8 (UDP) bytes from MTU
        UdpTraceClientHelper client(serverAddress, port, "trace-file.txt"); // Assuming a trace file is used
        client.SetAttribute("MaxPacketSize", UintegerValue(maxPacketSize));
        apps = client.Install(n.Get(0));
        apps.Start(Seconds(2.0));
        apps.Stop(Seconds(10.0));

        // Run simulation
        Simulator::Run();
        Simulator::Destroy();

        std::cout << "UDP Client-Server communication test passed." << std::endl;
    }

    // Test CSMA channel setup, including data rate and delay
    void TestCsmaChannelSetup()
    {
        NodeContainer n;
        n.Create(2);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
        csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
        csma.SetDeviceAttribute("Mtu", UintegerValue(1500));
        NetDeviceContainer d = csma.Install(n);

        // Validate that the channel data rate and delay are correctly set
        Ptr<CsmaChannel> channel = csma.GetChannel();
        DataRate dataRate = channel->GetDataRate();
        Time delay = channel->GetDelay();

        NS_TEST_ASSERT_MSG_EQ(dataRate.GetBitRate(), 5000000, "CSMA DataRate setup failed.");
        NS_TEST_ASSERT_MSG_EQ(delay, MilliSeconds(2), "CSMA Delay setup failed.");

        std::cout << "CSMA Channel setup test passed." << std::endl;
    }

    // Test UDP Trace Client setup
    void TestUdpTraceClientSetup()
    {
        NodeContainer n;
        n.Create(2);

        InternetStackHelper internet;
        internet.Install(n);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
        csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
        csma.SetDeviceAttribute("Mtu", UintegerValue(1500));
        NetDeviceContainer d = csma.Install(n);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer i = ipv4.Assign(d);
        Address serverAddress(i.GetAddress(1));

        UdpTraceClientHelper client(serverAddress, 4000, "trace-file.txt");

        // Ensure the trace file path is set correctly
        NS_TEST_ASSERT_MSG_EQ(client.GetTraceFilePath(), "trace-file.txt", "UDP Trace Client setup failed.");
        std::cout << "UDP Trace Client setup test passed." << std::endl;
    }
};

// Main function to run the tests
int main(int argc, char* argv[])
{
    UdpTraceClientServerTestSuite ts;
    ts.Run();
    return 0;
}
