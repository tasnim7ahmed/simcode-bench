#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/mesh-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-interface-container.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ZigBeeWsnExample");

int main(int argc, char *argv[])
{
    uint32_t numNodes = 10;
    double simTime = 10.0;
    double areaSide = 100.0;
    Time interPacketInterval = MilliSeconds(500);
    uint32_t packetSize = 128;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    // Mobility: RandomWalk2dMobilityModel in 100x100m area
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0.0, areaSide, 0.0, areaSide)),
                             "Distance", DoubleValue(5.0),
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=2.0]"));
    mobility.Install(nodes);

    // LrWpan (Zigbee) Setup
    LrWpanHelper lrWpanHelper;
    NetDeviceContainer devs = lrWpanHelper.Install(nodes);

    // PAN ID and ZigBee PRO mode
    lrWpanHelper.AssociateToPan(devs, 0x1337);
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        devs.Get(i)->SetAttribute("MacType", EnumValue(LrWpanMac::IEEE_802_15_4_MAC_TYPE_PAN_COORDINATOR));
    }
    devs.Get(0)->SetAttribute("MacType", EnumValue(LrWpanMac::IEEE_802_15_4_MAC_TYPE_PAN_COORDINATOR)); // Node 0 is coordinator
    for (uint32_t i = 1; i < numNodes; ++i)
    {
        devs.Get(i)->SetAttribute("MacType", EnumValue(LrWpanMac::IEEE_802_15_4_MAC_TYPE_END_DEVICE));
    }
    devs.Get(0)->GetObject<LrWpanNetDevice>()->GetMac()->SetAttribute("IsZigBeePRO", BooleanValue(true));
    for (uint32_t i = 1; i < numNodes; ++i)
    {
        devs.Get(i)->GetObject<LrWpanNetDevice>()->GetMac()->SetAttribute("IsZigBeePRO", BooleanValue(true));
    }

    // Create and configure the 802.15.4 PHY channel
    Ptr<YansWifiChannel> channel = CreateObject<YansWifiChannel>();
    Ptr<ConstantSpeedPropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel>();
    channel->SetPropagationDelayModel(delay);
    Ptr<LogDistancePropagationLossModel> loss = CreateObject<LogDistancePropagationLossModel>();
    channel->SetPropagationLossModel(loss);

    for (uint32_t i = 0; i < devs.GetN(); ++i)
    {
        Ptr<LrWpanPhy> phy = devs.Get(i)->GetObject<LrWpanNetDevice>()->GetPhy();
        phy->SetChannel(channel);
    }

    // Internet stack over 6LoWPAN
    SixLowPanHelper sixlowpan;
    NetDeviceContainer sixlowpanDevs = sixlowpan.Install(devs);

    InternetStackHelper internetv6;
    internetv6.Install(nodes);

    Ipv6AddressHelper ipv6h;
    ipv6h.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifs = ipv6h.Assign(sixlowpanDevs);
    ifs.SetForwarding(0, true);
    ifs.SetDefaultRouteInAllNodes(0);

    // Applications: Each end device sends 128-byte UDP to coordinator (node 0) every 500ms
    uint16_t port = 5683;
    Address coordinatorAddress = Address(ifs.GetAddress(0,1)); // Use link-local

    // UDP server on coordinator
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    // UDP clients on end devices
    for (uint32_t i = 1; i < numNodes; ++i)
    {
        UdpClientHelper udpClient(ifs.GetAddress(0,1), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(uint32_t(simTime*1000.0/500.0)));
        udpClient.SetAttribute("Interval", TimeValue(interPacketInterval));
        udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer client = udpClient.Install(nodes.Get(i));
        client.Start(Seconds(1.0));
        client.Stop(Seconds(simTime));
    }

    // Enable PCAP tracing on all LrWpan devices
    lrWpanHelper.EnablePcapAll("wsn-zigbee");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}