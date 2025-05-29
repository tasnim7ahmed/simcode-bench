#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: STA and AP
    NodeContainer staNodes;
    staNodes.Create(1);
    NodeContainer apNodes;
    apNodes.Create(1);

    // Configure WiFi channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure WiFi MAC layer
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();

    // Configure STA
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    // Configure AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(1.0)));

    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(staNodes);

    Ptr<ConstantPositionMobilityModel> apMobility = apNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    apMobility->SetPosition(Vector(0.0, 0.0, 0.0));
    Ptr<ConstantPositionMobilityModel> staMobility = staNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    staMobility->SetPosition(Vector(5.0, 0.0, 0.0));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevices);

    // UDP Echo Server on AP
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(apNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Echo Client on STA
    UdpEchoClientHelper echoClient(apInterface.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = echoClient.Install(staNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}