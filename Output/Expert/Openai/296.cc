#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    uint32_t nSta = 3;
    double simTime = 10.0;

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nSta);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;

    Ssid ssid = Ssid("ns3-wifi");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobilitySta;
    mobilitySta.SetPositionAllocator("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue(0.0),
                                    "MinY", DoubleValue(0.0),
                                    "DeltaX", DoubleValue(5.0),
                                    "DeltaY", DoubleValue(5.0),
                                    "GridWidth", UintegerValue(3),
                                    "LayoutType", StringValue("RowFirst"));
    mobilitySta.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                                "Distance", DoubleValue(5.0));
    mobilitySta.Install(wifiStaNodes);

    MobilityHelper mobilityAp;
    mobilityAp.SetPositionAllocator("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue(0.0),
                                   "MinY", DoubleValue(0.0),
                                   "DeltaX", DoubleValue(0.0),
                                   "DeltaY", DoubleValue(0.0),
                                   "GridWidth", UintegerValue(1),
                                   "LayoutType", StringValue("RowFirst"));
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(wifiApNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    UdpClientHelper udpClient(apInterface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(320));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(25)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = udpClient.Install(wifiStaNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}