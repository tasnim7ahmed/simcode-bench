#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiRandomWalkSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes: 2 STAs and 1 AP
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Set up mobility models for mobile stations (random walk)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(10),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(wifiStaNodes);

    // Set fixed position for AP in center of area
    Ptr<ListPositionAllocator> apPositionAlloc = CreateObject<ListPositionAllocator>();
    apPositionAlloc->Add(Vector(50.0, 50.0, 0.0));
    mobility.SetPositionAllocator(apPositionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);

    // Configure Wi-Fi with 802.11n and 5GHz
    WifiMacHelper wifiMac;
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode", StringValue("HtMcs7"),
                                       "ControlMode", StringValue("HtMcs0"));

    Ssid ssid = Ssid("ns-3-ssid");

    // Setup MAC layers
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifiHelper.Install(wifiPhyHelper.Default(), wifiMac, wifiStaNodes);

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices = wifiHelper.Install(wifiPhyHelper.Default(), wifiMac, wifiApNode);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);

    // Setup UDP applications on port 9
    uint16_t port = 9;

    // From STA 0 to AP
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(wifiApNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(staInterfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(wifiStaNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // From STA 1 to AP
    UdpServerHelper server2(port);
    ApplicationContainer serverApps2 = server2.Install(wifiApNode.Get(0));
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(10.0));

    UdpClientHelper client2(staInterfaces.GetAddress(1), port);
    client2.SetAttribute("MaxPackets", UintegerValue(1000));
    client2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client2.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps2 = client2.Install(wifiStaNodes.Get(1));
    clientApps2.Start(Seconds(1.0));
    clientApps2.Stop(Seconds(10.0));

    // Enable PCAP tracing
    wifiPhyHelper.EnablePcapAll("wifi-random-walk");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}