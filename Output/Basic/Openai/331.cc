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

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns-3-ssid");
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(0));

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices;
    apDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(1));

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(staDevices, apDevices));

    uint16_t port = 9;
    Address bindAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    Address remoteAddress(InetSocketAddress(interfaces.GetAddress(1), port));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", bindAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    OnOffHelper client("ns3::TcpSocketFactory", remoteAddress);
    client.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    client.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
    client.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    ApplicationContainer clientApp = client.Install(nodes.Get(0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}