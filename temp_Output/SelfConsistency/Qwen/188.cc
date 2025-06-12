#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/animation-interface.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiTwoStationsWithAP");

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create AP and STAs
    NodeContainer apNode;
    apNode.Create(1);

    NodeContainer staNodes;
    staNodes.Create(2);

    // Create channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Setup MAC and Wi-Fi network
    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHz);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs7"),
                                 "ControlMode", StringValue("HtMcs0"));

    // Set up BSS
    Ssid ssid = Ssid("wifi-network");
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    // Mobility models
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterface;
    Ipv4InterfaceContainer staInterfaces;

    apInterface = address.Assign(apDevice);
    staInterfaces = address.Assign(staDevices);

    // UDP flow from STA 0 to STA 1
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer sinkApp = server.Install(staNodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    UdpClientHelper client(staInterfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(staNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable ASCII tracing
    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("wifi-two-stations-ap.tr"));

    // Enable PCAP tracing
    phy.EnablePcap("wifi-two-stations-ap", apDevice.Get(0));
    phy.EnablePcap("wifi-two-stations-ap", staDevices.Get(0));
    phy.EnablePcap("wifi-two-stations-ap", staDevices.Get(1));

    // Animation
    AnimationInterface anim("wifi-two-stations-ap.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Assign node colors for visualization
    anim.UpdateNodeDescription(apNode.Get(0), "Access Point");
    anim.UpdateNodeColor(apNode.Get(0), 255, 0, 0); // Red

    anim.UpdateNodeDescription(staNodes.Get(0), "STA-Source");
    anim.UpdateNodeColor(staNodes.Get(0), 0, 255, 0); // Green

    anim.UpdateNodeDescription(staNodes.Get(1), "STA-Destination");
    anim.UpdateNodeColor(staNodes.Get(1), 0, 0, 255); // Blue

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}