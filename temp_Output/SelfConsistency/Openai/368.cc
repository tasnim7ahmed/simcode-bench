#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Enable logging for debugging if needed
    // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    // Set simulation time
    double simTime = 10.0;

    // Create nodes: 0 = STA, 1 = AP
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    NodeContainer staNode = NodeContainer(wifiNodes.Get(0));
    NodeContainer apNode = NodeContainer(wifiNodes.Get(1));

    // Setup Wi-Fi PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Set up 802.11n Wi-Fi MAC and standard
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);

    WifiMacHelper mac;

    Ssid ssid = Ssid("ns3-80211n");

    // Configure STA
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, staNode);

    // Configure AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, apNode);

    // Mobility
    MobilityHelper mobility;

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));

    // STA is mobile or static, for now static
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces = address.Assign(NetDeviceContainer(staDevices, apDevices));

    // Set up UDP server on AP
    uint16_t port = 50000;
    UdpServerHelper serverHelper(port);
    ApplicationContainer serverApps = serverHelper.Install(apNode.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    // Set up UDP client on STA
    UdpClientHelper clientHelper(interfaces.GetAddress(1), port);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(320));
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(0.03))); // ~33 packets/sec
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = clientHelper.Install(staNode.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simTime));

    // Enable PCAP tracing (optional)
    phy.EnablePcap("wifi-simple-80211n", apDevices.Get(0));
    phy.EnablePcap("wifi-simple-80211n", staDevices.Get(0), true);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}