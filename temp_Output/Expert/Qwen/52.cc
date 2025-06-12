#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTimingParametersExample");

int main(int argc, char *argv[])
{
    double slotTime = 9;
    double sifsTime = 16;
    double pifsTime = 25;
    double simulationTime = 10; // seconds

    CommandLine cmd(__FILE__);
    cmd.AddValue("slotTime", "Slot time in microseconds", slotTime);
    cmd.AddValue("sifsTime", "SIFS time in microseconds", sifsTime);
    cmd.AddValue("pifsTime", "PIFS time in microseconds", pifsTime);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNode;
    NodeContainer wifiApNode;
    wifiStaNode.Create(1);
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);

    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    Ssid ssid = Ssid("ns3-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNode);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(1.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterface;
    staInterface = address.Assign(staDevices);

    uint16_t port = 9;

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(wifiStaNode.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpClientHelper client(staInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295U));
    client.SetAttribute("Interval", TimeValue(Seconds(0.001)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(wifiApNode.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelSwitchingDelay", TimeValue(MicroSeconds(0)));

    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Slot", TimeValue(MicroSeconds(slotTime)));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Sifs", TimeValue(MicroSeconds(sifsTime)));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Pifs", TimeValue(MicroSeconds(pifsTime)));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}