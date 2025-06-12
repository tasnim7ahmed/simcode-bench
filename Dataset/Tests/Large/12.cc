#include "ns3/test.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpTraceClientServerTest : public TestCase
{
public:
    UdpTraceClientServerTest() : TestCase("UDP Trace Client-Server Test") {}

private:
    virtual void DoRun()
    {
        bool useV6 = false;
        Address serverAddress;

        // Create nodes
        NodeContainer n;
        n.Create(2);
        NS_TEST_ASSERT_MSG_EQ(n.GetN(), 2, "Should create exactly 2 nodes.");

        // Install Internet Stack
        InternetStackHelper internet;
        internet.Install(n);

        // Create CSMA channel
        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
        csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
        csma.SetDeviceAttribute("Mtu", UintegerValue(1500));
        NetDeviceContainer d = csma.Install(n);
        NS_TEST_ASSERT_MSG_EQ(d.GetN(), 2, "Should have 2 CSMA devices.");

        // Assign IP Addresses
        if (!useV6)
        {
            Ipv4AddressHelper ipv4;
            ipv4.SetBase("10.1.1.0", "255.255.255.0");
            Ipv4InterfaceContainer i = ipv4.Assign(d);
            serverAddress = Address(i.GetAddress(1));
            NS_TEST_ASSERT_MSG_NE(i.GetAddress(1), Ipv4Address("0.0.0.0"), "Server should have a valid IP address.");
        }
        else
        {
            Ipv6AddressHelper ipv6;
            ipv6.SetBase("2001:0000:f00d:cafe::", Ipv6Prefix(64));
            Ipv6InterfaceContainer i6 = ipv6.Assign(d);
            serverAddress = Address(i6.GetAddress(1, 1));
        }

        // Install UDP Server
        uint16_t port = 4000;
        UdpServerHelper server(port);
        ApplicationContainer serverApp = server.Install(n.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        Ptr<UdpServer> serverPtr = serverApp.Get(0)->GetObject<UdpServer>();
        NS_TEST_ASSERT_MSG_NE(serverPtr, nullptr, "Server application should be installed.");

        // Install UDP Trace Client
        uint32_t MaxPacketSize = 1472;
        UdpTraceClientHelper client(serverAddress, port, "");
        client.SetAttribute("MaxPacketSize", UintegerValue(MaxPacketSize));
        ApplicationContainer clientApp = client.Install(n.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        Ptr<UdpTraceClient> clientPtr = clientApp.Get(0)->GetObject<UdpTraceClient>();
        NS_TEST_ASSERT_MSG_NE(clientPtr, nullptr, "Client application should be installed.");

        // Run simulation
        Simulator::Run();

        // Validate that packets were received
        uint32_t packetsReceived = serverPtr->GetReceived();
        NS_TEST_ASSERT_MSG_GT(packetsReceived, 0, "UDP Server should receive at least one packet.");

        Simulator::Destroy();
    }
};

// Register the test case
static UdpTraceClientServerTest udpTraceClientServerTest;
