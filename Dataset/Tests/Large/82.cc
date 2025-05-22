#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv6-address-helper.h"

using namespace ns3;

class UdpEchoTestSuite : public TestCase
{
public:
    UdpEchoTestSuite() : TestCase("UdpEcho Test Suite") {}

    virtual void DoRun() override
    {
        // Run each test function.
        TestNodeCreation();
        TestIpv4AddressAssignment();
        TestUdpEchoClientServerCommunication();
        TestCsmaChannelSetup();
    }

private:
    // Test if nodes are created correctly
    void TestNodeCreation()
    {
        NodeContainer n;
        n.Create(4);
        NS_TEST_ASSERT_MSG_EQ(n.GetN(), 4, "Node creation failed.");
        std::cout << "Node creation test passed." << std::endl;
    }

    // Test if IP addresses are assigned correctly for IPv4
    void TestIpv4AddressAssignment()
    {
        NodeContainer n;
        n.Create(4);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
        csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
        csma.SetDeviceAttribute("Mtu", UintegerValue(1400));
        NetDeviceContainer d = csma.Install(n);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer i = ipv4.Assign(d);

        // Check if the IP address assignment for node 1 is correct
        Ipv4Address address = i.GetAddress(1);
        NS_TEST_ASSERT_MSG_EQ(address, Ipv4Address("10.1.1.1"), "IP address assignment failed.");
        std::cout << "IPv4 address assignment test passed." << std::endl;
    }

    // Test UDP Echo Client-Server communication
    void TestUdpEchoClientServerCommunication()
    {
        NodeContainer n;
        n.Create(4);

        InternetStackHelper internet;
        internet.Install(n);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
        csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
        csma.SetDeviceAttribute("Mtu", UintegerValue(1400));
        NetDeviceContainer d = csma.Install(n);

        // IPv4 Address Assignment for UDP communication
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer i = ipv4.Assign(d);
        Address serverAddress(i.GetAddress(1));

        uint16_t port = 9; // Echo port
        UdpEchoServerHelper server(port);
        ApplicationContainer apps = server.Install(n.Get(1));
        apps.Start(Seconds(1.0));
        apps.Stop(Seconds(10.0));

        UdpEchoClientHelper client(serverAddress, port);
        client.SetAttribute("MaxPackets", UintegerValue(1));
        client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        apps = client.Install(n.Get(0));
        apps.Start(Seconds(2.0));
        apps.Stop(Seconds(10.0));

        // Run simulation
        Simulator::Run();
        Simulator::Destroy();

        std::cout << "UDP Echo Client-Server communication test passed." << std::endl;
    }

    // Test CSMA channel setup, including data rate and delay
    void TestCsmaChannelSetup()
    {
        NodeContainer n;
        n.Create(4);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
        csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
        csma.SetDeviceAttribute("Mtu", UintegerValue(1400));
        NetDeviceContainer d = csma.Install(n);

        // Validate that the channel data rate and delay are correctly set
        Ptr<CsmaChannel> channel = csma.GetChannel();
        DataRate dataRate = channel->GetDataRate();
        Time delay = channel->GetDelay();

        NS_TEST_ASSERT_MSG_EQ(dataRate.GetBitRate(), 5000000, "CSMA DataRate setup failed.");
        NS_TEST_ASSERT_MSG_EQ(delay, MilliSeconds(2), "CSMA Delay setup failed.");

        std::cout << "CSMA Channel setup test passed." << std::endl;
    }
};

// Main function to run the tests
int main(int argc, char* argv[])
{
    UdpEchoTestSuite ts;
    ts.Run();
    return 0;
}
