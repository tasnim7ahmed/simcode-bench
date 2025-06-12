#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTwoStaOneApSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes: 2 STAs and 1 AP
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Set up mobility models for STA nodes (Random Walk)
    MobilityHelper mobilitySta;
    mobilitySta.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                     "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                     "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobilitySta.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                 "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobilitySta.Install(wifiStaNodes);

    // Set fixed position for the AP node
    MobilityHelper mobilityAp;
    Ptr<ListPositionAllocator> apPositionAlloc = CreateObject<ListPositionAllocator>();
    apPositionAlloc->Add(Vector(50.0, 50.0, 0.0)); // Center of the area
    mobilityAp.SetPositionAllocator(apPositionAlloc);
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(wifiApNode);

    // Configure Wi-Fi with 802.11n at 5 GHz
    WifiMacHelper wifiMac;
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode", StringValue("HtMcs7"),
                                       "ControlMode", StringValue("HtMcs0"));

    // Create channel and PHY
    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(YansWifiChannelHelper::Default().Create());

    // Install AP and STA devices
    wifiMac.SetType("ns3::ApWifiMac");
    NetDeviceContainer apDevices = wifiHelper.Install(wifiPhy, wifiMac, wifiApNode);

    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", StringValue("wifi-network"));
    NetDeviceContainer staDevices = wifiHelper.Install(wifiPhy, wifiMac, wifiStaNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Setup UDP applications on both STAs sending to AP
    uint16_t port = 9;   // Discard port (UDP sink uses this)
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(wifiApNode.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper clientHelper(apInterfaces.GetAddress(0), port);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(10));
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps1 = clientHelper.Install(wifiStaNodes.Get(0));
    clientApps1.Start(Seconds(1.0));
    clientApps1.Stop(Seconds(10.0));

    ApplicationContainer clientApps2 = clientHelper.Install(wifiStaNodes.Get(1));
    clientApps2.Start(Seconds(1.0));
    clientApps2.Stop(Seconds(10.0));

    // Enable PCAP tracing
    wifiPhy.EnablePcap("wifi_two_sta_one_ap", apDevices.Get(0));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}