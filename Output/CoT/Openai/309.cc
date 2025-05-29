#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(2);

    // Wi-Fi configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    WifiMacHelper wifiMac;
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    // Use Adhoc since infra requires AP
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Fixed positions
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP server on node 1 (nodes.Get(1))
    uint16_t port = 8080;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP client on node 0 (nodes.Get(0))
    uint32_t maxPacketCount = 1000;
    uint32_t packetSize = 512;
    Time interPacketInterval = Seconds(0.01); // 10 ms
    UdpClientHelper udpClient(interfaces.GetAddress(1), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    udpClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = udpClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}