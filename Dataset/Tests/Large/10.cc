#include "ns3/test.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"

using namespace ns3;

class Ns3UdpEchoTest : public TestCase {
public:
    Ns3UdpEchoTest() : TestCase("Test UDP Echo Application") {}
    virtual void DoRun() {
        NodeContainer nodes;
        nodes.Create(4);
        
        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
        csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
        csma.SetDeviceAttribute("Mtu", UintegerValue(1400));
        NetDeviceContainer devices = csma.Install(nodes);
        
        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApp = echoServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(1));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
        
        Simulator::Run();
        Simulator::Destroy();
    }
};

static Ns3UdpEchoTest g_ns3UdpEchoTest;
