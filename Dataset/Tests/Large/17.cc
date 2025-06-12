#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/test.h"

using namespace ns3;

class Ns3TestCase : public TestCase
{
public:
  Ns3TestCase() : TestCase("NS-3 Simulation Test Case") {}
  virtual void DoRun()
  {
    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);
    NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Incorrect number of nodes created");

    // Set up mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    NetDeviceContainer devices;
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    devices = p2p.Install(nodes);
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Verify IP addresses
    NS_TEST_ASSERT_MSG_EQ(interfaces.GetAddress(0), Ipv4Address("192.168.1.1"), "Incorrect IP address");
    NS_TEST_ASSERT_MSG_EQ(interfaces.GetAddress(1), Ipv4Address("192.168.1.2"), "Incorrect IP address");

    // Set up UDP communication
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
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

static Ns3TestCase ns3TestCaseInstance;
