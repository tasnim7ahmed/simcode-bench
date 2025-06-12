#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AodvAdhocExample");

int main (int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse (argc, argv);

    uint32_t numNodes = 5;
    double simTime = 20.0;

    // Create nodes
    NodeContainer nodes;
    nodes.Create (numNodes);

    // WiFi configuration
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());

    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

    // Mobility: Random position in 100x100m area
    MobilityHelper mobility;
    Ptr<UniformRandomVariable> xVar = CreateObject<UniformRandomVariable> ();
    Ptr<UniformRandomVariable> yVar = CreateObject<UniformRandomVariable> ();
    mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                   "X", PointerValue(xVar),
                                   "Y", PointerValue(yVar),
                                   "XMin", DoubleValue(0.0),
                                   "XMax", DoubleValue(100.0),
                                   "YMin", DoubleValue(0.0),
                                   "YMax", DoubleValue(100.0));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    // Install Internet stack with AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper (aodv);
    internet.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    // UDP Server application on node 4
    uint16_t port = 9;
    UdpServerHelper server (port);
    ApplicationContainer serverApp = server.Install (nodes.Get (4));
    serverApp.Start (Seconds (1.0));
    serverApp.Stop (Seconds (simTime));

    // UDP Client on node 0 (to node 4)
    UdpClientHelper client (interfaces.GetAddress (4), port);
    client.SetAttribute ("MaxPackets", UintegerValue (100));
    client.SetAttribute ("Interval", TimeValue (Seconds (0.2)));
    client.SetAttribute ("PacketSize", UintegerValue (64));
    ApplicationContainer clientApp = client.Install (nodes.Get (0));
    clientApp.Start (Seconds (2.0));
    clientApp.Stop (Seconds (simTime));

    // Enable pcap
    wifiPhy.EnablePcap ("aodv-adhoc", devices);

    // Tracing route discovery
    Config::Connect ("/NodeList/*/$ns3::Ipv4L3Protocol/Tx", 
        MakeCallback ([](Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t iff) {
            NS_LOG_INFO ("IPv4 Tx at " << Simulator::Now ().GetSeconds () << "s, node " << ipv4->GetObject<Node> ()->GetId ());
        })
    );

    Config::Connect ("/NodeList/*/$ns3::Ipv4L3Protocol/Rx", 
        MakeCallback ([](Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t iff) {
            NS_LOG_INFO ("IPv4 Rx at " << Simulator::Now ().GetSeconds () << "s, node " << ipv4->GetObject<Node> ()->GetId ());
        })
    );

    Config::Connect ("/NodeList/*/$ns3::aodv::RoutingProtocol/RouteRequest",
        MakeCallback ([](const Ipv4Address &src, const Ipv4Address &dst, uint32_t ttl) {
            NS_LOG_INFO ("AODV RouteRequest: src=" << src << ", dst=" << dst << ", ttl=" << ttl << " at " << Simulator::Now ().GetSeconds () << "s");
        })
    );

    Config::Connect ("/NodeList/*/$ns3::aodv::RoutingProtocol/RouteReply",
        MakeCallback ([](const Ipv4Address &src, const Ipv4Address &dst, uint32_t hopCount) {
            NS_LOG_INFO ("AODV RouteReply: src=" << src << ", dst=" << dst << ", hopCount=" << hopCount << " at " << Simulator::Now ().GetSeconds () << "s");
        })
    );

    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}