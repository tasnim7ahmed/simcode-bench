#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/yans-wifi-channel.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create nodes
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Set up Wi-Fi PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    // Set a fixed delay of 2ms for the Wi-Fi channel
    Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel>();
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.SetPropagationDelayModel(delayModel);
    channel.SetPropagationLoss("ns3::FriisPropagationLossModel"); // simple loss model

    Ptr<YansWifiChannel> wifiChannel = channel.Create();

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(wifiChannel);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    // MAC
    WifiMacHelper mac;

    Ssid ssid = Ssid("ns3-wifi");

    NetDeviceContainer devices;

    // Configure AP (node 1) and STA (node 0)
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    devices.Add(wifi.Install(phy, mac, wifiNodes.Get(0)));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    devices.Add(wifi.Install(phy, mac, wifiNodes.Get(1)));

    // Mobility
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

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // IP Assign
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Server on node 1
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiNodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(11.0));

    // UDP Client on node 0
    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 32000; // large enough to send until stop time
    Time interPacketInterval = Seconds(packetSize * 8.0 / 10e6); // 10 Mbps

    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    client.SetAttribute("Interval", TimeValue(interPacketInterval));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = client.Install(wifiNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable routing tables (optional for point-to-point, but good practice)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}