#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;

    Ssid ssid = Ssid("ns3-ssid");

    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(1));

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(0));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(10.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    InternetStackHelper stack;
    stack.Install(wifiNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterface, staInterface;
    apInterface = address.Assign(apDevice);
    staInterface = address.Assign(staDevice);

    uint16_t port = 8080;

    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(wifiNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper udpClient(apInterface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(20));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = udpClient.Install(wifiNodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}