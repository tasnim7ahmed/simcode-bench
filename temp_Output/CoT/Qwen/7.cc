#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RingTopologyUdpEcho");

int main (int argc, char *argv[])
{
    CommandLine cmd (__FILE__);
    cmd.Parse (argc, argv);

    Time::SetResolution (Time::NS);
    LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create (4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer devices[4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        uint32_t j = (i + 1) % 4;
        devices[i] = p2p.Install (nodes.Get(i), nodes.Get(j));
    }

    InternetStackHelper stack;
    stack.Install (nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[4];

    for (uint32_t i = 0; i < 4; ++i)
    {
        std::ostringstream subnet;
        subnet << "192.9.39." << (i * 64) << "/26";
        address.SetBase (subnet.str ().c_str (), "255.255.255.0");
        interfaces[i] = address.Assign (devices[i]);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    uint32_t packetSize = 1024;
    uint32_t maxPackets = 4;
    Time interPacketInterval = Seconds (1.0);

    for (uint32_t i = 0; i < 4; ++i)
    {
        uint32_t serverNode = (i + 1) % 4;
        uint32_t clientNode = i;

        UdpEchoServerHelper echoServer (9);
        ApplicationContainer serverApps = echoServer.Install (nodes.Get (serverNode));
        serverApps.Start (Seconds (i * 5.0));
        serverApps.Stop (Seconds (i * 5.0 + 5.0));

        UdpEchoClientHelper echoClient (interfaces[serverNode].GetAddress (0), 9);
        echoClient.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
        echoClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
        echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

        ApplicationContainer clientApps = echoClient.Install (nodes.Get (clientNode));
        clientApps.Start (Seconds (i * 5.0 + 1.0));
        clientApps.Stop (Seconds (i * 5.0 + 5.0));
    }

    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}