#include "ns3/test.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpClientServerTest : public TestCase {
public:
    UdpClientServerTest() : TestCase("Test UDP Client-Server Communication") {}

    virtual void DoRun() {
        // Create nodes
        NodeContainer nodes;
        nodes.Create(2);

        // Install Internet Stack
        InternetStackHelper internet;
        internet.Install(nodes);

        // Create CSMA Channel
        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
        csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
        NetDeviceContainer devices = csma.Install(nodes);

        // Assign IP Addresses
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        // Setup UDP Server
        uint16_t port = 4000;
        UdpServerHelper server(port);
        ApplicationContainer serverApp = server.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Setup UDP Client
        uint32_t packetSize = 1024;
        uint32_t maxPacketCount = 10;
        Time interPacketInterval = Seconds(0.05);
        UdpClientHelper client(interfaces.GetAddress(1), port);
        client.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
        client.SetAttribute("Interval", TimeValue(interPacketInterval));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer clientApp = client.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Run Simulation
        Simulator::Run();
        
        // Check if packets are received
        Ptr<UdpServer> udpServer = DynamicCast<UdpServer>(serverApp.Get(0));
        NS_TEST_EXPECT_MSG_EQ(udpServer->GetReceived(), maxPacketCount, "Server should receive all packets");
        
        Simulator::Destroy();
    }
};

// Register the test case
static UdpClientServerTest udpClientServerTestInstance;
